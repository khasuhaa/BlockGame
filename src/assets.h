#ifndef ASSETS_H
#define ASSETS_H

#include "content.h"
#include "raylib.h"

#define TEXTURE_PIXELS_PER_TILE 32.0f

/* Loads the textures the content names, from `<dir>/textures`. Needs a
 * window, and the content to be loaded first. A texture that is missing is
 * reported and drawn as a placeholder. */
void assets_load(const char *dir);
/* The same from several directories: the game's own first, then each mod's, a later one's textures coming before an earlier one's. */
void assets_load_from(const char *const *dirs, int count);
void assets_unload(void);
/* Which season blocks that change with the seasons are drawn in from now on. */
void assets_set_season(int season);

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
/* The same for a block with a fire in it, which looks different while that is
 * burning: its frames of flame, one after another. */
void assets_draw_tile_lit(TileType t, int x, int y, float px, float py, float tile_size, float dim, TilePart part,
                          bool lit);
/* A block drawn plainly, as it is or mirrored left to right: steps, which rise one way or the other. */
void assets_draw_tile_facing(TileType t, int x, int y, float px, float py, float tile_size, float dim, bool flip);
/* Something in the background as it looks fixed to what `mount` says (a MOUNT_*), where it has a look for that. */
void assets_draw_tile_mounted(TileType t, int x, int y, float px, float py, float tile_size, float dim, int mount);
/* And for a block that joins up with the ones beside it: `joined` says which
 * sides have one, 1 for above + 2 right + 4 below + 8 left. */
void assets_draw_tile_joined(TileType t, int x, int y, float px, float py, float tile_size, float dim, int joined);
/* Which part a tile is, from the tiles of its layer above and below it. A
 * block that stands alone counts as a top. */
TilePart assets_tile_part(TileType t, TileType above, TileType below);
/* Draws an item in the middle of a rectangle. Wherever in its image the
 * picture sits, it is what is drawn that gets centred, and it is sized to sit
 * a little inside the rectangle. */
void assets_draw_item(ItemId id, Rectangle r);
/* The same for a two-tone item: the parts of it that take its two colours are
 * drawn over it in them. `tones` is the pair, as tone_pack makes it. */
void assets_draw_item_tones(ItemId id, Rectangle r, int tones);
/* The coloured parts of a two-tone block, drawn over the block itself. */
void assets_draw_tile_tones(TileType t, float px, float py, float tile_size, float dim, int tones);
/* The one colour that stands for a tile on a small map: the average of its texture. */
Color assets_tile_color(TileType t);
/* Draws any other image under `<dir>/textures`, loading it on first use. */
void assets_draw_image(const char *file, Rectangle r);
/* The same, optionally mirrored left to right. */
void assets_draw_sprite(const char *file, Rectangle r, bool flip);

#endif
