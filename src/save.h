#ifndef SAVE_H
#define SAVE_H

#include "game.h"

/* Writes the whole world and the player's state to a file. The file is
 * written beside its destination first and renamed into place, so a failed
 * save leaves the old one intact. */
bool save_game(const Game *g, const char *path);

/* Loads a save into `g`, which must not be initialised. On failure `g` is
 * left empty. Actions that were queued when the game was saved are not kept. */
bool load_game(Game *g, const char *path);

#define PREVIEW_RADIUS 10
#define PREVIEW_SIZE (2 * PREVIEW_RADIUS + 1)

/* The blocks around a saved world's spawn point, every layer, row by row. */
typedef struct {
    uint16_t tiles[LAYER_COUNT][PREVIEW_SIZE * PREVIEW_SIZE];
    uint64_t tick;
} WorldPreview;

/* Reads only the spawn area out of a save: PREVIEW_RADIUS tiles each way,
 * with whatever has been built or dug there. Much cheaper than loading the
 * world. Returns false if the file is not a save this game can read. */
bool save_read_preview(const char *path, WorldPreview *out);

/* What belongs to the player rather than to any one world: their crystals,
 * and what is in the shared chest. It is kept in a small file of its own. A
 * missing file is a new player, who starts with what the data gives them. */
void profile_load(const char *path, Game *g);
bool profile_save(const char *path, const Game *g);

#endif
