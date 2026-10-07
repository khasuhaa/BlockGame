#include "world.h"

#include "water.h"

#include "tree.h"

#include <math.h>
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
    int count = 0, target = rng_range(rng, v->min_size, v->max_size);
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

    if (count < v->min_size) return;
    for (int i = 0; i < count; i++) world_set(w, cells[i].x, cells[i].y, v->tile);
}

/* A scar: a long gash through the rock, thick in the middle and running out
 * to nothing at its ends, on a slant that wanders. It takes the place of
 * whatever it crosses that it may replace, earlier scars included. */
static void place_scar(World *w, const VeinDef *v, uint32_t *rng, int x, int y)
{
    int length = rng_range(rng, v->min_size, v->max_size), thick = rng_range(rng, v->min_thick, v->max_thick);
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
            if (def->below_sea) share = sea ? 1.0f : 0.0f;
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

/* Here and there a biome has something buried in it: at the very bottom of
 * its own ground, with some of that ground over it. */
static void bury(World *w, const int *level, const int *biome)
{
    for (int x = 0; x < w->width; x++) {
        const BiomeDef *b = &content.biomes[biome[x]];
        if (b->buried == TILE_AIR || b->buried_one_in <= 0) continue;
        if (hash01(w->seed ^ 0x7EA5u, x) >= 1.0f / (float)b->buried_one_in) continue;
        int top = world_y_to_row(w, level[x] - 1), y = top;
        while (y < w->height - 1 && (world_get(w, x, y) == b->surface || world_get(w, x, y) == b->soil)) y++;
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
        int size = rng_range(&rng, f->min_size, f->max_size);
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
     * keeps a depth of its own, as the sea bed does. */
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
        /* Some country has rock of its own under its ground: solid for the
         * top half of its depth, then more and more broken up by the
         * ordinary rock, and thinner toward the country's edge. */
        int under = b->under != TILE_AIR ? (int)floorf((float)b->under_depth * fminf(1.0f, share[x]) + 0.5f) : 0;
        int top = world_y_to_row(w, level[x] - 1);
        for (int y = top < 0 ? 0 : top; y < w->height; y++) {
            int below = y - top, deep = below - soil;
            TileType rock = gen->rock;
            if (deep >= 0 && deep < under &&
                (deep * 2 < under || hash01(w->seed ^ 0x5A4Du, x * 8191 + y) < 2.0f * (float)(under - deep) / (float)under - 0.15f))
                rock = b->under;
            world_set(w, x, y, below >= soil ? rock : below == 0 ? b->surface
                               : below < own ? b->soil : content.biomes[0].soil);
        }
    }
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
    bury(w, level, biome);
    /* The sea, before anything that only grows under it is planted. */
    water_fill_sea(w);
    plant_world(w, level, biome, &rng);
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

bool world_init(World *w, int width, int height, uint32_t seed, WorldKind kind)
{
    memset(w, 0, sizeof *w);
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
    if (!w->biome || !w->water || !w->water_awake || !w->meta) {
        world_free(w);
        return false;
    }
    w->width = width;
    w->height = height;
    w->seed = seed;
    w->y_top = height / 2;
    w->sea_row = -1;

    if (kind == WORLD_BLANK) return true;
    if (kind == WORLD_FLAT) {
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
    memset(w, 0, sizeof *w);
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
    if (w->meta && w->meta[cell] != 0 && tile_def((TileType)w->layers[layer][cell])->two_tone) w->meta[cell] = 0;
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
    return tile_def(world_get(w, x, y))->solid;
}
