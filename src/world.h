#ifndef WORLD_H
#define WORLD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TILE_AIR = 0,
    TILE_DIRT,
    TILE_WORKBENCH,
    TILE_SPAWN_PORTAL,
    TILE_GRASS,
    TILE_STONE,
    TILE_PORTAL_BASE,
    TILE_OAK_LOG,
    TILE_OAK_LEAVES,
    TILE_CHERRY_LOG,
    TILE_CHERRY_LEAVES,
    TILE_WOOD,
    TILE_COUNT
} TileType;

typedef struct {
    bool solid;      /* blocks movement and can be stood on */
    bool bench;      /* a workbench: can be opened to craft */
    bool removable;  /* can be mined */
    float mine_time; /* seconds to mine */
} TileDef;

const TileDef *tile_def(TileType t);

typedef enum {
    WORLD_FLAT,   /* level dirt, for tests */
    WORLD_TERRAIN, /* plains, mountains and valleys, with trees */
    WORLD_BLANK   /* nothing at all: filled in from a save file */
} WorldKind;

/* Terrain limits, in blocks. */
#define CHUNK_WIDTH 16 /* columns per chunk; each chunk gets its own random events */
#define PORTAL_HEIGHT 3
#define FEATURE_MIN_SIZE 10 /* shortest mountain / shallowest valley */
#define FEATURE_MAX_SIZE 60 /* tallest mountain / deepest valley */
#define PLAINS_MAX_LEVEL 20 /* plains wander between Y 0 and this */

/* Tile grid that wraps around horizontally: column `width` is column 0 again.
 * Tile coordinates have x growing right and y growing down from the top row.
 * World "Y" is height: it grows upward and is 0 at the spawn point's feet.
 *
 * There are two layers of tiles. The foreground is the one the player lives
 * in: everything generated so far goes there, and every function below works
 * on it. The background sits behind it and is only drawn. Dropped items form
 * a third layer in front of both; they belong to the game, not the grid. */
typedef struct {
    int width, height;
    uint32_t seed;
    int y_top;        /* world Y of tile row 0 */
    int spawn_x;      /* tile where the player's feet start */
    int spawn_y;
    int surface_y;    /* WORLD_FLAT only: row of the ground's top layer */
    uint8_t *tiles;      /* foreground */
    uint8_t *background;
} World;

/* Allocates the grid and generates terrain from the seed. */
bool world_init(World *w, int width, int height, uint32_t seed, WorldKind kind);
void world_free(World *w);

/* Any x is valid and wraps; only y can be out of bounds. */
bool world_in_bounds(const World *w, int x, int y);
int world_wrap_x(const World *w, int x);
/* Signed distance from column `from` to column `to`, the short way round. */
int world_dx(const World *w, int from, int to);
/* Height of a tile row, and back. */
int world_row_to_y(const World *w, int row);
int world_y_to_row(const World *w, int y);

/* Out-of-bounds reads as TILE_AIR. */
TileType world_get(const World *w, int x, int y);
void world_set(World *w, int x, int y, TileType t);
/* Above and below the world counts as solid. */
bool world_is_solid(const World *w, int x, int y);

#endif
