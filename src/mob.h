#ifndef MOB_H
#define MOB_H

#include "game.h"
#include "tree.h"

/* Creatures: where they come from, how they get about, and how they end. */

/* Puts a creature in the world. Returns its slot, or -1 if there is no room
 * for another. */
int mob_spawn(Game *g, int kind, int x, int y, bool owned, bool baby);
/* One tick for every creature: growing up, wandering, laying, and going away
 * for good once far enough from the player, unless the player owns it. */
void mobs_tick(Game *g);
/* What a random event does to a tile, as far as creatures go: a wild one may
 * turn up on open ground near the player, and an egg hatches. Neither happens
 * to a wild creature while the cap is reached. Each returns true if that is
 * what happened. */
bool mobs_try_spawn(Game *g, int x, int y);
bool mobs_try_hatch(Game *g, int x, int y);
/* Takes so much off a creature. Returns true if that was the end of it. */
bool mob_hurt(Game *g, int mob, int damage);
/* Kills a creature, leaving what a grown one leaves. */
void mob_kill(Game *g, int mob);
/* How many creatures of the land are within spawning range of the player. */
int mobs_near_player(const Game *g);
/* The same for the creatures of the water, which have a cap of their own. */
int mobs_near_player_in_water(const Game *g);

/* The creature under a point in tile space, or -1. */
int game_mob_at(const Game *g, float x, float y);
/* Where a creature is drawn: the top-left of its tile, part-way through its
 * step, and which way it faces. */
void game_mob_pos(const Game *g, int mob, float *x, float *y);
/* Whether a creature is on the move - walking, climbing or falling - and how
 * far through its stride it is, 0..1. */
bool game_mob_walking(const Game *g, int mob, float *stride);
/* A click on a creature: queues going after it and killing it, or takes that
 * back if it is already queued. */
bool game_click_mob(Game *g, int mob);

#endif
