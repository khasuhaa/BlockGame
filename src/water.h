#ifndef WATER_H
#define WATER_H

#include "world.h"

/* Water is not a block: every square of the world holds so many eighths of
 * it, none to WATER_FULL. It is never made or lost by moving - it falls,
 * spreads out level, and runs over edges - so however the land is dug about,
 * the amount of it stays the same. */
#define WATER_FULL 8

/* Eighths of water in a square. None in a solid block, or outside the world. */
int world_water(const World *w, int x, int y);
/* Sets it outright, as generating a world does. */
void world_set_water(World *w, int x, int y, int level);
/* Fills everything open to the sky below sea level, as a new world has it. */
void water_fill_sea(World *w);
/* The sea as a world starts with it: filled, let run into whatever caves
 * open on it, and filled up again, until it is both full and still. */
void water_make_sea(World *w);
/* Tells the water round a column that something there has changed. */
void water_wake(World *w, int x);
/* Moves the water on one step. Only the columns where something has lately
 * changed are looked at; water at rest costs nothing. Returns how many
 * eighths moved. */
int water_step(World *w);
/* Wakes all of it and lets it run until it is still, or `max_steps` have
 * gone by: what a sea does to the caves it opens into, done once when a
 * world is made rather than whenever somebody first disturbs it. */
void water_settle(World *w, int max_steps);
/* A block has been put where there was water: it goes up, or failing that
 * aside. */
void water_displace(World *w, int x, int y);
/* All the water in the world, in eighths. */
long water_total(const World *w);

#endif
