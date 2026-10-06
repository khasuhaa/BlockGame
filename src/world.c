#include "world.h"

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

static int pick_tree(uint32_t *rng)
{
    int total = 0;
    for (int i = 0; i < content.tree_count; i++) total += content.trees[i].weight;
    if (total <= 0) return -1;
    int roll = rng_range(rng, 0, total - 1);
    for (int i = 0; i < content.tree_count; i++) {
        roll -= content.trees[i].weight;
        if (roll < 0) return i;
    }
    return -1;
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

    /* Fill the columns: surface over soil over rock. The soil thins out into
     * a feature until it is bare rock. */
    for (int x = 0; x < width; x++) {
        float bare = smoothstep(gen->bare_at > 0.0f ? c.shape[x] / gen->bare_at : 1.0f);
        float depth = (gen->soil_depth +
                       gen->soil_variation * ring_noise(w->seed ^ 0xC3u, x, width, 12)) *
                      (1.0f - bare);
        depth += (hash01(w->seed ^ 0xD4u, x) - 0.5f) * 0.8f * (bare > 0.0f ? 1.0f : 0.0f);
        int soil = (int)floorf(depth + 0.5f);
        int top = world_y_to_row(w, level[x] - 1);
        for (int y = top < 0 ? 0 : top; y < w->height; y++) {
            int below = y - top;
            world_set(w, x, y, below >= soil ? gen->rock : below == 0 ? gen->surface : gen->soil);
        }
    }

    /* Trees stand on the plains' surface, at every stage of growth from a
     * lone trunk block to full size. */
    int last_tree = -gen->tree_spacing;
    for (int x = 0; x < width && gen->tree_one_in > 0; x++) {
        int feet = world_y_to_row(w, level[x]);
        if (c.shape[x] > 0.0f || world_get(w, x, feet + 1) != gen->surface) continue;
        if (x - last_tree < gen->tree_spacing) continue;
        if (abs(world_dx(w, w->spawn_x, x)) < gen->tree_spawn_clearance) continue;
        if (rng_range(&rng, 0, gen->tree_one_in - 1) != 0) continue;
        int kind_of_tree = pick_tree(&rng);
        if (kind_of_tree < 0) break;
        tree_plant(w, x, feet, kind_of_tree);
        int steps = rng_unit(&rng) < gen->tree_grown_chance ? 1000
                                                           : rng_range(&rng, 0, gen->tree_young_steps);
        while (steps-- > 0 && tree_grow(w, x, feet, &rng).grew) {}
        last_tree = x;
    }

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
        memset(w->layers[LAYER_FOREGROUND] + (size_t)y * w->width, content.worldgen.soil,
               (size_t)w->width);
}

bool world_init(World *w, int width, int height, uint32_t seed, WorldKind kind)
{
    memset(w, 0, sizeof *w);
    if (width < 4 || height < 8) return false;
    for (int i = 0; i < LAYER_COUNT; i++) {
        w->layers[i] = calloc((size_t)width, (size_t)height);
        if (!w->layers[i]) {
            world_free(w);
            return false;
        }
    }
    w->width = width;
    w->height = height;
    w->seed = seed;
    w->y_top = height / 2;

    if (kind == WORLD_BLANK) return true;
    if (kind == WORLD_FLAT) {
        generate_flat(w);
    } else if (!generate_terrain(w)) {
        world_free(w);
        return false;
    }

    /* The spawn portal: a base block set in the ground and the portal above. */
    const WorldGenDef *gen = &content.worldgen;
    world_set(w, w->spawn_x, w->spawn_y + 1, gen->portal_base);
    for (int i = 0; i < gen->portal_height; i++)
        world_set(w, w->spawn_x, w->spawn_y - i,
                  i == gen->portal_height - 1 ? gen->portal_top : gen->portal_body);
    return true;
}

void world_free(World *w)
{
    for (int i = 0; i < LAYER_COUNT; i++) free(w->layers[i]);
    memset(w, 0, sizeof *w);
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
    w->layers[layer][(size_t)y * w->width + world_wrap_x(w, x)] = (uint8_t)t;
}

TileType world_get(const World *w, int x, int y)
{
    return world_get_at(w, LAYER_FOREGROUND, x, y);
}

void world_set(World *w, int x, int y, TileType t)
{
    world_set_at(w, LAYER_FOREGROUND, x, y, t);
}

bool world_is_solid(const World *w, int x, int y)
{
    if (!world_in_bounds(w, x, y)) return true;
    return tile_def(world_get(w, x, y))->solid;
}
