#ifndef SHIP_H
#define SHIP_H

#include "game.h"

/* A world adrift (see "adrift" in WorldGenDef): open sea, and in the middle of it a ship made of blocks that
 * never moves while the sea goes by. Everything here is worked out from the data: what a block adds to a ship,
 * what a keel allows, what each sea asks, gives and brings. */

/* Whether this world is one. */
bool game_adrift(const Game *g);
/* The sea she is in. NULL in a world that is not adrift. */
const SeaDef *game_sea(const Game *g);
/* Counts the ship up afresh: done every so many ticks, and by anything that cannot wait. */
void ship_measure(Game *g);
/* A tick of the sea: things appear ahead and drift astern, nets and trawls fill, things come alongside and go,
 * a passage is made, the guns speak. */
void ship_tick(Game *g);
/* How much she has of something a sea may ask (NEED_KEEL and the rest). */
int ship_has(const Game *g, int need);
/* Whether she could go to a sea; `lacking` gets the first thing she has too little of, or -1. */
bool ship_can_sail(const Game *g, int sea, int *lacking);
/* Sets a course for a sea: whatever is alongside is let go. False if she cannot go there, or is on her way already. */
bool ship_set_course(Game *g, int sea);
/* Lets go of what is alongside. What was built there by hand stays. */
bool ship_cast_off(Game *g);
/* What is alongside, as the sea calls it; NULL for nothing. */
const char *ship_alongside(const Game *g);
/* With a hook in hand, hauls in a thing adrift that is near enough. False if there is no hook or it is too far. */
bool ship_hook(Game *g, int drop);
/* Whether a drop is being carried by the sea, and how far it is toward the next square it will be in (0 to 1). */
bool ship_drop_adrift(const Game *g, int drop);
float ship_drift_phase(const Game *g);
/* How fast the sea goes by, in squares a second: for whatever draws it. */
float ship_speed(const Game *g);
/* Whether the block that is open is a helm. */
bool game_helm_open(const Game *g);

#endif
