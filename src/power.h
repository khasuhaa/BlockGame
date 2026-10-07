#ifndef POWER_H
#define POWER_H

#include "game.h"

/* Electricity. Generators make it, a tick at a time; batteries hold it;
 * wires carry it, one block a tick; electric benches and lift motors use it.
 * Whatever wants some asks the nearest thing that has some, along the wires,
 * and it comes down the wire to them: nothing arrives sooner than the wire
 * is long. */

/* One tick of all of it. */
void power_tick(Game *g);
/* Starts everything afresh: nothing on its way anywhere. */
void power_reset(Game *g);
/* How much a thing at a square has in it now, and the most it holds (0 for a
 * thing that only passes it on or uses it as it comes). */
int game_power_at(const Game *g, int x, int y, int *capacity);
/* What a generator at a square made in the last tick. */
int game_power_made(const Game *g, int x, int y);
/* What the character's bench is still waiting for before it can start, and
 * how much of that has come; 0 if it is waiting for nothing. */
int game_craft_power(const Game *g, int *arrived);
/* What making something at an electric bench takes: 0 at any other. */
int game_power_for(const Game *g, TileType bench, const Recipe *r, int count);
/* Electricity on its way along a wire, for drawing: the squares it is in
 * now and the ones it is going to next. Returns how many, at most `max`. */
typedef struct {
    int x, y, to_x, to_y;
} Spark;
int game_sparks(const Game *g, Spark *out, int max);

#endif
