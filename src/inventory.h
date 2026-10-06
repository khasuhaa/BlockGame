#ifndef INVENTORY_H
#define INVENTORY_H

#include "world.h"

#define INVENTORY_SLOTS 8
#define BASKET_SLOTS 4 /* slots of a container shown when it is opened in the bar */
#define ARMOR_SLOTS 4
#define CONTAINER_MAX_SLOTS 16
#define MAX_CONTAINERS 256
/* The one container shared by every world. It is never handed out for an
 * item or a block, and nothing with contents of its own may go in it: what it
 * holds is saved as plain items, outside any world. */
#define SHARED_CONTAINER (MAX_CONTAINERS - 1)

/* `container` is the item's metadata: which Inventory.containers entry holds
 * what is inside it, or 0 for nothing inside. It travels with the stack
 * wherever it is moved. Every item in a stack has the same contents, and two
 * stacks only merge if their contents are the same. */
typedef struct {
    uint8_t id;
    uint8_t count;
    uint16_t container;
} ItemStack;

typedef struct {
    bool used;
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

void inventory_select(Inventory *inv, int slot, int inner);
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
