#ifndef CRAFT_H
#define CRAFT_H

#include "inventory.h"

#define MAX_CRAFT_AMOUNT 99
#define MAX_BENCH_RECIPES 64

/* What a kind of bench can make: fills `out` with up to `max` recipes and
 * returns how many. A tile that is not a bench has none. */
int bench_recipes(TileType bench, const Recipe **out, int max);

/* How many times the recipe can be made from what the inventory holds, up to
 * MAX_CRAFT_AMOUNT. */
int recipe_max_crafts(const Inventory *inv, const Recipe *r);

/* Makes the recipe once. Returns -1 if the inputs are missing, otherwise how
 * many of the output did not fit in the inventory. */
int recipe_craft(Inventory *inv, const Recipe *r);

#endif
