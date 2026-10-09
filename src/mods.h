#ifndef MODS_H
#define MODS_H

#include <stdbool.h>
#include <stddef.h>

/* Mods. Each is a directory under MODS_DIR holding a `mod.json` - what it is
 * called and its icon - beside data files and textures laid out like the
 * game's own. A mod that is on is loaded on top of the game's data, in the
 * order of the mods' names.
 *
 * The mods that are on make a playset, and each playset has worlds of its
 * own: the game's own are in SAVES_DIR, and a playset's in a directory under
 * it named for its mods. Turning a mod off and on again comes back to the
 * same worlds. Which mods are on is kept in a file in SAVES_DIR. */

#define MODS_DIR "mods"
#define SAVES_DIR "saves"
#define MAX_MODS 64
#define MOD_NAME_MAX 31

typedef struct {
    char id[MOD_NAME_MAX + 1];    /* its directory's name */
    char label[48];               /* what it is called */
    char dir[160];                /* where it is */
    char icon[200];               /* its icon's file, or empty */
    bool enabled;
    bool utility;                 /* "utility": true - it has no content to speak of, and no book of quests is looked for */
    bool standalone;              /* a game of its own: loaded in place of the game's data, with no other mod */
} ModInfo;

typedef struct {
    ModInfo mods[MAX_MODS];
    int count;
} ModList;

/* Finds the mods there are under `mods_dir`, in order of name, and marks the
 * ones the file in `saves_dir` says are on. */
void mods_scan(ModList *list, const char *mods_dir, const char *saves_dir);
/* Writes down which are on. False if it could not be written. */
bool mods_remember(const ModList *list, const char *saves_dir);
/* The name of the playset the mods that are on make: their names, joined; empty for none. */
void mods_playset(const ModList *list, char *out, size_t size);
/* Where that playset's worlds are kept; the directory is made if it is not there. */
void mods_saves(const ModList *list, const char *saves_dir, char *out, size_t size);
/* Playsets. A playset is a name, the mods that are on in it, and a directory
 * of worlds of its own. They are made, named and filled by the player, so
 * that a world is found again by what its playset is called and not by
 * remembering which mods it was made with. The first is always there: the
 * game by itself, with no mods, whose worlds are in SAVES_DIR. They are kept
 * in a file in SAVES_DIR. Taking a playset off the list leaves its directory,
 * and the worlds in it, where they are. */
#define MAX_PLAYSETS 32
#define PLAYSET_NAME_MAX 31

typedef struct {
    char name[PLAYSET_NAME_MAX + 1];
    char dir[96];                                /* under the saves directory; empty for that directory itself */
    char mods[MAX_MODS][MOD_NAME_MAX + 1];
    int mod_count;
} Playset;

typedef struct {
    Playset sets[MAX_PLAYSETS];
    int count;
    int active;
} PlaysetList;

/* Reads the playsets. Where none have been written down yet they are made from what is there: the game by itself,
 * and one for every directory of worlds the mods that were on once made, named for those mods - and the one that
 * is in use is whichever the mods now on make. */
void playsets_load(PlaysetList *sets, const ModList *mods, const char *saves_dir);
bool playsets_save(const PlaysetList *sets, const char *saves_dir);
/* A new one, with no mods in it and a directory of its own. Returns which, or -1 if there is no room or the name will not do. */
int playsets_add(PlaysetList *sets, const char *name, const char *saves_dir);
/* Takes one off the list (never the first). Its worlds stay on the disk. */
bool playsets_remove(PlaysetList *sets, int which);
bool playsets_rename(PlaysetList *sets, int which, const char *name);
/* Whether a mod is in a playset, and putting it in or taking it out (not the first: that is the game by itself). */
bool playset_has(const Playset *set, const char *mod);
bool playset_toggle(Playset *set, const char *mod);
/* Turns on the mods of the playset in use, and every other off. */
void playsets_apply(const PlaysetList *sets, ModList *mods);
/* Where the worlds of the playset in use are kept; the directory is made if it is not there. */
void playsets_saves(const PlaysetList *sets, const char *saves_dir, char *out, size_t size);

/* Whether two mods cannot be on together: each defines something - a block,
 * an item, a creature or a tree - under a name the other also defines. `what`
 * gets that name. */
bool mods_conflict(const ModInfo *a, const ModInfo *b, char *what, size_t size);
/* Loads the game's data from `data_dir` and then each mod that is on, afresh.
 * If two of the mods that are on cannot be on together, none is loaded: every
 * mod is turned off, and `error` says which two and over what. A mod that
 * will not load by itself is turned off - `error` says which, and why - and
 * the rest are loaded without it. False only if the game's own data will not load. */
bool mods_load_content(ModList *list, const char *data_dir, char *error, size_t error_size);
/* The directories textures are looked for in, the game's own first: fills
 * `dirs` (room for MAX_MODS + 1) and returns how many. */
int mods_texture_dirs(const ModList *list, const char *data_dir, const char **dirs);

#endif
