#include "craft.h"

int bench_recipes(TileType bench, const Recipe **out, int max)
{
    int n = 0;
    for (int i = 0; i < content.recipe_count && n < max; i++)
        if (content.recipes[i].bench == bench) out[n++] = &content.recipes[i];
    return n;
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
