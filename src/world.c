#include "world.h"

#include "tree.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const TileDef tile_defs[TILE_COUNT] = {
    [TILE_AIR] = { false, false, false, 0.0f },
    [TILE_DIRT] = { true, false, true, 0.35f },
    [TILE_WORKBENCH] = { true, true, true, 0.35f },
    [TILE_SPAWN_PORTAL] = { false, true, false, 0.0f },
    [TILE_GRASS] = { true, false, true, 0.35f },
    [TILE_STONE] = { true, false, true, 0.7f },
    [TILE_PORTAL_BASE] = { true, false, false, 0.0f },
    [TILE_OAK_LOG] = { true, false, true, 0.5f },
    [TILE_OAK_LEAVES] = { true, false, true, 0.15f },
    [TILE_CHERRY_LOG] = { true, false, true, 0.5f },
    [TILE_CHERRY_LEAVES] = { true, false, true, 0.15f },
    [TILE_WOOD] = { true, false, true, 0.5f },
};

const TileDef *tile_def(TileType t)
{
    return &tile_defs[t < TILE_COUNT ? t : TILE_AIR];
}

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
    return lo + (int)(rng_next(state) % (uint32_t)(hi - lo + 1));
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
    int cells = width / wavelength;
    if (cells < 1) cells = 1;
    float pos = (float)x * (float)cells / (float)width;
    int i = (int)pos;
    float a = hash01(seed, i % cells), b = hash01(seed, (i + 1) % cells);
    return (a + (b - a) * smoothstep(pos - (float)i)) * 2.0f - 1.0f;
}

/* ---- terrain ----------------------------------------------------------- */

typedef enum {
    FEATURE_MOUNTAIN,
    FEATURE_STEEP_VALLEY,
    FEATURE_SMOOTH_VALLEY,
    FEATURE_KINDS
} FeatureKind;

/* Scratch columns used while shaping the land. */
typedef struct {
    float *level; /* feet level: Y of the lowest air tile */
    float *shape; /* 0 on the plains, rising to 1 at a peak or valley floor */
} Columns;

static int feature_half_width(FeatureKind kind, int size)
{
    switch (kind) {
    case FEATURE_MOUNTAIN: return size * 9 / 10 + 10;
    case FEATURE_STEEP_VALLEY: return size * 11 / 20 + 8;
    default: return size * 22 / 10 + 20;
    }
}

/* Mountains rise in ledges: the slope is sampled every one to three columns
 * and held in between, working outward from the peak so the peak survives. */
static void shape_mountain(World *w, Columns c, uint32_t *rng, int center, int half, int size)
{
    for (int side = -1; side <= 1; side += 2) {
        float held = 0.0f;
        int run = 0;
        for (int d = side < 0 ? 0 : 1; d <= half; d++) {
            int x = world_wrap_x(w, center + side * d);
            float s = 1.0f - (float)d / (float)half;
            /* Rounded foot, pointed top. */
            float sh = 0.5f * smoothstep(s) + 0.5f * s * s;
            if (run == 0) {
                /* Shoulders part-way up, and a block or two of roughness. */
                float ridge = 0.5f + 0.5f * ring_noise(w->seed ^ 0x51u, x, w->width, 16);
                float jitter = ((float)rng_range(rng, -15, 15) / 10.0f) * fminf(1.0f, sh * 5.0f);
                held = (float)size * (sh - 0.8f * sh * (1.0f - sh) * ridge) + jitter;
                if (d == 0) held = (float)size;
                run = rng_range(rng, 1, 3);
            }
            run--;
            c.level[x] += held;
            c.shape[x] = sh;
        }
    }
}

static void shape_valley(World *w, Columns c, uint32_t *rng, int center, int half, int size,
                         bool steep)
{
    for (int d = -half; d <= half; d++) {
        int x = world_wrap_x(w, center + d);
        float s = 1.0f - fabsf((float)d) / (float)half;
        /* A steep valley drops fast to a wide flat floor. */
        float sh = smoothstep(steep ? s * 1.7f : s);
        float drop = (float)size * sh;
        if (steep) drop += ((float)rng_range(rng, -10, 10) / 10.0f) * 4.0f * sh * (1.0f - sh);
        c.level[x] -= drop;
        c.shape[x] = sh;
    }
}

/* Limits how much neighbouring columns may differ across a span. */
static void limit_steps(World *w, int *level, int from, int to, int max_step)
{
    for (int i = from + 1; i <= to; i++) {
        int x = world_wrap_x(w, i), prev = world_wrap_x(w, i - 1);
        if (level[x] > level[prev] + max_step) level[x] = level[prev] + max_step;
        if (level[x] < level[prev] - max_step) level[x] = level[prev] - max_step;
    }
}

static bool generate_terrain(World *w)
{
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

    /* Plains everywhere to begin with: broad, gentle swells above Y 0. */
    for (int x = 0; x < width; x++) {
        c.level[x] = PLAINS_MAX_LEVEL / 2.0f +
                     0.7f * (PLAINS_MAX_LEVEL / 2.0f) * ring_noise(w->seed ^ 0xA1u, x, width, 96) +
                     0.3f * (PLAINS_MAX_LEVEL / 2.0f) * ring_noise(w->seed ^ 0xB2u, x, width, 28);
    }

    /* Walk round the world laying down mountains and valleys with a stretch
     * of plains before each, so no two of them run into each other. The
     * first stretch, at column 0, holds the spawn. */
    w->spawn_x = rng_range(&rng, 30, 50);
    int pos = 80;
    int kind = rng_range(&rng, 0, FEATURE_KINDS - 1);
    int mountain_spans[64][2], mountains = 0;
    for (;;) {
        int plains = rng_range(&rng, 50, 120);
        int size = rng_range(&rng, FEATURE_MIN_SIZE, FEATURE_MAX_SIZE);
        int half = feature_half_width((FeatureKind)kind, size);
        int center = pos + plains + half;
        if (center + half + 50 > width) break;
        if (kind == FEATURE_MOUNTAIN) {
            shape_mountain(w, c, &rng, center, half, size);
            if (mountains < 64) {
                mountain_spans[mountains][0] = center - half;
                mountain_spans[mountains][1] = center + half;
                mountains++;
            }
        } else {
            shape_valley(w, c, &rng, center, half, size, kind == FEATURE_STEEP_VALLEY);
        }
        pos = center + half;
        kind = (kind + rng_range(&rng, 1, FEATURE_KINDS - 1)) % FEATURE_KINDS;
    }

    /* Ease the land down to Y 0 around the spawn point. */
    for (int d = -24; d <= 24; d++) {
        int x = world_wrap_x(w, w->spawn_x + d);
        c.level[x] *= smoothstep(((float)abs(d) - 3.0f) / 21.0f);
    }

    for (int x = 0; x < width; x++) level[x] = (int)floorf(c.level[x] + 0.5f);
    for (int i = 0; i < mountains; i++) {
        /* Clamp outward from each peak so the peak keeps its height. */
        int mid = (mountain_spans[i][0] + mountain_spans[i][1]) / 2;
        limit_steps(w, level, mid, mountain_spans[i][1], 3);
        for (int x = mid - 1; x >= mountain_spans[i][0]; x--) {
            int a = world_wrap_x(w, x), b = world_wrap_x(w, x + 1);
            if (level[a] > level[b] + 3) level[a] = level[b] + 3;
            if (level[a] < level[b] - 3) level[a] = level[b] - 3;
        }
    }

    /* Fill the columns: grass over dirt over stone. The dirt thins out up a
     * mountain and down a valley until it is bare stone. */
    memset(w->tiles, TILE_AIR, (size_t)width * w->height);
    for (int x = 0; x < width; x++) {
        float bare = smoothstep(c.shape[x] / 0.6f);
        float depth = (4.6f + 0.6f * ring_noise(w->seed ^ 0xC3u, x, width, 12)) * (1.0f - bare);
        depth += (hash01(w->seed ^ 0xD4u, x) - 0.5f) * 0.8f * (bare > 0.0f ? 1.0f : 0.0f);
        int dirt = (int)floorf(depth + 0.5f);
        int top = world_y_to_row(w, level[x] - 1);
        for (int y = top < 0 ? 0 : top; y < w->height; y++) {
            int below = y - top;
            TileType t = below >= dirt ? TILE_STONE : below == 0 ? TILE_GRASS : TILE_DIRT;
            w->tiles[(size_t)y * width + x] = (uint8_t)t;
        }
    }

    /* Trees stand on the grass of the plains, at every stage of growth from a
     * lone trunk block to full size. */
    int last_tree = -100;
    for (int x = 0; x < width; x++) {
        int feet = world_y_to_row(w, level[x]);
        if (c.shape[x] > 0.0f || world_get(w, x, feet + 1) != TILE_GRASS) continue;
        if (x - last_tree < 7 || abs(world_dx(w, w->spawn_x, x)) < 8) continue;
        if (rng_range(&rng, 0, 9) != 0) continue;
        tree_plant(w, x, feet, rng_range(&rng, 0, 1) ? TREE_OAK : TREE_CHERRY);
        int steps = rng_range(&rng, 0, 1) ? 100 : rng_range(&rng, 0, 20);
        while (steps-- > 0 && tree_grow(w, x, feet, &rng).grew) {}
        last_tree = x;
    }

    free(c.level);
    free(c.shape);
    free(level);
    w->spawn_y = world_y_to_row(w, 0);
    return true;
}

/* Level dirt; the seed only picks the ground level and spawn column. */
static void generate_flat(World *w)
{
    uint32_t rng = w->seed ? w->seed : 0x9E3779B9u;
    for (int i = 0; i < 8; i++) rng_next(&rng);

    w->surface_y = w->height / 2 + rng_range(&rng, -w->height / 8, w->height / 8);
    w->spawn_x = rng_range(&rng, w->width / 4, w->width * 3 / 4);
    w->spawn_y = w->surface_y - 1;
    w->y_top = w->spawn_y;

    for (int y = 0; y < w->height; y++) {
        TileType t = y >= w->surface_y ? TILE_DIRT : TILE_AIR;
        memset(w->tiles + (size_t)y * w->width, t, (size_t)w->width);
    }
}

bool world_init(World *w, int width, int height, uint32_t seed, WorldKind kind)
{
    memset(w, 0, sizeof *w);
    if (width < 4 || height < 8) return false;
    w->tiles = calloc((size_t)width, (size_t)height);
    w->background = calloc((size_t)width, (size_t)height);
    if (!w->tiles || !w->background) {
        world_free(w);
        return false;
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
    world_set(w, w->spawn_x, w->spawn_y + 1, TILE_PORTAL_BASE);
    for (int i = 0; i < PORTAL_HEIGHT; i++)
        world_set(w, w->spawn_x, w->spawn_y - i, TILE_SPAWN_PORTAL);
    return true;
}

void world_free(World *w)
{
    free(w->tiles);
    free(w->background);
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

TileType world_get(const World *w, int x, int y)
{
    if (!world_in_bounds(w, x, y)) return TILE_AIR;
    return (TileType)w->tiles[(size_t)y * w->width + world_wrap_x(w, x)];
}

void world_set(World *w, int x, int y, TileType t)
{
    if (!world_in_bounds(w, x, y)) return;
    w->tiles[(size_t)y * w->width + world_wrap_x(w, x)] = (uint8_t)t;
}

bool world_is_solid(const World *w, int x, int y)
{
    if (!world_in_bounds(w, x, y)) return true;
    return tile_def(world_get(w, x, y))->solid;
}
