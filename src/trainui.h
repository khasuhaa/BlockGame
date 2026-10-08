#ifndef TRAINUI_H
#define TRAINUI_H

#include "game.h"
#include "raylib.h"
#include "view.h"

/* The little menu over something on the rails - what can be done with it,
 * one button to a choice - and the slots of the one that is open: an
 * engine's fuel, a freight car's load. */
typedef struct {
    bool open;     /* the menu is up */
    int vehicle;   /* over which */
    int x;         /* the square that was clicked: which seat is offered goes by it */
    bool pressed;  /* a press that began on the menu or the slots */
} TrainUi;

void trainui_show(TrainUi *ui, int vehicle, int x);
/* Returns true while the mouse belongs to the menu. */
bool trainui_handle_input(TrainUi *ui, Game *g, const View *v);
bool trainui_mouse_over(const TrainUi *ui, const Game *g, const View *v);
void trainui_draw(const TrainUi *ui, const Game *g, const View *v);
/* Where the slots of the open vehicle are on screen, for the inventory to
 * take them in as it does a chest's. Returns how many; none if none is open. */
int trainui_storage_slots(const Game *g, const View *v, Rectangle *rects, int max, uint16_t *container);

#endif
