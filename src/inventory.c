#include "inventory.h"

#include <string.h>

#define MAX_NESTING 32

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
        inv->containers[i].used = CONTAINER_ITEMS;
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
    inv->containers[copy].used = inv->containers[id].used;
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

static void normalize_selection(Inventory *inv);

/* ---- meters ------------------------------------------------------------ */

static bool has_meter(ItemId id)
{
    return item_def(id)->meter_max > 0;
}

/* How the meters of a stack read, one to an item, bottom of the stack first. */
typedef struct {
    uint16_t reads[256];
    int count;
} Meters;

static void meters_read(const Inventory *inv, const ItemStack *s, Meters *m)
{
    m->count = 0;
    if (is_empty(s)) return;
    uint16_t fresh = (uint16_t)item_def(s->id)->meter_start;
    if (s->container != 0 && s->container < MAX_CONTAINERS && inv->containers[s->container].used == CONTAINER_METERS) {
        const ItemStack *runs = inv->containers[s->container].slots;
        for (int i = 0; i < CONTAINER_MAX_SLOTS; i++)
            for (int k = 0; k < runs[i].count && m->count < s->count; k++) m->reads[m->count++] = (uint16_t)(runs[i].id - 1);
    }
    while (m->count < s->count) m->reads[m->count++] = fresh; /* anything unrecorded is as new */
}

/* Whether a stack of these readings can be written down: in few enough runs. */
static bool meters_fit(const Meters *m)
{
    int runs = 0;
    for (int i = 0; i < m->count; i++)
        if (i == 0 || m->reads[i] != m->reads[i - 1]) runs++;
    return runs <= CONTAINER_MAX_SLOTS;
}

/* Writes a stack's readings down, and sets its count to match. A stack that
 * is all as new needs no record at all. False, changing nothing, if there is
 * no room for the record. */
static bool meters_write(Inventory *inv, ItemStack *s, const Meters *m)
{
    if (!meters_fit(m)) return false;
    uint16_t fresh = (uint16_t)item_def(s->id)->meter_start;
    bool plain = true;
    for (int i = 0; i < m->count; i++) plain = plain && m->reads[i] == fresh;
    if (plain) {
        container_free(inv, s->container);
        s->container = 0;
    } else {
        if (s->container == 0) {
            s->container = container_alloc(inv);
            if (s->container == 0) return false;
        }
        Container *c = &inv->containers[s->container];
        memset(c->slots, 0, sizeof c->slots);
        c->used = CONTAINER_METERS;
        int run = -1;
        for (int i = 0; i < m->count; i++) {
            if (run < 0 || c->slots[run].id != m->reads[i] + 1 || c->slots[run].count == 255) {
                run++;
                c->slots[run] = (ItemStack){ (uint16_t)(m->reads[i] + 1), 0, 0 };
            }
            c->slots[run].count++;
        }
    }
    s->count = (uint8_t)m->count;
    if (m->count == 0) *s = (ItemStack){ 0 };
    return true;
}

/* Moves the top `n` items of one metered stack onto the top of another (or
 * into an empty slot), in the same order. Returns how many went: all or none. */
static int meters_move(Inventory *inv, ItemStack *to, ItemStack *from, int n)
{
    static Meters a, b;
    if (n <= 0 || n > from->count) return 0;
    bool fresh_slot = is_empty(to);
    ItemStack onto = fresh_slot ? (ItemStack){ from->id, 0, 0 } : *to;
    b.count = 0;
    if (!fresh_slot) meters_read(inv, &onto, &b);
    meters_read(inv, from, &a);
    if (b.count + n > 255) return 0;
    for (int i = 0; i < n; i++) b.reads[b.count + i] = a.reads[a.count - n + i];
    b.count += n;
    a.count -= n;
    if (!meters_fit(&a) || !meters_fit(&b) || !meters_write(inv, &onto, &b)) return 0;
    *to = onto;
    ItemStack rest = *from;
    meters_write(inv, &rest, &a); /* (fewer of what was there: this always fits) */
    *from = rest;
    return n;
}

int inventory_meter(const Inventory *inv, const ItemStack *s)
{
    static Meters m;
    if (!s || is_empty(s) || !has_meter(s->id)) return -1;
    meters_read(inv, s, &m);
    return m.reads[m.count - 1];
}

bool inventory_set_meter(Inventory *inv, ItemStack *s, int value)
{
    static Meters m;
    if (!s || is_empty(s) || !has_meter(s->id)) return false;
    int max = item_def(s->id)->meter_max;
    meters_read(inv, s, &m);
    m.reads[m.count - 1] = (uint16_t)(value < 0 ? 0 : value > max ? max : value);
    return meters_write(inv, s, &m);
}

/* Two stacks that could be one: the same item with the same things inside,
 * or the same item with a meter, however the meters read - unless it is a
 * two-tone thing, which only goes with others of exactly its colours. */
static bool can_merge(const Inventory *inv, const ItemStack *a, const ItemStack *b)
{
    if (a->id != b->id || item_def(a->id)->max_stack <= 1) return false;
    if (item_def(a->id)->two_tone) return inventory_meter(inv, a) == inventory_meter(inv, b);
    return has_meter(a->id) || containers_equal(inv, a->container, b->container);
}

/* ---- slots ------------------------------------------------------------- */

void inventory_init(Inventory *inv)
{
    memset(inv, 0, sizeof *inv);
    inv->sel_inner = -1;
    memset(inv->choice, -1, sizeof inv->choice);
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
        if (has_meter(stack->id)) {
            meters_move(inv, &slots[i], stack, moved); /* onto the top, readings and all */
            continue;
        }
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
        if (has_meter(stack->id)) {
            if (meters_move(inv, &slots[i], stack, moved) == 0) return;
            continue;
        }
        bool ok = true;
        uint16_t contents = moved == stack->count ? stack->container
                                                  : container_clone(inv, stack->container, &ok);
        if (!ok) return;
        slots[i] = (ItemStack){ stack->id, (uint8_t)moved, contents };
        stack->count = (uint8_t)(stack->count - moved);
        if (stack->count == 0) stack->container = 0;
    }
}

/* With the inventory full, a basket coming in takes the first bar slot that
 * holds something other than a basket, and keeps what was there inside
 * itself - chosen, so the slot still shows and gives the same item. */
static bool take_slot_for_basket(Inventory *inv, ItemStack basket)
{
    const ItemDef *def = item_def(basket.id);
    if (!def->opens_in_bar) return false;
    int room = def->container_slots < BASKET_SLOTS ? def->container_slots : BASKET_SLOTS;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        ItemStack *held = &inv->slots[i];
        if (is_empty(held) || item_def(held->id)->opens_in_bar) continue;
        bool made = basket.container == 0;
        uint16_t c = made ? container_alloc(inv) : basket.container;
        if (c == 0) return false;
        int free_slot = -1;
        for (int j = 0; j < room && free_slot < 0; j++)
            if (is_empty(&inv->containers[c].slots[j])) free_slot = j;
        if (free_slot < 0) {
            if (made) container_free(inv, c);
            return false; /* the basket itself is full */
        }
        inv->containers[c].slots[free_slot] = *held;
        *held = (ItemStack){ basket.id, 1, c };
        inv->choice[i] = (int8_t)free_slot;
        if (inv->sel_slot == i && !inventory_hand(inv)) inv->sel_inner = free_slot;
        return true;
    }
    return false;
}

/* Adds a stack wherever it fits and returns what did not. */
static ItemStack add_stack_rest(Inventory *inv, ItemStack stack);

int inventory_add_stack(Inventory *inv, ItemStack stack)
{
    return add_stack_rest(inv, stack).count;
}

static ItemStack add_stack_rest(Inventory *inv, ItemStack stack)
{
    if (stack.id <= ITEM_NONE || stack.id >= content.item_count) return stack;

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
    /* A basket always finds room if anything in the bar is not a basket: it
     * takes that thing's slot and takes the thing inside itself. */
    if (stack.count == 1 && take_slot_for_basket(inv, stack)) return (ItemStack){ 0 };
    /* Whatever merged into a matching stack no longer needs its own contents. */
    if (stack.count == 0 && !has_meter(stack.id)) container_free(inv, stack.container);
    if (stack.count == 0) stack = (ItemStack){ 0 };
    return stack;
}

int inventory_add(Inventory *inv, ItemId id, int count)
{
    int left = 0;
    while (count > 0) {
        int batch = count > 255 ? 255 : count;
        int rest = inventory_add_stack(inv, (ItemStack){ (uint16_t)id, (uint8_t)batch, 0 });
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
    /* (With their bare hands chosen, that is what they use, whatever they carry.) */
    if (tool_class < 0 || inventory_hand(inv)) return best;
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
    *want -= taken;
    if (has_meter(id)) {
        /* Off the top, with their readings. */
        ItemStack gone = { 0 };
        if (meters_move(inv, &gone, s, taken) == taken) container_free(inv, gone.container);
        return;
    }
    s->count = (uint8_t)(s->count - taken);
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

bool inventory_split(Inventory *inv, ItemStack *from, int n, ItemStack *out)
{
    *out = (ItemStack){ 0 };
    if (!from || is_empty(from) || n < 1 || n > from->count) return false;
    if (has_meter(from->id)) return meters_move(inv, out, from, n) == n;
    bool ok = true;
    /* The last of a stack takes the contents with it; the others take a copy. */
    uint16_t contents = n == from->count ? from->container : container_clone(inv, from->container, &ok);
    if (!ok) return false;
    *out = (ItemStack){ from->id, (uint8_t)n, contents };
    from->count = (uint8_t)(from->count - n);
    if (from->count == 0) *from = (ItemStack){ 0 };
    return true;
}

ItemStack *inventory_find(Inventory *inv, ItemId id)
{
    return find_stack(inv, id);
}

bool inventory_take_one(Inventory *inv, ItemId id, ItemStack *out)
{
    return inventory_split(inv, find_stack(inv, id), 1, out);
}

/* Every stack of an item that counts as held: in the bar, and in the baskets open in it. */
static int held_stacks(Inventory *inv, ItemId id, ItemStack **out, int max)
{
    int n = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        ItemStack *s = &inv->slots[i];
        if (is_empty(s)) continue;
        if (s->id == id && n < max) out[n++] = s;
        if (s->container == 0 || !item_def(s->id)->opens_in_bar) continue;
        for (int j = 0; j < BASKET_SLOTS; j++) {
            ItemStack *inner = &inv->containers[s->container].slots[j];
            if (!is_empty(inner) && inner->id == id && n < max) out[n++] = inner;
        }
    }
    return n;
}

int inventory_count_full(const Inventory *inv, ItemId id)
{
    static Meters m;
    ItemStack *stacks[INVENTORY_SLOTS * (1 + BASKET_SLOTS)];
    int n = held_stacks((Inventory *)inv, id, stacks, INVENTORY_SLOTS * (1 + BASKET_SLOTS)), total = 0;
    int full = item_def(id)->meter_max;
    if (full <= 0) return 0;
    for (int i = 0; i < n; i++) {
        meters_read(inv, stacks[i], &m);
        for (int k = 0; k < m.count; k++) total += m.reads[k] == full;
    }
    return total;
}

int inventory_drain(Inventory *inv, ItemId id, int count)
{
    static Meters m;
    ItemStack *stacks[INVENTORY_SLOTS * (1 + BASKET_SLOTS)];
    int n = held_stacks(inv, id, stacks, INVENTORY_SLOTS * (1 + BASKET_SLOTS)), done = 0;
    int full = item_def(id)->meter_max;
    for (int i = 0; i < n && done < count && full > 0; i++) {
        meters_read(inv, stacks[i], &m);
        bool changed = false;
        /* From the top of the stack down. */
        for (int k = m.count - 1; k >= 0 && done < count; k--) {
            if (m.reads[k] != full) continue;
            m.reads[k] = (uint16_t)item_def(id)->meter_start;
            changed = true;
            done++;
        }
        if (changed && !meters_write(inv, stacks[i], &m)) break;
    }
    return done;
}

void inventory_wear_tool(Inventory *inv, int tool_class)
{
    if (tool_class < 0) return;
    ItemStack *best = NULL;
    float speed = 0.0f;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        for (int j = -1; j < BASKET_SLOTS; j++) {
            if (j >= 0 && (inv->slots[i].container == 0 || !item_def(inv->slots[i].id)->opens_in_bar)) break;
            ItemStack *s = j < 0 ? &inv->slots[i] : inventory_slot(inv, (SlotRef){ SLOT_BASKET, i, j });
            if (!s || is_empty(s)) continue;
            const ItemDef *def = item_def(s->id);
            if (def->tool_class == tool_class && def->tool_speed > speed) speed = def->tool_speed, best = s;
        }
    }
    if (!best || !has_meter(best->id) || item_def(best->id)->holds_water) return;
    int left = inventory_meter(inv, best) - 1;
    if (left > 0) {
        inventory_set_meter(inv, best, left);
        return;
    }
    /* Worn out: that one is gone, and the next in the stack comes to the top. */
    ItemStack gone;
    if (inventory_split(inv, best, 1, &gone)) container_free(inv, gone.container);
    normalize_selection(inv);
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
        if (has_meter(a->id)) {
            /* The ones taken come off the top of one stack and go on top of the other. */
            if (meters_move(inv, b, a, moved) == 0) return false;
            normalize_selection(inv);
            return true;
        }
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
        return inventory_split(inv, a, count, b);
    }

    if (!slot_accepts(inv, from, b)) return false;
    ItemStack tmp = *a;
    *a = is_empty(b) ? (ItemStack){ 0 } : *b;
    *b = tmp;
    normalize_selection(inv);
    return true;
}

int inventory_quick_move(Inventory *inv, SlotRef from, uint16_t container, int slots)
{
    ItemStack *src = inventory_slot(inv, from);
    if (!src || is_empty(src) || container == 0 || container >= MAX_CONTAINERS) return 0;
    if (slots > CONTAINER_MAX_SLOTS) slots = CONTAINER_MAX_SLOTS;
    ItemStack moving = *src;
    int before = moving.count;

    if (from.kind == SLOT_CONTAINER && from.index == container) {
        /* Out of the container: lift the stack out, and put back what the
         * inventory has no room for. */
        *src = (ItemStack){ 0 };
        ItemStack rest = add_stack_rest(inv, moving);
        if (rest.count > 0) *src = rest;
        normalize_selection(inv);
        return before - rest.count;
    }

    if (!slot_accepts(inv, (SlotRef){ SLOT_CONTAINER, container, 0 }, &moving)) return 0;
    ItemStack *dest = inv->containers[container].slots;
    merge_into(inv, dest, slots, &moving);
    fill_empty(inv, dest, slots, &moving);
    if (has_meter(moving.id)) {
        /* (Its record went with it, or was rewritten for what is left.) */
        int went = before - moving.count;
        *src = moving.count > 0 ? moving : (ItemStack){ 0 };
        normalize_selection(inv);
        return went;
    }
    if (moving.count == 0) {
        /* All of it went. If it merged into a matching stack, its own copy
         * of the contents is no longer needed. */
        bool handed_over = false;
        for (int i = 0; i < slots; i++) handed_over = handed_over || dest[i].container == src->container;
        if (!handed_over) container_free(inv, src->container);
        *src = (ItemStack){ 0 };
    } else {
        src->count = moving.count;
    }
    normalize_selection(inv);
    return before - moving.count;
}

void inventory_select(Inventory *inv, int slot, int inner)
{
    if (slot < 0 || slot >= INVENTORY_SLOTS) return;
    inv->sel_slot = slot;
    inv->sel_inner = inner;
    normalize_selection(inv);
    inv->choice[slot] = (int8_t)(inventory_selection_inside(inv, slot) ? inv->sel_inner : -1);
}

int inventory_basket_choice(const Inventory *inv, int slot)
{
    if (slot < 0 || slot >= INVENTORY_SLOTS) return -1;
    const ItemStack *holder = &inv->slots[slot];
    int inner = inv->choice[slot];
    if (is_empty(holder) || holder->container == 0 || !item_def(holder->id)->opens_in_bar) return -1;
    if (inner < 0 || inner >= BASKET_SLOTS || is_empty(&inv->containers[holder->container].slots[inner])) return -1;
    return inner;
}

void inventory_select_hand(Inventory *inv)
{
    inv->sel_inner = SEL_HAND;
}

bool inventory_hand(const Inventory *inv)
{
    return inv->sel_inner == SEL_HAND;
}

void inventory_select_slot(Inventory *inv, int slot)
{
    if (slot < 0 || slot >= INVENTORY_SLOTS) return;
    int inner = inventory_basket_choice(inv, slot);
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
    static const ItemStack nothing = { 0 };
    if (inventory_hand(inv)) return &nothing;
    const ItemStack *s = &inv->slots[inv->sel_slot];
    if (inventory_selection_inside(inv, inv->sel_slot))
        return &inv->containers[s->container].slots[inv->sel_inner];
    return s;
}
