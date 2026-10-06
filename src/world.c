#include "world.h"

#include <stdlib.h>
#include <string.h>

static const TileDef tile_defs[TILE_COUNT] = {
    [TILE_AIR] = { false, false, false },
    [TILE_DIRT] = { true, false, true },
    [TILE_WORKBENCH] = { true, true, true },
    [TILE_SPAWN_PORTAL] = { false, true, false },
};

const TileDef *tile_def(TileType t)
{
    return &tile_defs[t < TILE_COUNT ? t : TILE_AIR];
}

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

/* Flat dirt world: the seed only picks the ground level and spawn column.
 * The spawn portal stands on the ground there, two tiles tall. */
static void world_generate(World *w)
{
    uint32_t rng = w->seed ? w->seed : 0x9E3779B9u;
    for (int i = 0; i < 8; i++) rng_next(&rng);

    w->surface_y = w->height / 2 + rng_range(&rng, -w->height / 8, w->height / 8);
    w->spawn_x = rng_range(&rng, w->width / 4, w->width * 3 / 4);

    for (int y = 0; y < w->height; y++) {
        TileType t = y >= w->surface_y ? TILE_DIRT : TILE_AIR;
        memset(w->tiles + (size_t)y * w->width, t, (size_t)w->width);
    }
    world_set(w, w->spawn_x, w->surface_y - 1, TILE_SPAWN_PORTAL);
    world_set(w, w->spawn_x, w->surface_y - 2, TILE_SPAWN_PORTAL);
}

bool world_init(World *w, int width, int height, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    if (width < 4 || height < 8) return false;
    w->tiles = malloc((size_t)width * height);
    if (!w->tiles) return false;
    w->width = width;
    w->height = height;
    w->seed = seed;
    world_generate(w);
    return true;
}

void world_free(World *w)
{
    free(w->tiles);
    memset(w, 0, sizeof *w);
}

bool world_in_bounds(const World *w, int x, int y)
{
    return x >= 0 && y >= 0 && x < w->width && y < w->height;
}

TileType world_get(const World *w, int x, int y)
{
    if (!world_in_bounds(w, x, y)) return TILE_AIR;
    return (TileType)w->tiles[(size_t)y * w->width + x];
}

void world_set(World *w, int x, int y, TileType t)
{
    if (!world_in_bounds(w, x, y)) return;
    w->tiles[(size_t)y * w->width + x] = (uint8_t)t;
}

bool world_is_solid(const World *w, int x, int y)
{
    if (!world_in_bounds(w, x, y)) return true;
    return tile_def(world_get(w, x, y))->solid;
}
