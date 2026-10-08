#include "craft.h"

#include <string.h>

int bench_recipes(TileType bench, const Recipe **out, int max)
{
    const TileDef *def = tile_def(bench);
    int n = 0;
    if (!def->bench) return 0;
    for (int i = 0; i < content.recipe_count && n < max; i++) {
        const Recipe *r = &content.recipes[i];
        if (strcmp(r->bench, def->bench_kind) != 0) continue;
        /* Lowest level first; within a level, in the order the data gives. */
        int at = n++;
        for (; at > 0 && out[at - 1]->level > r->level; at--) out[at] = out[at - 1];
        out[at] = r;
    }
    return n;
}

bool recipe_locked(TileType bench, const Recipe *r)
{
    return r->level > tile_def(bench)->bench_level;
}

int recipe_input_held(const Inventory *inv, RecipeItem item)
{
    return item.full ? inventory_count_full(inv, item.id) : inventory_count(inv, item.id);
}

bool inventory_has_all(const Inventory *inv, const RecipeItem *items, int count)
{
    for (int i = 0; i < count; i++)
        if (recipe_input_held(inv, items[i]) < items[i].count) return false;
    return true;
}

void inventory_remove_all(Inventory *inv, const RecipeItem *items, int count)
{
    /* What has to be full is emptied and kept; the rest is used up. */
    for (int i = 0; i < count; i++) {
        if (items[i].full) inventory_drain(inv, items[i].id, items[i].count);
        else inventory_remove(inv, items[i].id, items[i].count);
    }
}

int recipe_max_crafts(const Inventory *inv, const Recipe *r, int crystals)
{
    int max = MAX_CRAFT_AMOUNT;
    for (int i = 0; i < r->input_count; i++) {
        int possible = recipe_input_held(inv, r->inputs[i]) / r->inputs[i].count;
        if (possible < max) max = possible;
    }
    if (r->crystals > 0 && crystals / r->crystals < max) max = crystals / r->crystals;
    return max;
}

int recipe_craft(Inventory *inv, const Recipe *r, int *crystals)
{
    if (recipe_max_crafts(inv, r, *crystals) < 1) return -1;
    inventory_remove_all(inv, r->inputs, r->input_count);
    *crystals -= r->crystals;
    if (r->tint > 0 && item_def(r->output.id)->tinted) {
        /* What comes out in a colour carries it: each on its own meter. */
        int left = 0;
        for (int i = 0; i < r->output.count; i++) {
            ItemStack one = { (uint16_t)r->output.id, 1, 0 };
            inventory_set_meter(inv, &one, r->tint);
            if (inventory_add_stack(inv, one) != 0) {
                container_free(inv, one.container);
                left++;
            }
        }
        return left;
    }
    return inventory_add(inv, r->output.id, r->output.count);
}

ItemId recipe_shown(const Recipe *r, double seconds)
{
    if (r->one_of_count < 2) return r->output.id;
    int turn = (int)(seconds / 0.8);
    return r->one_of[(turn % r->one_of_count + r->one_of_count) % r->one_of_count].item;
}
