#ifndef INVENTORY_H
#define INVENTORY_H

#include "world.h"

#define INVENTORY_SLOTS 8
#define BASKET_SLOTS 4
#define ARMOR_SLOTS 4
#define MAX_STACK 99
#define MAX_CONTAINERS 256

typedef enum {
    ITEM_NONE = 0,
    ITEM_BASKET = 1,
    ITEM_DIRT = 2,
    ITEM_WORKBENCH = 3,
    ITEM_STONE = 4,
    ITEM_COUNT
} ItemId;

/* Armor slots, top to bottom. */
typedef enum {
    EQUIP_HEAD = 0,
    EQUIP_CHEST,
    EQUIP_LEGS,
    EQUIP_BOOTS,
    EQUIP_NONE
} EquipSlot;

typedef struct {
    const char *name;
    int max_stack;
    bool container;  /* carries BASKET_SLOTS item slots as metadata */
    EquipSlot equip; /* armor slot it can be worn in, or EQUIP_NONE */
    TileType places; /* block it becomes when placed, or TILE_AIR */
} ItemDef;

/* `container` is the item's metadata: 0, or which Inventory.containers entry
 * holds its contents. It travels with the stack wherever it is moved. */
typedef struct {
    uint8_t id;
    uint8_t count;
    uint16_t container;
} ItemStack;

typedef struct {
    bool used;
    ItemStack slots[BASKET_SLOTS];
} Container;

typedef enum {
    SLOT_NONE = 0,
    SLOT_BAR,    /* index: bar slot */
    SLOT_BASKET, /* index: bar slot holding the basket, inner: slot inside it */
    SLOT_ARMOR   /* index: EquipSlot */
} SlotKind;

typedef struct {
    SlotKind kind;
    int index, inner;
} SlotRef;

typedef struct {
    ItemStack slots[INVENTORY_SLOTS];
    ItemStack armor[ARMOR_SLOTS];
    Container containers[MAX_CONTAINERS]; /* entry 0 is never used */
    /* Selection: a bar slot, and optionally a slot inside the basket there. */
    int sel_slot, sel_inner;
} Inventory;

const ItemDef *item_def(ItemId id);
/* Item a mined tile drops, or ITEM_NONE. */
ItemId item_for_tile(TileType t);

/* Starts the player off with one basket in the first slot. */
void inventory_init(Inventory *inv);

/* The slot a reference points at, or NULL if there is no such slot. Slots
 * inside a basket exist only while the basket sits in the bar. */
ItemStack *inventory_slot(Inventory *inv, SlotRef ref);

/* Adds items wherever they fit: existing stacks first, then empty bar slots,
 * then empty slots of baskets in the bar. Returns how many did not fit. */
int inventory_add(Inventory *inv, ItemId id, int count);

/* Counts and takes items from the bar and the baskets in it. Removing takes
 * from the selected stack first and returns how many were taken. */
int inventory_count(const Inventory *inv, ItemId id);
int inventory_remove(Inventory *inv, ItemId id, int count);

/* Moves up to `count` items from `from` onto `to`. A whole stack merges with
 * an equal stack or swaps with anything else; part of a stack can only go to
 * an empty slot or an equal stack. Returns false if nothing could move. */
bool inventory_move(Inventory *inv, SlotRef from, SlotRef to, int count);

void inventory_select(Inventory *inv, int slot, int inner);
/* The selected stack: the chosen item inside a basket if there is one there,
 * otherwise whatever is in the selected bar slot. */
const ItemStack *inventory_selected(const Inventory *inv);
/* True if the selection is an item inside the basket in bar slot `slot`. */
bool inventory_selection_inside(const Inventory *inv, int slot);

#endif
