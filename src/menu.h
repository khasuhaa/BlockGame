#ifndef MENU_H
#define MENU_H

#include "game.h"

#define SAVE_DIR "saves"
#define WORLD_NAME_MAX 24

/* ---- key bindings ------------------------------------------------------ */

typedef enum {
    BIND_LEFT,
    BIND_RIGHT,
    BIND_UP,
    BIND_DOWN,
    BIND_FOLLOW,
    BIND_COUNT
} BindId;

typedef struct {
    int keys[BIND_COUNT]; /* raylib key codes */
} Keybinds;

void keybinds_defaults(Keybinds *k);
/* Bindings are kept in a small text file shared by every world. */
void keybinds_load(Keybinds *k);
void keybinds_save(const Keybinds *k);

/* ---- main menu --------------------------------------------------------- */

typedef enum {
    MAIN_MENU_STAY,
    MAIN_MENU_LOAD,   /* open the save at `path` */
    MAIN_MENU_CREATE, /* make a new world and save it to `path` */
    MAIN_MENU_QUIT
} MainMenuResult;

typedef struct {
    bool creating; /* on the "new world" screen */
    char name[WORLD_NAME_MAX + 1];
    char error[96];
    float scroll;
    char path[256];
} MainMenu;

/* Runs and draws one frame of the main menu. Call between BeginDrawing and
 * EndDrawing. */
MainMenuResult main_menu_frame(MainMenu *m);

/* ---- pause menu -------------------------------------------------------- */

typedef enum {
    PAUSE_STAY,
    PAUSE_RESUME,
    PAUSE_QUIT /* save and go back to the main menu */
} PauseResult;

typedef enum {
    PAUSE_PAGE_MAIN,
    PAUSE_PAGE_WORLD,
    PAUSE_PAGE_KEYS
} PausePage;

typedef struct {
    PausePage page;
    int rebinding; /* BindId waiting for a key press, or -1 */
    bool slider_held;
    bool just_opened; /* the key press that opened it is still down this frame */
} PauseMenu;

void pause_menu_open(PauseMenu *p);
/* Runs and draws one frame of the pause menu over the world. */
PauseResult pause_menu_frame(PauseMenu *p, Game *g, Keybinds *k, const char *world_name);

#endif
