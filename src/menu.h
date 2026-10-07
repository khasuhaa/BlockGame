#ifndef MENU_H
#define MENU_H

#include "game.h"
#include "recipeui.h"
#include "save.h"

#include "mods.h"

#define SAVE_DIR SAVES_DIR
#define WORLD_NAME_MAX 24
#define MAX_LISTED_WORLDS 64

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
    MAIN_MENU_MODS,   /* which mods are on has been changed: everything is to be loaded again */
    MAIN_MENU_QUIT
} MainMenuResult;

/* A saved world as the menu shows it. */
typedef struct {
    char name[WORLD_NAME_MAX + 1];
    char path[256];
    bool has_preview;
    WorldPreview preview;
} ListedWorld;

typedef struct {
    bool creating; /* on the "new world" screen */
    /* What the new world is to be made as: which of the sizes the data offers, and which of its scales for each of three things. */
    int size, mountains, valleys, veins;
    /* Which tab of that screen is up (0 the world, 1 advanced), and whether a debug world was asked for. */
    int tab, debug;
    bool deleting; /* asking whether the chosen world is really to be deleted */
    /* The mods there are and which are on; whether the screen that turns them on and off is up, and whether it has changed anything. */
    ModList mods;
    bool modding, mods_changed;
    char name[WORLD_NAME_MAX + 1];
    char error[96];
    char path[256];

    /* The saved worlds, side by side: one is in the middle, and the row
     * slides left and right to bring another there. */
    bool listed;    /* the list below is up to date; clear it to have the saves read again */
    ListedWorld worlds[MAX_LISTED_WORLDS];
    int world_count;
    int selected;
    float slide;    /* where the row has slid to, in worlds; eases toward `selected` */
} MainMenu;

/* Where worlds are kept now: the directory of the playset the mods that are on make. Set it whenever they change. */
void main_menu_set_saves(const char *dir);
const char *main_menu_saves(void);

/* What the "new world" screen was set to, as the generator wants it. */
int main_menu_world_width(const MainMenu *m);
WorldOptions main_menu_world_options(const MainMenu *m);
/* Whether it is to be a debug world: one of every block, set out to be looked at. */
bool main_menu_world_debug(const MainMenu *m);

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
    PAUSE_PAGE_KEYS,
    PAUSE_PAGE_RECIPES
} PausePage;

typedef struct {
    PausePage page;
    int rebinding; /* BindId waiting for a key press, or -1 */
    bool slider_held;
    bool just_opened; /* the key press that opened it is still down this frame */
    RecipeUi recipes; /* the recipe book; it keeps its place while the world is open */
} PauseMenu;

void pause_menu_open(PauseMenu *p);
/* Runs and draws one frame of the pause menu over the world. */
PauseResult pause_menu_frame(PauseMenu *p, Game *g, Keybinds *k, const char *world_name);

#endif
