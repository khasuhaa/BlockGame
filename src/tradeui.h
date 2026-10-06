#ifndef TRADEUI_H
#define TRADEUI_H

#include "game.h"
#include "raylib.h"
#include "view.h"

/* The menu that flies out of an open trade block: a tab for each category of
 * item to buy and sell, and one for converting coins. */
typedef struct {
    bool was_open;
    TilePos shown;
    int tab;       /* a category, or one past the last for the convert tab */
    ItemId selected;
    float scroll;  /* of the item grid, in pixels */
    bool pressed;  /* a press that began on the menu */
} TradeUi;

/* Returns true while the mouse belongs to the trade menu. */
bool tradeui_handle_input(TradeUi *ui, Game *g, const View *v);
bool tradeui_mouse_over(const TradeUi *ui, const Game *g, const View *v);
void tradeui_draw(const TradeUi *ui, const Game *g, const View *v);

#endif
