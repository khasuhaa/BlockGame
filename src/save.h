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

/* What belongs to the player rather than to any one world: their crystals,
 * and what is in the shared chest. It is kept in a small file of its own. A
 * missing file is a new player, who starts with what the data gives them. */
void profile_load(const char *path, Game *g);
bool profile_save(const char *path, const Game *g);

#endif
