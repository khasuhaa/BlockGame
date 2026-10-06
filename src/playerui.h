#ifndef PLAYERUI_H
#define PLAYERUI_H

#include "game.h"
#include "raylib.h"

/* The player's corner of the screen: a small button with their health and
 * hunger under it. Clicking it flies a panel out beside it, with all three
 * bars and the buttons for sleeping and meditating. */
typedef struct {
    bool open;
    bool pressed; /* a press that began on it */
} PlayerUi;

/* Returns true while the mouse belongs to the button or its panel. */
bool playerui_handle_input(PlayerUi *ui, Game *g);
bool playerui_mouse_over(const PlayerUi *ui);
void playerui_draw(const PlayerUi *ui, const Game *g);
/* A bar of one of the player's needs: green on dark red, or gold when
 * `golden`. `value` runs from 0 to 1. */
void playerui_draw_bar(Rectangle r, float value, bool golden);

#endif
