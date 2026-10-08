#ifndef DAOUI_H
#define DAOUI_H

#include "game.h"

/* The cultivation of the character being played: a button in the corner that says how far they have got and
 * how full of qi they are, and the screen it opens - everything about their road, and the breakthrough. */
typedef struct {
    bool open;
    bool pressed;
    bool sight; /* spirit sight: the world is shown tinted by its qi (drawn by whoever draws the world) */
} DaoUi;

/* True if the press was on it. */
bool daoui_handle_input(DaoUi *ui, Game *g);
bool daoui_mouse_over(const DaoUi *ui);
void daoui_draw(const DaoUi *ui, const Game *g);

#endif
