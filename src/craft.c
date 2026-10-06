#include "craft.h"

#include <string.h>

int bench_recipes(TileType bench, const Recipe **out, int max)
{
    const TileDef *def = tile_def(bench);
    int n = 0;
    if (!def->bench) return 0;
    for (int i = 0; i < content.recipe_count && n < max; i++)
        if (strcmp(content.recipes[i].bench, def->bench_kind) == 0) out[n++] = &content.recipes[i];
    return n;
}

bool recipe_locked(TileType bench, const Recipe *r)
{
    return r->level > tile_def(bench)->bench_level;
}

bool inventory_has_all(const Inventory *inv, const RecipeItem *items, int count)
{
    for (int i = 0; i < count; i++)
        if (inventory_count(inv, items[i].id) < items[i].count) return false;
    return true;
}

void inventory_remove_all(Inventory *inv, const RecipeItem *items, int count)
{
    for (int i = 0; i < count; i++) inventory_remove(inv, items[i].id, items[i].count);
}

int recipe_max_crafts(const Inventory *inv, const Recipe *r, int crystals)
{
    int max = MAX_CRAFT_AMOUNT;
    for (int i = 0; i < r->input_count; i++) {
        int possible = inventory_count(inv, r->inputs[i].id) / r->inputs[i].count;
        if (possible < max) max = possible;
    }
    if (r->crystals > 0 && crystals / r->crystals < max) max = crystals / r->crystals;
    return max;
}

int recipe_craft(Inventory *inv, const Recipe *r, int *crystals)
{
    if (recipe_max_crafts(inv, r, *crystals) < 1) return -1;
    for (int i = 0; i < r->input_count; i++)
        inventory_remove(inv, r->inputs[i].id, r->inputs[i].count);
    *crystals -= r->crystals;
    return inventory_add(inv, r->output.id, r->output.count);
}
