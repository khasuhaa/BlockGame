#ifndef WORLD_H
#define WORLD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TILE_AIR = 0,
    TILE_DIRT,
    TILE_WORKBENCH,
    TILE_SPAWN_PORTAL,
    TILE_COUNT
} TileType;

typedef struct {
    bool solid;     /* blocks movement and can be stood on */
    bool bench;     /* a workbench: can be opened to craft */
    bool removable; /* can be mined */
} TileDef;

const TileDef *tile_def(TileType t);

/* Tile grid. x grows right, y grows down; (0,0) is the top-left tile. */
typedef struct {
    int width, height;
    uint32_t seed;
    int surface_y; /* y of the topmost ground row */
    int spawn_x;   /* column the player should spawn in */
    uint8_t *tiles;
} World;

/* Allocates the grid and generates terrain from the seed. */
bool world_init(World *w, int width, int height, uint32_t seed);
void world_free(World *w);

bool world_in_bounds(const World *w, int x, int y);
/* Out-of-bounds reads as TILE_AIR. */
TileType world_get(const World *w, int x, int y);
void world_set(World *w, int x, int y, TileType t);
/* Out-of-bounds counts as solid, so the world edges act as walls. */
bool world_is_solid(const World *w, int x, int y);

#endif
