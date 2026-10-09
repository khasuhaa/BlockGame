#ifndef HELMUI_H
#define HELMUI_H

#include "game.h"
#include "raylib.h"
#include "view.h"

/* The menu of a helm (a ship adrift; see ship.h): the seas, what each asks of her and what she has of it, a
 * course to set, what is alongside to let go, and what she is counted as. */
typedef struct {
    bool pressed; /* a press that began on the menu */
} HelmUi;

/* Returns true while the mouse belongs to the menu. */
bool helmui_handle_input(HelmUi *ui, Game *g, const View *v);
bool helmui_mouse_over(const HelmUi *ui, const Game *g, const View *v);
void helmui_draw(const HelmUi *ui, const Game *g, const View *v);

#endif
