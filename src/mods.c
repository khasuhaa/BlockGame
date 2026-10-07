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

bool mods_load_content(ModList *list, const char *data_dir, char *error, size_t error_size)
{
    char why[256];
    if (error_size > 0) error[0] = '\0';
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
