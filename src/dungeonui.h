#ifndef DUNGEONUI_H
#define DUNGEONUI_H

#include "game.h"
#include "hud.h"
#include "raylib.h"
#include "view.h"

/* The menus of a dungeon gate (which dungeon, how hard) and of a forge (upgrading and selling what was found), and
 * what is drawn over the game for both: what a rolled thing under the cursor has, and a boss's health. */
typedef struct {
    bool pressed; /* a press that began on a menu */
} DungeonUi;

/* Returns true while the mouse belongs to a menu. */
bool dungeonui_handle_input(DungeonUi *ui, Game *g, const View *v);
bool dungeonui_mouse_over(const DungeonUi *ui, const Game *g, const View *v);
void dungeonui_draw(const DungeonUi *ui, const Game *g, const View *v);
void dungeonui_draw_over(const Game *g, const Hud *hud);

#endif
