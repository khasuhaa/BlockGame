#include "inventory.h"

#include <string.h>

static const ItemDef item_defs[ITEM_COUNT] = {
    [ITEM_NONE] = { "Nothing", 0, false, EQUIP_NONE, TILE_AIR },
    [ITEM_BASKET] = { "Basket", 1, true, EQUIP_NONE, TILE_AIR },
    [ITEM_DIRT] = { "Dirt", MAX_STACK, false, EQUIP_NONE, TILE_DIRT },
    [ITEM_WORKBENCH] = { "Workbench", MAX_STACK, false, EQUIP_NONE, TILE_WORKBENCH },
    [ITEM_STONE] = { "Stone", MAX_STACK, false, EQUIP_NONE, TILE_STONE },
};

const ItemDef *item_def(ItemId id)
{
    return &item_defs[id < ITEM_COUNT ? id : ITEM_NONE];
}

ItemId item_for_tile(TileType t)
{
    switch (t) {
    case TILE_DIRT:
    case TILE_GRASS: return ITEM_DIRT;
    case TILE_STONE: return ITEM_STONE;
    case TILE_WORKBENCH: return ITEM_WORKBENCH;
    default: return ITEM_NONE;
    }
}

static bool is_empty(const ItemStack *s)
{
    return s->id == ITEM_NONE || s->count == 0;
}

/* A fresh stack, with its own empty container if the item is one. Returns an
 * empty stack if the item needs a container and none are left. */
static ItemStack item_make(Inventory *inv, ItemId id, int count)
{
    ItemStack s = { (uint8_t)id, (uint8_t)count, 0 };
    if (!item_def(id)->container) return s;
    for (int i = 1; i < MAX_CONTAINERS; i++) {
        if (inv->containers[i].used) continue;
        memset(&inv->containers[i], 0, sizeof inv->containers[i]);
        inv->containers[i].used = true;
        s.container = (uint16_t)i;
        return s;
    }
    return (ItemStack){ 0 };
}

void inventory_init(Inventory *inv)
{
    memset(inv, 0, sizeof *inv);
    inv->sel_inner = -1;
    inv->slots[0] = item_make(inv, ITEM_BASKET, 1);
}

ItemStack *inventory_slot(Inventory *inv, SlotRef ref)
{
    switch (ref.kind) {
    case SLOT_BAR:
        if (ref.index < 0 || ref.index >= INVENTORY_SLOTS) return NULL;
        return &inv->slots[ref.index];
    case SLOT_ARMOR:
        if (ref.index < 0 || ref.index >= ARMOR_SLOTS) return NULL;
        return &inv->armor[ref.index];
    case SLOT_BASKET: {
        if (ref.index < 0 || ref.index >= INVENTORY_SLOTS) return NULL;
        if (ref.inner < 0 || ref.inner >= BASKET_SLOTS) return NULL;
        const ItemStack *basket = &inv->slots[ref.index];
        if (is_empty(basket) || basket->container == 0) return NULL;
        return &inv->containers[basket->container].slots[ref.inner];
    }
    default:
        return NULL;
    }
}

/* Tops up stacks of `id` in `slots`; returns what is left of `count`. */
static int merge_into(ItemStack *slots, int n, ItemId id, int count)
{
    int max = item_def(id)->max_stack;
    for (int i = 0; i < n && count > 0; i++) {
        if (is_empty(&slots[i]) || slots[i].id != id || slots[i].count >= max) continue;
        int moved = max - slots[i].count < count ? max - slots[i].count : count;
        slots[i].count = (uint8_t)(slots[i].count + moved);
        count -= moved;
    }
    return count;
}

/* Starts new stacks of `id` in empty `slots`; returns what is left. */
static int fill_empty(Inventory *inv, ItemStack *slots, int n, ItemId id, int count)
{
    int max = item_def(id)->max_stack;
    for (int i = 0; i < n && count > 0; i++) {
        if (!is_empty(&slots[i])) continue;
        int moved = max < count ? max : count;
        ItemStack s = item_make(inv, id, moved);
        if (is_empty(&s)) break;
        slots[i] = s;
        count -= moved;
    }
    return count;
}

int inventory_add(Inventory *inv, ItemId id, int count)
{
    if (id == ITEM_NONE || id >= ITEM_COUNT) return count;

    /* Pass 0 tops up existing stacks, pass 1 starts new ones; within each,
     * the bar comes before the baskets sitting in it. */
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 0 && item_def(id)->max_stack <= 1) continue;
        for (int b = -1; b < INVENTORY_SLOTS && count > 0; b++) {
            ItemStack *slots = inv->slots;
            int n = INVENTORY_SLOTS;
            if (b >= 0) {
                slots = inventory_slot(inv, (SlotRef){ SLOT_BASKET, b, 0 });
                n = BASKET_SLOTS;
                if (!slots) continue;
            }
            count = pass == 0 ? merge_into(slots, n, id, count)
                              : fill_empty(inv, slots, n, id, count);
        }
    }
    return count;
}

int inventory_count(const Inventory *inv, ItemId id)
{
    int total = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        const ItemStack *s = &inv->slots[i];
        if (is_empty(s)) continue;
        if (s->id == id) total += s->count;
        if (s->container == 0) continue;
        const ItemStack *inner = inv->containers[s->container].slots;
        for (int j = 0; j < BASKET_SLOTS; j++)
            if (!is_empty(&inner[j]) && inner[j].id == id) total += inner[j].count;
    }
    return total;
}

static void take_from(ItemStack *s, ItemId id, int *want)
{
    if (!s || is_empty(s) || s->id != id || *want <= 0) return;
    int taken = s->count < *want ? s->count : *want;
    s->count = (uint8_t)(s->count - taken);
    *want -= taken;
    if (s->count == 0) *s = (ItemStack){ 0 };
}

int inventory_remove(Inventory *inv, ItemId id, int count)
{
    int want = count;
    if (inventory_selection_inside(inv, inv->sel_slot))
        take_from(inventory_slot(inv, (SlotRef){ SLOT_BASKET, inv->sel_slot, inv->sel_inner }), id,
                  &want);
    take_from(&inv->slots[inv->sel_slot], id, &want);
    for (int i = 0; i < INVENTORY_SLOTS; i++) take_from(&inv->slots[i], id, &want);
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        for (int j = 0; j < BASKET_SLOTS; j++)
            take_from(inventory_slot(inv, (SlotRef){ SLOT_BASKET, i, j }), id, &want);
    return count - want;
}

/* Whether `s` may be put in the slot `ref` points at. */
static bool slot_accepts(Inventory *inv, SlotRef ref, const ItemStack *s)
{
    if (is_empty(s)) return true;
    if (ref.kind == SLOT_ARMOR) return (int)item_def(s->id)->equip == ref.index;
    /* A basket cannot go inside itself. */
    if (ref.kind == SLOT_BASKET && s->container != 0 &&
        s->container == inv->slots[ref.index].container)
        return false;
    return true;
}

static void normalize_selection(Inventory *inv)
{
    if (inv->sel_inner < 0) return;
    if (!inventory_slot(inv, (SlotRef){ SLOT_BASKET, inv->sel_slot, inv->sel_inner }))
        inv->sel_inner = -1;
}

bool inventory_move(Inventory *inv, SlotRef from, SlotRef to, int count)
{
    ItemStack *a = inventory_slot(inv, from);
    ItemStack *b = inventory_slot(inv, to);
    if (!a || !b || a == b || is_empty(a) || count < 1) return false;
    if (!slot_accepts(inv, to, a)) return false;
    if (count > a->count) count = a->count;

    int max = item_def(a->id)->max_stack;
    if (!is_empty(b) && b->id == a->id && max > 1) {
        int moved = max - b->count < count ? max - b->count : count;
        if (moved == 0) return false;
        b->count = (uint8_t)(b->count + moved);
        a->count = (uint8_t)(a->count - moved);
        if (a->count == 0) *a = (ItemStack){ 0 };
        return true;
    }

    if (count < a->count) {
        /* Splitting a stack needs somewhere empty to put the part taken. */
        if (!is_empty(b)) return false;
        *b = (ItemStack){ a->id, (uint8_t)count, 0 };
        a->count = (uint8_t)(a->count - count);
        return true;
    }

    if (!slot_accepts(inv, from, b)) return false;
    ItemStack tmp = *a;
    *a = is_empty(b) ? (ItemStack){ 0 } : *b;
    *b = tmp;
    normalize_selection(inv);
    return true;
}

void inventory_select(Inventory *inv, int slot, int inner)
{
    if (slot < 0 || slot >= INVENTORY_SLOTS) return;
    inv->sel_slot = slot;
    inv->sel_inner = inner;
    normalize_selection(inv);
}

bool inventory_selection_inside(const Inventory *inv, int slot)
{
    if (inv->sel_slot != slot || inv->sel_inner < 0) return false;
    const ItemStack *basket = &inv->slots[slot];
    if (is_empty(basket) || basket->container == 0) return false;
    return !is_empty(&inv->containers[basket->container].slots[inv->sel_inner]);
}

const ItemStack *inventory_selected(const Inventory *inv)
{
    const ItemStack *s = &inv->slots[inv->sel_slot];
    if (inventory_selection_inside(inv, inv->sel_slot))
        return &inv->containers[s->container].slots[inv->sel_inner];
    return s;
}
