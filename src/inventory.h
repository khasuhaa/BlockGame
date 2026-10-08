#ifndef INVENTORY_H
#define INVENTORY_H

#include "world.h"

#define INVENTORY_SLOTS 8
#define BASKET_SLOTS 4 /* slots of a container shown when it is opened in the bar */
#define ARMOR_SLOTS 4
#define CONTAINER_MAX_SLOTS 16
#define MAX_CONTAINERS 1024
/* The one container shared by every world. It is never handed out for an
 * item or a block, and nothing with contents of its own may go in it: what it
 * holds is saved as plain items, outside any world. */
#define SHARED_CONTAINER (MAX_CONTAINERS - 1)

/* `container` is the item's metadata: which Inventory.containers entry holds
 * what is inside it, or 0 for nothing inside. It travels with the stack
 * wherever it is moved. Every item in a stack has the same contents, and two
 * stacks only merge if their contents are the same. */
typedef struct {
    uint16_t id;
    uint8_t count;
    uint16_t container;
} ItemStack;

/* A container holds items - or, for a stack of things with meters, how the
 * meters of the stack read: each slot then is a run of items reading the
 * same, bottom of the stack first, as { reading + 1, how many, 0 }. */
enum {
    CONTAINER_FREE = 0,
    CONTAINER_ITEMS = 1,
    CONTAINER_METERS = 2,
    CONTAINER_RELICS = 3 /* a character's relics: its first RELIC_SLOTS slots, each for one relic, no two the same */
};

typedef struct {
    uint8_t used; /* one of the above (it was a bool: old saves read as items) */
    ItemStack slots[CONTAINER_MAX_SLOTS];
} Container;

typedef enum {
    SLOT_NONE = 0,
    SLOT_BAR,      /* index: bar slot */
    SLOT_BASKET,   /* index: bar slot holding the container, inner: slot inside it */
    SLOT_ARMOR,    /* index: EquipSlot */
    SLOT_CONTAINER /* index: container, inner: slot inside it */
} SlotKind;

typedef struct {
    SlotKind kind;
    int index, inner;
} SlotRef;

typedef struct {
    ItemStack slots[INVENTORY_SLOTS];
    ItemStack armor[ARMOR_SLOTS];
    /* Every container in the game lives here, whether the thing holding it is
     * an item in the bar, an item inside another container, a block placed in
     * the world or an item lying on the ground. Entry 0 is never used. */
    Container containers[MAX_CONTAINERS];
    /* Selection: a bar slot, and optionally a slot inside the container there. */
    int sel_slot, sel_inner;
    /* For each bar slot, the slot inside the basket there that was last
     * chosen, or -1: a closed basket shows that item, and choosing the bar
     * slot chooses it. (At the end, so older saves read as a shorter record.) */
    int8_t choice[INVENTORY_SLOTS];
} Inventory;

/* Starts the player off with the items the rules give them. */
void inventory_init(Inventory *inv);

/* The slot a reference points at, or NULL if there is no such slot. Slots
 * inside a bar item exist only for items that open in the bar. */
ItemStack *inventory_slot(Inventory *inv, SlotRef ref);

/* Adds items wherever they fit: stacks they can merge with first, then empty
 * bar slots, then empty slots of containers open in the bar. Returns how many
 * did not fit. Adding a stack hands over its contents with it. */
int inventory_add(Inventory *inv, ItemId id, int count);
int inventory_add_stack(Inventory *inv, ItemStack stack);

/* Counts and takes items from the bar and the containers open in it. Removing
 * takes from the selected stack first and returns how many were taken;
 * whatever was inside them is lost. */
int inventory_count(const Inventory *inv, ItemId id);
int inventory_remove(Inventory *inv, ItemId id, int count);
/* How much faster the best tool of a kind in the bar, or in a container open
 * in it, mines; 1 if there is none. */
float inventory_tool_speed(const Inventory *inv, int tool_class);
/* How many times over the best such tool makes an ore block drop; 1 if none does. */
int inventory_ore_multiplier(const Inventory *inv, int tool_class);
/* Takes a single item, selected stack first, along with what is inside it. */
bool inventory_take_one(Inventory *inv, ItemId id, ItemStack *out);

/* Moves up to `count` items from `from` onto `to`. A whole stack merges with
 * a stack it matches or swaps with anything else; part of a stack can only go
 * to an empty slot or a matching stack. Returns false if nothing could move. */
bool inventory_move(Inventory *inv, SlotRef from, SlotRef to, int count);

/* Sends a whole stack across between the player's inventory and an open
 * storage container of `slots` slots: out of the container into the
 * inventory, or from anywhere else into the container. It tops up matching
 * stacks first, then takes free slots; what does not fit stays where it was.
 * Returns how many items moved. */
int inventory_quick_move(Inventory *inv, SlotRef from, uint16_t container, int slots);

/* Takes the top `n` items of a stack off into a stack of their own, in the
 * same order, with what is inside them or how their meters read. False if
 * there are not that many or no room to record it. */
bool inventory_split(Inventory *inv, ItemStack *from, int n, ItemStack *out);
/* The first stack of an item, as items are taken: the selected stack, then
 * the bar, then the baskets open in it. NULL if none is held. */
ItemStack *inventory_find(Inventory *inv, ItemId id);
/* What the meter of the item on top of a stack reads, or -1 if the item has
 * none; and setting it. */
int inventory_meter(const Inventory *inv, const ItemStack *s);
bool inventory_set_meter(Inventory *inv, ItemStack *s, int value);
/* How many of an item are held whose meter reads full, and emptying that
 * many: a bucket of water counted, and poured out. */
int inventory_count_full(const Inventory *inv, ItemId id);
int inventory_drain(Inventory *inv, ItemId id, int count);
/* Wears the best tool of a kind by one use, if it is one that wears: the one
 * on top of its stack, which is gone when it reads nothing. */
void inventory_wear_tool(Inventory *inv, int tool_class);

void inventory_select(Inventory *inv, int slot, int inner);
/* Chooses a bar slot as the number keys do: if a basket is there, the item
 * last chosen inside it, as long as it is still there. */
void inventory_select_slot(Inventory *inv, int slot);
/* Selecting nothing: the character's bare hands. Then nothing is held - there is nothing to put down or eat - and
 * no tool is used, whatever tools are carried. (Kept as a value of `sel_inner` that no basket has.) */
#define SEL_HAND (-2)
void inventory_select_hand(Inventory *inv);
bool inventory_hand(const Inventory *inv);
/* The slot inside the basket in a bar slot that it shows when closed, or -1. */
int inventory_basket_choice(const Inventory *inv, int slot);
/* The selected stack: the chosen item inside a container if there is one
 * there, otherwise whatever is in the selected bar slot. */
const ItemStack *inventory_selected(const Inventory *inv);
/* True if the selection is an item inside the container in bar slot `slot`. */
bool inventory_selection_inside(const Inventory *inv, int slot);

/* ---- containers -------------------------------------------------------- */

/* A new empty container, or 0 if there are none left. */
uint16_t container_alloc(Inventory *inv);
/* Releases a container and everything nested inside it. */
void container_free(Inventory *inv, uint16_t id);
/* A copy of a container and everything nested inside it; 0 if it is empty or
 * there is no room. `ok` is cleared only when room ran out. */
uint16_t container_clone(Inventory *inv, uint16_t id, bool *ok);
/* Whether two items have the same things inside them. Nothing inside and an
 * empty container count as the same. */
bool containers_equal(const Inventory *inv, uint16_t a, uint16_t b);
bool container_is_empty(const Inventory *inv, uint16_t id);

#endif
