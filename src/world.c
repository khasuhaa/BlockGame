#include "world.h"

#include "water.h"

#include "tree.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- random numbers and noise ----------------------------------------- */

static uint32_t rng_next(uint32_t *state)
{
    /* xorshift32 */
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

static int rng_range(uint32_t *state, int lo, int hi)
{
    if (hi <= lo) return lo;
    return lo + (int)(rng_next(state) % (uint32_t)(hi - lo + 1));
}

static float rng_unit(uint32_t *state)
{
    return (float)(rng_next(state) >> 8) / 16777216.0f;
}

/* Repeatable value in [0, 1) for a lattice point. */
static float hash01(uint32_t seed, int32_t i)
{
    uint32_t h = seed ^ ((uint32_t)i * 0x9E3779B1u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return (float)(h >> 8) / 16777216.0f;
}

static float smoothstep(float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

/* Smooth noise in [-1, 1] along the world's circumference. It repeats
 * exactly every `width` columns, so there is no seam where the world wraps. */
static float ring_noise(uint32_t seed, int x, int width, int wavelength)
{
    int cells = wavelength > 0 ? width / wavelength : 1;
    if (cells < 1) cells = 1;
    float pos = (float)x * (float)cells / (float)width;
    int i = (int)pos;
    float a = hash01(seed, i % cells), b = hash01(seed, (i + 1) % cells);
    return (a + (b - a) * smoothstep(pos - (float)i)) * 2.0f - 1.0f;
}

/* ---- terrain ----------------------------------------------------------- */

/* Scratch columns used while shaping the land. */
typedef struct {
    float *level; /* feet level: Y of the lowest air tile */
    float *shape; /* 0 on the plains, rising to 1 at a peak or valley floor */
} Columns;

static float profile_at(const FeatureDef *f, float s)
{
    switch (f->profile) {
    case PROFILE_PEAK: return 0.5f * smoothstep(s) + 0.5f * s * s;
    case PROFILE_BASIN: return smoothstep(s * f->steepness);
    default: return smoothstep(s);
    }
}

/* Raises or sinks one feature into the columns, working outward from its
 * middle so the peak or floor comes out at exactly its size. A feature with
 * ledges samples its slope every few columns and holds it in between. */
static void shape_feature(World *w, Columns c, uint32_t *rng, const FeatureDef *f, int center,
                          int half, int size)
{
    for (int side = -1; side <= 1; side += 2) {
        float held = 0.0f;
        int run = 0;
        for (int d = side < 0 ? 0 : 1; d <= half; d++) {
            int x = world_wrap_x(w, center + side * d);
            float sh = profile_at(f, 1.0f - (float)d / (float)half);
            if (run == 0) {
                float slope = 4.0f * sh * (1.0f - sh); /* 0 at the foot and the top */
                float ridge = 0.5f + 0.5f * ring_noise(w->seed ^ 0x51u, x, w->width, 16);
                held = (float)size * (sh - 0.25f * f->shoulders * slope * ridge) +
                       f->jitter * (rng_unit(rng) * 2.0f - 1.0f) * slope;
                if (d == 0) held = (float)size;
                run = f->ledge_max > 0 ? rng_range(rng, f->ledge_min, f->ledge_max) : 1;
            }
            run--;
            c.level[x] += (float)f->direction * held;
            c.shape[x] = sh;
        }
    }
}

/* Limits how much neighbouring columns differ, outward from a feature's middle. */
static void limit_steps(World *w, int *level, int center, int half, int max_step)
{
    for (int side = -1; side <= 1; side += 2) {
        for (int d = 1; d <= half; d++) {
            int x = world_wrap_x(w, center + side * d), prev = world_wrap_x(w, center + side * (d - 1));
            if (level[x] > level[prev] + max_step) level[x] = level[prev] + max_step;
            if (level[x] < level[prev] - max_step) level[x] = level[prev] - max_step;
        }
    }
}

/* ---- veins ------------------------------------------------------------- */

typedef struct {
    int x, y;
} Cell;

static bool vein_can_take(const World *w, const VeinDef *v, int x, int y)
{
    static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    TileType t = world_get(w, x, y);
    bool host = false;
    for (int i = 0; i < v->host_count; i++) host = host || v->hosts[i] == t;
    if (!host || !world_in_bounds(w, x, y)) return false;
    /* A buried vein keeps a solid block between itself and the open air. */
    for (int i = 0; v->buried && i < 4; i++)
        if (!world_is_solid(w, x + dirs[i][0], y + dirs[i][1])) return false;
    /* Veins of one material keep clear of each other, corners included, so
     * two never join into one that is bigger than it should be. */
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (world_get(w, x + dx, y + dy) == v->tile) return false;
    return true;
}

static bool vein_add(const World *w, const VeinDef *v, Cell *cells, int *count, int x, int y)
{
    x = world_wrap_x(w, x);
    for (int i = 0; i < *count; i++)
        if (cells[i].x == x && cells[i].y == y) return false;
    if (!vein_can_take(w, v, x, y)) return false;
    cells[(*count)++] = (Cell){ x, y };
    return true;
}

/* Grows one vein from a starting block and writes it into the world, unless
 * it could not reach its smallest size, in which case nothing changes. */
static void place_vein(World *w, const VeinDef *v, uint32_t *rng, int x, int y)
{
    static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    Cell cells[MAX_VEIN_SIZE];
    int count = 0, target = (int)((float)rng_range(rng, v->min_size, v->max_size) * w->options.veins + 0.5f);
    int least = (int)((float)v->min_size * w->options.veins + 0.5f);
    if (target > MAX_VEIN_SIZE) target = MAX_VEIN_SIZE;
    if (target < 1) target = 1;
    if (least > target) least = target;
    if (least < 1) least = 1;
    if (!vein_add(w, v, cells, &count, x, y)) return;

    if (v->shape == VEIN_STREAK) {
        /* A scratch: it runs sideways on a shallow slant, wandering a little. */
        float fy = (float)y, slope = rng_unit(rng) * 1.2f - 0.6f;
        int dir = rng_range(rng, 0, 1) ? 1 : -1;
        for (int step = 1; count < target; step++) {
            fy += slope + (rng_unit(rng) - 0.5f) * 0.6f;
            /* It ends where it runs out of rock to cut through. */
            if (!vein_add(w, v, cells, &count, x + dir * step, (int)floorf(fy + 0.5f))) break;
        }
    } else {
        /* A clump: each new block is put beside one already in it. */
        for (int tries = target * 24; tries > 0 && count < target; tries--) {
            Cell from = cells[rng_range(rng, 0, count - 1)];
            const int *d = dirs[rng_range(rng, 0, 3)];
            vein_add(w, v, cells, &count, from.x + d[0], from.y + d[1]);
        }
    }

    if (count < least) return;
    for (int i = 0; i < count; i++) {
        /* (One in so many of its blocks, in a vein that has such a thing, is the rare kind.) */
        bool rare = v->rare != TILE_AIR && v->rare_one_in > 0 && rng_range(rng, 1, v->rare_one_in) == 1;
        world_set(w, cells[i].x, cells[i].y, rare ? v->rare : v->tile);
    }
}

/* A scar: a long gash through the rock, thick in the middle and running out
 * to nothing at its ends, on a slant that wanders. It takes the place of
 * whatever it crosses that it may replace, earlier scars included. */
static void place_scar(World *w, const VeinDef *v, uint32_t *rng, int x, int y)
{
    int length = (int)((float)rng_range(rng, v->min_size, v->max_size) * w->options.veins + 0.5f);
    int thick = (int)((float)rng_range(rng, v->min_thick, v->max_thick) * sqrtf(w->options.veins) + 0.5f);
    if (thick < 1) thick = 1;
    int dir = rng_range(rng, 0, 1) ? 1 : -1;
    float fy = (float)y, slope = rng_unit(rng) * 1.6f - 0.8f;
    for (int step = 0; step < length; step++) {
        float along = length > 1 ? (float)step / (float)(length - 1) : 0.5f;
        float across = (float)thick * (0.3f + 0.7f * sinf(along * 3.14159265f));
        int rows = (int)floorf(across + 0.5f), cx = world_wrap_x(w, x + dir * step);
        if (rows < 1) rows = 1;
        int top = (int)floorf(fy - across / 2.0f + 0.5f);
        for (int yy = top; yy < top + rows; yy++) {
            TileType t = world_get(w, cx, yy);
            bool host = false;
            for (int i = 0; i < v->host_count; i++) host = host || v->hosts[i] == t;
            if (host && world_in_bounds(w, cx, yy)) world_set(w, cx, yy, v->tile);
        }
        slope += (rng_unit(rng) - 0.5f) * 0.16f;
        if (slope > 1.1f) slope = 1.1f;
        if (slope < -1.1f) slope = -1.1f;
        fy += slope + (rng_unit(rng) - 0.5f) * 0.6f;
    }
}

/* Scatters every kind of vein through the columns. `level` is each column's feet level. */
static void place_veins(World *w, const int *level, uint32_t *rng)
{
    for (int k = 0; k < content.vein_count; k++) {
        const VeinDef *v = &content.veins[k];
        if (v->dimension >= 0) continue; /* another place's: see region_riches */
        float wanted = v->per_100_columns * (float)w->width / 100.0f;
        int attempts = (int)wanted + (rng_unit(rng) < wanted - floorf(wanted) ? 1 : 0);
        for (int i = 0; i < attempts; i++) {
            int x = rng_range(rng, 0, w->width - 1);
            int top = world_y_to_row(w, level[x] - 1);
            int y = top + rng_range(rng, v->min_depth, v->max_depth);
            if (v->shape == VEIN_SCAR) place_scar(w, v, rng, x, y);
            else place_vein(w, v, rng, x, y);
        }
    }
}

/* ---- islands in the sky ------------------------------------------------ */

/* Scatters islands through the sky: each a ragged wedge, widest at its top
 * and narrowing to a point underneath, of the base rock under soil and a
 * surface. Its top rises and falls, its underside juts, and the soil runs
 * deeper in some places than others, so nothing about it is a straight line;
 * and a cave winds through the rock of it from one end to the other, open to
 * the sky underneath at both. One goes only where there is nothing but air
 * for it and for a tree on top of it, and never over the spawn. */
static void place_islands(World *w, uint32_t *rng)
{
    const WorldGenDef *gen = &content.worldgen;
    if (gen->island_rate <= 0.0f || gen->island_base == TILE_AIR) return;
    float wanted = gen->island_rate * (float)w->width / 100.0f;
    int attempts = (int)wanted + (rng_unit(rng) < wanted - floorf(wanted) ? 1 : 0);
    int jag = gen->island_jag;
    int *tops = malloc(sizeof(int) * 3 * (size_t)(gen->island_max_length + 1));
    if (!tops) return;
    int *rock = tops + gen->island_max_length + 1, *ends = rock + gen->island_max_length + 1;
    for (int i = 0; i < attempts; i++) {
        int length = rng_range(rng, gen->island_min_length, gen->island_max_length);
        int depth = rng_range(rng, gen->island_min_depth, gen->island_max_depth);
        int soil = rng_range(rng, gen->island_min_soil, gen->island_max_soil);
        int x0 = rng_range(rng, 0, w->width - 1);
        /* (Its lowest point is no lower than islands go: so its top is that much higher.) */
        int lowest = gen->island_min_y + depth + jag, highest = gen->island_max_y > lowest ? gen->island_max_y : lowest;
        int top_y = rng_range(rng, lowest, highest), top = world_y_to_row(w, top_y);
        if (top - 14 - jag < 0 || top + depth + jag + 3 >= w->height) continue;
        bool clear = abs(world_dx(w, w->spawn_x, x0 + length / 2)) > length + 16;
        for (int dx = -3; clear && dx < length + 3; dx++)
            for (int y = top - 14 - jag; clear && y <= top + depth + jag + 3; y++)
                clear = world_get(w, x0 + dx, y) == TILE_AIR && world_get_at(w, LAYER_BACKGROUND, x0 + dx, y) == TILE_AIR;
        if (!clear) continue;
        int rise = jag > 0 ? rng_range(rng, 0, jag) : 0, soil_here = soil;
        float full = 0.72f + 0.28f * rng_unit(rng);
        for (int dx = 0; dx < length; dx++) {
            float across = ((float)dx + 0.5f) / (float)length, in = 1.0f - fabsf(2.0f * across - 1.0f);
            int deep;
            if (jag > 0) {
                /* The top wanders a row at a time; the soil thickens and thins; the underside is all juts. */
                rise += rng_range(rng, -1, 1);
                if (rise < 0) rise = 0;
                if (rise > jag) rise = jag;
                soil_here += rng_range(rng, -1, 1);
                if (soil_here < 1) soil_here = 1;
                if (soil_here > gen->island_max_soil + 1) soil_here = gen->island_max_soil + 1;
                /* (How full the wedge is drifts from column to column, so the underside bulges and
                 * pinches rather than bristling; and here and there a spur of rock hangs down from it.) */
                full += (rng_unit(rng) - 0.5f) * 0.22f;
                if (full < 0.72f) full = 0.72f;
                if (full > 1.0f) full = 1.0f;
                deep = (int)floorf((float)depth * powf(in, 0.6f) * full + 0.5f);
                if (rng_unit(rng) < 0.18f) deep += rng_range(rng, 1, jag);
                /* (Thick enough through the middle for the cave.) */
                if (in > 0.5f && deep < soil_here + 5) deep = soil_here + 5;
                if (deep > depth + jag) deep = depth + jag;
            } else {
                deep = (int)floorf((float)depth * powf(in, 0.85f) + (rng_unit(rng) - 0.5f) * 1.6f + 0.5f);
                if (deep > depth) deep = depth;
            }
            if (deep < 1) deep = 1;
            int first = top - rise, last = top + deep - 1;
            for (int y = first; y <= last; y++)
                world_set(w, x0 + dx, y, y == first ? gen->island_surface : y - first <= soil_here ? gen->island_soil : gen->island_base);
            tops[dx] = first;
            rock[dx] = first + soil_here + 1; /* the first row of rock, if there is any */
            ends[dx] = last;
        }
        /* The cave: along the rock, under a roof of it and over a floor of it, from where the rock is
         * first thick enough for one to where it last is - the rock at each end is left whole, and
         * holds the floor up - with a way down and out through the floor somewhere along it. */
        if (gen->island_max_cave > 0) {
            int from = -1, to = -1;
            for (int dx = 0; dx < length; dx++) {
                if (ends[dx] - rock[dx] + 1 < 3) continue;
                if (from < 0) from = dx;
                to = dx;
            }
            int cy = from >= 0 ? (rock[from] + ends[from]) / 2 : 0;
            int mouth = to - from > 2 ? rng_range(rng, from + 1, to - 1) : -1;
            for (int dx = from + 1; from >= 0 && dx < to; dx++) {
                /* (No lower than leaves a floor under it that joins the floor on either side.) */
                int floor_at = ends[dx];
                if (ends[dx - 1] < floor_at) floor_at = ends[dx - 1];
                if (ends[dx + 1] < floor_at) floor_at = ends[dx + 1];
                if (floor_at - rock[dx] < 2) continue;
                int high = rng_range(rng, gen->island_min_cave > 1 ? gen->island_min_cave : 1, gen->island_max_cave);
                int middle = (rock[dx] + floor_at) / 2;
                cy += (middle > cy) - (middle < cy) + (rng_unit(rng) < 0.4f ? rng_range(rng, -1, 1) : 0);
                if (cy < rock[dx] + 1) cy = rock[dx] + 1;
                if (cy > floor_at - 1) cy = floor_at - 1;
                for (int y = cy - (high - 1) / 2; y <= (dx == mouth ? ends[dx] : cy + high / 2); y++) {
                    if (y <= rock[dx] || (dx != mouth && y >= floor_at)) continue;
                    if (world_get(w, x0 + dx, y) == gen->island_base) world_set(w, x0 + dx, y, TILE_AIR);
                }
            }
        }
        /* Whatever the cave has cut loose from the rest - rock hanging in the air by itself - goes. */
        if (gen->island_max_cave > 0) {
            int rows = depth + 2 * jag + 2, first_row = top - jag;
            uint8_t *held = calloc((size_t)length, (size_t)rows);
            int *todo = malloc(sizeof(int) * (size_t)length * (size_t)rows);
            int n = 0;
            for (int dx = 0; held && todo && dx < length; dx++) {
                held[(tops[dx] - first_row) * length + dx] = 1;
                todo[n++] = (tops[dx] - first_row) * length + dx;
            }
            while (held && todo && n > 0) {
                static const int beside[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
                int at = todo[--n], ax = at % length, ay = at / length;
                for (int d = 0; d < 4; d++) {
                    int nx = ax + beside[d][0], ny = ay + beside[d][1];
                    if (nx < 0 || nx >= length || ny < 0 || ny >= rows || held[ny * length + nx]) continue;
                    if (world_get(w, x0 + nx, first_row + ny) == TILE_AIR) continue;
                    held[ny * length + nx] = 1;
                    todo[n++] = ny * length + nx;
                }
            }
            for (int ry = 0; held && todo && ry < rows; ry++)
                for (int dx = 0; dx < length; dx++)
                    if (!held[ry * length + dx] && world_get(w, x0 + dx, first_row + ry) != TILE_AIR)
                        world_set(w, x0 + dx, first_row + ry, TILE_AIR);
            free(held);
            free(todo);
        }
        /* A tree on some: one of the kinds that grow at this height, the commoner the likelier. */
        if (gen->island_tree_count == 0 || rng_unit(rng) >= gen->island_tree_chance) continue;
        int total = 0;
        for (int k = 0; k < gen->island_tree_count; k++)
            if (gen->island_trees[k].min_y <= top_y) total += gen->island_trees[k].weight;
        if (total <= 0) continue;
        int roll = rng_range(rng, 0, total - 1), kind = -1;
        for (int k = 0; k < gen->island_tree_count && kind < 0; k++) {
            if (gen->island_trees[k].min_y > top_y) continue;
            roll -= gen->island_trees[k].weight;
            if (roll < 0) kind = gen->island_trees[k].tree;
        }
        int at = length / 2 + rng_range(rng, -2, 2), tx = world_wrap_x(w, x0 + at);
        if (kind < 0 || at < 0 || at >= length || world_get(w, tx, tops[at]) != gen->island_surface) continue;
        tree_plant(w, tx, tops[at] - 1, kind);
        for (int steps = 0; steps < 400 && tree_grow(w, tx, tops[at] - 1, rng).grew; steps++) {}
    }
    free(tops);
}

/* ---- backwalls and caves ------------------------------------------------ */

/* Puts a backwall behind every block that leaves one. Done before anything
 * is carved or changed, so the walls record the ground as it first was. */
static void raise_walls(World *w)
{
    size_t cells = (size_t)w->width * w->height;
    for (size_t i = 0; i < cells; i++) {
        TileType t = w->layers[LAYER_FOREGROUND][i];
        if (tile_def(t)->wall) w->layers[LAYER_WALL][i] = (uint16_t)t;
    }
}

/* Carves one tunnel: a round brush that wanders, mostly sideways. Only rock
 * is ever removed, so a cave stops dead at the soil and at the open air. */
static void carve_cave(World *w, uint32_t *rng, int x, int y)
{
    const WorldGenDef *gen = &content.worldgen;
    float fx = (float)x, fy = (float)y;
    float angle = (rng_unit(rng) - 0.5f) * 1.6f + (rng_range(rng, 0, 1) ? 3.14159265f : 0.0f);
    float radius = gen->cave_min_radius + rng_unit(rng) * (gen->cave_max_radius - gen->cave_min_radius);
    int length = rng_range(rng, gen->cave_min_length, gen->cave_max_length);

    for (int step = 0; step < length; step++) {
        int reach = (int)ceilf(radius);
        for (int dy = -reach; dy <= reach; dy++) {
            for (int dx = -reach; dx <= reach; dx++) {
                int cx = (int)floorf(fx + 0.5f) + dx, cy = (int)floorf(fy + 0.5f) + dy;
                if ((float)(dx * dx + dy * dy) > radius * radius || cy < 1 || cy >= w->height - 2) continue;
                if (world_get(w, cx, cy) == gen->rock) world_set(w, cx, cy, TILE_AIR);
            }
        }
        angle += (rng_unit(rng) - 0.5f) * 0.7f;
        radius += (rng_unit(rng) - 0.5f) * 0.5f;
        if (radius < gen->cave_min_radius) radius = gen->cave_min_radius;
        if (radius > gen->cave_max_radius) radius = gen->cave_max_radius;
        fx += cosf(angle);
        fy += sinf(angle) * 0.6f;
    }
}

static void carve_caves(World *w, const int *level, uint32_t *rng)
{
    const WorldGenDef *gen = &content.worldgen;
    if (gen->cave_per_100_columns <= 0.0f || gen->cave_max_radius <= 0.0f) return;
    int count = (int)(gen->cave_per_100_columns * (float)w->width / 100.0f);
    for (int i = 0; i < count; i++) {
        /* A cave starts somewhere in rock; a few tries find some. */
        for (int tries = 0; tries < 12; tries++) {
            int x = rng_range(rng, 0, w->width - 1);
            int y = world_y_to_row(w, level[x] - 1) + rng_range(rng, gen->cave_min_depth, gen->cave_max_depth);
            if (y >= w->height - 2 || world_get(w, x, y) != gen->rock) continue;
            carve_cave(w, rng, x, y);
            break;
        }
    }
}

/* Sets gems, crystals and the like into the caves: in open space carved out
 * of the rock, against a block to sit on or cling to. */
static bool place_decor(World *w, const int *level, uint32_t *rng)
{
    const WorldGenDef *gen = &content.worldgen;
    static const int dirs[4][2] = { { 0, 1 }, { 1, 0 }, { -1, 0 }, { 0, -1 } };
    if (content.decor_count == 0) return true;

    size_t cells = (size_t)w->width * w->height;
    int *open = malloc(sizeof(int) * cells), open_count = 0;
    if (!open) return false;
    for (size_t i = 0; i < cells; i++)
        if (w->layers[LAYER_FOREGROUND][i] == TILE_AIR && w->layers[LAYER_WALL][i] == gen->rock)
            open[open_count++] = (int)i;

    for (int k = 0; k < content.decor_count && open_count > 0; k++) {
        const DecorDef *d = &content.decor[k];
        if (d->dimension >= 0) continue;
        int count = (int)(d->per_100_columns * (float)w->width / 100.0f + rng_unit(rng));
        for (int i = 0; i < count; i++) {
            for (int tries = 0; tries < 60; tries++) {
                int cell = open[rng_range(rng, 0, open_count - 1)];
                int x = cell % w->width, y = cell / w->width;
                int depth = y - world_y_to_row(w, level[x] - 1);
                if (depth < d->min_depth || depth > d->max_depth) continue;
                if (world_get_at(w, LAYER_BACKGROUND, x, y) != TILE_AIR) continue;
                bool held = false;
                if (d->hang_max > 0) {
                    /* Something that hangs: from the roof, down as far as it goes or there is room. */
                    if (!tile_def(world_get(w, x, y - 1))->solid) continue;
                    int layer = tile_def(d->tile)->layer;
                    for (int n = rng_range(rng, d->hang_min > 0 ? d->hang_min : 1, d->hang_max), k = 0; k < n; k++) {
                        if (!world_in_bounds(w, x, y + k) || world_get(w, x, y + k) != TILE_AIR ||
                            world_get_at(w, layer, x, y + k) != TILE_AIR)
                            break;
                        world_set_at(w, layer, x, y + k, d->tile);
                    }
                    break;
                }
                for (int n = 0; n < (d->on_floor ? 1 : 4); n++)
                    held = held || tile_def(world_get(w, x + dirs[n][0], y + dirs[n][1]))->solid;
                if (!held) continue;
                world_set_at(w, LAYER_BACKGROUND, x, y, d->tile);
                break;
            }
        }
    }
    free(open);
    return true;
}

/* ---- biomes ------------------------------------------------------------ */

/* How far inside a range a value is, as a share of `blend`: 0 at the edge or
 * outside, 1 once it is `blend` or more inside. */
static float inside(float v, float lo, float hi, float blend)
{
    float margin = fminf(v - lo, hi - v);
    if (margin <= 0.0f) return 0.0f;
    return blend > 0.0f ? fminf(1.0f, margin / blend) : 1.0f;
}

/* Decides what kind of country every column is. The sea claims everything
 * below its level outright. On land, heat and dryness - two smooth maps round
 * the world - pick the climate, and the shore biome reaches in from the sea.
 * `share` is how fully a column belongs to its biome: 1 well inside it,
 * falling toward 0 at its edge. The ground there is that biome's all the way
 * along, but thinner and thinner over what lies beneath (see the filling of
 * the columns), so one kind of country gives way to another as a wedge and
 * not as a line, nor as stripes. */
static void assign_biomes(World *w, const int *level, int *biome, float *share_out)
{
    const WorldGenDef *gen = &content.worldgen;
    int width = w->width, reach = 0;
    for (int b = 0; b < content.biome_count; b++)
        if (content.biomes[b].near_sea > reach) reach = content.biomes[b].near_sea;

    for (int x = 0; x < width; x++) {
        bool sea = gen->has_sea && level[x] < gen->sea_level;
        int to_sea = reach + 1;
        for (int d = 1; gen->has_sea && !sea && d <= reach && to_sea > reach; d++)
            if (level[world_wrap_x(w, x + d)] < gen->sea_level || level[world_wrap_x(w, x - d)] < gen->sea_level)
                to_sea = d;
        float heat = ring_noise(w->seed ^ 0xE5u, x, width, gen->heat_wavelength);
        float dry = ring_noise(w->seed ^ 0xF6u, x, width, gen->dryness_wavelength);
        /* Nothing but the first biome round the spawn; past that clearing
         * the others come in over as far again, not all at once. */
        int from_spawn = abs(world_dx(w, w->spawn_x, x)) - gen->climate_spawn_clearance;
        bool near_spawn = from_spawn < 0;
        float clearing = gen->climate_spawn_clearance > 0
                             ? fminf(1.0f, (float)(from_spawn + 1) / (float)gen->climate_spawn_clearance) : 1.0f;

        biome[x] = 0;
        share_out[x] = 1.0f;
        for (int b = 1; b < content.biome_count; b++) {
            const BiomeDef *def = &content.biomes[b];
            float share = 0.0f;
            if (def->dimension >= 0) continue; /* country of another place */
            /* (A sea with a climate of its own is that sea only where the climate is right: so there can be more than one kind.) */
            if (def->below_sea)
                share = !sea ? 0.0f
                        : def->has_climate ? fminf(inside(heat, def->heat_min, def->heat_max, gen->climate_blend), inside(dry, def->dry_min, def->dry_max, gen->climate_blend))
                                           : 1.0f;
            else if (sea) share = 0.0f;
            else if (def->near_sea > 0) share = 1.0f - (float)(to_sea - 1) / (float)def->near_sea;
            else if (def->has_climate && !near_spawn)
                share = clearing * fminf(inside(heat, def->heat_min, def->heat_max, gen->climate_blend),
                                         inside(dry, def->dry_min, def->dry_max, gen->climate_blend));
            if (share > 0.0f) {
                biome[x] = b;
                share_out[x] = share;
            }
        }
    }

    /* A world of sea: an island is one country from the list given, whatever the climate there - and a long
     * island one for each stretch of it. Its shores stay shore. (Chosen by the seed and where the island is,
     * not from the world's run of numbers.) */
    int kinds[16], kind_count = 0;
    for (int k = 0; gen->ocean && gen->has_sea && k < gen->isle_biome_count; k++)
        for (int b = 1; b < content.biome_count; b++)
            if (strcmp(content.biomes[b].name, gen->isle_biomes[k]) == 0 && content.biomes[b].dimension < 0) kinds[kind_count++] = b;
    for (int x = 0; kind_count > 0 && x < width;) {
        if (level[x] < gen->sea_level) { x++; continue; }
        int end = x;
        while (end < width && level[end] >= gen->sea_level) end++;
        int parts = (end - x + 109) / 110;
        for (int p = 0; p < parts; p++) {
            int from = x + (end - x) * p / parts, to = x + (end - x) * (p + 1) / parts;
            uint32_t h = (w->seed ^ 0x151Eu) * 2654435761u + (uint32_t)from * 40503u;
            h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
            for (int i = from; i < to; i++) {
                if (content.biomes[biome[i]].near_sea > 0) continue;
                biome[i] = kinds[h % (uint32_t)kind_count];
                share_out[i] = 1.0f;
            }
        }
        x = end;
    }
}

/* Which kind of cave a square underground is in, or -1: by its column and how far down it is. */
static int cave_biome_at(const World *w, int x, int depth)
{
    for (int k = 0; k < content.cave_biome_count; k++) {
        const CaveBiomeDef *c = &content.cave_biomes[k];
        if (depth < c->min_depth || depth > c->max_depth) continue;
        if (ring_noise(w->seed ^ (0xCA7Eu + (uint32_t)k * 7919u), x, w->width, c->wavelength) >= 1.0f - c->share) return k;
    }
    return -1;
}

/* Makes the caves of each kind what that kind is: their rock, the wall behind
 * them, and what grows in them. (Nothing here is drawn from the world's run
 * of random numbers: a world with no kinds of cave is as it would have been.) */
static void dress_caves(World *w, const int *level)
{
    static const int dirs[4][2] = { { 0, 1 }, { 1, 0 }, { -1, 0 }, { 0, -1 } };
    TileType rock = content.worldgen.rock;
    if (content.cave_biome_count == 0) return;
    for (int x = 0; x < w->width; x++) {
        int top = world_y_to_row(w, level[x] - 1);
        for (int y = top + 1; y < w->height - 1; y++) {
            if (world_get(w, x, y) != TILE_AIR || world_get_at(w, LAYER_WALL, x, y) != rock) continue;
            int kind = cave_biome_at(w, x, y - top);
            if (kind < 0) continue;
            const CaveBiomeDef *c = &content.cave_biomes[kind];
            if (c->lining != TILE_AIR) {
                world_set_at(w, LAYER_WALL, x, y, c->lining);
                for (int d = 0; d < 4; d++)
                    if (world_get(w, x + dirs[d][0], y + dirs[d][1]) == rock) world_set(w, x + dirs[d][0], y + dirs[d][1], c->lining);
            }
        }
    }
    /* What grows there: on a floor, or down from a roof. One thing to a square, the first whose turn comes up. */
    for (int x = 0; x < w->width; x++) {
        int top = world_y_to_row(w, level[x] - 1);
        for (int y = top + 1; y < w->height - 1; y++) {
            if (world_get(w, x, y) != TILE_AIR || world_get_at(w, LAYER_WALL, x, y) == TILE_AIR || world_get_at(w, LAYER_BACKGROUND, x, y) != TILE_AIR) continue;
            int kind = cave_biome_at(w, x, y - top);
            if (kind < 0) continue;
            const CaveBiomeDef *c = &content.cave_biomes[kind];
            bool placed = false;
            for (int i = 0; i < c->floor_count && !placed && world_is_solid(w, x, y + 1); i++) {
                if (hash01(w->seed ^ (0xF100u + (uint32_t)i * 131u), x * 8191 + y) >= 1.0f / (float)c->floor[i].one_in) continue;
                if (tile_def(c->floor[i].tile)->needs_water != world_is_water(w, x, y)) continue;
                world_set_at(w, tile_def(c->floor[i].tile)->layer, x, y, c->floor[i].tile);
                placed = true;
            }
            for (int i = 0; i < c->roof_count && !placed && world_is_solid(w, x, y - 1); i++) {
                if (hash01(w->seed ^ (0x700Fu + (uint32_t)i * 131u), x * 8191 + y) >= 1.0f / (float)c->roof[i].one_in) continue;
                int span = c->roof[i].hang_max - c->roof[i].hang_min + 1;
                int length = c->roof[i].hang_min + (span > 1 ? (int)(hash01(w->seed ^ 0x1E46u, x * 8191 + y) * (float)span) : 0);
                int layer = tile_def(c->roof[i].tile)->layer;
                for (int k = 0; k < length; k++) {
                    /* (Not right down onto the floor: a gap is left under it.) */
                    if (world_get(w, x, y + k) != TILE_AIR || world_get_at(w, layer, x, y + k) != TILE_AIR || world_is_solid(w, x, y + k + 1)) break;
                    world_set_at(w, layer, x, y + k, c->roof[i].tile);
                }
                placed = true;
            }
        }
    }
}

/* Here and there a biome has something buried in it: at the very bottom of
 * its own ground, with some of that ground over it. */
static void bury(World *w, const int *level, const int *biome)
{
    for (int x = 0; x < w->width; x++) {
        const BiomeDef *b = &content.biomes[biome[x]];
        if (b->buried == TILE_AIR || b->buried_one_in <= 0) continue;
        if (hash01(w->seed ^ 0x7EA5u, x) >= 1.0f / (float)b->buried_one_in) continue;
        int top = world_y_to_row(w, level[x] - 1), y = top;
        while (y < w->height - 1 && (world_get(w, x, y) == b->surface || world_get(w, x, y) == b->soil ||
                                     (b->seam != TILE_AIR && world_get(w, x, y) == b->seam)))
            y++;
        int lowest = y - 1;
        /* (Under at least one block of its own ground, on whatever lies beneath that.) */
        if (lowest <= top || !world_is_solid(w, x, y)) continue;
        world_set_at(w, tile_def(b->buried)->layer, x, lowest, b->buried);
    }
}

static int pick_tree(const BiomeDef *b, uint32_t *rng)
{
    int total = 0;
    for (int i = 0; i < b->tree_count; i++) total += b->trees[i].weight;
    if (total <= 0) return -1;
    int roll = rng_range(rng, 0, total - 1);
    for (int i = 0; i < b->tree_count; i++) {
        roll -= b->trees[i].weight;
        if (roll < 0) return b->trees[i].tree;
    }
    return -1;
}

/* Trees and plants, each kind of country with its own, at every stage of
 * growth from just planted to full size. */
static void plant_world(World *w, const int *level, const int *biome, uint32_t *rng)
{
    const WorldGenDef *gen = &content.worldgen;
    int last_tree = -1000;
    for (int x = 0; x < w->width; x++) {
        const BiomeDef *b = &content.biomes[biome[x]];
        int feet = world_y_to_row(w, level[x]);
        if (world_get(w, x, feet + 1) != b->surface || world_get(w, x, feet) != TILE_AIR) continue;
        if (abs(world_dx(w, w->spawn_x, x)) < gen->tree_spawn_clearance) continue;

        int spacing = b->tree_spacing > 0 ? b->tree_spacing : gen->tree_spacing;
        if (b->tree_one_in > 0 && x - last_tree >= spacing && rng_range(rng, 0, b->tree_one_in - 1) == 0) {
            int kind = pick_tree(b, rng);
            if (kind >= 0 && (!content.trees[kind].needs_water || world_is_water(w, x, feet))) {
                tree_plant(w, x, feet, kind);
                int steps = rng_unit(rng) < gen->tree_grown_chance ? 1000
                                                                  : rng_range(rng, 0, gen->tree_young_steps);
                while (steps-- > 0 && tree_grow(w, x, feet, rng).grew) {}
                last_tree = x;
                continue;
            }
        }
        for (int i = 0; i < b->plant_count; i++) {
            TileType t = b->plants[i].tile;
            if (b->plants[i].one_in <= 0 || rng_range(rng, 0, b->plants[i].one_in - 1) != 0) continue;
            if (tile_def(t)->needs_water && !world_is_water(w, x, feet)) continue;
            if (world_get_at(w, tile_def(t)->layer, x, feet) != TILE_AIR) continue;
            int layer = tile_def(t)->layer;
            world_set_at(w, layer, x, feet, t);
            /* A wild two-tone flower is one of its natural colours, all over. */
            if (tile_def(t)->two_tone && tile_def(t)->tone_color_count > 0) {
                const unsigned char *c = tile_def(t)->tone_colors[rng_range(rng, 0, tile_def(t)->tone_color_count - 1)];
                int color = tone_nearest(c[0], c[1], c[2]);
                world_set_meta(w, x, feet, tone_pack(color, color));
            }
            /* Anywhere along its growth, from sown to ripe. */
            int stages = 0;
            for (TileType s = t; tile_def(s)->grows_into != TILE_AIR && stages < 16; s = tile_def(s)->grows_into) stages++;
            for (int stage = rng_range(rng, 0, stages > 3 ? stages : 3); stage > 0 && world_grow_plant(w, layer, x, feet); stage--) {}
            break;
        }
    }
}

int world_pole_x(const World *w, int pole)
{
    if (pole < 0 || pole >= content.worldgen.pole_count) return w->spawn_x;
    return world_wrap_x(w, w->spawn_x + (int)floorf(content.worldgen.poles[pole].at * (float)w->width + 0.5f));
}

/* Stands the poles up: each on the ground of its column - or from the sea
 * bed up through the sea, where it is in the sea - and so many blocks above
 * that, with what goes on top of it on top. (Nothing random about it.) */
static void place_poles(World *w)
{
    const WorldGenDef *gen = &content.worldgen;
    for (int i = 0; i < gen->pole_count; i++) {
        int x = world_pole_x(w, i), ground = 0;
        while (ground < w->height - 1 && !world_is_solid(w, x, ground)) ground++;
        int foot = ground - 1, level = w->sea_row >= 0 && foot > w->sea_row ? w->sea_row : foot;
        int top = level - gen->poles[i].height + 1;
        if (top < 2) continue;
        int layer = tile_def(gen->poles[i].tile)->layer;
        for (int y = top; y <= foot; y++) {
            if (world_get(w, x, y) != TILE_AIR && layer != LAYER_FOREGROUND) world_set(w, x, y, TILE_AIR);
            world_set_at(w, layer, x, y, gen->poles[i].tile);
        }
        if (gen->poles[i].top != TILE_AIR) {
            int over = tile_def(gen->poles[i].top)->layer;
            for (int l = 0; l < BLOCK_LAYERS; l++) world_set_at(w, l, x, top - 1, TILE_AIR);
            world_set_at(w, over, x, top - 1, gen->poles[i].top);
        }
    }
}

/* ---- villages ------------------------------------------------------------ */

/* Sets a thing that is built down in the world, its bottom row on a row of the world. */
static void stamp_structure(World *w, const StructureDef *s, int x0, int base)
{
    for (int sy = 0; sy < s->height; sy++) {
        for (int sx = 0; sx < s->width; sx++) {
            int y = base - (s->height - 1) + sy;
            if (s->cells[sy][sx].keep || y < 1 || y >= w->height - 1) continue;
            size_t cell = (size_t)y * w->width + world_wrap_x(w, x0 + sx);
            w->layers[LAYER_FOREGROUND][cell] = (uint16_t)s->cells[sy][sx].front;
            w->layers[LAYER_BACKGROUND][cell] = (uint16_t)s->cells[sy][sx].back;
            /* (What has no wall of its own keeps whatever is behind it: the rock of a cave, or the sky.) */
            if (s->cells[sy][sx].wall != TILE_AIR) w->layers[LAYER_WALL][cell] = (uint16_t)s->cells[sy][sx].wall;
            w->water[cell] = 0;
            w->meta[cell] = 0;
        }
    }
    /* (A door takes its look from what it has been built into.) */
    for (int sy = 0; sy < s->height; sy++)
        for (int sx = 0; sx < s->width; sx++)
            if (!s->cells[sy][sx].keep && tile_def(s->cells[sy][sx].front)->mimics != MIMIC_NONE)
                world_fix_look(w, x0 + sx, base - (s->height - 1) + sy);
}

/* Makes one column of a village's site: open above a row, a street on it, and ground under that as far as there is none. */
static void village_column(World *w, const VillageDef *v, int x, int row, int clear, bool walled)
{
    for (int y = row - clear; y < row; y++) {
        if (y < 1) continue;
        size_t cell = (size_t)y * w->width + world_wrap_x(w, x);
        w->layers[LAYER_FOREGROUND][cell] = TILE_AIR;
        w->layers[LAYER_BACKGROUND][cell] = TILE_AIR;
        if (!walled) w->layers[LAYER_WALL][cell] = TILE_AIR;
        w->water[cell] = 0;
        w->meta[cell] = 0;
    }
    for (int y = row, filled = 0; y < w->height - 1 && filled < 40; y++, filled++) {
        size_t cell = (size_t)y * w->width + world_wrap_x(w, x);
        bool solid = tile_def((TileType)w->layers[LAYER_FOREGROUND][cell])->solid;
        if (y > row + 2 && solid) break;
        w->layers[LAYER_FOREGROUND][cell] = (uint16_t)(y == row ? v->street : v->ground);
        w->layers[LAYER_BACKGROUND][cell] = TILE_AIR;
        if (walled && tile_def(v->ground)->wall) w->layers[LAYER_WALL][cell] = (uint16_t)v->ground;
        w->water[cell] = 0;
    }
}

/* The villages of a new world: so many of each kind, each on a site made for it. They are made last of all,
 * from numbers of their own, so that the rest of the world is what it would have been without them. */
static void place_villages(World *w, const int *level)
{
    const WorldGenDef *gen = &content.worldgen;
    uint32_t rng = w->seed ^ 0x51A6E5u;
    if (rng == 0) rng = 1;
    int placed[64], placed_count = 0;
    for (int k = 0; k < content.village_count; k++) {
        const VillageDef *v = &content.villages[k];
        float more = v->per_1000_columns * (float)w->width / 1000.0f;
        int want = v->count_min + (int)more + (rng_unit(&rng) < more - floorf(more) ? 1 : 0);
        for (int n = 0; n < want && placed_count < 64; n++) {
            /* What it is made of, left to right: buildings, the centre, more buildings, with a little street between each. */
            int plan[MAX_VILLAGE_HOUSES + 1], gaps[MAX_VILLAGE_HOUSES + 1], count = rng_range(&rng, v->houses_min, v->houses_max), total = 6;
            if (count > MAX_VILLAGE_HOUSES) count = MAX_VILLAGE_HOUSES;
            int middle = count / 2;
            for (int i = 0; i <= count; i++) {
                plan[i] = i == middle ? v->centre : v->houses[rng_range(&rng, 0, v->house_count - 1)];
                gaps[i] = rng_range(&rng, 2, 4);
                total += content.structures[plan[i]].width + gaps[i];
            }
            if (total + 60 > w->width / 2) continue;
            for (int go = 0; go < 80; go++) {
                int cx = rng_range(&rng, 0, w->width - 1), x0 = cx - total / 2, row = 0;
                bool ok = abs(world_dx(w, w->spawn_x, cx)) > total / 2 + 40;
                for (int i = 0; ok && i < placed_count; i++) ok = abs(world_dx(w, placed[i], cx)) > total + 120;
                /* Never in the sea, nor under it. */
                int low = level[world_wrap_x(w, cx)], high = low;
                for (int dx = -6; ok && dx < total + 6; dx++) {
                    int at = level[world_wrap_x(w, x0 + dx)];
                    if (at < low) low = at;
                    if (at > high) high = at;
                }
                ok = ok && (!gen->has_sea || low > gen->sea_level + 2);
                if (!ok) continue;
                if (v->where == VILLAGE_GROUND) {
                    if (high - low > 16) continue;
                    row = world_y_to_row(w, (low + high) / 2) + 1;
                    for (int dx = -2; dx < total + 2; dx++) village_column(w, v, x0 + dx, row, 22, false);
                    /* The land either side comes up or down to it a step at a time. */
                    for (int side = -1; side <= 1; side += 2) {
                        int h = row;
                        for (int step = 1; step <= 30; step++) {
                            int x = side < 0 ? x0 - 2 - step : x0 + total + 1 + step, natural = world_y_to_row(w, level[world_wrap_x(w, x)]) + 1;
                            if (natural == h) break;
                            h += natural > h ? 1 : -1;
                            village_column(w, v, x, h, natural < h ? h - natural + 3 : 3, false);
                            w->layers[LAYER_FOREGROUND][(size_t)h * w->width + world_wrap_x(w, x)] = (uint16_t)v->ground;
                        }
                    }
                } else if (v->where == VILLAGE_CAVE) {
                    row = world_y_to_row(w, low) + rng_range(&rng, 40, 110);
                    if (row > w->height - 24) continue;
                    for (int dx = -3; dx < total + 3; dx++) {
                        /* (A hall: lower at its ends than in the middle.) */
                        int edge = dx + 3 < total + 2 - dx ? dx + 3 : total + 2 - dx;
                        village_column(w, v, x0 + dx, row, edge < 4 ? 8 + edge * 2 : 16, true);
                    }
                } else {
                    row = world_y_to_row(w, rng_range(&rng, gen->island_min_y + 15, gen->island_max_y > gen->island_min_y + 30 ? gen->island_max_y : gen->island_min_y + 60));
                    if (row < 30 || row > world_y_to_row(w, high) - 30) continue;
                    /* (Not through an island that is there already.) */
                    bool clear = true;
                    for (int dx = 0; clear && dx < total; dx += 3)
                        for (int dy = -18; clear && dy <= 8; dy += 2) clear = world_get(w, x0 + dx, row + dy) == TILE_AIR;
                    if (!clear) continue;
                    int span = total + 8;
                    for (int i = 0; i < span; i++) {
                        float t = ((float)i + 0.5f) / (float)span * 2.0f - 1.0f;
                        int thick = 2 + (int)(11.0f * sqrtf(1.0f - t * t) + rng_unit(&rng) * 1.5f);
                        for (int d = 0; d < thick; d++) {
                            size_t cell = (size_t)(row + d) * w->width + world_wrap_x(w, x0 - 4 + i);
                            if (row + d >= w->height - 2) break;
                            TileType tile = d == 0 ? v->street : d <= 3 ? v->ground : gen->island_base != TILE_AIR ? gen->island_base : gen->rock;
                            w->layers[LAYER_FOREGROUND][cell] = (uint16_t)tile;
                            if (d > 0) w->layers[LAYER_WALL][cell] = (uint16_t)tile;
                        }
                    }
                }
                int x = x0 + 3;
                for (int i = 0; i <= count; i++) {
                    stamp_structure(w, &content.structures[plan[i]], x, row);
                    x += content.structures[plan[i]].width + gaps[i];
                }
                placed[placed_count++] = world_wrap_x(w, cx);
                break;
            }
        }
    }
}

/* An ocean world: the land the world would have had is put aside, and in its place is a sea bed with islands
 * standing out of it - one where the world begins, then one after another round it, every so many a great one.
 * (All from numbers of its own, so that a world made without it is what it always was.) */
static void ocean_levels(World *w, int *level)
{
    const WorldGenDef *gen = &content.worldgen;
    uint32_t rng = w->seed ^ 0x0CEA11u;
    if (rng == 0) rng = 1;
    int width = w->width;
    for (int x = 0; x < width; x++) {
        float deep = (ring_noise(w->seed ^ 0xDEE9u, x, width, 70) + 1.0f) * 0.5f;
        level[x] = gen->sea_level - gen->sea_depth_min - (int)(deep * (float)(gen->sea_depth_max - gen->sea_depth_min)) - (int)(hash01(w->seed ^ 0x5EAu, x / 3) * 2.0f);
    }
    int at = w->spawn_x - 9, count = 0;
    int done = 0;
    while (done < width - gen->isle_gap_min) {
        bool great = count > 0 && gen->great_every > 0 && count % gen->great_every == gen->great_every - 1;
        int length = count == 0 ? 18 : great ? rng_range(&rng, gen->great_min, gen->great_max) : rng_range(&rng, gen->isle_min, gen->isle_max);
        /* (Most are small: the long ones are the rarer.) */
        if (count > 0 && !great && rng_range(&rng, 0, 2) != 0) length = gen->isle_min + (length - gen->isle_min) / 2;
        if (done + length > width - gen->isle_gap_min) break;
        int peak = great ? gen->great_peak : 3 + (gen->isle_peak - 3) * length / (gen->isle_max > 0 ? gen->isle_max : 1);
        peak = count == 0 ? 3 : rng_range(&rng, peak / 2 + 2, peak + 2);
        int flat = rng_range(&rng, 0, 2);
        for (int i = 0; i < length; i++) {
            int x = world_wrap_x(w, at + i);
            float t = ((float)i + 0.5f) / (float)length, edge = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
            /* A beach either end, rising to the middle - or to a table, or to two hills with a saddle between. */
            float shape = flat == 0 ? smoothstep(edge) : flat == 1 ? smoothstep(edge * 2.5f) : smoothstep(edge) * (0.55f + 0.45f * fabsf(t - 0.5f) * 2.0f + 0.2f);
            int high = gen->sea_level + 2 + (int)(shape * (float)peak + ring_noise(w->seed ^ 0x151Eu, x, width, 9) * (great ? 3.0f : 1.2f) * shape);
            /* (The shore shelves away under the water.) */
            if (high > level[x]) level[x] = high;
        }
        for (int side = -1; side <= 1; side += 2) {
            for (int k = 1; k <= 12; k++) {
                int x = world_wrap_x(w, side < 0 ? at - k : at + length - 1 + k), shelf = gen->sea_level - k;
                if (shelf > level[x]) level[x] = shelf;
            }
        }
        int gap = rng_range(&rng, gen->isle_gap_min, gen->isle_gap_max);
        at += length + gap;
        done += length + gap;
        count++;
    }
}

/* What lies on the bottom of an ocean world: wrecks, each where the water is deep enough to hide it. */
static void sink_wrecks(World *w, const int *level)
{
    const WorldGenDef *gen = &content.worldgen;
    uint32_t rng = w->seed ^ 0x3EC4ED5u;
    if (rng == 0) rng = 1;
    /* And under the beaches, a little way down, what somebody buried. */
    TileType buried = gen->buried_name[0] ? tile_id(gen->buried_name) : -1;
    for (int n = 0; buried > 0 && n < (int)(gen->buried_rate * (float)w->width / 1000.0f); n++) {
        for (int go = 0; go < 60; go++) {
            int x = rng_range(&rng, 0, w->width - 1);
            if (level[x] <= gen->sea_level || level[x] > gen->sea_level + 7) continue;
            int row = world_y_to_row(w, level[x]) + rng_range(&rng, 3, 5);
            if (!world_is_solid(w, x, row) || !world_is_solid(w, x, row - 1) || abs(world_dx(w, w->spawn_x, x)) < 12) continue;
            world_set(w, x, row, buried);
            break;
        }
    }
    int want = (int)(gen->wreck_rate * (float)w->width / 1000.0f);
    for (int n = 0; n < want && gen->wreck_count > 0; n++) {
        const char *which = gen->wrecks[rng_range(&rng, 0, gen->wreck_count - 1)];
        const StructureDef *s = NULL;
        for (int i = 0; i < content.structure_count; i++)
            if (strcmp(content.structures[i].name, which) == 0) s = &content.structures[i];
        if (!s) continue;
        for (int go = 0; go < 40; go++) {
            int x = rng_range(&rng, 0, w->width - 1), low = level[x], high = low;
            for (int dx = 0; dx < s->width; dx++) {
                int here = level[world_wrap_x(w, x + dx)];
                if (here < low) low = here;
                if (here > high) high = here;
            }
            if (high > gen->sea_level - s->height - 2 || high - low > 3) continue;
            stamp_structure(w, s, x, world_y_to_row(w, low));
            break;
        }
    }
}

static bool generate_terrain(World *w)
{
    const WorldGenDef *gen = &content.worldgen;
    uint32_t rng = w->seed ? w->seed : 0x9E3779B9u;
    for (int i = 0; i < 8; i++) rng_next(&rng);

    int width = w->width;
    Columns c = { calloc((size_t)width, sizeof(float)), calloc((size_t)width, sizeof(float)) };
    int *level = calloc((size_t)width, sizeof(int));
    if (!c.level || !c.shape || !level) {
        free(c.level);
        free(c.shape);
        free(level);
        return false;
    }

    /* Plains everywhere to begin with: broad, gentle swells. */
    for (int x = 0; x < width; x++) {
        c.level[x] = (float)gen->plains_level;
        for (int i = 0; i < gen->swell_count; i++)
            c.level[x] += gen->swells[i].amplitude *
                          ring_noise(w->seed ^ (0xA1u + (uint32_t)i * 0x11u), x, width,
                                     gen->swells[i].wavelength);
    }

    /* Walk round the world laying down features with a stretch of plains
     * before each, so no two of them run into each other. The first stretch,
     * at column 0, holds the spawn. */
    enum { MAX_SPANS = 256 };
    int spans[MAX_SPANS][3], span_count = 0;
    w->spawn_x = world_wrap_x(w, rng_range(&rng, gen->spawn_min, gen->spawn_max));
    int pos = gen->first_feature_at;
    int kind = content.feature_count > 0 ? rng_range(&rng, 0, content.feature_count - 1) : -1;
    while (kind >= 0) {
        const FeatureDef *f = &content.features[kind];
        int plains = rng_range(&rng, gen->gap_min, gen->gap_max);
        /* (Bigger or smaller than the data says, by what the world was asked for.) */
        float asked = f->direction > 0 ? w->options.mountains : w->options.valleys;
        int size = (int)((float)rng_range(&rng, f->min_size, f->max_size) * asked + 0.5f);
        if (size < 2) size = 2;
        if (size > w->height / 2 - 60) size = w->height / 2 - 60;
        int half = (int)((float)size * f->width_per_size) + f->width_base;
        int center = pos + plains + half;
        if (half < 1 || center + half + gen->gap_min > width) break;
        shape_feature(w, c, &rng, f, center, half, size);
        if (f->max_step > 0 && span_count < MAX_SPANS) {
            spans[span_count][0] = center;
            spans[span_count][1] = half;
            spans[span_count++][2] = f->max_step;
        }
        pos = center + half;
        if (content.feature_count > 1)
            kind = (kind + rng_range(&rng, 1, content.feature_count - 1)) % content.feature_count;
    }

    /* Ease the land down to Y 0 around the spawn point. */
    for (int d = -gen->ease_radius; d <= gen->ease_radius; d++) {
        int x = world_wrap_x(w, w->spawn_x + d);
        float span = (float)(gen->ease_radius - gen->flat_radius);
        c.level[x] *= smoothstep(span > 0.0f ? ((float)abs(d) - (float)gen->flat_radius) / span : 1.0f);
    }

    for (int x = 0; x < width; x++) level[x] = (int)floorf(c.level[x] + 0.5f);
    for (int i = 0; i < span_count; i++) limit_steps(w, level, spans[i][0], spans[i][1], spans[i][2]);
    if (gen->ocean && gen->has_sea) ocean_levels(w, level);

    int *biome = calloc((size_t)width, sizeof(int));
    float *share = calloc((size_t)width, sizeof(float));
    if (!biome || !share) {
        free(biome);
        free(share);
        free(c.level);
        free(c.shape);
        free(level);
        return false;
    }
    w->sea_row = gen->has_sea ? world_y_to_row(w, gen->sea_level) : -1;
    assign_biomes(w, level, biome, share);
    for (int x = 0; x < width; x++) w->biome[x] = (uint8_t)biome[x];

    /* Fill the columns: the biome's surface over its soil over rock. The
     * soil thins out into a feature until it is bare rock, unless the biome
     * keeps a depth of its own, as the sea bed does. (First how deep each
     * layer is in every column, then the blocks.) */
    int *layers = calloc((size_t)width * 3, sizeof(int));
    if (!layers) {
        free(biome);
        free(share);
        free(c.level);
        free(c.shape);
        free(level);
        return false;
    }
    int *soil_of = layers, *own_of = layers + width, *under_of = layers + 2 * width;
    for (int x = 0; x < width; x++) {
        const BiomeDef *b = &content.biomes[biome[x]];
        float bare = smoothstep(gen->bare_at > 0.0f ? c.shape[x] / gen->bare_at : 1.0f);
        float depth = (gen->soil_depth +
                       gen->soil_variation * ring_noise(w->seed ^ 0xC3u, x, width, 12)) *
                      (1.0f - bare);
        depth += (hash01(w->seed ^ 0xD4u, x) - 0.5f) * 0.8f * (bare > 0.0f ? 1.0f : 0.0f);
        if (b->soil_depth > 0.0f) depth = b->soil_depth + (hash01(w->seed ^ 0xD4u, x) - 0.5f) * 0.8f;
        int soil = (int)floorf(depth + 0.5f);
        /* At the edge of its country a biome's ground is only a skin over the
         * ordinary soil, a block deep at the very edge and deepening inward. */
        int own = share[x] >= 1.0f ? soil : 1 + (int)floorf(share[x] * (float)(soil > 1 ? soil - 1 : 0) + 0.5f);
        /* Some country has rock of its own under its ground - but only
         * where its own ground goes all the way down to the rock. At the
         * country's edge, where its ground is a skin over the ordinary soil,
         * the ordinary rock lies under that; from where the skin has
         * deepened to the full depth, its own rock comes in beneath, thin at
         * first and thickening inward. So the wedge that the ground makes
         * above is carried on by the rock below. It is solid for the top
         * half of its depth, then more and more broken up by the ordinary rock. */
        int under = 0;
        if (b->under != TILE_AIR && soil >= 1 && own >= soil) {
            float from = soil > 1 ? ((float)soil - 1.5f) / ((float)soil - 1.0f) : 0.0f;
            float in = share[x] >= 1.0f || from >= 1.0f ? 1.0f : (share[x] - from) / (1.0f - from);
            under = (int)floorf((float)b->under_depth * fmaxf(0.0f, fminf(1.0f, in)) + 0.5f);
        }
        soil_of[x] = soil;
        own_of[x] = own;
        under_of[x] = under;
    }
    /* That rock comes and goes by degrees along the ground too: never more
     * than a couple of blocks deeper than in the column beside it, so it
     * runs out to nothing wherever it stops and does not end in a cliff. */
    for (int pass = 0; pass < 2; pass++) {
        for (int x = 0; x < width * 2; x++) {
            int at = pass == 0 ? x % width : (width * 2 - 1 - x) % width;
            int beside = (at + (pass == 0 ? width - 1 : 1)) % width;
            if (under_of[at] > under_of[beside] + 2) under_of[at] = under_of[beside] + 2;
        }
    }
    for (int x = 0; x < width; x++) {
        const BiomeDef *b = &content.biomes[biome[x]];
        int soil = soil_of[x], own = own_of[x], under = under_of[x];
        int top = world_y_to_row(w, level[x] - 1);
        for (int y = top < 0 ? 0 : top; y < w->height; y++) {
            int below = y - top, deep = below - soil;
            TileType rock = gen->rock;
            if (deep >= 0 && deep < under &&
                (deep * 2 < under || hash01(w->seed ^ 0x5A4Du, x * 8191 + y) < 2.0f * (float)(under - deep) / (float)under - 0.15f))
                rock = b->under;
            TileType ground = below == 0 ? b->surface : below < own ? b->soil : content.biomes[0].soil;
            /* The layer of ground lying on that rock has something else mixed through it. */
            if (under > 0 && b->seam != TILE_AIR && below == soil - 1 && below > 0 &&
                hash01(w->seed ^ 0xB1A5u, x * 8191 + y) < b->seam_share)
                ground = b->seam;
            world_set(w, x, y, below >= soil ? rock : ground);
        }
    }
    free(layers);
    free(share);

    raise_walls(w);
    carve_caves(w, level, &rng);
    if (!place_decor(w, level, &rng)) {
        free(c.level);
        free(c.shape);
        free(level);
        free(biome);
        return false;
    }
    place_veins(w, level, &rng);
    dress_caves(w, level);
    bury(w, level, biome);
    /* (After the backwalls were raised: an island has only the sky behind it.) */
    place_islands(w, &rng);
    /* The sea, before anything that only grows under it is planted. */
    water_fill_sea(w);
    plant_world(w, level, biome, &rng);
    place_poles(w);
    if (content.village_count > 0) place_villages(w, level);
    if (gen->ocean && gen->has_sea) sink_wrecks(w, level);
    free(biome);

    free(c.level);
    free(c.shape);
    free(level);
    w->spawn_y = world_y_to_row(w, 0);
    return true;
}

bool world_stamp(World *w, const char *structure, int x0, int base, int *width, int *height)
{
    const StructureDef *s = NULL;
    for (int i = 0; i < content.structure_count; i++)
        if (strcmp(content.structures[i].name, structure) == 0) s = &content.structures[i];
    if (!s) return false;
    stamp_structure(w, s, x0, base);
    for (int sy = 0; sy < s->height; sy++)
        for (int sx = 0; sx < s->width; sx++) {
            int x = world_wrap_x(w, x0 + sx), y = base - (s->height - 1) + sy;
            if (y < 1 || y >= w->height - 1) continue;
            size_t cell = (size_t)y * w->width + x;
            bool open = !tile_def((TileType)w->layers[LAYER_FOREGROUND][cell])->solid && w->layers[LAYER_WALL][cell] == TILE_AIR;
            if (w->sea_row >= 0 && y > w->sea_row && open) w->water[cell] = WATER_FULL;
        }
    w->devices_known = false;
    if (width) *width = s->width;
    if (height) *height = s->height;
    return true;
}

void world_unstamp(World *w, int x0, int base, int width, int height)
{
    for (int sy = 0; sy < height; sy++)
        for (int sx = 0; sx < width; sx++) {
            int x = world_wrap_x(w, x0 + sx), y = base - (height - 1) + sy;
            if (y < 1 || y >= w->height - 1) continue;
            size_t cell = (size_t)y * w->width + x;
            for (int l = 0; l < LAYER_COUNT; l++) w->layers[l][cell] = TILE_AIR;
            w->meta[cell] = 0;
            w->paint[0][cell] = w->paint[1][cell] = 0;
            w->water[cell] = w->sea_row >= 0 && y > w->sea_row ? WATER_FULL : 0;
        }
    w->devices_known = false;
}

bool world_build_allowed(const World *w, int x, int y, TileType tile)
{
    if (!w->build_limit) return true;
    int dx = world_dx(w, w->spawn_x, x);
    if (dx < w->build_x0 || dx > w->build_x1 || y < w->build_y0 || y > w->build_y1) return false;
    return tile_def(tile)->ship[SHIP_WEIGHT] <= w->build_room;
}

/* A world adrift: sea from the waterline down to a floor nobody reaches, and afloat in the middle of it the
 * structure the data names - the ship as she begins. */
static void generate_adrift(World *w)
{
    const WorldGenDef *gen = &content.worldgen;
    int water = gen->adrift_waterline;
    if (water < 8) water = 8;
    if (water > w->height - 8) water = w->height - 8;
    w->sea_row = water - 1;
    w->y_top = w->sea_row;
    w->spawn_x = w->width / 2;
    w->spawn_y = w->sea_row;
    for (int x = 0; x < w->width; x++) w->layers[LAYER_FOREGROUND][(size_t)(w->height - 1) * w->width + x] = (uint16_t)gen->soil;
    int wide = 0;
    for (int i = 0; i < content.structure_count; i++)
        if (strcmp(content.structures[i].name, gen->adrift_start) == 0) wide = content.structures[i].width;
    world_stamp(w, gen->adrift_start, w->spawn_x - wide / 2, water, NULL, NULL);
}

/* Level soil; the seed only picks the ground level and spawn column. */
static void generate_flat(World *w)
{
    uint32_t rng = w->seed ? w->seed : 0x9E3779B9u;
    for (int i = 0; i < 8; i++) rng_next(&rng);

    w->surface_y = w->height / 2 + rng_range(&rng, -w->height / 8, w->height / 8);
    w->spawn_x = rng_range(&rng, w->width / 4, w->width * 3 / 4);
    w->spawn_y = w->surface_y - 1;
    w->y_top = w->spawn_y;

    for (int y = w->surface_y; y < w->height; y++)
        for (int x = 0; x < w->width; x++) w->layers[LAYER_FOREGROUND][(size_t)y * w->width + x] = (uint16_t)content.worldgen.soil;
}

/* ---- the debug world --------------------------------------------------- */

#define DEBUG_FIRST 14 /* the column the first exhibit stands in: clear of the portal */
#define DEBUG_GAP 2    /* empty columns between one exhibit and the next */

static void debug_put(World *w, int layer, int x, int y, TileType t)
{
    if (w) world_set_at(w, layer, x, y, t);
}

/* One block, whole: every tile of its height from `row` up, and its other half beside it if it has one. */
static void debug_block(World *w, TileType t, int x, int row)
{
    const TileDef *def = tile_def(t);
    for (int i = 0; i < def->height; i++) debug_put(w, def->layer, x, row - i, t);
    if (def->pair != TILE_AIR) debug_put(w, tile_def(def->pair)->layer, x + 1, row, def->pair);
    for (int i = 0; w && def->mimics != MIMIC_NONE && i < def->height; i++) world_fix_look(w, x, row - i);
}

/* Sets out one of every block along the ground from column `x`, each in every
 * way it can be put up, and returns the column after the last. `ground` is
 * the row a block standing on the ground is in. Anything that goes on, under
 * or beside another block is shown against a block of the soil. With no
 * world, nothing is put anywhere: only the room it would take is worked out. */
static int debug_exhibits(World *w, int ground, int x)
{
    TileType holder = content.worldgen.soil;
    for (TileType t = 1; t < content.tile_count; t++) {
        const TileDef *def = tile_def(t);
        if (def->pair_of != TILE_AIR) continue; /* (the other half of something: shown with it) */
        int wide = def->pair != TILE_AIR ? 2 : 1, tall = def->height, air = ground - 2;
        bool mounted = def->hangs || def->texture_mount[0][0] || def->texture_mount[1][0] || def->texture_mount[2][0];
        if (!mounted) {
            /* As it is, standing on the ground. */
            debug_block(w, t, x, ground);
            x += wide + DEBUG_GAP;
        } else {
            /* On a backwall; */
            if (!def->hangs && def->layer != LAYER_FOREGROUND) {
                for (int i = 0; i < wide; i++)
                    for (int k = 0; k < tall; k++) debug_put(w, LAYER_WALL, x + i, air - k, holder);
                debug_block(w, t, x, air);
                x += wide + DEBUG_GAP;
            }
            /* on top of a block; */
            if (!def->hangs) {
                for (int i = 0; i < wide; i++) debug_put(w, LAYER_FOREGROUND, x + i, ground, holder);
                debug_block(w, t, x, ground - 1);
                x += wide + DEBUG_GAP;
            }
            /* under one; */
            for (int i = 0; i < wide; i++) debug_put(w, LAYER_FOREGROUND, x + i, air - tall, holder);
            debug_block(w, t, x, air);
            x += wide + DEBUG_GAP;
            /* and to the right and the left of one. */
            if (!def->hangs) {
                for (int k = 0; k < tall; k++) debug_put(w, LAYER_FOREGROUND, x, air - k, holder);
                debug_block(w, t, x + 1, air);
                x += wide + 1 + DEBUG_GAP;
                debug_block(w, t, x, air);
                for (int k = 0; k < tall; k++) debug_put(w, LAYER_FOREGROUND, x + wide, air - k, holder);
                x += wide + 1 + DEBUG_GAP;
            }
        }
        if (def->steps) {
            /* Steps: a flight rising to the right, and one to the left. */
            for (int k = 0; k < 3; k++) debug_put(w, def->layer, x + k, ground - k, t);
            x += 3 + DEBUG_GAP;
            for (int k = 0; k < 3; k++) debug_put(w, def->layer, x + 2 - k, ground - k, t);
            x += 3 + DEBUG_GAP;
        }
        if (def->mimics == MIMIC_UPRIGHT) {
            /* A door between two blocks, taking their look. */
            debug_put(w, LAYER_FOREGROUND, x, ground - tall, holder);
            debug_block(w, t, x, ground);
            x += 1 + DEBUG_GAP;
        } else if (def->mimics == MIMIC_FLAT) {
            debug_put(w, LAYER_FOREGROUND, x, air, holder);
            debug_put(w, LAYER_FOREGROUND, x + 2, air, holder);
            debug_block(w, t, x + 1, air);
            x += 3 + DEBUG_GAP;
        }
        if (def->join_count > 0 || def->wire) {
            /* Something that joins to its like: nine of it together, and a cross of it. */
            for (int i = 0; i < 3; i++)
                for (int k = 0; k < 3; k++) debug_put(w, def->layer, x + i, ground - k, t);
            x += 3 + DEBUG_GAP;
            for (int k = 0; k < 3; k++) {
                debug_put(w, def->layer, x + k, ground - 1, t);
                debug_put(w, def->layer, x + 1, ground - k, t);
            }
            x += 3 + DEBUG_GAP;
        }
    }
    return x;
}

int world_debug_width(void)
{
    int wide = debug_exhibits(NULL, 0, DEBUG_FIRST) + DEBUG_FIRST;
    return (wide + 63) / 64 * 64;
}

/* Level soil, the portal near its left end, and everything set out to the right of it. */
static void generate_debug(World *w)
{
    w->surface_y = w->height / 2;
    w->spawn_x = DEBUG_FIRST / 2;
    w->spawn_y = w->surface_y - 1;
    w->y_top = w->spawn_y;
    for (int y = w->surface_y; y < w->height; y++)
        for (int x = 0; x < w->width; x++) w->layers[LAYER_FOREGROUND][(size_t)y * w->width + x] = (uint16_t)content.worldgen.soil;
    raise_walls(w);
    debug_exhibits(w, w->surface_y - 1, DEBUG_FIRST);
}

bool world_init(World *w, int width, int height, uint32_t seed, WorldKind kind)
{
    return world_init_with(w, width, height, seed, kind, NULL);
}

bool world_init_with(World *w, int width, int height, uint32_t seed, WorldKind kind, const WorldOptions *options)
{
    memset(w, 0, sizeof *w);
    w->options = options ? *options : (WorldOptions){ 1.0f, 1.0f, 1.0f };
    if (w->options.mountains <= 0.0f) w->options.mountains = 1.0f;
    if (w->options.valleys <= 0.0f) w->options.valleys = 1.0f;
    if (w->options.veins <= 0.0f) w->options.veins = 1.0f;
    if (width < 4 || height < 8) return false;
    for (int i = 0; i < LAYER_COUNT; i++) {
        w->layers[i] = calloc((size_t)width * (size_t)height, sizeof(uint16_t));
        if (!w->layers[i]) {
            world_free(w);
            return false;
        }
    }
    w->biome = calloc((size_t)width, 1);
    w->water = calloc((size_t)width, (size_t)height);
    w->water_awake = calloc((size_t)width, 1);
    w->meta = calloc((size_t)width * (size_t)height, sizeof(uint16_t));
    w->paint[0] = calloc((size_t)width, (size_t)height);
    w->paint[1] = calloc((size_t)width, (size_t)height);
    if (!w->biome || !w->water || !w->water_awake || !w->meta || !w->paint[0] || !w->paint[1]) {
        world_free(w);
        return false;
    }
    w->width = width;
    w->height = height;
    w->seed = seed;
    w->y_top = height / 2;
    w->sea_row = -1;

    if (kind == WORLD_BLANK) return true;
    if (kind == WORLD_DEBUG) {
        generate_debug(w);
    } else if (kind == WORLD_FLAT) {
        generate_flat(w);
        raise_walls(w);
    } else if (content.worldgen.adrift) {
        generate_adrift(w);
    } else if (!generate_terrain(w)) {
        world_free(w);
        return false;
    }

    /* The spawn portal: a base block set in the ground and the portal above. */
    const WorldGenDef *gen = &content.worldgen;
    world_set(w, w->spawn_x, w->spawn_y + 1, gen->portal_base);
    for (int i = 0; i < gen->portal_height; i++)
        world_set_at(w, tile_def(gen->portal_body)->layer, w->spawn_x, w->spawn_y - i,
                     i == gen->portal_height - 1 ? gen->portal_top : gen->portal_body);
    /* A new world's water has already found its level: the sea has run into
     * whatever caves open on it, and is full and at rest. */
    if (kind == WORLD_TERRAIN) water_make_sea(w);
    memset(w->water_awake, 0, (size_t)w->width);
    w->water_stirring = false;
    return true;
}

void world_free(World *w)
{
    for (int i = 0; i < LAYER_COUNT; i++) free(w->layers[i]);
    free(w->biome);
    free(w->water);
    free(w->water_awake);
    free(w->meta);
    free(w->paint[0]);
    free(w->paint[1]);
    free(w->devices);
    memset(w, 0, sizeof *w);
}

int world_region_at(const World *w, int y)
{
    for (int i = 0; i < w->region_count; i++)
        if (y >= w->regions[i].y0 && y < w->regions[i].y0 + w->regions[i].height) return i;
    return -1;
}

int world_region_of(const World *w, int dim)
{
    for (int i = 0; dim >= 0 && i < w->region_count; i++)
        if (w->regions[i].dim == dim) return i;
    return -1;
}

int world_dimension_at(const World *w, int y)
{
    int r = world_region_at(w, y);
    return r < 0 ? -1 : w->regions[r].dim;
}

void world_region_rows(const World *w, int region, int *y0, int *y1)
{
    bool home = region < 0 || region >= w->region_count;
    *y0 = home ? 0 : w->regions[region].y0;
    *y1 = home ? (w->region_count > 0 ? w->home_height : w->height) : w->regions[region].y0 + w->regions[region].height;
}

int world_sky_top(const World *w, int y)
{
    int r = world_region_at(w, y);
    if (r < 0) return 0;
    int dim = w->regions[r].dim;
    return dim >= 0 && dim < content.dimension_count && content.dimensions[dim].sky ? w->regions[r].y0 + 1 : -1;
}

int world_biome_at(const World *w, int x, int y)
{
    int r = world_region_at(w, y);
    if (r < 0 || !w->biome) return world_biome(w, x);
    int b = w->biome[(size_t)(r + 1) * w->width + world_wrap_x(w, x)];
    return b < content.biome_count ? b : 0;
}

/* ---- dimensions --------------------------------------------------------- */

/* Sets a block and the backwall behind it. */
static void put_rock(World *w, int x, int y, TileType t)
{
    size_t cell = (size_t)y * w->width + world_wrap_x(w, x);
    w->layers[LAYER_FOREGROUND][cell] = (uint16_t)t;
    w->layers[LAYER_WALL][cell] = (uint16_t)t;
}

/* Which biome each column of a dimension is: stretches of one and then another, the first of them where the world began. */
static void region_biomes(World *w, int region, uint32_t *rng)
{
    const Region *g = &w->regions[region];
    const DimensionDef *d = &content.dimensions[g->dim];
    uint8_t *out = w->biome + (size_t)(region + 1) * w->width;
    int kinds[MAX_BIOMES], count = 0;
    for (int i = 0; i < content.biome_count; i++)
        if (content.biomes[i].dimension == g->dim) kinds[count++] = i;
    if (count == 0) return;
    int wave = d->climate_wavelength > 8 ? d->climate_wavelength : 8, last = 0;
    int start = world_wrap_x(w, w->spawn_x - wave / 2);
    for (int done = 0, first = 1; done < w->width; first = 0) {
        int run = first ? wave : rng_range(rng, wave / 2, wave + wave / 2), pick = 0;
        if (!first && count > 1) do pick = rng_range(rng, 0, count - 1); while (pick == last);
        for (int i = 0; i < run && done < w->width; i++, done++) out[world_wrap_x(w, start + done)] = (uint8_t)kinds[pick];
        last = pick;
    }
}

/* Trees and plants on every bit of a dimension's ground that is open above. */
static void region_plant(World *w, int region, uint32_t *rng)
{
    const Region *g = &w->regions[region];
    for (int x = 0; x < w->width; x++) {
        const BiomeDef *b = &content.biomes[world_biome_at(w, x, g->y0 + 1)];
        if (b->dimension != g->dim) continue;
        for (int y = g->y0 + 6; y < g->y0 + g->height - 2; y++) {
            if (world_get(w, x, y + 1) != b->surface || world_get(w, x, y) != TILE_AIR || world_is_water(w, x, y)) continue;
            if (world_get(w, x, y - 1) != TILE_AIR || world_get(w, x, y - 2) != TILE_AIR) continue;
            /* What stands up out of the ground here: a column of something, or two with a beam across them. */
            bool stood = false;
            for (int i = 0; i < b->spike_count && !stood; i++) {
                if (b->spikes[i].hang || b->spikes[i].one_in <= 0 || rng_range(rng, 0, b->spikes[i].one_in - 1) != 0) continue;
                int tall = rng_range(rng, b->spikes[i].min, b->spikes[i].max), span = b->spikes[i].arch ? rng_range(rng, 3, 5) : 0;
                int layer = tile_def(b->spikes[i].tile)->layer;
                if (b->spikes[i].arch && (world_get(w, x + span, y + 1) != b->surface || world_get(w, x + span, y) != TILE_AIR)) continue;
                for (int k = 0; k < tall && world_get(w, x, y - k) == TILE_AIR && y - k > g->y0 + 3; k++) {
                    world_set_at(w, layer, x, y - k, b->spikes[i].tile);
                    if (span > 0 && world_get_at(w, layer, x + span, y - k) == TILE_AIR) world_set_at(w, layer, x + span, y - k, b->spikes[i].tile);
                    if (span > 0 && k == tall - 1)
                        for (int dx = 1; dx < span; dx++)
                            if (world_get_at(w, layer, x + dx, y - k) == TILE_AIR) world_set_at(w, layer, x + dx, y - k, b->spikes[i].tile);
                }
                stood = true;
            }
            if (stood) continue;
            if (b->treasure != TILE_AIR && b->treasure_one_in > 0 && rng_range(rng, 0, b->treasure_one_in - 1) == 0 &&
                world_get_at(w, tile_def(b->treasure)->layer, x, y) == TILE_AIR) {
                world_set_at(w, tile_def(b->treasure)->layer, x, y, b->treasure);
                continue;
            }
            if (b->tree_one_in > 0 && rng_range(rng, 0, b->tree_one_in - 1) == 0 && world_get_at(w, LAYER_BACKGROUND, x - 1, y) == TILE_AIR &&
                world_get_at(w, LAYER_BACKGROUND, x - 2, y) == TILE_AIR) {
                int kind = pick_tree(b, rng);
                if (kind >= 0 && !content.trees[kind].needs_water) {
                    tree_plant(w, x, y, kind);
                    int steps = rng_unit(rng) < 0.7f ? 1000 : rng_range(rng, 0, 6);
                    while (steps-- > 0 && tree_grow(w, x, y, rng).grew) {}
                    continue;
                }
            }
            for (int i = 0; i < b->plant_count; i++) {
                TileType t = b->plants[i].tile;
                int layer = tile_def(t)->layer;
                if (b->plants[i].one_in <= 0 || rng_range(rng, 0, b->plants[i].one_in - 1) != 0) continue;
                if (tile_def(t)->needs_water || world_get_at(w, layer, x, y) != TILE_AIR) continue;
                world_set_at(w, layer, x, y, t);
                if (tile_def(t)->two_tone && tile_def(t)->tone_color_count > 0) {
                    const unsigned char *c = tile_def(t)->tone_colors[rng_range(rng, 0, tile_def(t)->tone_color_count - 1)];
                    int color = tone_nearest(c[0], c[1], c[2]);
                    world_set_meta(w, x, y, tone_pack(color, color));
                }
                int stages = 0;
                for (TileType s = t; tile_def(s)->grows_into != TILE_AIR && stages < 16; s = tile_def(s)->grows_into) stages++;
                for (int stage = rng_range(rng, 0, stages > 3 ? stages : 3); stage > 0 && world_grow_plant(w, layer, x, y); stage--) {}
                break;
            }
        }
    }
}

/* What hangs from the undersides of a dimension's ground and from the roofs of its caves. */
static void region_hangings(World *w, int region, uint32_t *rng)
{
    const Region *g = &w->regions[region];
    for (int x = 0; x < w->width; x++) {
        const BiomeDef *b = &content.biomes[world_biome_at(w, x, g->y0 + 1)];
        if (b->dimension != g->dim) continue;
        for (int y = g->y0 + 6; y < g->y0 + g->height - 6; y++) {
            if (world_get(w, x, y) != TILE_AIR || world_is_water(w, x, y)) continue;
            TileType over = world_get(w, x, y - 1);
            if (over == TILE_AIR || !tile_def(over)->wall || tile_def(over)->harm > 0.0f) continue;
            for (int i = 0; i < b->spike_count; i++) {
                if (!b->spikes[i].hang || b->spikes[i].one_in <= 0 || rng_range(rng, 0, b->spikes[i].one_in - 1) != 0) continue;
                int tall = rng_range(rng, b->spikes[i].min, b->spikes[i].max), layer = tile_def(b->spikes[i].tile)->layer;
                /* (Never right down to what is under it: there is always a way past.) */
                for (int k = 0; k < tall && world_get(w, x, y + k) == TILE_AIR && world_get(w, x, y + k + 3) == TILE_AIR && y + k < g->y0 + g->height - 6; k++)
                    world_set_at(w, layer, x, y + k, b->spikes[i].tile);
                break;
            }
        }
    }
}

/* Geodes: hollow balls of a rock of their own, with a gem growing over the inside of each. */
static void region_geodes(World *w, int region, uint32_t *rng)
{
    const Region *g = &w->regions[region];
    const DimensionDef *d = &content.dimensions[g->dim];
    for (int k = 0; k < d->geode_count; k++) {
        int count = (int)(d->geodes[k].per_100_columns * (float)w->width / 100.0f + rng_unit(rng));
        int lo = g->y0 + 14 + d->geodes[k].min_depth, hi = g->y0 + d->geodes[k].max_depth;
        if (hi > g->y0 + g->height - 16) hi = g->y0 + g->height - 16;
        for (int n = 0; n < count && lo <= hi; n++) {
            int cx = rng_range(rng, 0, w->width - 1), cy = rng_range(rng, lo, hi), r = rng_range(rng, d->geodes[k].min_radius, d->geodes[k].max_radius);
            for (int dy = -r - 2; dy <= r + 2; dy++) {
                for (int dx = -r - 3; dx <= r + 3; dx++) {
                    /* (Wider than it is high.) */
                    float far = sqrtf((float)(dx * dx) * 0.7f + (float)(dy * dy));
                    size_t cell = (size_t)(cy + dy) * w->width + world_wrap_x(w, cx + dx);
                    if (far > (float)r + 2.0f) continue;
                    if (far > (float)r) {
                        put_rock(w, cx + dx, cy + dy, d->geodes[k].shell);
                    } else {
                        w->layers[LAYER_FOREGROUND][cell] = TILE_AIR;
                        w->layers[LAYER_WALL][cell] = (uint16_t)d->geodes[k].shell;
                        w->layers[LAYER_BACKGROUND][cell] = TILE_AIR;
                        world_set_water(w, cx + dx, cy + dy, 0);
                        /* The gem, on whatever of the inside is against the shell. */
                        if (d->geodes[k].gem != TILE_AIR && far > (float)r - 1.3f && rng_range(rng, 0, 2) == 0)
                            w->layers[tile_def(d->geodes[k].gem)->layer][cell] = (uint16_t)d->geodes[k].gem;
                    }
                }
            }
        }
    }
}

/* The veins and the things set into the rock that belong to a dimension. */
static void region_riches(World *w, int region, uint32_t *rng)
{
    const Region *g = &w->regions[region];
    for (int i = 0; i < content.vein_count; i++) {
        const VeinDef *v = &content.veins[i];
        if (v->dimension != g->dim) continue;
        int lo = g->y0 + 2 + v->min_depth, hi = g->y0 + (v->max_depth > 0 ? v->max_depth : g->height) ;
        if (hi > g->y0 + g->height - 2) hi = g->y0 + g->height - 2;
        if (lo > hi) continue;
        int tries = (int)(v->per_100_columns * (float)w->width / 100.0f * w->options.veins);
        for (int n = 0; n < tries; n++) {
            /* (A few goes at finding rock that will take it: there is a lot of air.) */
            for (int go = 0; go < 12; go++) {
                int x = rng_range(rng, 0, w->width - 1), y = rng_range(rng, lo, hi);
                if (!vein_can_take(w, v, x, y)) continue;
                if (v->shape == VEIN_SCAR) place_scar(w, v, rng, x, y);
                else place_vein(w, v, rng, x, y);
                break;
            }
        }
    }
    for (int i = 0; i < content.decor_count; i++) {
        const DecorDef *dc = &content.decor[i];
        if (dc->dimension != g->dim) continue;
        int lo = g->y0 + 2 + dc->min_depth, hi = g->y0 + (dc->max_depth > 0 ? dc->max_depth : g->height);
        if (hi > g->y0 + g->height - 3) hi = g->y0 + g->height - 3;
        if (lo > hi) continue;
        int layer = tile_def(dc->tile)->layer;
        int want = (int)(dc->per_100_columns * (float)w->width / 100.0f);
        for (int n = 0; n < want; n++) {
            for (int go = 0; go < 40; go++) {
                int x = rng_range(rng, 0, w->width - 1), y = rng_range(rng, lo, hi);
                if (world_get(w, x, y) != TILE_AIR || world_get_at(w, layer, x, y) != TILE_AIR || world_is_water(w, x, y)) continue;
                if (dc->hang_max > 0) {
                    if (!tile_def(world_get(w, x, y - 1))->solid) continue;
                    int reach = rng_range(rng, dc->hang_min > 0 ? dc->hang_min : 1, dc->hang_max);
                    for (int k = 0; k < reach && world_get(w, x, y + k) == TILE_AIR && world_get_at(w, layer, x, y + k) == TILE_AIR; k++)
                        world_set_at(w, layer, x, y + k, dc->tile);
                    break;
                }
                bool floor = tile_def(world_get(w, x, y + 1))->solid;
                bool held = floor || tile_def(world_get(w, x, y - 1))->solid || tile_def(world_get(w, x - 1, y))->solid || tile_def(world_get(w, x + 1, y))->solid;
                if (dc->on_floor ? !floor : !held) continue;
                world_set_at(w, layer, x, y, dc->tile);
                break;
            }
        }
    }
}

/* The shapes an island comes in. */
enum { ISLE_LENS, ISLE_MESA, ISLE_SPIRE, ISLE_HILL, ISLE_BASIN, ISLE_KINDS };

/* One island: ground with its top the biome's surface and its heart the rock. A lens is flat-topped and ragged
 * and thins to nothing at its ends; a mesa is a slab, as thick at its edge as its middle; a spire is a peak on
 * top of a long point; a hill humps up in the middle; a basin is a level lens with a lake let into its top. */
static void region_island(World *w, int region, uint32_t *rng, int cx, int cy, int length, int depth, int kind)
{
    const Region *g = &w->regions[region];
    const DimensionDef *d = &content.dimensions[g->dim];
    int rise = 0;
    if (kind == ISLE_BASIN && length < 22) kind = ISLE_LENS;
    for (int i = 0; i < length; i++) {
        int x = cx - length / 2 + i;
        const BiomeDef *b = &content.biomes[world_biome_at(w, x, g->y0 + 1)];
        float t = ((float)i + 0.5f) / (float)length * 2.0f - 1.0f, edge = 1.0f - fabsf(t);
        float lens = sqrtf(1.0f - t * t), body = lens;
        int hump = 0;
        if (kind == ISLE_MESA) body = edge < 0.12f ? edge / 0.12f : 1.0f;
        if (kind == ISLE_SPIRE) body = edge * edge, hump = (int)(edge * (float)depth * 0.35f);
        if (kind == ISLE_HILL) hump = (int)(lens * lens * (float)depth * 0.8f);
        int thick = (int)((float)depth * body + rng_unit(rng) * 1.5f) + hump;
        if (i % 3 == 0 && kind != ISLE_MESA && kind != ISLE_BASIN) rise += rng_range(rng, -1, 1);
        if (rise > 2) rise = 2;
        if (rise < -2) rise = -2;
        int top = cy + rise - hump, soil = rng_range(rng, 2, 3);
        for (int k = 0; k < thick; k++) {
            int y = top + k;
            if (y <= g->y0 + 4 || y >= g->y0 + g->height - 4 || world_get(w, x, y) != TILE_AIR) continue;
            bool mine = b->dimension == g->dim;
            TileType tile = !mine ? d->rock : k == 0 ? b->surface : k <= soil ? b->soil : b->under != TILE_AIR ? b->under : d->rock;
            /* (Ground with ground over it is not the top of anything.) */
            if (k == 0 && world_get(w, x, y - 1) != TILE_AIR && mine) tile = b->soil;
            put_rock(w, x, y, tile);
        }
    }
    /* A basin has a lake in the top of it: the middle of its surface taken out, and water put there. */
    if (kind == ISLE_BASIN) {
        int half = length / 4, deep = depth >= 8 ? 3 : 2;
        for (int dx = -half; dx <= half; dx++) {
            int lip = abs(dx) > half - 2 ? 1 : deep;
            for (int k = 0; k < lip; k++) {
                size_t cell = (size_t)(cy + k) * w->width + world_wrap_x(w, cx + dx);
                if (cy + k >= g->y0 + g->height - 4 || world_get(w, cx + dx, cy + k + 1) == TILE_AIR) break;
                w->layers[LAYER_FOREGROUND][cell] = TILE_AIR;
                w->layers[LAYER_WALL][cell] = TILE_AIR;
                world_set_water(w, cx + dx, cy + k, WATER_FULL);
            }
        }
        return;
    }
    /* A big one is hollow at the heart. */
    if (kind != ISLE_SPIRE && length >= 26 && depth >= 9) {
        int rx = length / 5, ry = depth / 4, oy = cy + depth / 2;
        for (int dy = -ry; dy <= ry; dy++)
            for (int dx = -rx; dx <= rx; dx++)
                if ((float)(dx * dx) / (float)(rx * rx) + (float)(dy * dy) / (float)(ry * ry + 1) <= 1.0f &&
                    world_get(w, cx + dx, oy + dy) != TILE_AIR && oy + dy < g->y0 + g->height - 4)
                    w->layers[LAYER_FOREGROUND][(size_t)(oy + dy) * w->width + world_wrap_x(w, cx + dx)] = TILE_AIR;
    }
}

static void region_islands(World *w, int region, uint32_t *rng)
{
    const Region *g = &w->regions[region];
    const DimensionDef *d = &content.dimensions[g->dim];
    int count = (int)(d->island_rate * (float)w->width / 100.0f);
    int top = g->y0 + 48, bottom = g->y0 + g->height - 44;
    /* One where the world began, so that whoever comes first has somewhere to come to. */
    region_island(w, region, rng, w->spawn_x, (top + bottom) / 2, d->island_max_length, d->island_max_depth, ISLE_LENS);
    for (int n = 0; n < count && bottom > top; n++) {
        int length = rng_range(rng, d->island_min_length, d->island_max_length);
        /* (Most are small: the big ones are the rarer.) */
        if (rng_range(rng, 0, 2) != 0) length = d->island_min_length + (length - d->island_min_length) / 3;
        int depth = length / 3 + rng_range(rng, 0, 2);
        if (depth < d->island_min_depth) depth = d->island_min_depth;
        if (depth > d->island_max_depth) depth = d->island_max_depth;
        int x = rng_range(rng, 0, w->width - 1), y = rng_range(rng, top, bottom), kind = rng_range(rng, 0, ISLE_KINDS + 2);
        if (kind == ISLE_SPIRE) {
            /* A spire is narrow, and goes a long way down. */
            length = rng_range(rng, 5, 11);
            depth = rng_range(rng, 14, 34);
            region_island(w, region, rng, x, y, length, depth, ISLE_SPIRE);
        } else if (kind == ISLE_KINDS) {
            /* A scatter of little ones: stepping stones, with cloud between some of them to walk on. */
            int stones = rng_range(rng, 4, 8), at = x, level = y;
            for (int k = 0; k < stones; k++) {
                int size = rng_range(rng, 3, 8), gap = rng_range(rng, 3, 7);
                region_island(w, region, rng, at, level, size, rng_range(rng, 2, 4), ISLE_LENS);
                for (int dx = size / 2 + 1; d->cloud != TILE_AIR && k + 1 < stones && rng_range(rng, 0, 1) && dx < size / 2 + gap; dx++)
                    if (world_get(w, at + dx, level) == TILE_AIR) w->layers[LAYER_FOREGROUND][(size_t)level * w->width + world_wrap_x(w, at + dx)] = (uint16_t)d->cloud;
                at += size + gap;
                level += rng_range(rng, -3, 3);
                if (level < top) level = top;
                if (level > bottom) level = bottom;
            }
        } else if (kind == ISLE_KINDS + 1) {
            /* A great one: a country to itself. */
            region_island(w, region, rng, x, y, d->island_max_length + rng_range(rng, 20, 70), d->island_max_depth + rng_range(rng, 2, 8), rng_range(rng, 0, 1) ? ISLE_HILL : ISLE_LENS);
        } else {
            region_island(w, region, rng, x, y, length, depth, kind > ISLE_KINDS ? ISLE_LENS : kind);
        }
    }
    /* Small clouds at every height: something to stand on between one island and the next. */
    for (int n = 0; d->cloud != TILE_AIR && n < w->width / 14; n++) {
        int cx = rng_range(rng, 0, w->width - 1), cy = rng_range(rng, top, bottom + 20), rx = rng_range(rng, 2, 6);
        for (int dx = -rx; dx <= rx; dx++)
            for (int dy = 0; dy <= (abs(dx) < rx - 1 ? 1 : 0); dy++)
                if (world_get(w, cx + dx, cy + dy) == TILE_AIR) w->layers[LAYER_FOREGROUND][(size_t)(cy + dy) * w->width + world_wrap_x(w, cx + dx)] = (uint16_t)d->cloud;
    }
    /* Banks of cloud along the bottom. */
    for (int n = 0; d->cloud != TILE_AIR && n < w->width / 10; n++) {
        int cx = rng_range(rng, 0, w->width - 1), cy = g->y0 + g->height - rng_range(rng, 10, 30), rx = rng_range(rng, 4, 12), ry = rng_range(rng, 1, 3);
        for (int dy = -ry; dy <= ry; dy++)
            for (int dx = -rx; dx <= rx; dx++)
                if ((float)(dx * dx) / (float)(rx * rx) + (float)(dy * dy) / (float)(ry * ry) <= 1.0f && world_get(w, cx + dx, cy + dy) == TILE_AIR)
                    w->layers[LAYER_FOREGROUND][(size_t)(cy + dy) * w->width + world_wrap_x(w, cx + dx)] = (uint16_t)d->cloud;
    }
}

/* A hall: a round brush, much bigger than a tunnel's, that wanders sideways through the rock. */
static void region_hall(World *w, int region, uint32_t *rng, int x, int y, float rmin, float rmax, int length)
{
    const Region *g = &w->regions[region];
    float fx = (float)x, fy = (float)y;
    float angle = (rng_unit(rng) - 0.5f) * 1.2f + (rng_range(rng, 0, 1) ? 3.14159265f : 0.0f);
    float radius = rmin + rng_unit(rng) * (rmax - rmin);
    for (int step = 0; step < length; step++) {
        int reach = (int)ceilf(radius);
        for (int dy = -reach; dy <= reach; dy++) {
            for (int dx = -reach; dx <= reach; dx++) {
                int cx = (int)floorf(fx + 0.5f) + dx, cy = (int)floorf(fy + 0.5f) + dy;
                /* (Flatter than it is wide: a hall, not a ball.) */
                if ((float)(dx * dx) + (float)(dy * dy) * 1.6f > radius * radius || cy < g->y0 + 4 || cy >= g->y0 + g->height - 3) continue;
                w->layers[LAYER_FOREGROUND][(size_t)cy * w->width + world_wrap_x(w, cx)] = TILE_AIR;
            }
        }
        angle += (rng_unit(rng) - 0.5f) * 0.5f;
        radius += (rng_unit(rng) - 0.5f) * 1.2f;
        if (radius < rmin) radius = rmin;
        if (radius > rmax) radius = rmax;
        fx += cosf(angle) * 1.5f;
        fy += sinf(angle) * 0.7f;
        if (fy < (float)(g->y0 + 8) || fy > (float)(g->y0 + g->height - 8)) angle = -angle;
    }
}

static void region_caverns(World *w, int region, uint32_t *rng)
{
    const Region *g = &w->regions[region];
    const DimensionDef *d = &content.dimensions[g->dim];
    for (int y = g->y0 + 1; y < g->y0 + g->height; y++) {
        for (int x = 0; x < w->width; x++) {
            TileType tile = d->rock;
            float share = (float)(y - g->y0) / (float)g->height + (ring_noise(w->seed ^ 0xD1Au, x, w->width, 48) - 0.5f) * 0.08f;
            for (int s = 0; s < d->strata_count; s++)
                if (share >= d->strata[s].from) tile = d->strata[s].tile;
            put_rock(w, x, y, tile);
        }
    }
    int halls = (int)(d->cavern_rate * (float)w->width / 100.0f), mid = g->y0 + g->height / 2;
    /* One where the world began, and then the rest; and narrow ways between them. */
    region_hall(w, region, rng, w->spawn_x - 20, mid, d->cavern_min_radius, d->cavern_max_radius, d->cavern_max_length);
    int lakes = 0;
    for (int n = 0; n < halls; n++) {
        int x = rng_range(rng, 0, w->width - 1), y = rng_range(rng, g->y0 + 16, g->y0 + g->height - 16);
        region_hall(w, region, rng, x, y, d->cavern_min_radius, d->cavern_max_radius, rng_range(rng, d->cavern_min_length, d->cavern_max_length));
        /* Water in the bottom of some: whatever is open below a level, a little way round. */
        if (rng_unit(rng) >= d->lake_share || lakes > 400) continue;
        lakes++;
        int floor = y;
        while (floor < g->y0 + g->height - 4 && world_get(w, x, floor + 1) == TILE_AIR) floor++;
        int level = floor - rng_range(rng, 1, 4);
        for (int dx = -40; dx <= 40; dx++)
            for (int yy = level; yy <= floor + 6 && yy < g->y0 + g->height - 3; yy++)
                if (world_get(w, x + dx, yy) == TILE_AIR) world_set_water(w, x + dx, yy, WATER_FULL);
    }
    for (int n = 0; n < halls * 3; n++)
        region_hall(w, region, rng, rng_range(rng, 0, w->width - 1), rng_range(rng, g->y0 + 10, g->y0 + g->height - 10), 1.6f, 3.0f, rng_range(rng, 60, 220));
    /* Magma lies in whatever is open at the very bottom. */
    for (int y = g->y0 + g->height - 1 - d->magma_rows; d->magma != TILE_AIR && y < g->y0 + g->height - 1; y++)
        for (int x = 0; x < w->width; x++)
            if (world_get(w, x, y) == TILE_AIR) {
                world_set_water(w, x, y, 0);
                w->layers[LAYER_FOREGROUND][(size_t)y * w->width + x] = (uint16_t)d->magma;
            }
    /* The floors of the halls are their country's ground. */
    for (int x = 0; x < w->width; x++) {
        const BiomeDef *b = &content.biomes[world_biome_at(w, x, g->y0 + 1)];
        if (b->dimension != g->dim) continue;
        for (int y = g->y0 + 5; y < g->y0 + g->height - 3; y++) {
            if (world_get(w, x, y) != TILE_AIR) continue;
            TileType under = world_get(w, x, y + 1);
            if (under == TILE_AIR || under == d->magma || !tile_def(under)->solid) continue;
            put_rock(w, x, y + 1, b->surface);
            if (world_get(w, x, y + 2) != TILE_AIR && world_get(w, x, y + 2) != d->magma && b->soil != b->surface) put_rock(w, x, y + 2, b->soil);
        }
    }
}

/* A grove: no ground but a lake's bed, and out of the lake trunks as wide as houses going up the whole height of
 * the place - with boughs out from them to walk on, leaves heaped on the boughs, hollows in the wood, and vines
 * hanging from bough to bough. */
static void region_grove(World *w, int region, uint32_t *rng)
{
    const Region *g = &w->regions[region];
    const DimensionDef *d = &content.dimensions[g->dim];
    int top = g->y0 + 6, bottom = g->y0 + g->height - 2, lake = bottom - 14;
    /* The bed of the lake, uneven, and the water over it. */
    for (int x = 0; x < w->width; x++) {
        int bed = bottom - 6 - (int)(ring_noise(w->seed ^ 0x6A0Eu, x, w->width, 40) * 6.0f);
        for (int y = bed; y <= bottom; y++) put_rock(w, x, y, d->rock);
        for (int y = lake; y < bed; y++) world_set_water(w, x, y, WATER_FULL);
    }
    int width = d->trunk_width > 2 ? d->trunk_width : 3, gap = d->trunk_gap > width * 3 ? d->trunk_gap : width * 3;
    for (int at = world_wrap_x(w, w->spawn_x - width / 2), done = 0; done < w->width - gap / 2; ) {
        int wide = width + rng_range(rng, -1, 2), lean = 0;
        /* The trunk: it wanders a little as it rises, and is hollow here and there. */
        for (int y = bottom - 4; y >= top; y--) {
            if ((bottom - y) % 9 == 0) lean += rng_range(rng, -1, 1);
            for (int i = 0; i < wide; i++) {
                size_t cell = (size_t)y * w->width + world_wrap_x(w, at + lean + i);
                w->layers[LAYER_FOREGROUND][cell] = (uint16_t)d->trunk;
                w->layers[LAYER_WALL][cell] = (uint16_t)d->trunk;
                w->water[cell] = 0;
            }
        }
        for (int n = 0; n < g->height / 22; n++) {
            int hy = rng_range(rng, top + 8, lake - 6), hx = at + lean + 1, tall = rng_range(rng, 3, 4);
            for (int dy = 0; dy < tall; dy++)
                for (int i = 0; i < wide - 2; i++) w->layers[LAYER_FOREGROUND][(size_t)(hy - dy) * w->width + world_wrap_x(w, hx + i)] = TILE_AIR;
        }
        /* Boughs: out to one side or the other at every few rows, each ending in a heap of leaves. */
        for (int y = top + rng_range(rng, 4, 9); y < lake - 3; y += rng_range(rng, 6, 11)) {
            int side = rng_range(rng, 0, 1) ? 1 : -1, reach = rng_range(rng, 8, gap / 2 + 4), from = side > 0 ? at + lean + wide : at + lean - 1, droop = 0;
            for (int k = 0; k < reach; k++) {
                if (k % 5 == 4) droop += rng_range(rng, -1, 1);
                int bx = from + side * k, by = y + droop;
                if (by <= top || by >= lake - 1) break;
                size_t cell = (size_t)by * w->width + world_wrap_x(w, bx);
                if (w->layers[LAYER_FOREGROUND][cell] == TILE_AIR) w->layers[LAYER_FOREGROUND][cell] = (uint16_t)d->bough;
                /* (Leaves on it, thicker toward the end; and now and then a vine let down.) */
                int heap = d->leaf != TILE_AIR && k > reach / 3 ? 1 + (k * 3) / reach : 0;
                for (int up = 1; up <= heap && by - up > top; up++) {
                    size_t over = (size_t)(by - up) * w->width + world_wrap_x(w, bx);
                    if (rng_range(rng, 0, 3) != 0 && w->layers[LAYER_BACKGROUND][over] == TILE_AIR && w->layers[LAYER_FOREGROUND][over] == TILE_AIR)
                        w->layers[LAYER_BACKGROUND][over] = (uint16_t)d->leaf;
                }
                if (d->vine != TILE_AIR && rng_range(rng, 0, 6) == 0)
                    for (int down = 1, most = rng_range(rng, 3, 12); down <= most && by + down < lake; down++) {
                        size_t under = (size_t)(by + down) * w->width + world_wrap_x(w, bx);
                        if (w->layers[LAYER_FOREGROUND][under] != TILE_AIR || w->layers[LAYER_BACKGROUND][under] != TILE_AIR) break;
                        w->layers[LAYER_BACKGROUND][under] = (uint16_t)d->vine;
                    }
            }
        }
        int step = gap + rng_range(rng, -gap / 4, gap / 4);
        at = world_wrap_x(w, at + step);
        done += step;
    }
}

/* Ground with a sky over it: a line of hills by the seed, craters knocked out of it, a canyon if the place has one,
 * each country's own ground on top - and under it lava tubes, whose floors are that country's ground too, so that
 * what grows and what is left lying there is found below as well as above. */
static void region_surface(World *w, int region, uint32_t *rng)
{
    const Region *g = &w->regions[region];
    const DimensionDef *d = &content.dimensions[g->dim];
    int base = g->y0 + (int)((float)g->height * d->surface_level), bottom = g->y0 + g->height - 2;
    int *top = malloc(sizeof(int) * (size_t)w->width);
    if (!top) return;
    /* (In terraces: level stretches of this length and that, a step between - so that there is somewhere to stand, and to build.) */
    for (int x = 0, from = 0, level = base; x < w->width; x++) {
        if (x >= from) {
            level = base + (int)(ring_noise(w->seed ^ 0x5F0Bu, x, w->width, 90) * (float)d->surface_relief + ring_noise(w->seed ^ 0x77A1u, x, w->width, 17) * (float)d->surface_relief * 0.25f);
            from = x + 8 + (int)(hash01(w->seed ^ 0x7E44u, x) * 26.0f);
        }
        top[x] = level;
    }
    /* Craters: bowls with a raised rim. */
    int craters = (int)(d->crater_rate * (float)w->width / 100.0f);
    for (int n = 0; n < craters; n++) {
        int cx = rng_range(rng, 0, w->width - 1), rad = rng_range(rng, d->crater_min, d->crater_max);
        if (abs(world_dx(w, cx, w->spawn_x)) < rad + 6) continue;
        for (int dx = -rad - 2; dx <= rad + 2; dx++) {
            int x = world_wrap_x(w, cx + dx);
            float t = (float)abs(dx) / (float)rad;
            if (t <= 1.0f) top[x] += (int)((1.0f - t * t) * (float)rad * 0.55f);
            else top[x] -= 1;
        }
    }
    /* A canyon: one, a long way from where the place is come into, cut in steps. */
    if (d->canyon_width > 0 && d->canyon_depth > 0) {
        int cx = world_wrap_x(w, w->spawn_x + w->width / 3), half = d->canyon_width / 2;
        for (int dx = -half; dx <= half; dx++) {
            float t = 1.0f - (float)abs(dx) / (float)half;
            int step = (int)(t * 5.0f);
            top[world_wrap_x(w, cx + dx)] += (int)((float)d->canyon_depth * (float)(step > 4 ? 4 : step) / 4.0f) + (int)(ring_noise(w->seed ^ 0xCA41u, cx + dx, w->width, 9) * 2.0f);
        }
    }
    for (int x = 0; x < w->width; x++) {
        const BiomeDef *b = &content.biomes[world_biome_at(w, x, g->y0 + 1)];
        bool mine = b->dimension == g->dim;
        if (top[x] < g->y0 + 12) top[x] = g->y0 + 12;
        if (top[x] > bottom - 12) top[x] = bottom - 12;
        int soil = 3 + (int)(hash01(w->seed ^ 0x5011u, x) * 3.0f);
        for (int y = top[x]; y <= bottom; y++) {
            int k = y - top[x];
            TileType tile = !mine ? d->rock : k == 0 ? b->surface : k <= soil ? b->soil : (b->under != TILE_AIR && k <= b->under_depth) ? b->under : d->rock;
            float share = (float)(y - g->y0) / (float)g->height;
            for (int s = 0; s < d->strata_count && k > soil; s++)
                if (share >= d->strata[s].from) tile = d->strata[s].tile;
            put_rock(w, x, y, tile);
        }
    }
    /* Lava tubes, and a few great chambers. */
    int tubes = (int)(d->tube_rate * (float)w->width / 100.0f);
    for (int n = 0; n < tubes; n++) {
        int x = rng_range(rng, 0, w->width - 1), y = top[x] + rng_range(rng, 10, (bottom - top[x]) - 8 > 12 ? (bottom - top[x]) - 8 : 12);
        if (y > bottom - 6) continue;
        bool great = rng_range(rng, 0, 5) == 0;
        region_hall(w, region, rng, x, y, great ? 5.0f : 2.0f, great ? 10.0f : 4.0f, rng_range(rng, great ? 30 : 50, great ? 70 : 180));
    }
    /* The floors below are their country's ground. */
    for (int x = 0; x < w->width; x++) {
        const BiomeDef *b = &content.biomes[world_biome_at(w, x, g->y0 + 1)];
        if (b->dimension != g->dim) continue;
        for (int y = top[x] + 4; y < bottom; y++)
            if (world_get(w, x, y) == TILE_AIR && world_get(w, x, y + 1) != TILE_AIR && world_get(w, x, y - 1) == TILE_AIR && world_get(w, x, y - 2) == TILE_AIR)
                w->layers[LAYER_FOREGROUND][(size_t)(y + 1) * w->width + x] = (uint16_t)b->surface;
    }
    free(top);
}

/* What has been built on a dimension's islands: on so many of the level stretches wide enough, a structure;
 * and between islands that are near and level with each other, what is left of a bridge. */
static void region_built(World *w, int region, uint32_t *rng)
{
    const Region *g = &w->regions[region];
    const DimensionDef *d = &content.dimensions[g->dim];
    if (d->structure_count == 0 && d->bridge == TILE_AIR) return;
    for (int y = g->y0 + 24; y < g->y0 + g->height - 10; y++) {
        for (int x = 0; x < w->width; x++) {
            if (world_get(w, x, y) == TILE_AIR || world_get(w, x, y - 1) != TILE_AIR || world_get(w, x - 1, y) != TILE_AIR) continue;
            /* The left end of a stretch of ground: how far it runs level. */
            int run = 0;
            while (run < 60 && world_get(w, x + run, y) != TILE_AIR && world_get(w, x + run, y - 1) == TILE_AIR) run++;
            if (d->structure_count > 0 && rng_unit(rng) < d->structure_share) {
                const StructureDef *s = &content.structures[d->structures[rng_range(rng, 0, d->structure_count - 1)]];
                bool room = run >= s->width + 2;
                for (int dy = 1; room && dy < s->height; dy++)
                    for (int dx = 0; room && dx < s->width; dx += 2) room = world_get(w, x + 1 + dx, y - dy) == TILE_AIR && world_get_at(w, LAYER_BACKGROUND, x + 1 + dx, y - dy) == TILE_AIR;
                if (room) stamp_structure(w, s, x + 1 + rng_range(rng, 0, run - s->width - 2), y);
            }
            /* A bridge from its right-hand end to whatever is level with it and near. */
            if (d->bridge == TILE_AIR) continue;
            int end = x + run, far = 0;
            for (int k = 2; k <= 26 && far == 0; k++)
                if (world_get(w, end + k, y) != TILE_AIR) far = k;
            for (int k = 0; k < far; k++)
                if (rng_range(rng, 0, 5) != 0 && world_get(w, end + k, y) == TILE_AIR && world_get(w, end + k, y - 1) == TILE_AIR)
                    w->layers[LAYER_FOREGROUND][(size_t)y * w->width + world_wrap_x(w, end + k)] = (uint16_t)d->bridge;
        }
    }
}

/* Nothing: walls of the edge either side of so many columns, and in the middle of them one island with a tree on it. */
static void region_void(World *w, int region, uint32_t *rng)
{
    const Region *g = &w->regions[region];
    const DimensionDef *d = &content.dimensions[g->dim];
    int half = d->width > 0 && d->width < w->width ? d->width / 2 : w->width / 2;
    for (int x = 0; x < w->width; x++) {
        if (abs(world_dx(w, w->spawn_x, x)) < half) continue;
        for (int y = g->y0 + 1; y < g->y0 + g->height; y++) w->layers[LAYER_FOREGROUND][(size_t)y * w->width + x] = (uint16_t)d->edge;
    }
    int cy = g->y0 + g->height / 2;
    /* (Level, if something is to be built on it.) */
    region_island(w, region, rng, w->spawn_x, cy, d->void_length, d->void_depth, d->structure_count > 0 ? ISLE_MESA : ISLE_LENS);
    /* (Level under the tree, whatever the island's top did.) */
    if (d->tree >= 0) {
        int y = cy - 4;
        while (y < g->y0 + g->height - 4 && world_get(w, w->spawn_x + 3, y + 1) == TILE_AIR) y++;
        tree_plant(w, w->spawn_x + 3, y, d->tree);
        for (int steps = 0; steps < 1000 && tree_grow(w, w->spawn_x + 3, y, rng).grew; steps++) {}
    }
}

int world_add_region(World *w, int dim, const char *name, int height, bool generate)
{
    if (w->region_count >= MAX_REGIONS) return -1;
    const DimensionDef *d = dim >= 0 && dim < content.dimension_count ? &content.dimensions[dim] : NULL;
    if (generate && !d) return -1;
    if (generate) height = d->height;
    if (height < 8) return -1;
    size_t old = (size_t)w->width * w->height, cells = (size_t)w->width * (w->height + height);
    /* (Everything is grown before anything is counted on: a failure half way leaves the world as it was, with room to spare.) */
    for (int i = 0; i < LAYER_COUNT; i++) {
        uint16_t *more = realloc(w->layers[i], cells * sizeof(uint16_t));
        if (!more) return -1;
        memset(more + old, 0, (cells - old) * sizeof(uint16_t));
        w->layers[i] = more;
    }
    uint16_t *meta = realloc(w->meta, cells * sizeof(uint16_t));
    if (!meta) return -1;
    memset(meta + old, 0, (cells - old) * sizeof(uint16_t));
    w->meta = meta;
    uint8_t **bytes[3] = { &w->water, &w->paint[0], &w->paint[1] };
    for (int i = 0; i < 3; i++) {
        uint8_t *more = realloc(*bytes[i], cells);
        if (!more) return -1;
        memset(more + old, 0, cells - old);
        *bytes[i] = more;
    }
    uint8_t *biome = realloc(w->biome, (size_t)w->width * (size_t)(w->region_count + 2));
    if (!biome) return -1;
    memset(biome + (size_t)w->width * (size_t)(w->region_count + 1), 0, (size_t)w->width);
    w->biome = biome;

    if (w->region_count == 0) w->home_height = w->height;
    w->devices_known = false; /* (the new rows are looked through with the rest, when next it matters) */
    int region = w->region_count++;
    Region *g = &w->regions[region];
    memset(g, 0, sizeof *g);
    snprintf(g->name, sizeof g->name, "%s", d ? d->name : name ? name : "");
    g->dim = d ? dim : -1;
    g->y0 = w->height;
    g->height = height;
    w->height += height;
    if (!generate) return region;

    for (int x = 0; x < w->width; x++) w->layers[LAYER_FOREGROUND][(size_t)g->y0 * w->width + x] = (uint16_t)d->edge;
    uint32_t rng = w->seed ^ 0xD1AE5u;
    for (const char *c = d->name; *c; c++) rng = rng * 16777619u ^ (uint32_t)(unsigned char)*c;
    if (rng == 0) rng = 1;
    if (d->generator == DIM_DUNGEON) {
        /* (Solid, until somebody goes in: world_dungeon_build makes it what it is, each time.) */
        for (int y = g->y0; y < g->y0 + g->height; y++)
            for (int x = 0; x < w->width; x++) w->layers[LAYER_FOREGROUND][(size_t)y * w->width + x] = (uint16_t)d->edge;
        return region;
    }
    region_biomes(w, region, &rng);
    if (d->generator == DIM_ISLANDS) region_islands(w, region, &rng);
    else if (d->generator == DIM_CAVERNS) region_caverns(w, region, &rng);
    else if (d->generator == DIM_GROVE) region_grove(w, region, &rng);
    else if (d->generator == DIM_SURFACE) region_surface(w, region, &rng);
    else region_void(w, region, &rng);
    region_built(w, region, &rng);
    region_geodes(w, region, &rng);
    region_riches(w, region, &rng);
    region_hangings(w, region, &rng);
    region_plant(w, region, &rng);
    memset(w->water_awake, 0, (size_t)w->width);
    w->water_stirring = false;
    return region;
}

int world_dungeon_run(const World *w, int y, int *difficulty)
{
    int region = world_region_at(w, y);
    if (region < 0 || w->regions[region].dim < 0 || content.dimensions[w->regions[region].dim].generator != DIM_DUNGEON) return -1;
    int says = w->meta[(size_t)w->regions[region].y0 * w->width + world_wrap_x(w, w->spawn_x)];
    if (says <= 0 || (says - 1) / 8 >= content.dungeon_count) return -1;
    if (difficulty) *difficulty = (says - 1) % 8;
    return (says - 1) / 8;
}

static void dungeon_put(World *w, TileType t, int x, int y)
{
    if (t == TILE_AIR) return;
    size_t at = (size_t)y * w->width + world_wrap_x(w, x);
    if (w->layers[tile_def(t)->layer][at] == TILE_AIR || tile_def(t)->layer == LAYER_FOREGROUND) w->layers[tile_def(t)->layer][at] = (uint16_t)t;
}

static void dungeon_carve(World *w, const DungeonDef *d, int x0, int x1, int y0, int y1)
{
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            size_t at = (size_t)y * w->width + world_wrap_x(w, x);
            w->layers[LAYER_FOREGROUND][at] = TILE_AIR;
            w->layers[LAYER_WALL][at] = (uint16_t)d->back;
        }
}

bool world_dungeon_build(World *w, int region, int dungeon, int difficulty, uint32_t seed, int *start_x, int *start_y)
{
    if (region < 0 || region >= w->region_count || dungeon < 0 || dungeon >= content.dungeon_count) return false;
    const Region *g = &w->regions[region];
    const DimensionDef *dim = &content.dimensions[g->dim];
    const DungeonDef *d = &content.dungeons[dungeon];
    int more = difficulty >= 0 && difficulty < content.rules.difficulty_count ? content.rules.difficulties[difficulty].more : 0;
    uint32_t rng_state = seed ? seed : 1u, *rng = &rng_state;
    int width = w->width, y_end = g->y0 + g->height;
    if (dim->generator != DIM_DUNGEON || g->height < 24 || width < 200) return false;
    /* Whatever was there before is gone: rock from side to side. */
    for (int y = g->y0; y < y_end; y++)
        for (int x = 0; x < width; x++) {
            size_t at = (size_t)y * width + x;
            for (int layer = 0; layer < LAYER_COUNT; layer++) w->layers[layer][at] = 0;
            w->layers[LAYER_FOREGROUND][at] = (uint16_t)dim->edge;
            w->meta[at] = 0;
            w->water[at] = 0;
            w->paint[0][at] = w->paint[1][at] = 0;
        }
    w->devices_known = false;
    int rooms = rng_range(rng, d->rooms_min, d->rooms_max), fits = (width - 60) / 28;
    if (rooms > fits) rooms = fits;
    if (rooms < 3) rooms = 3;
    int left = w->spawn_x - 4, x = left, floor = g->y0 + g->height * 3 / 5;
    *start_x = world_wrap_x(w, w->spawn_x), *start_y = floor;
    for (int i = 0; i < rooms; i++) {
        bool first = i == 0, last = i == rooms - 1, boss = i == rooms - 2;
        int wide = first ? 9 : last ? 11 : boss ? 24 : rng_range(rng, 13, 22), high = first || last ? 5 : boss ? 9 : rng_range(rng, 5, 8);
        dungeon_carve(w, d, x, x + wide - 1, floor - high + 1, floor);
        /* What stands in it: lamps along the wall, things on the floor, things from the roof. */
        for (int lx = x + 2; d->light != TILE_AIR && lx < x + wide - 1; lx += 5) dungeon_put(w, d->light, lx, floor - 2);
        for (int k = rng_range(rng, 1, 3); d->decor_count > 0 && k > 0; k--) dungeon_put(w, d->decor[rng_range(rng, 0, d->decor_count - 1)], x + rng_range(rng, 1, wide - 2), floor);
        for (int k = rng_range(rng, 1, 3); d->hanging_count > 0 && k > 0; k--) dungeon_put(w, d->hanging[rng_range(rng, 0, d->hanging_count - 1)], x + rng_range(rng, 1, wide - 2), floor - high + 1);
        if (first) {
            dungeon_put(w, d->exit, x + 1, floor);
        } else if (last) {
            w->layers[tile_def(d->chest)->layer][(size_t)floor * width + world_wrap_x(w, x + wide / 2)] = (uint16_t)d->chest;
            w->layers[tile_def(d->exit)->layer][(size_t)floor * width + world_wrap_x(w, x + wide - 3)] = (uint16_t)d->exit;
        } else {
            /* Who is waiting: more of them the further in, and the last room but one has the one whose place it is. */
            int many = boss ? 1 + more : 2 + (i + 1) / 3 + more;
            if (many > (wide - 4) / 2) many = (wide - 4) / 2;
            for (int k = 0; k < many; k++) {
                size_t at = (size_t)floor * width + world_wrap_x(w, x + 3 + k * (wide - 5) / (many > 1 ? many - 1 : 1));
                w->layers[tile_def(d->mark)->layer][at] = (uint16_t)d->mark;
            }
            if (boss) w->layers[tile_def(d->boss_mark)->layer][(size_t)floor * width + world_wrap_x(w, x + wide - 6)] = (uint16_t)d->boss_mark;
        }
        if (last) { x += wide; break; }
        /* The way on: a passage with a door in it, and the next room a step up or down. */
        dungeon_carve(w, d, x + wide, x + wide + 4, floor - 2, floor);
        for (int k = 0; k < 3; k++) w->layers[LAYER_FOREGROUND][(size_t)(floor - k) * width + world_wrap_x(w, x + wide + 2)] = (uint16_t)d->door;
        x += wide + 5;
        int step = rng_range(rng, -1, 1);
        if (floor + step > g->y0 + 14 && floor + step < y_end - 6) floor += step;
    }
    /* Built: whatever rock is beside the open is the dungeon's own wall, and under it its floor. */
    for (int y = g->y0 + 1; y < y_end - 1; y++)
        for (int cx = left - 2; cx <= x + 2; cx++) {
            size_t at = (size_t)y * width + world_wrap_x(w, cx);
            if (w->layers[LAYER_FOREGROUND][at] != dim->edge) continue;
            bool beside = false;
            for (int dy = -1; dy <= 1 && !beside; dy++)
                for (int dx = -1; dx <= 1 && !beside; dx++) beside = w->layers[LAYER_WALL][(size_t)(y + dy) * width + world_wrap_x(w, cx + dx)] == d->back;
            if (!beside) continue;
            w->layers[LAYER_FOREGROUND][at] = (uint16_t)(w->layers[LAYER_WALL][(size_t)(y - 1) * width + world_wrap_x(w, cx)] == d->back ? d->floor : d->wall);
        }
    w->meta[(size_t)g->y0 * width + world_wrap_x(w, w->spawn_x)] = (uint16_t)(1 + dungeon * 8 + difficulty);
    return true;
}

int world_biome(const World *w, int x)
{
    int b = w->biome ? w->biome[world_wrap_x(w, x)] : 0;
    return b < content.biome_count ? b : 0;
}

void world_guess_biomes(World *w)
{
    int *level = calloc((size_t)w->width, sizeof(int)), *biome = calloc((size_t)w->width, sizeof(int));
    float *share = calloc((size_t)w->width, sizeof(float));
    if (level && biome && share) {
        /* The ground is as high as its backwall stands, whatever has been dug since. */
        for (int x = 0; x < w->width; x++) {
            int top = 0;
            while (top < w->height - 1 && world_get_at(w, LAYER_WALL, x, top) == TILE_AIR && !world_is_solid(w, x, top))
                top++;
            level[x] = world_row_to_y(w, top) + 1;
        }
        assign_biomes(w, level, biome, share);
        for (int x = 0; x < w->width; x++) w->biome[x] = (uint8_t)biome[x];
    }
    free(level);
    free(biome);
    free(share);
}

bool world_in_bounds(const World *w, int x, int y)
{
    (void)x;
    return y >= 0 && y < w->height;
}

int world_wrap_x(const World *w, int x)
{
    x %= w->width;
    return x < 0 ? x + w->width : x;
}

int world_dx(const World *w, int from, int to)
{
    int d = world_wrap_x(w, to - from);
    return d > w->width / 2 ? d - w->width : d;
}

int world_row_to_y(const World *w, int row)
{
    return w->y_top - row;
}

int world_y_to_row(const World *w, int y)
{
    return w->y_top - y;
}

TileType world_get_at(const World *w, int layer, int x, int y)
{
    if (!world_in_bounds(w, x, y)) return TILE_AIR;
    return (TileType)w->layers[layer][(size_t)y * w->width + world_wrap_x(w, x)];
}

static void device_note(World *w, int32_t cell)
{
    for (int i = 0; i < w->device_count; i++)
        if (w->devices[i] == cell) return;
    if (w->device_count >= w->device_room) {
        int room = w->device_room > 0 ? w->device_room * 2 : 256;
        int32_t *more = realloc(w->devices, sizeof(int32_t) * (size_t)room);
        if (!more) return;
        w->devices = more;
        w->device_room = room;
    }
    w->devices[w->device_count++] = cell;
}

void world_find_devices(World *w)
{
    w->device_count = 0;
    size_t cells = (size_t)w->width * w->height;
    for (size_t i = 0; i < cells; i++)
        if (tile_def((TileType)w->layers[LAYER_FOREGROUND][i])->device || tile_def((TileType)w->layers[LAYER_BACKGROUND][i])->device) device_note(w, (int32_t)i);
    w->devices_known = true;
}

void world_set_at(World *w, int layer, int x, int y, TileType t)
{
    if (!world_in_bounds(w, x, y)) return;
    size_t cell = (size_t)y * w->width + world_wrap_x(w, x);
    /* (A square with anything to do with signals is kept note of; one that has no more to do with them is forgotten.) */
    if (w->devices_known && layer < BLOCK_LAYERS) {
        bool had = tile_def((TileType)w->layers[LAYER_FOREGROUND][cell])->device || tile_def((TileType)w->layers[LAYER_BACKGROUND][cell])->device;
        bool other = tile_def((TileType)w->layers[layer == LAYER_FOREGROUND ? LAYER_BACKGROUND : LAYER_FOREGROUND][cell])->device;
        bool has = tile_def(t)->device || other;
        if (has && !had) device_note(w, (int32_t)cell);
        for (int i = 0; had && !has && i < w->device_count; i++)
            if (w->devices[i] == (int32_t)cell) {
                w->devices[i] = w->devices[--w->device_count];
                break;
            }
    }
    /* A piston that goes while it is out takes its head with it. */
    {
        const TileDef *was = tile_def((TileType)w->layers[layer][cell]);
        if (was->piston && was->piston_out && (TileType)w->layers[layer][cell] != t && was->piston_head != TILE_AIR &&
            world_get(w, x + was->piston_dir[0], y + was->piston_dir[1]) == was->piston_head) {
            w->layers[layer][cell] = (uint16_t)t; /* (first, so that this is not gone round again) */
            world_set(w, x + was->piston_dir[0], y + was->piston_dir[1], TILE_AIR);
        }
    }
    /* What was kept with the block that was there goes with it. */
    if (w->meta && w->meta[cell] != 0) {
        const TileDef *was = tile_def((TileType)w->layers[layer][cell]);
        if (was->two_tone || was->written || was->mimics != MIMIC_NONE || was->motor_power > 0 || was->revert_ticks > 0 || was->signal_delay > 0 || (TileType)w->layers[layer][cell] == content.rules.ice) w->meta[cell] = 0;
    }
    /* Paint goes with the block, or the backwall, it was on. */
    if (w->paint[0] && (TileType)w->layers[layer][cell] != t) {
        if (layer == LAYER_FOREGROUND) w->paint[0][cell] = 0;
        if (layer == LAYER_WALL) w->paint[1][cell] = 0;
    }
    w->layers[layer][cell] = (uint16_t)t;
    if (layer != LAYER_FOREGROUND || !w->water) return;
    /* A block where there was water pushes it out; one taken away lets it in. */
    if (tile_def(t)->solid) water_displace(w, x, y);
    else water_wake(w, x);
}

int world_meta(const World *w, int x, int y)
{
    if (!world_in_bounds(w, x, y) || !w->meta) return 0;
    return w->meta[(size_t)y * w->width + world_wrap_x(w, x)];
}

void world_set_meta(World *w, int x, int y, int value)
{
    if (!world_in_bounds(w, x, y) || !w->meta) return;
    w->meta[(size_t)y * w->width + world_wrap_x(w, x)] = (uint16_t)(value < 0 || value > 0xFFFF ? 0 : value);
}

float world_qi(const World *w, int x, int y)
{
    if (!world_in_bounds(w, x, y)) return 1.0f;
    float qi = 1.0f;
    int region = world_region_at(w, y), biome = world_biome_at(w, x, y);
    if (region >= 0 && w->regions[region].dim >= 0) qi += content.dimensions[w->regions[region].dim].qi;
    /* (Where the world began: thicker the further from the ground that it began on, up or down.) */
    else qi += (float)abs(world_row_to_y(w, y)) / 150.0f;
    if (biome >= 0 && biome < content.biome_count) qi += content.biomes[biome].qi;
    /* The richest source within reach of here. */
    float best = 0.0f;
    for (int dy = -8; dy <= 8; dy++) {
        for (int dx = -8; dx <= 8; dx++) {
            if (!world_in_bounds(w, x + dx, y + dy)) continue;
            for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
                const TileDef *def = tile_def(world_get_at(w, layer, x + dx, y + dy));
                if (def->qi > best && abs(dx) <= def->qi_reach && abs(dy) <= def->qi_reach) best = def->qi;
            }
        }
    }
    /* The lie of the land: richer here and poorer there, by the seed. (Nothing random is drawn for it.) */
    qi += 0.125f * (ring_noise(w->seed ^ 0x51C0u, x, w->width, 70) + 1.0f);
    if (w->qi_more) best += w->qi_more(w->qi_game, x, y);
    return qi + best;
}

bool world_grow_plant(World *w, int layer, int x, int y)
{
    /* (Some things grow only where the qi is thick enough for them.) */
    if (tile_def(world_get_at(w, layer, x, y))->grows_into != TILE_AIR && tile_def(tile_def(world_get_at(w, layer, x, y))->grows_into)->needs_qi > 0.0f &&
        world_qi(w, x, y) < tile_def(tile_def(world_get_at(w, layer, x, y))->grows_into)->needs_qi) return false;
    const TileDef *def = tile_def(world_get_at(w, layer, x, y));
    if (def->grows_into == TILE_AIR) return false;
    if (def->needs_water && !world_is_water(w, x, y)) return false;
    /* One that is about to stand two blocks tall needs the second free. */
    if (def->grows_top != TILE_AIR && (!world_in_bounds(w, x, y - 1) || world_get(w, x, y - 1) != TILE_AIR ||
                                       world_get_at(w, layer, x, y - 1) != TILE_AIR))
        return false;
    int meta = world_meta(w, x, y);
    world_set_at(w, layer, x, y, def->grows_into);
    world_set_meta(w, x, y, meta);
    if (def->grows_top != TILE_AIR) world_set_at(w, layer, x, y - 1, def->grows_top);
    return true;
}

TileType world_get(const World *w, int x, int y)
{
    return world_get_at(w, LAYER_FOREGROUND, x, y);
}

void world_set(World *w, int x, int y, TileType t)
{
    world_set_at(w, LAYER_FOREGROUND, x, y, t);
}

int world_structure(const World *w, int layer, int x, int y, int *bottom)
{
    TileType t = world_get_at(w, layer, x, y);
    int height = tile_def(t)->height;
    *bottom = y;
    if (t == TILE_AIR || height <= 1) return 1;
    /* Count up from the bottom of the unbroken column of this block: every
     * `height` tiles is one structure, even with another stacked on top. */
    int lowest = y;
    while (world_get_at(w, layer, x, lowest + 1) == t) lowest++;
    *bottom = lowest - ((lowest - y) / height) * height;
    int count = 0;
    while (count < height && world_get_at(w, layer, x, *bottom - count) == t) count++;
    return count;
}

TileType world_looks_like(const World *w, int x, int y)
{
    TileType t = world_get(w, x, y);
    if (tile_def(t)->mimics == MIMIC_NONE) return t;
    int look = world_meta(w, x, y);
    return look > 0 && look < content.tile_count && tile_def((TileType)look)->solid ? (TileType)look : t;
}

void world_fix_look(World *w, int x, int y)
{
    TileType t = world_get(w, x, y);
    if (tile_def(t)->mimics == MIMIC_NONE) return;
    TileType look = world_mimic_of(w, x, y);
    world_set_meta(w, x, y, look == t ? 0 : (int)look);
}

TileType world_mimic_of(const World *w, int x, int y)
{
    TileType t = world_get(w, x, y);
    if (tile_def(t)->mimics == MIMIC_NONE) return t;
    int dx = tile_def(t)->mimics == MIMIC_FLAT, dy = tile_def(t)->mimics == MIMIC_UPRIGHT;
    TileType ends[2] = { TILE_AIR, TILE_AIR };
    for (int side = 0; side < 2; side++) {
        int way = side ? 1 : -1;
        for (int k = 1; k <= 32; k++) {
            ends[side] = world_get(w, x + dx * k * way, y + dy * k * way);
            if (tile_def(ends[side])->mimics == MIMIC_NONE) break;
            ends[side] = TILE_AIR;
        }
    }
    return ends[0] == ends[1] && ends[0] != TILE_AIR && tile_def(ends[0])->solid ? ends[0] : t;
}

int world_paint(const World *w, bool wall, int x, int y)
{
    if (!world_in_bounds(w, x, y) || !w->paint[wall]) return 0;
    return w->paint[wall][(size_t)y * w->width + world_wrap_x(w, x)];
}

void world_set_paint(World *w, bool wall, int x, int y, int tint)
{
    if (!world_in_bounds(w, x, y) || !w->paint[wall]) return;
    w->paint[wall][(size_t)y * w->width + world_wrap_x(w, x)] = (uint8_t)(tint < 0 || tint > 255 ? 0 : tint);
}

bool world_is_wired(const World *w, int x, int y)
{
    if (!world_in_bounds(w, x, y)) return false;
    return tile_def(world_get(w, x, y))->wire || tile_def(world_get_at(w, LAYER_BACKGROUND, x, y))->wire;
}

static bool is_motor(const World *w, int x, int y)
{
    return tile_def(world_get(w, x, y))->motor_power > 0 || tile_def(world_get_at(w, LAYER_BACKGROUND, x, y))->motor_power > 0;
}

bool world_is_shaft(const World *w, int x, int y)
{
    if (!world_in_bounds(w, x, y)) return false;
    return tile_def(world_get(w, x, y))->shaft || tile_def(world_get_at(w, LAYER_BACKGROUND, x, y))->shaft || is_motor(w, x, y);
}

bool world_lift_runs(const World *w, int x, int y)
{
    if (!world_is_shaft(w, x, y)) return false;
    for (int way = -1; way <= 1; way += 2)
        for (int at = way < 0 ? y : y + 1; world_is_shaft(w, x, at); at += way)
            if (is_motor(w, x, at) && world_meta(w, x, at) != 0) return true;
    return false;
}

int world_mount(const World *w, int x, int y)
{
    if (world_get_at(w, LAYER_WALL, x, y) != TILE_AIR) return MOUNT_WALL;
    return world_mount_block(w, x, y);
}

int world_mount_block(const World *w, int x, int y)
{
    if (!world_in_bounds(w, x, y)) return MOUNT_NONE;
    if (world_is_solid(w, x, y + 1) || tile_def(world_get_at(w, LAYER_BACKGROUND, x, y + 1))->supports) return MOUNT_FLOOR;
    if (world_is_solid(w, x, y - 1)) return MOUNT_CEILING;
    if (world_is_solid(w, x - 1, y)) return MOUNT_LEFT;
    if (world_is_solid(w, x + 1, y)) return MOUNT_RIGHT;
    return MOUNT_NONE;
}

bool world_is_step(const World *w, int x, int y)
{
    if (!world_in_bounds(w, x, y)) return false;
    for (int layer = 0; layer < BLOCK_LAYERS; layer++)
        if (tile_def(world_get_at(w, layer, x, y))->steps) return true;
    return false;
}

bool world_is_solid(const World *w, int x, int y)
{
    if (!world_in_bounds(w, x, y)) return true;
    if (tile_def(world_get(w, x, y))->solid) return true;
    TileType back = world_get_at(w, LAYER_BACKGROUND, x, y);
    return tile_def(back)->solid_foot && world_get_at(w, LAYER_BACKGROUND, x, y + 1) != back;
}
