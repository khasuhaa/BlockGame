#ifndef CRAFT_H
#define CRAFT_H

#include "inventory.h"

#define MAX_CRAFT_AMOUNT 99
#define MAX_BENCH_RECIPES 768

/* Every recipe of a bench tile's kind, whatever level it needs: fills `out`
 * with up to `max` and returns how many. A tile that is not a bench has none. */
int bench_recipes(TileType bench, const Recipe **out, int max);
/* True if the recipe needs a higher level of bench than this tile is. */
bool recipe_locked(TileType bench, const Recipe *r);

/* Whether the inventory holds everything on a list, and taking it. */
bool inventory_has_all(const Inventory *inv, const RecipeItem *items, int count);
void inventory_remove_all(Inventory *inv, const RecipeItem *items, int count);

/* How many of an input are held: of a "full" one, only those that are full. */
int recipe_input_held(const Inventory *inv, RecipeItem item);

/* How many times the recipe can be made from what the inventory holds and the
 * crystals there are, up to MAX_CRAFT_AMOUNT. */
int recipe_max_crafts(const Inventory *inv, const Recipe *r, int crystals);

/* Makes the recipe once, taking its inputs and its crystals. Returns -1 if
 * either is missing, otherwise how many of the output did not fit in the
 * inventory. */
int recipe_craft(Inventory *inv, const Recipe *r, int *crystals);

/* What a recipe is shown as making. For one that makes one of several things by chance it is each of them in
 * turn, a little under a second apiece, by the time given: so that the picture goes through all it might be. */
ItemId recipe_shown(const Recipe *r, double seconds);

#endif
