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
    free(biome);

    free(c.level);
    free(c.shape);
    free(level);
    w->spawn_y = world_y_to_row(w, 0);
    return true;
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

/* One island: a lens of ground, flat-topped and ragged, its top the biome's surface and its heart the rock. */
static void region_island(World *w, int region, uint32_t *rng, int cx, int cy, int length, int depth)
{
    const Region *g = &w->regions[region];
    const DimensionDef *d = &content.dimensions[g->dim];
    int rise = 0;
    for (int i = 0; i < length; i++) {
        int x = cx - length / 2 + i;
        const BiomeDef *b = &content.biomes[world_biome_at(w, x, g->y0 + 1)];
        float t = ((float)i + 0.5f) / (float)length * 2.0f - 1.0f;
        int thick = (int)((float)depth * sqrtf(1.0f - t * t) + rng_unit(rng) * 1.5f);
        if (i % 3 == 0) rise += rng_range(rng, -1, 1);
        if (rise > 2) rise = 2;
        if (rise < -2) rise = -2;
        int top = cy + rise, soil = rng_range(rng, 2, 3);
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
    /* A big one is hollow at the heart. */
    if (length >= 26 && depth >= 9) {
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
    int top = g->y0 + 24, bottom = g->y0 + g->height - 44;
    /* One where the world began, so that whoever comes first has somewhere to come to. */
    region_island(w, region, rng, w->spawn_x, (top + bottom) / 2, d->island_max_length, d->island_max_depth);
    for (int n = 0; n < count && bottom > top; n++) {
        int length = rng_range(rng, d->island_min_length, d->island_max_length);
        /* (Most are small: the big ones are the rarer.) */
        if (rng_range(rng, 0, 2) != 0) length = d->island_min_length + (length - d->island_min_length) / 3;
        int depth = length / 3 + rng_range(rng, 0, 2);
        if (depth < d->island_min_depth) depth = d->island_min_depth;
        if (depth > d->island_max_depth) depth = d->island_max_depth;
        region_island(w, region, rng, rng_range(rng, 0, w->width - 1), rng_range(rng, top, bottom), length, depth);
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
    region_island(w, region, rng, w->spawn_x, cy, 15, 5);
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
    region_biomes(w, region, &rng);
    if (d->generator == DIM_ISLANDS) region_islands(w, region, &rng);
    else if (d->generator == DIM_CAVERNS) region_caverns(w, region, &rng);
    else region_void(w, region, &rng);
    region_riches(w, region, &rng);
    region_plant(w, region, &rng);
    memset(w->water_awake, 0, (size_t)w->width);
    w->water_stirring = false;
    return region;
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

void world_set_at(World *w, int layer, int x, int y, TileType t)
{
    if (!world_in_bounds(w, x, y)) return;
    size_t cell = (size_t)y * w->width + world_wrap_x(w, x);
    /* What was kept with the block that was there goes with it. */
    if (w->meta && w->meta[cell] != 0) {
        const TileDef *was = tile_def((TileType)w->layers[layer][cell]);
        if (was->two_tone || was->written || was->mimics != MIMIC_NONE || was->motor_power > 0 || (TileType)w->layers[layer][cell] == content.rules.ice) w->meta[cell] = 0;
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

bool world_grow_plant(World *w, int layer, int x, int y)
{
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
