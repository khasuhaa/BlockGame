#ifndef WORLD_H
#define WORLD_H

#include "content.h"

typedef enum {
    WORLD_FLAT,    /* level soil, for tests */
    WORLD_TERRAIN, /* plains broken up by the features in the data, with trees */
    WORLD_BLANK    /* nothing at all: filled in from a save file */
} WorldKind;

/* Tile grid that wraps around horizontally: column `width` is column 0 again.
 * Tile coordinates have x growing right and y growing down from the top row.
 * World "Y" is height: it grows upward and is 0 at the spawn point's feet.
 *
 * There are two layers of tiles. The foreground is the one the player
 * collides with, and the plain accessors below work on it. The background
 * sits behind the player. Dropped items form a third layer in front of both;
 * they belong to the game, not the grid. */
typedef struct {
    int width, height;
    uint32_t seed;
    int y_top;        /* world Y of tile row 0 */
    int spawn_x;      /* tile where the player's feet start */
    int spawn_y;
    int surface_y;    /* WORLD_FLAT only: row of the ground's top layer */
    int sea_row;      /* row of sea level: open sky below it is water; -1 for a world with no sea */
    uint16_t *layers[LAYER_COUNT];
    uint8_t *biome;   /* which of content.biomes each column is */
    /* Water: eighths of it in every square (see water.h), and which columns
     * have water that may still be on the move. */
    uint8_t *water;
    uint8_t *water_awake;
    bool water_stirring; /* any column is awake */
    /* A number kept with a square, for a block there that is more than its
     * kind says: the two colours of a two-tone flower (see tone_pack). 0 for none. */
    uint16_t *meta;
} World;

/* Allocates the grid and generates terrain from the seed. */
bool world_init(World *w, int width, int height, uint32_t seed, WorldKind kind);
void world_free(World *w);

/* The kind of country a column is. */
int world_biome(const World *w, int x);
/* Works the biomes out again from the land as it lies: for a world saved
 * before they were recorded. */
void world_guess_biomes(World *w);

/* Any x is valid and wraps; only y can be out of bounds. */
bool world_in_bounds(const World *w, int x, int y);
int world_wrap_x(const World *w, int x);
/* Signed distance from column `from` to column `to`, the short way round. */
int world_dx(const World *w, int from, int to);
/* Height of a tile row, and back. */
int world_row_to_y(const World *w, int row);
int world_y_to_row(const World *w, int y);

/* Out-of-bounds reads as TILE_AIR. */
TileType world_get_at(const World *w, int layer, int x, int y);
void world_set_at(World *w, int layer, int x, int y, TileType t);
/* The foreground. */
TileType world_get(const World *w, int x, int y);
void world_set(World *w, int x, int y, TileType t);
/* Whether a square is under water: at least half full of it. Water is not a
 * block; see water.h. */
bool world_is_water(const World *w, int x, int y);

/* The number kept with a square (see World.meta). It goes when a block that
 * had one is replaced, so set it after the block. */
int world_meta(const World *w, int x, int y);
void world_set_meta(World *w, int x, int y, int value);
/* Grows the plant in a square on by one stage, if it has one to grow to and
 * the room and the water it needs. A plant whose last stage is two blocks
 * tall gets its top put on. True if it grew. */
bool world_grow_plant(World *w, int layer, int x, int y);

/* Some blocks stand several tiles tall and are one thing. Given any tile of
 * one, this returns how many tiles it has and sets `bottom` to its lowest
 * row; its tiles are that row and the ones above it. 1 for an ordinary block. */
int world_structure(const World *w, int layer, int x, int y, int *bottom);

/* Whether a square has steps in it: something not solid that can still be stood on. */
bool world_is_step(const World *w, int x, int y);
/* Above and below the world counts as solid. */
bool world_is_solid(const World *w, int x, int y);

#endif
