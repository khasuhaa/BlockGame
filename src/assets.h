#ifndef ASSETS_H
#define ASSETS_H

#include "content.h"
#include "raylib.h"

#define TEXTURE_PIXELS_PER_TILE 32.0f

/* Loads the textures the content names, from `<dir>/textures`. Needs a
 * window, and the content to be loaded first. A texture that is missing is
 * reported and drawn as a placeholder. */
void assets_load(const char *dir);
void assets_unload(void);

/* Draws a tile with its top-left corner at (px, py). `x` and `y` are its
 * position in the world, which picks its texture and shade. `dim` darkens it,
 * 0..1. `part` says where the tile is in a column of its own kind, for blocks
 * that look different at the top or the bottom of one. */
typedef enum {
    TILE_PART_BODY,
    TILE_PART_TOP,
    TILE_PART_BASE
} TilePart;
void assets_draw_tile(TileType t, int x, int y, float px, float py, float tile_size, float dim, TilePart part);
/* Which part a tile is, from the tiles of its layer above and below it. A
 * block that stands alone counts as a top. */
TilePart assets_tile_part(TileType t, TileType above, TileType below);
void assets_draw_item(ItemId id, Rectangle r);
/* Draws any other image under `<dir>/textures`, loading it on first use. */
void assets_draw_image(const char *file, Rectangle r);
/* The same, optionally mirrored left to right. */
void assets_draw_sprite(const char *file, Rectangle r, bool flip);

#endif
