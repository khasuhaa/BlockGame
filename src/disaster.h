#ifndef DISASTER_H
#define DISASTER_H

#include "game.h"

/* Disasters ("disasters" in the rules): now and then one begins, lasts a while, and every so many ticks does what
 * its data says somewhere near a character - a bolt, a blast, a burning, water, a shaking loose, hurt. With none
 * in the data nothing here does anything. One that is going on is not saved: it is over when the world is opened again. */

/* The one going on, or NULL. */
const DisasterDef *game_disaster(const Game *g);
/* Begins one (which of content.rules.disasters). False if there is no such, or one is going on already. */
bool disaster_start(Game *g, int which);
void disaster_tick(Game *g);

#endif
