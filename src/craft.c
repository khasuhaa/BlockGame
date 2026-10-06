#include "craft.h"

#include <stddef.h>

static const Recipe spawn_portal_recipes[] = {
    { { ITEM_WORKBENCH, 1 }, { { ITEM_DIRT, 1 } }, 1 },
};

static const Recipe workbench_recipes[] = {
    { { ITEM_BASKET, 1 }, { { ITEM_DIRT, 1 } }, 1 },
};

const Recipe *bench_recipes(TileType bench, int *count)
{
    switch (bench) {
    case TILE_SPAWN_PORTAL:
        *count = (int)(sizeof spawn_portal_recipes / sizeof spawn_portal_recipes[0]);
        return spawn_portal_recipes;
    case TILE_WORKBENCH:
        *count = (int)(sizeof workbench_recipes / sizeof workbench_recipes[0]);
        return workbench_recipes;
    default:
        *count = 0;
        return NULL;
    }
}

int recipe_max_crafts(const Inventory *inv, const Recipe *r)
{
    int max = MAX_CRAFT_AMOUNT;
    for (int i = 0; i < r->input_count; i++) {
        int possible = inventory_count(inv, r->inputs[i].id) / r->inputs[i].count;
        if (possible < max) max = possible;
    }
    return max;
}

int recipe_craft(Inventory *inv, const Recipe *r)
{
    if (recipe_max_crafts(inv, r) < 1) return -1;
    for (int i = 0; i < r->input_count; i++)
        inventory_remove(inv, r->inputs[i].id, r->inputs[i].count);
    return inventory_add(inv, r->output.id, r->output.count);
}
