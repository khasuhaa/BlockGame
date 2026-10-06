#include "inventory.h"

#include <string.h>

#define MAX_NESTING 8

static bool is_empty(const ItemStack *s)
{
    return s->id == ITEM_NONE || s->count == 0;
}

/* ---- containers -------------------------------------------------------- */

uint16_t container_alloc(Inventory *inv)
{
    for (int i = 1; i < SHARED_CONTAINER; i++) {
        if (inv->containers[i].used) continue;
        memset(&inv->containers[i], 0, sizeof inv->containers[i]);
        inv->containers[i].used = true;
        return (uint16_t)i;
    }
    return 0;
}

static void free_nested(Inventory *inv, uint16_t id, int depth)
{
    if (id == 0 || id >= MAX_CONTAINERS || !inv->containers[id].used || depth > MAX_NESTING) return;
    for (int i = 0; i < CONTAINER_MAX_SLOTS; i++)
        free_nested(inv, inv->containers[id].slots[i].container, depth + 1);
    memset(&inv->containers[id], 0, sizeof inv->containers[id]);
}

void container_free(Inventory *inv, uint16_t id)
{
    free_nested(inv, id, 0);
}

bool container_is_empty(const Inventory *inv, uint16_t id)
{
    if (id == 0 || id >= MAX_CONTAINERS) return true;
    for (int i = 0; i < CONTAINER_MAX_SLOTS; i++)
        if (!is_empty(&inv->containers[id].slots[i])) return false;
    return true;
}

static uint16_t clone_nested(Inventory *inv, uint16_t id, bool *ok, int depth)
{
    if (container_is_empty(inv, id) || depth > MAX_NESTING) return 0;
    uint16_t copy = container_alloc(inv);
    if (copy == 0) {
        *ok = false;
        return 0;
    }
    for (int i = 0; i < CONTAINER_MAX_SLOTS; i++) {
        ItemStack s = inv->containers[id].slots[i];
        if (is_empty(&s)) continue;
        s.container = clone_nested(inv, s.container, ok, depth + 1);
        inv->containers[copy].slots[i] = s;
    }
    return copy;
}

uint16_t container_clone(Inventory *inv, uint16_t id, bool *ok)
{
    bool fine = true;
    uint16_t copy = clone_nested(inv, id, &fine, 0);
    if (!fine) {
        container_free(inv, copy);
        copy = 0;
    }
    if (ok) *ok = fine;
    return copy;
}

static bool equal_nested(const Inventory *inv, uint16_t a, uint16_t b, int depth)
{
    bool a_empty = container_is_empty(inv, a), b_empty = container_is_empty(inv, b);
    if (a_empty || b_empty) return a_empty && b_empty;
    if (a == b) return true;
    if (depth > MAX_NESTING) return false;
    for (int i = 0; i < CONTAINER_MAX_SLOTS; i++) {
        const ItemStack *x = &inv->containers[a].slots[i], *y = &inv->containers[b].slots[i];
        if (is_empty(x) != is_empty(y)) return false;
        if (is_empty(x)) continue;
        if (x->id != y->id || x->count != y->count) return false;
        if (!equal_nested(inv, x->container, y->container, depth + 1)) return false;
    }
    return true;
}

bool containers_equal(const Inventory *inv, uint16_t a, uint16_t b)
{
    return equal_nested(inv, a, b, 0);
}

/* Whether container `target` is `outer` itself or somewhere inside it. */
static bool holds(const Inventory *inv, uint16_t outer, uint16_t target, int depth)
{
    if (outer == 0 || target == 0 || depth > MAX_NESTING) return false;
    if (outer == target) return true;
    for (int i = 0; i < CONTAINER_MAX_SLOTS; i++)
        if (holds(inv, inv->containers[outer].slots[i].container, target, depth + 1)) return true;
    return false;
}

/* Two stacks that could be one: the same item with the same things inside. */
static bool can_merge(const Inventory *inv, const ItemStack *a, const ItemStack *b)
{
    return a->id == b->id && item_def(a->id)->max_stack > 1 &&
           containers_equal(inv, a->container, b->container);
}

/* ---- slots ------------------------------------------------------------- */

void inventory_init(Inventory *inv)
{
    memset(inv, 0, sizeof *inv);
    inv->sel_inner = -1;
    for (int i = 0; i < content.rules.start_item_count; i++)
        inventory_add(inv, content.rules.start_items[i].id, content.rules.start_items[i].count);
}

/* The container a bar item opens into, made on first use; 0 if it has none. */
static uint16_t bar_container(Inventory *inv, int slot)
{
    ItemStack *s = &inv->slots[slot];
    if (is_empty(s) || !item_def(s->id)->opens_in_bar) return 0;
    if (s->container == 0) s->container = container_alloc(inv);
    return s->container;
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
        uint16_t c = bar_container(inv, ref.index);
        int slots = c ? item_def(inv->slots[ref.index].id)->container_slots : 0;
        if (ref.inner < 0 || ref.inner >= slots || ref.inner >= BASKET_SLOTS) return NULL;
        return &inv->containers[c].slots[ref.inner];
    }
    case SLOT_CONTAINER:
        if (ref.index == SHARED_CONTAINER) inv->containers[ref.index].used = true;
        if (ref.index <= 0 || ref.index >= MAX_CONTAINERS || !inv->containers[ref.index].used)
            return NULL;
        if (ref.inner < 0 || ref.inner >= CONTAINER_MAX_SLOTS) return NULL;
        return &inv->containers[ref.index].slots[ref.inner];
    default:
        return NULL;
    }
}

/* The container a slot reference reaches into, or 0. */
static uint16_t ref_container(const Inventory *inv, SlotRef ref)
{
    if (ref.kind == SLOT_CONTAINER) return (uint16_t)ref.index;
    if (ref.kind == SLOT_BASKET && ref.index >= 0 && ref.index < INVENTORY_SLOTS)
        return inv->slots[ref.index].container;
    return 0;
}

/* Tops up stacks in `slots` that `stack` can merge with. */
static void merge_into(Inventory *inv, ItemStack *slots, int n, ItemStack *stack)
{
    int max = item_def(stack->id)->max_stack;
    for (int i = 0; i < n && stack->count > 0; i++) {
        if (is_empty(&slots[i]) || slots[i].count >= max || !can_merge(inv, &slots[i], stack)) continue;
        int moved = max - slots[i].count < stack->count ? max - slots[i].count : stack->count;
        slots[i].count = (uint8_t)(slots[i].count + moved);
        stack->count = (uint8_t)(stack->count - moved);
    }
}

/* Starts new stacks in empty `slots`. Each takes a copy of the contents; the
 * last one takes the original. */
static void fill_empty(Inventory *inv, ItemStack *slots, int n, ItemStack *stack)
{
    int max = item_def(stack->id)->max_stack;
    for (int i = 0; i < n && stack->count > 0; i++) {
        if (!is_empty(&slots[i])) continue;
        int moved = max < stack->count ? max : stack->count;
        bool ok = true;
        uint16_t contents = moved == stack->count ? stack->container
                                                  : container_clone(inv, stack->container, &ok);
        if (!ok) return;
        slots[i] = (ItemStack){ stack->id, (uint8_t)moved, contents };
        stack->count = (uint8_t)(stack->count - moved);
        if (stack->count == 0) stack->container = 0;
    }
}

int inventory_add_stack(Inventory *inv, ItemStack stack)
{
    if (stack.id <= ITEM_NONE || stack.id >= content.item_count) return stack.count;

    /* Pass 0 tops up existing stacks, pass 1 starts new ones; within each,
     * the bar comes before the containers open in it. */
    for (int pass = 0; pass < 2; pass++) {
        for (int b = -1; b < INVENTORY_SLOTS && stack.count > 0; b++) {
            ItemStack *slots = inv->slots;
            int n = INVENTORY_SLOTS;
            if (b >= 0) {
                /* Not into itself or anything it contains. */
                slots = inventory_slot(inv, (SlotRef){ SLOT_BASKET, b, 0 });
                if (!slots || holds(inv, stack.container, inv->slots[b].container, 0)) continue;
                n = item_def(inv->slots[b].id)->container_slots;
                if (n > BASKET_SLOTS) n = BASKET_SLOTS;
            }
            if (pass == 0) merge_into(inv, slots, n, &stack);
            else fill_empty(inv, slots, n, &stack);
        }
    }
    /* Whatever merged into a matching stack no longer needs its own contents. */
    if (stack.count == 0) container_free(inv, stack.container);
    return stack.count;
}

int inventory_add(Inventory *inv, ItemId id, int count)
{
    int left = 0;
    while (count > 0) {
        int batch = count > 255 ? 255 : count;
        int rest = inventory_add_stack(inv, (ItemStack){ (uint8_t)id, (uint8_t)batch, 0 });
        count -= batch;
        left += rest;
        if (rest > 0) return left + count;
    }
    return left;
}

int inventory_count(const Inventory *inv, ItemId id)
{
    int total = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        const ItemStack *s = &inv->slots[i];
        if (is_empty(s)) continue;
        if (s->id == id) total += s->count;
        if (s->container == 0 || !item_def(s->id)->opens_in_bar) continue;
        const ItemStack *inner = inv->containers[s->container].slots;
        for (int j = 0; j < BASKET_SLOTS; j++)
            if (!is_empty(&inner[j]) && inner[j].id == id) total += inner[j].count;
    }
    return total;
}

float inventory_tool_speed(const Inventory *inv, int tool_class)
{
    float best = 1.0f;
    if (tool_class < 0) return best;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        const ItemStack *s = &inv->slots[i];
        if (is_empty(s)) continue;
        const ItemDef *def = item_def(s->id);
        if (def->tool_class == tool_class && def->tool_speed > best) best = def->tool_speed;
        if (s->container == 0 || !def->opens_in_bar) continue;
        for (int j = 0; j < BASKET_SLOTS; j++) {
            const ItemStack *inner = &inv->containers[s->container].slots[j];
            if (is_empty(inner)) continue;
            const ItemDef *held = item_def(inner->id);
            if (held->tool_class == tool_class && held->tool_speed > best) best = held->tool_speed;
        }
    }
    return best;
}

int inventory_ore_multiplier(const Inventory *inv, int tool_class)
{
    int best = 1;
    if (tool_class < 0) return best;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        const ItemStack *s = &inv->slots[i];
        if (is_empty(s)) continue;
        const ItemDef *def = item_def(s->id);
        if (def->tool_class == tool_class && def->ore_multiplier > best) best = def->ore_multiplier;
        if (s->container == 0 || !def->opens_in_bar) continue;
        for (int j = 0; j < BASKET_SLOTS; j++) {
            const ItemStack *inner = &inv->containers[s->container].slots[j];
            if (is_empty(inner)) continue;
            const ItemDef *held = item_def(inner->id);
            if (held->tool_class == tool_class && held->ore_multiplier > best) best = held->ore_multiplier;
        }
    }
    return best;
}

static void take_from(Inventory *inv, ItemStack *s, ItemId id, int *want)
{
    if (!s || is_empty(s) || s->id != id || *want <= 0) return;
    int taken = s->count < *want ? s->count : *want;
    s->count = (uint8_t)(s->count - taken);
    *want -= taken;
    if (s->count == 0) {
        container_free(inv, s->container);
        *s = (ItemStack){ 0 };
    }
}

/* The first stack of an item, in the order items are taken: the selected
 * stack, then the bar, then the containers open in it. */
static ItemStack *find_stack(Inventory *inv, ItemId id)
{
    SlotRef order[2 + INVENTORY_SLOTS * (1 + BASKET_SLOTS)];
    int n = 0;
    if (inventory_selection_inside(inv, inv->sel_slot))
        order[n++] = (SlotRef){ SLOT_BASKET, inv->sel_slot, inv->sel_inner };
    order[n++] = (SlotRef){ SLOT_BAR, inv->sel_slot, 0 };
    for (int i = 0; i < INVENTORY_SLOTS; i++) order[n++] = (SlotRef){ SLOT_BAR, i, 0 };
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        if (inv->slots[i].container == 0) continue; /* nothing in it, and no need to make it */
        for (int j = 0; j < BASKET_SLOTS; j++) order[n++] = (SlotRef){ SLOT_BASKET, i, j };
    }
    for (int i = 0; i < n; i++) {
        ItemStack *s = inventory_slot(inv, order[i]);
        if (s && !is_empty(s) && s->id == id) return s;
    }
    return NULL;
}

int inventory_remove(Inventory *inv, ItemId id, int count)
{
    int want = count;
    ItemStack *s;
    while (want > 0 && (s = find_stack(inv, id)) != NULL) take_from(inv, s, id, &want);
    return count - want;
}

bool inventory_take_one(Inventory *inv, ItemId id, ItemStack *out)
{
    ItemStack *s = find_stack(inv, id);
    if (!s) return false;
    bool ok = true;
    /* The last of a stack takes the contents with it; the others take a copy. */
    uint16_t contents = s->count == 1 ? s->container : container_clone(inv, s->container, &ok);
    if (!ok) return false;
    *out = (ItemStack){ (uint8_t)id, 1, contents };
    if (--s->count == 0) *s = (ItemStack){ 0 };
    return true;
}

/* Whether `s` may be put in the slot `ref` points at. */
static bool slot_accepts(Inventory *inv, SlotRef ref, const ItemStack *s)
{
    if (is_empty(s)) return true;
    if (ref.kind == SLOT_ARMOR) return (int)item_def(s->id)->equip == ref.index;
    /* The shared container takes only plain items. */
    if (ref_container(inv, ref) == SHARED_CONTAINER && !container_is_empty(inv, s->container)) return false;
    /* A container cannot go inside itself, however deep. */
    return !holds(inv, s->container, ref_container(inv, ref), 0);
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

    if (!is_empty(b) && can_merge(inv, a, b)) {
        int max = item_def(a->id)->max_stack;
        int moved = max - b->count < count ? max - b->count : count;
        if (moved == 0) return false;
        b->count = (uint8_t)(b->count + moved);
        a->count = (uint8_t)(a->count - moved);
        if (a->count == 0) {
            container_free(inv, a->container);
            *a = (ItemStack){ 0 };
        }
        normalize_selection(inv);
        return true;
    }

    if (count < a->count) {
        /* Splitting a stack needs somewhere empty to put the part taken,
         * which gets its own copy of the contents. */
        if (!is_empty(b)) return false;
        bool ok = true;
        uint16_t contents = container_clone(inv, a->container, &ok);
        if (!ok) return false;
        *b = (ItemStack){ a->id, (uint8_t)count, contents };
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
    if (inv->sel_slot != slot || inv->sel_inner < 0 || inv->sel_inner >= BASKET_SLOTS) return false;
    const ItemStack *holder = &inv->slots[slot];
    if (is_empty(holder) || holder->container == 0 || !item_def(holder->id)->opens_in_bar) return false;
    return !is_empty(&inv->containers[holder->container].slots[inv->sel_inner]);
}

const ItemStack *inventory_selected(const Inventory *inv)
{
    const ItemStack *s = &inv->slots[inv->sel_slot];
    if (inventory_selection_inside(inv, inv->sel_slot))
        return &inv->containers[s->container].slots[inv->sel_inner];
    return s;
}
