#ifndef INDUSTRY_H
#define INDUSTRY_H

#include "game.h"

/* Industry: things that work by themselves. Miners take ore from blocks that
 * never run out; machines make what a bench makes, from what is put in them;
 * extractors take things out of whatever holds them and conveyors carry them
 * to the nearest thing that will have them. All of it is what the blocks'
 * definitions say they are: nothing here knows any block by name. */

/* One tick of all of it. */
void industry_tick(Game *g);

/* Whether the block at a square is a machine that is told what to make, and
 * the recipes it could be told: those of the benches beside it that it has
 * the slots for. Returns how many, at most `max`. */
bool game_machine_chooses(const Game *g, int x, int y);
int game_machine_recipes(const Game *g, int x, int y, const Recipe **out, int max);
/* What a machine is making, or has been set to make; NULL for nothing. Setting
 * it (NULL for nothing) is refused for a recipe it could not be told. */
const Recipe *game_machine_recipe(const Game *g, int x, int y);
bool game_machine_set_recipe(Game *g, int x, int y, const Recipe *r);
/* How far along the one it is making is, 0..1; -1 if it is making nothing. */
float game_machine_progress(const Game *g, int x, int y);
/* How many of a machine's slots are for what goes in; the one after them is for what comes out. 0 for a block that is no such machine. */
int game_machine_inputs(const Game *g, int x, int y);

#endif
