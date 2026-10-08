#ifndef PLAYERUI_H
#define PLAYERUI_H

#include "game.h"
#include "raylib.h"
#include "view.h"

/* The players' corner of the screen: for each character a small button with
 * their health and hunger under it, one below another. Clicking the button
 * of the one being played flies a panel out beside it, with all their bars
 * and the buttons for sleeping and meditating; clicking another's hands
 * control to them. */
typedef struct {
    bool open;
    bool pressed;  /* a press that began on it */
    bool switched; /* set for the frame control passes to another character */
    bool tapped;   /* set for the frame the button of the character being played is pressed: the view goes to them */
    int death_open; /* the dead character whose menu is out beside their button, plus one; 0 for none */
    int confirm;    /* the one whose "Remove" has been pressed once and wants pressing again, plus one */
} PlayerUi;

/* Where the world is on the screen, for the menus that stand over the dead
 * in it; and whether the world is what is showing (not the map). Called
 * every frame before the rest. */
void playerui_set_view(const View *v, bool world_showing);

/* Returns true while the mouse belongs to the button or its panel. */
bool playerui_handle_input(PlayerUi *ui, Game *g);
bool playerui_mouse_over(const PlayerUi *ui);
void playerui_draw(const PlayerUi *ui, const Game *g);
/* A bar of one of the player's needs, on dark red: green, gold for a full
 * stomach, or blue for breath. `value` runs from 0 to 1. */
typedef enum {
    BAR_GREEN,
    BAR_GOLD,
    BAR_BLUE,
    BAR_RED, /* the death bar */
    BAR_POISON /* health, while poison is at work on it */
} BarColor;
void playerui_draw_bar(Rectangle r, float value, BarColor color);

#endif
