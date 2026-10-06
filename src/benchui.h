#ifndef BENCHUI_H
#define BENCHUI_H

#include "game.h"
#include "raylib.h"
#include "view.h"

typedef enum {
    BENCH_PRESS_NONE,
    BENCH_PRESS_PANEL, /* somewhere on a panel that does nothing */
    BENCH_PRESS_LIST,
    BENCH_PRESS_SLIDER,
    BENCH_PRESS_CRAFT,
    BENCH_PRESS_UPGRADE,
    BENCH_PRESS_OPEN,
    BENCH_PRESS_REMOVE
} BenchPress;

/* Bench interface drawn in world space, anchored to the bench's tile so it
 * moves with the map: the "open / remove" popup for a bench that can be
 * removed, and the crafting menu flying out of the bench that is open. */
typedef struct {
    bool popup;
    TilePos popup_tile;

    /* Crafting menu state, reset whenever a different bench opens. */
    bool was_open;
    TilePos shown;
    int selected; /* recipe index, or -1 */
    float scroll; /* recipe list offset in pixels */
    int amount;   /* how many to craft */

    BenchPress press;
    Vector2 press_mouse;
    float press_scroll;
    bool dragged;
} BenchUi;

/* Returns true while the mouse belongs to the bench interface. */
bool benchui_handle_input(BenchUi *ui, Game *g, const View *v);
void benchui_show_popup(BenchUi *ui, TilePos tile);
/* Dismisses the popup and closes the open bench. */
void benchui_close(BenchUi *ui, Game *g);
bool benchui_mouse_over(const BenchUi *ui, const Game *g, const View *v);
/* Where the slots of the open storage block are on screen, in order. Fills
 * `rects` and the container they belong to; returns how many, 0 if what is
 * open is not a storage block. */
int benchui_storage_slots(const Game *g, const View *v, Rectangle *rects, int max, uint16_t *container);
void benchui_draw(const BenchUi *ui, const Game *g, const View *v);

#endif
