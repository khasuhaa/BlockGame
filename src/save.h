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

/* Crystals belong to the player, not to a world: they are kept in a small
 * file of their own. A missing file is a new player, who starts with what the
 * data gives them. */
int load_crystals(const char *path);
bool save_crystals(const char *path, int crystals);

#endif
