#ifndef WORKERS_H
#define WORKERS_H

#include "game.h"

/* Workers. A post is a block that stores things and has "work" in its data: with an item that "hires" in its
 * chest (and, if the rules ask for meals, something to eat) it does its work every so many ticks, out of that
 * chest and into it, and a creature - its worker - is shown where the work is. Nothing about a worker is kept
 * but what the post's record already has room for: the work is found again each time by looking at the world.
 *
 *   mine    a shaft down beside the post, laddered, and from it a tunnel two high as far as the tools in the
 *           chest will dig; then two further down, and again; at the bottom of the world, nothing.
 *   farm    between the markers either side of it: what is ripe is reaped, and what is bare is sown from the chest.
 *   lumber  grown trees within reach are felled, with an axe from the chest.
 *   breed   eggs laid within reach are left to hatch until there is a flock, and gathered after.
 *   fish    with water within reach, something from its list now and then.
 *   gather  whatever lies dropped within reach is brought in.
 *   guard   whatever would hurt somebody, within reach, is struck with the best weapon in the chest. */
void workers_tick(Game *g);
/* What the post on a square is doing, for whoever wants to say: 0 nothing (no post, or nobody hired), 1 working,
 * 2 hungry, 3 nothing left that it can do. */
int game_worker_state(const Game *g, int x, int y);

#endif
