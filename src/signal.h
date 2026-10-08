#ifndef SIGNAL_H
#define SIGNAL_H

#include "game.h"

/* Signals, and the machinery that goes by them.
 *
 * A signal is a strength from 0 to 15. Some blocks make one (a lever that is on, a button pressed, a plate stood
 * on, a clock, a block that is nothing but a source); wire carries it, a point weaker with every square; and
 * anything else beside a square that has one is powered. What a powered block does is in its definition: it
 * turns into another block, pushes what is in front of it, digs, builds, pours, blows up.
 *
 * Blocks that read a signal on one side and make one on another - repeaters, inverters, gates - follow their
 * inputs a tick late at the soonest, so a loop of them settles or ticks over and never hangs.
 *
 * None of it is saved but what the blocks themselves are: after loading, a tick puts every signal back. */

/* One tick of all of it. */
void signal_tick(Game *g);
/* The signal in a square just now: what a wire there carries or a source there makes; 0 for none. */
int game_signal_at(const Game *g, int x, int y);
/* Uses the block in a square, if it is one that using turns into another: a lever thrown, a piston turned. */
bool game_use_block(Game *g, int x, int y);

#endif
