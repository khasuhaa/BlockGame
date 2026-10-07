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
#define MAX_MODS 16
#define MOD_NAME_MAX 31

typedef struct {
    char id[MOD_NAME_MAX + 1];    /* its directory's name */
    char label[48];               /* what it is called */
    char dir[160];                /* where it is */
    char icon[200];               /* its icon's file, or empty */
    bool enabled;
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
/* Loads the game's data from `data_dir` and then each mod that is on, afresh.
 * A mod that will not load is turned off - `error` says which, and why - and
 * the rest are loaded without it. False only if the game's own data will not load. */
bool mods_load_content(ModList *list, const char *data_dir, char *error, size_t error_size);
/* The directories textures are looked for in, the game's own first: fills
 * `dirs` (room for MAX_MODS + 1) and returns how many. */
int mods_texture_dirs(const ModList *list, const char *data_dir, const char **dirs);

#endif
