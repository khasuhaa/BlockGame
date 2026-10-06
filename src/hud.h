#ifndef HUD_H
#define HUD_H

#include "inventory.h"
#include "raylib.h"

#define HUD_MAX_EXTRA_SLOTS 16

enum {
    HUD_EAT_NONE, /* what is selected is not food */
    HUD_EAT_READY,
    HUD_EAT_FULL  /* it is food, but the player can eat no more */
};

/* A slot drawn somewhere other than the bar, such as one of an open chest's. */
typedef struct {
    Rectangle rect;
    SlotRef ref;
} HudSlot;

/* On-screen inventory: a head button followed by the bar of slots along the
 * bottom of the window. The head button and each basket in the bar can extend
 * a column of slots upward. */
typedef struct {
    /* Slots outside the bar that take part in dragging this frame. */
    HudSlot extra[HUD_MAX_EXTRA_SLOTS];
    int extra_count;

    bool armor_open;
    bool basket_open[INVENTORY_SLOTS];

    /* Mouse press that started on the inventory. Holding it on a stack picks
     * items up one tick at a time; `picked` is how many are on the cursor, and
     * `dragging` is set once the cursor has left the slot they came from. */
    bool pressed, dragging;
    float hold_time;

    /* The selected item's name, shown for a moment whenever the selection changes. */
    int named_slot, named_inner, named_item;
    float name_time;
    int picked;
    /* The "Eat" button over a selected food. Whoever runs the HUD says each
     * frame whether it shows, and is told when it has been clicked. */
    int eat;          /* HUD_EAT_NONE, HUD_EAT_READY or HUD_EAT_FULL */
    bool eat_pressed; /* a press that began on it */
    bool eat_clicked; /* set for the frame it is clicked */
    /* A stack dragged off the inventory and let go over the world: set for
     * the frame it happens, for whoever runs the HUD to throw it out. */
    bool discard;
    SlotRef discard_slot;
    int discard_count;
    bool press_head;
    SlotRef press_slot;
    Vector2 press_mouse;
} Hud;

/* Handles taps and drags on the inventory. Returns true while the mouse
 * belongs to the inventory, so the world should ignore it. */
bool hud_handle_input(Hud *hud, Inventory *inv);
/* Collapses the armor column and every basket. */
void hud_close_all(Hud *hud);
bool hud_mouse_over(const Hud *hud, const Inventory *inv);
void hud_draw(const Hud *hud, const Inventory *inv);
void hud_draw_item_icon(ItemId id, Rectangle r);
/* The player's face, as on their buttons. */
void hud_draw_head_icon(Rectangle r);

#endif
