#define _POSIX_C_SOURCE 200809L
#include "mods.h"

#include "content.h"
#include "../third_party/cjson/cJSON.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ENABLED_FILE "mods.txt"

static int by_id(const void *a, const void *b)
{
    return strcmp(((const ModInfo *)a)->id, ((const ModInfo *)b)->id);
}

static char *read_all(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *text = NULL;
    long size = 0;
    if (fseek(f, 0, SEEK_END) == 0 && (size = ftell(f)) >= 0 && fseek(f, 0, SEEK_SET) == 0 && (text = malloc((size_t)size + 1))) {
        size_t got = fread(text, 1, (size_t)size, f);
        text[got] = '\0';
    }
    fclose(f);
    return text;
}

/* A name that is safe to make part of a path: letters, digits, '_' and '-'. */
static bool plain_name(const char *name)
{
    if (!name[0] || strlen(name) > MOD_NAME_MAX) return false;
    for (const char *c = name; *c; c++)
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-')) return false;
    return true;
}

void mods_scan(ModList *list, const char *mods_dir, const char *saves_dir)
{
    memset(list, 0, sizeof *list);
    DIR *d = opendir(mods_dir);
    struct dirent *e;
    while (d && (e = readdir(d)) != NULL && list->count < MAX_MODS) {
        if (!plain_name(e->d_name)) continue;
        char path[512];
        snprintf(path, sizeof path, "%s/%s/mod.json", mods_dir, e->d_name);
        char *text = read_all(path);
        if (!text) continue;
        cJSON *root = cJSON_Parse(text);
        free(text);
        if (!root) continue;
        ModInfo *m = &list->mods[list->count++];
        snprintf(m->id, sizeof m->id, "%s", e->d_name);
        snprintf(m->dir, sizeof m->dir, "%s/%s", mods_dir, e->d_name);
        const cJSON *label = cJSON_GetObjectItemCaseSensitive(root, "label"), *icon = cJSON_GetObjectItemCaseSensitive(root, "icon");
        snprintf(m->label, sizeof m->label, "%s", cJSON_IsString(label) ? label->valuestring : e->d_name);
        if (cJSON_IsString(icon) && !strstr(icon->valuestring, "..")) snprintf(m->icon, sizeof m->icon, "%s/%s", m->dir, icon->valuestring);
        m->standalone = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "standalone"));
        m->utility = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "utility"));
        cJSON_Delete(root);
    }
    if (d) closedir(d);
    qsort(list->mods, (size_t)list->count, sizeof list->mods[0], by_id);

    char path[512], line[128];
    snprintf(path, sizeof path, "%s/%s", saves_dir, ENABLED_FILE);
    FILE *f = fopen(path, "r");
    while (f && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = '\0';
        for (int i = 0; i < list->count; i++)
            if (strcmp(list->mods[i].id, line) == 0) list->mods[i].enabled = true;
    }
    if (f) fclose(f);
}

bool mods_remember(const ModList *list, const char *saves_dir)
{
    char path[512];
    mkdir(saves_dir, 0755);
    snprintf(path, sizeof path, "%s/%s", saves_dir, ENABLED_FILE);
    FILE *f = fopen(path, "w");
    if (!f) return false;
    for (int i = 0; i < list->count; i++)
        if (list->mods[i].enabled) fprintf(f, "%s\n", list->mods[i].id);
    return fclose(f) == 0;
}

void mods_playset(const ModList *list, char *out, size_t size)
{
    size_t used = 0;
    if (size > 0) out[0] = '\0';
    for (int i = 0; i < list->count; i++) {
        if (!list->mods[i].enabled) continue;
        int n = snprintf(out + used, size - used, "%s%s", used ? "+" : "", list->mods[i].id);
        if (n < 0 || (size_t)n >= size - used) break;
        used += (size_t)n;
    }
}

void mods_saves(const ModList *list, const char *saves_dir, char *out, size_t size)
{
    char playset[MAX_MODS * (MOD_NAME_MAX + 1) + 1];
    mods_playset(list, playset, sizeof playset);
    mkdir(saves_dir, 0755);
    if (!playset[0]) {
        snprintf(out, size, "%s", saves_dir);
        return;
    }
    snprintf(out, size, "%s/playsets", saves_dir);
    mkdir(out, 0755);
    snprintf(out, size, "%s/playsets/%s", saves_dir, playset);
    mkdir(out, 0755);
}

/* ---- playsets ----------------------------------------------------------- */

#define PLAYSETS_FILE "playsets.txt"

/* A name that will do for a playset: something, not too long, and nothing in it that the file's lines are cut at. */
static bool playset_name_ok(const char *name)
{
    size_t n = strlen(name);
    if (n == 0 || n > PLAYSET_NAME_MAX || name[0] == ' ' || name[n - 1] == ' ') return false;
    for (const char *c = name; *c; c++)
        if (*c < 32 || *c > 126 || *c == '|') return false;
    return true;
}

bool playset_has(const Playset *set, const char *mod)
{
    for (int i = 0; i < set->mod_count; i++)
        if (strcmp(set->mods[i], mod) == 0) return true;
    return false;
}

bool playset_toggle(Playset *set, const char *mod)
{
    if (!plain_name(mod)) return false;
    for (int i = 0; i < set->mod_count; i++) {
        if (strcmp(set->mods[i], mod) != 0) continue;
        for (int k = i; k + 1 < set->mod_count; k++) memcpy(set->mods[k], set->mods[k + 1], sizeof set->mods[k]);
        set->mod_count--;
        return true;
    }
    if (set->mod_count >= MAX_MODS) return false;
    snprintf(set->mods[set->mod_count++], sizeof set->mods[0], "%s", mod);
    return true;
}

static bool dir_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* The playset a directory of the old kind - named for its mods, joined by '+' - was. */
static void playset_from_dir(Playset *set, const ModList *mods, const char *dir_name)
{
    memset(set, 0, sizeof *set);
    snprintf(set->dir, sizeof set->dir, "playsets/%s", dir_name);
    char ids[256];
    snprintf(ids, sizeof ids, "%s", dir_name);
    size_t used = 0;
    for (char *id = strtok(ids, "+"); id; id = strtok(NULL, "+")) {
        if (!plain_name(id) || set->mod_count >= MAX_MODS) continue;
        snprintf(set->mods[set->mod_count++], sizeof set->mods[0], "%s", id);
        /* (Called what its mods are called, as far as there is room.) */
        const char *label = id;
        for (int i = 0; i < mods->count; i++)
            if (strcmp(mods->mods[i].id, id) == 0) label = mods->mods[i].label;
        int n = snprintf(set->name + used, sizeof set->name - used, "%s%s", used ? " + " : "", label);
        if (n > 0) used = used + (size_t)n < sizeof set->name ? used + (size_t)n : sizeof set->name - 1;
    }
    if (!playset_name_ok(set->name)) snprintf(set->name, sizeof set->name, "Playset");
}

void playsets_load(PlaysetList *sets, const ModList *mods, const char *saves_dir)
{
    memset(sets, 0, sizeof *sets);
    snprintf(sets->sets[0].name, sizeof sets->sets[0].name, "No mods");
    sets->count = 1;
    char path[512], line[4096];
    snprintf(path, sizeof path, "%s/%s", saves_dir, PLAYSETS_FILE);
    FILE *f = fopen(path, "r");
    if (f) {
        /* "active N", then a line to a playset after the first: its directory, its name, its mods. */
        int active = 0;
        while (fgets(line, sizeof line, f)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (strncmp(line, "active ", 7) == 0) {
                active = atoi(line + 7);
                continue;
            }
            if (strncmp(line, "set ", 4) != 0 || sets->count >= MAX_PLAYSETS) continue;
            char *dir = line + 4, *name = strchr(dir, '|'), *list = name ? strchr(name + 1, '|') : NULL;
            if (!name || !list) continue;
            *name++ = '\0';
            *list++ = '\0';
            if (!playset_name_ok(name) || strstr(dir, "..") || dir[0] == '/' || strlen(dir) >= sizeof sets->sets[0].dir || !dir[0]) continue;
            Playset *set = &sets->sets[sets->count++];
            snprintf(set->name, sizeof set->name, "%s", name);
            snprintf(set->dir, sizeof set->dir, "%s", dir);
            for (char *id = strtok(list, ","); id; id = strtok(NULL, ","))
                if (plain_name(id) && !playset_has(set, id) && set->mod_count < MAX_MODS) snprintf(set->mods[set->mod_count++], sizeof set->mods[0], "%s", id);
        }
        fclose(f);
        sets->active = active >= 0 && active < sets->count ? active : 0;
        return;
    }
    /* None written down: one for every directory of worlds there is from before there were any to name. */
    snprintf(path, sizeof path, "%s/playsets", saves_dir);
    DIR *d = opendir(path);
    struct dirent *e;
    char names[MAX_PLAYSETS][256];
    int found = 0;
    while (d && (e = readdir(d)) != NULL && found < MAX_PLAYSETS - 1) {
        if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof names[0] || strlen(e->d_name) + 10 >= sizeof sets->sets[0].dir) continue;
        snprintf(names[found++], sizeof names[0], "%s", e->d_name);
    }
    if (d) closedir(d);
    qsort(names, (size_t)found, sizeof names[0], (int (*)(const void *, const void *))strcmp);
    for (int i = 0; i < found; i++) playset_from_dir(&sets->sets[sets->count++], mods, names[i]);
    /* The one in use is whichever has just the mods that are on now. */
    char now[MAX_MODS * (MOD_NAME_MAX + 1) + 1];
    mods_playset(mods, now, sizeof now);
    for (int i = 1; i < sets->count && now[0]; i++)
        if (strcmp(sets->sets[i].dir + strlen("playsets/"), now) == 0) sets->active = i;
    if (now[0] && sets->active == 0 && sets->count < MAX_PLAYSETS) {
        /* (Mods on, and no worlds made with them yet: that is a playset too.) */
        playset_from_dir(&sets->sets[sets->count], mods, now);
        sets->active = sets->count++;
    }
}

bool playsets_save(const PlaysetList *sets, const char *saves_dir)
{
    char path[512];
    mkdir(saves_dir, 0755);
    snprintf(path, sizeof path, "%s/%s", saves_dir, PLAYSETS_FILE);
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "active %d\n", sets->active);
    for (int i = 1; i < sets->count; i++) {
        fprintf(f, "set %s|%s|", sets->sets[i].dir, sets->sets[i].name);
        for (int k = 0; k < sets->sets[i].mod_count; k++) fprintf(f, "%s%s", k ? "," : "", sets->sets[i].mods[k]);
        fprintf(f, "\n");
    }
    return fclose(f) == 0;
}

int playsets_add(PlaysetList *sets, const char *name, const char *saves_dir)
{
    if (sets->count >= MAX_PLAYSETS || !playset_name_ok(name)) return -1;
    Playset *set = &sets->sets[sets->count];
    memset(set, 0, sizeof *set);
    snprintf(set->name, sizeof set->name, "%s", name);
    /* A directory named for it - in letters that are safe in a path - that nothing else has. */
    char slug[PLAYSET_NAME_MAX + 1];
    size_t n = 0;
    for (const char *c = name; *c && n < sizeof slug - 1; c++) {
        bool plain = (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9');
        slug[n++] = plain ? *c : *c >= 'A' && *c <= 'Z' ? (char)(*c - 'A' + 'a') : '_';
    }
    slug[n] = '\0';
    for (int k = 1; k < 1000; k++) {
        char path[512];
        if (k == 1) snprintf(set->dir, sizeof set->dir, "playsets/set_%s", slug);
        else snprintf(set->dir, sizeof set->dir, "playsets/set_%s_%d", slug, k);
        bool taken = false;
        for (int i = 0; i < sets->count; i++) taken = taken || strcmp(sets->sets[i].dir, set->dir) == 0;
        snprintf(path, sizeof path, "%s/%s", saves_dir, set->dir);
        if (!taken && !dir_exists(path)) return sets->count++;
    }
    return -1;
}

bool playsets_remove(PlaysetList *sets, int which)
{
    if (which < 1 || which >= sets->count) return false;
    for (int i = which; i + 1 < sets->count; i++) sets->sets[i] = sets->sets[i + 1];
    sets->count--;
    if (sets->active == which) sets->active = 0;
    else if (sets->active > which) sets->active--;
    return true;
}

bool playsets_rename(PlaysetList *sets, int which, const char *name)
{
    if (which < 1 || which >= sets->count || !playset_name_ok(name)) return false;
    snprintf(sets->sets[which].name, sizeof sets->sets[which].name, "%s", name);
    return true;
}

void playsets_apply(const PlaysetList *sets, ModList *mods)
{
    const Playset *set = &sets->sets[sets->active >= 0 && sets->active < sets->count ? sets->active : 0];
    for (int i = 0; i < mods->count; i++) mods->mods[i].enabled = playset_has(set, mods->mods[i].id);
}

void playsets_saves(const PlaysetList *sets, const char *saves_dir, char *out, size_t size)
{
    const Playset *set = &sets->sets[sets->active >= 0 && sets->active < sets->count ? sets->active : 0];
    mkdir(saves_dir, 0755);
    if (!set->dir[0]) {
        snprintf(out, size, "%s", saves_dir);
        return;
    }
    snprintf(out, size, "%s/playsets", saves_dir);
    mkdir(out, 0755);
    snprintf(out, size, "%s/%s", saves_dir, set->dir);
    mkdir(out, 0755);
}

/* The names a mod gives things of one kind, as its file for them lists them. NULL if it has no such file. */
static cJSON *mod_names(const ModInfo *mod, const char *file)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.json", mod->dir, file);
    char *text = read_all(path);
    if (!text) return NULL;
    cJSON *root = cJSON_Parse(text);
    free(text);
    return root;
}

bool mods_conflict(const ModInfo *a, const ModInfo *b, char *what, size_t size)
{
    static const char *const kinds[] = { "tiles", "items", "mobs", "trees" };
    bool clash = false;
    for (int k = 0; k < 4 && !clash; k++) {
        cJSON *mine = mod_names(a, kinds[k]), *theirs = mod_names(b, kinds[k]);
        const cJSON *x, *y;
        cJSON_ArrayForEach(x, cJSON_GetObjectItemCaseSensitive(mine, kinds[k])) {
            const cJSON *name = cJSON_GetObjectItemCaseSensitive(x, "name");
            if (!cJSON_IsString(name)) continue;
            cJSON_ArrayForEach(y, cJSON_GetObjectItemCaseSensitive(theirs, kinds[k])) {
                const cJSON *other = cJSON_GetObjectItemCaseSensitive(y, "name");
                if (!cJSON_IsString(other) || strcmp(name->valuestring, other->valuestring) != 0) continue;
                snprintf(what, size, "%s", name->valuestring);
                clash = true;
                break;
            }
            if (clash) break;
        }
        cJSON_Delete(mine);
        cJSON_Delete(theirs);
    }
    return clash;
}

bool mods_load_content(ModList *list, const char *data_dir, char *error, size_t error_size)
{
    char why[256];
    if (error_size > 0) error[0] = '\0';
    /* A mod that is a game of its own is loaded in place of the game's data, and nothing else with it: every
     * other mod names things of the game's that would not be there. */
    for (int i = 0; i < list->count; i++) {
        if (!list->mods[i].enabled || !list->mods[i].standalone) continue;
        for (int k = 0; k < list->count; k++) {
            if (k == i || !list->mods[k].enabled) continue;
            list->mods[k].enabled = false;
            snprintf(error, error_size, "%s is a game of its own: the other mods are off.", list->mods[i].label);
        }
        content_reset();
        if (content_load(list->mods[i].dir, why, sizeof why)) return true;
        list->mods[i].enabled = false;
        snprintf(error, error_size, "%s: %s", list->mods[i].label, why);
        break;
    }
    /* Two that cannot be on together: then none is. */
    for (int i = 0; i < list->count; i++) {
        for (int k = i + 1; k < list->count; k++) {
            if (!list->mods[i].enabled || !list->mods[k].enabled || !mods_conflict(&list->mods[i], &list->mods[k], why, sizeof why)) continue;
            snprintf(error, error_size, "%s and %s both change \"%.40s\": no mods are on.", list->mods[i].label, list->mods[k].label, why);
            for (int m = 0; m < list->count; m++) list->mods[m].enabled = false;
            content_reset();
            if (content_load(data_dir, why, sizeof why)) return true;
            snprintf(error, error_size, "%s", why);
            return false;
        }
    }
    for (;;) {
        content_reset();
        if (!content_load(data_dir, why, sizeof why)) {
            snprintf(error, error_size, "%s", why);
            return false;
        }
        int bad = -1;
        for (int i = 0; i < list->count && bad < 0; i++)
            if (list->mods[i].enabled && !content_load(list->mods[i].dir, why, sizeof why)) bad = i;
        if (bad < 0) return true;
        /* That one is left out, and everything is loaded again without it. */
        list->mods[bad].enabled = false;
        snprintf(error, error_size, "%s: %s", list->mods[bad].label, why);
    }
}

int mods_texture_dirs(const ModList *list, const char *data_dir, const char **dirs)
{
    int n = 0;
    dirs[n++] = data_dir;
    for (int i = 0; i < list->count; i++)
        if (list->mods[i].enabled) dirs[n++] = list->mods[i].dir;
    return n;
}
