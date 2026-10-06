#ifndef CRAFT_H
#define CRAFT_H

#include "inventory.h"

#define RECIPE_MAX_INPUTS 3
#define MAX_CRAFT_AMOUNT 99

typedef struct {
    ItemId id;
    int count;
} RecipeItem;

typedef struct {
    RecipeItem output;
    RecipeItem inputs[RECIPE_MAX_INPUTS];
    int input_count;
} Recipe;

/* What a kind of bench can make. Returns NULL and 0 for tiles that are not benches. */
const Recipe *bench_recipes(TileType bench, int *count);

/* How many times the recipe can be made from what the inventory holds, up to
 * MAX_CRAFT_AMOUNT. */
int recipe_max_crafts(const Inventory *inv, const Recipe *r);

/* Makes the recipe once. Returns -1 if the inputs are missing, otherwise how
 * many of the output did not fit in the inventory. */
int recipe_craft(Inventory *inv, const Recipe *r);

#endif
