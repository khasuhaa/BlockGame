#include "content.h"

#include "../third_party/cjson/cJSON.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Content content;

/* ---- lookup ------------------------------------------------------------ */

TileType tile_id(const char *name)
{
    for (int i = 0; i < content.tile_count; i++)
        if (strcmp(content.tiles[i].name, name) == 0) return i;
    return -1;
}

TileType tile_id_or_alias(const char *name)
{
    TileType t = tile_id(name);
    for (int i = 0; t < 0 && i < content.tile_count; i++)
        for (int a = 0; a < content.tiles[i].alias_count; a++)
            if (strcmp(content.tiles[i].aliases[a], name) == 0) t = i;
    return t;
}

ItemId item_id(const char *name)
{
    for (int i = 0; i < content.item_count; i++)
        if (strcmp(content.items[i].name, name) == 0) return i;
    return -1;
}

int tree_id(const char *name)
{
    for (int i = 0; i < content.tree_count; i++)
        if (strcmp(content.trees[i].name, name) == 0) return i;
    return -1;
}

int tool_class_id(const char *name)
{
    for (int i = 0; i < content.tool_class_count; i++)
        if (strcmp(content.tool_classes[i], name) == 0) return i;
    return -1;
}

const TileDef *tile_def(TileType t)
{
    return &content.tiles[t > 0 && t < content.tile_count ? t : TILE_AIR];
}

const ItemDef *item_def(ItemId id)
{
    return &content.items[id > 0 && id < content.item_count ? id : ITEM_NONE];
}

void content_reset(void)
{
    memset(&content, 0, sizeof content);
    snprintf(content.tiles[0].name, CONTENT_NAME_LEN, "air");
    content.tiles[0].log_of = content.tiles[0].leaves_of = -1;
    content.tiles[0].tool_class = -1;
    content.tile_count = 1;
    snprintf(content.items[0].name, CONTENT_NAME_LEN, "none");
    snprintf(content.items[0].label, CONTENT_NAME_LEN, "Nothing");
    content.items[0].equip = EQUIP_NONE;
    content.items[0].plants = -1;
    content.items[0].tool_class = -1;
    content.items[0].category = -1;
    content.item_count = 1;
}

/* ---- reading JSON ------------------------------------------------------ */

/* Where we are, for error messages. */
typedef struct {
    const char *file;
    char *error;
    size_t error_size;
    bool failed;
} Reader;

static bool fail(Reader *r, const char *fmt, ...)
{
    if (!r->failed) {
        va_list args;
        va_start(args, fmt);
        int n = snprintf(r->error, r->error_size, "%s: ", r->file);
        if (n >= 0 && (size_t)n < r->error_size)
            vsnprintf(r->error + n, r->error_size - (size_t)n, fmt, args);
        va_end(args);
        r->failed = true;
    }
    return false;
}

static const cJSON *field(const cJSON *obj, const char *key)
{
    return cJSON_GetObjectItemCaseSensitive(obj, key);
}

static double num(const cJSON *obj, const char *key, double fallback)
{
    const cJSON *v = field(obj, key);
    return cJSON_IsNumber(v) ? v->valuedouble : fallback;
}

static bool flag(const cJSON *obj, const char *key, bool fallback)
{
    const cJSON *v = field(obj, key);
    return cJSON_IsBool(v) ? cJSON_IsTrue(v) : fallback;
}

static const char *str(const cJSON *obj, const char *key)
{
    const cJSON *v = field(obj, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

static void copy_name(char *dst, size_t size, const char *src)
{
    snprintf(dst, size, "%s", src ? src : "");
}

/* The id of a kind of tool, adding it if this is the first mention of it. */
static bool tool_class_ref(Reader *r, const char *name, int *out)
{
    *out = -1;
    if (!name) return true;
    *out = tool_class_id(name);
    if (*out >= 0) return true;
    if (content.tool_class_count >= MAX_TOOL_CLASSES) return fail(r, "too many kinds of tool");
    *out = content.tool_class_count++;
    snprintf(content.tool_classes[*out], CONTENT_NAME_LEN, "%s", name);
    return true;
}

/* A two-number array such as "size": [10, 60]. */
static void pair(const cJSON *obj, const char *key, int *a, int *b)
{
    const cJSON *v = field(obj, key);
    if (cJSON_GetArraySize(v) != 2) return;
    *a = (int)cJSON_GetArrayItem(v, 0)->valuedouble;
    *b = (int)cJSON_GetArrayItem(v, 1)->valuedouble;
}

/* A field naming a tile or an item. Absent means "nothing"; unknown is an error. */
static bool tile_ref(Reader *r, const cJSON *obj, const char *key, TileType *out)
{
    const char *name = str(obj, key);
    if (!name) return true;
    if (tile_id(name) < 0) return fail(r, "\"%s\": there is no tile called \"%s\"", key, name);
    *out = tile_id(name);
    return true;
}

static bool item_ref(Reader *r, const cJSON *obj, const char *key, ItemId *out)
{
    const char *name = str(obj, key);
    if (!name) return true;
    if (item_id(name) < 0) return fail(r, "\"%s\": there is no item called \"%s\"", key, name);
    *out = item_id(name);
    return true;
}

static bool stack_ref(Reader *r, const cJSON *obj, RecipeItem *out)
{
    out->count = (int)num(obj, "count", 1);
    if (!str(obj, "item")) return fail(r, "an item stack needs an \"item\"");
    return item_ref(r, obj, "item", &out->id) && out->count > 0;
}

/* ---- tiles and items --------------------------------------------------- */

/* Tiles and items name each other, so every name is registered before any
 * definition is filled in. */
static bool register_names(Reader *r, const cJSON *root)
{
    const cJSON *e;
    cJSON_ArrayForEach(e, field(root, "tiles")) {
        const char *name = str(e, "name");
        if (!name) return fail(r, "a tile has no \"name\"");
        if (tile_id(name) >= 0) continue;
        if (content.tile_count >= MAX_TILES) return fail(r, "too many tiles");
        TileDef *t = &content.tiles[content.tile_count++];
        memset(t, 0, sizeof *t);
        copy_name(t->name, sizeof t->name, name);
        t->log_of = t->leaves_of = -1;
    }
    cJSON_ArrayForEach(e, field(root, "items")) {
        const char *name = str(e, "name");
        if (!name) return fail(r, "an item has no \"name\"");
        if (item_id(name) >= 0) continue;
        if (content.item_count >= MAX_ITEMS) return fail(r, "too many items");
        ItemDef *it = &content.items[content.item_count++];
        memset(it, 0, sizeof *it);
        copy_name(it->name, sizeof it->name, name);
        it->plants = -1;
        it->category = -1;
    }
    return true;
}

static bool read_tiles(Reader *r, const cJSON *root)
{
    const cJSON *e;
    cJSON_ArrayForEach(e, field(root, "tiles")) {
        TileDef *t = &content.tiles[tile_id(str(e, "name"))];
        t->solid = flag(e, "solid", false);
        t->layer = str(e, "layer") && strcmp(str(e, "layer"), "background") == 0 ? LAYER_BACKGROUND
                                                                                  : LAYER_FOREGROUND;
        t->bench = flag(e, "bench", false);
        t->trade = flag(e, "trade", false);
        t->removable = flag(e, "removable", false);
        t->climbable = flag(e, "climbable", false);
        t->mine_time = (float)num(e, "mine_time", 0.35);
        t->light = (int)num(e, "light", 0);
        t->height = (int)num(e, "height", 1);
        if (t->height < 1 || t->height > 8) return fail(r, "tile \"%s\": height must be 1 to 8", t->name);
        copy_name(t->texture_top, sizeof t->texture_top, str(e, "texture_top"));
        const cJSON *alias;
        t->alias_count = 0;
        cJSON_ArrayForEach(alias, field(e, "aliases")) {
            if (t->alias_count < MAX_TILE_ALIASES && cJSON_IsString(alias))
                copy_name(t->aliases[t->alias_count++], CONTENT_NAME_LEN, alias->valuestring);
        }
        t->light_needs_fuel = flag(e, "light_needs_fuel", false);
        t->ore = flag(e, "ore", false);
        t->drop_count = (int)num(e, "drop_count", 1);
        t->shade = (int)num(e, "shade", 0);
        t->front = flag(e, "front", false);
        t->pulse = (float)num(e, "pulse", 0.0);
        t->drop = ITEM_NONE;
        if (!item_ref(r, e, "drop", &t->drop)) return false;
        copy_name(t->label, sizeof t->label, str(e, "label") ? str(e, "label") : t->name);

        /* A bench is of its own kind, at level 1, unless it says otherwise. */
        copy_name(t->bench_kind, sizeof t->bench_kind,
                  str(e, "bench_kind") ? str(e, "bench_kind") : t->name);
        t->bench_level = (int)num(e, "bench_level", 1);
        copy_name(t->popup_name, sizeof t->popup_name,
                  str(e, "popup_name") ? str(e, "popup_name") : t->label);
        if (!str(e, "popup_name"))
            for (char *c = t->popup_name; *c; c++) *c = (char)tolower((unsigned char)*c);
        t->storage_columns = (int)num(field(e, "storage"), "columns", 0);
        t->storage_rows = (int)num(field(e, "storage"), "rows", 0);
        t->storage_shared = flag(field(e, "storage"), "shared", false);
        /* A block in the foreground leaves a backwall unless it says not to. */
        t->wall = flag(e, "wall", t->solid && t->layer == LAYER_FOREGROUND);
        t->fuel_capacity = (int)num(e, "fuel_capacity", 0);
        if (!tool_class_ref(r, str(e, "tool"), &t->tool_class)) return false;
        t->upgrade_to = TILE_AIR;
        t->upgrade_cost_count = 0;
        const cJSON *upgrade = field(e, "upgrade"), *cost;
        if (upgrade) {
            if (!str(upgrade, "to") || !tile_ref(r, upgrade, "to", &t->upgrade_to))
                return fail(r, "tile \"%s\": an upgrade needs a \"to\" tile", t->name);
            cJSON_ArrayForEach(cost, field(upgrade, "cost")) {
                if (t->upgrade_cost_count >= RECIPE_MAX_INPUTS)
                    return fail(r, "tile \"%s\": an upgrade costs too many kinds of item", t->name);
                if (!stack_ref(r, cost, &t->upgrade_cost[t->upgrade_cost_count++])) return false;
            }
        }

        /* "texture" is one file, or a list to vary between. */
        const cJSON *tex = field(e, "texture"), *one;
        t->texture_count = 0;
        if (cJSON_IsString(tex)) {
            copy_name(t->textures[t->texture_count++], CONTENT_PATH_LEN, tex->valuestring);
        } else {
            cJSON_ArrayForEach(one, tex) {
                if (t->texture_count < MAX_TILE_TEXTURES && cJSON_IsString(one))
                    copy_name(t->textures[t->texture_count++], CONTENT_PATH_LEN, one->valuestring);
            }
        }
    }
    return true;
}

static bool read_items(Reader *r, const cJSON *root)
{
    static const char *equip_names[] = { "head", "chest", "legs", "boots" };
    const cJSON *e;
    cJSON_ArrayForEach(e, field(root, "items")) {
        ItemDef *it = &content.items[item_id(str(e, "name"))];
        copy_name(it->label, sizeof it->label, str(e, "label") ? str(e, "label") : it->name);
        it->max_stack = (int)num(e, "max_stack", 99);
        if (it->max_stack < 1 || it->max_stack > 255)
            return fail(r, "item \"%s\": max_stack must be 1 to 255", it->name);
        /* "container" is true for a plain four-slot one, or says how many
         * slots and whether they open from the inventory bar. */
        const cJSON *box = field(e, "container");
        it->container_slots = cJSON_IsTrue(box) ? 4 : (int)num(box, "slots", 0);
        it->opens_in_bar = cJSON_IsTrue(box) || flag(box, "open_in_inventory", false);
        if (it->container_slots < 0 || it->container_slots > 16)
            return fail(r, "item \"%s\": a container has at most 16 slots", it->name);
        if (it->opens_in_bar && it->max_stack != 1)
            return fail(r, "item \"%s\": a container that opens in the inventory cannot stack",
                        it->name);
        copy_name(it->texture, sizeof it->texture, str(e, "texture"));
        it->fuel = (int)num(e, "fuel", 0);
        /* The category is matched to the trade file once that has been read. */
        copy_name(it->category_name, sizeof it->category_name, str(e, "category"));
        it->price = (int)num(e, "price", 0);
        /* "tool": what kind it is, and how much faster it makes mining. */
        const cJSON *tool = field(e, "tool");
        if (!tool_class_ref(r, str(tool, "class"), &it->tool_class)) return false;
        it->tool_speed = (float)num(tool, "speed", 1.0);
        it->ore_multiplier = (int)num(tool, "ore_multiplier", 1);
        it->crystals = (int)num(e, "crystals", 0);
        if (tool && (it->tool_class < 0 || it->tool_speed <= 0.0f))
            return fail(r, "item \"%s\": a tool needs a \"class\" and a \"speed\" above 0", it->name);
        it->places = TILE_AIR;
        if (!tile_ref(r, e, "places", &it->places)) return false;
        /* Trees are read later; the name is matched to one once they are. */
        copy_name(it->plants_name, sizeof it->plants_name, str(e, "plants"));
        it->equip = EQUIP_NONE;
        const char *equip = str(e, "equip");
        for (int i = 0; equip && i < 4; i++)
            if (strcmp(equip, equip_names[i]) == 0) it->equip = (EquipSlot)i;
    }
    return true;
}

/* ---- recipes and trees ------------------------------------------------- */

static bool read_recipes(Reader *r, const cJSON *root)
{
    const cJSON *e, *in;
    cJSON_ArrayForEach(e, field(root, "recipes")) {
        Recipe recipe = { 0 };
        if (!cJSON_IsObject(field(e, "output")) || !stack_ref(r, field(e, "output"), &recipe.output))
            return fail(r, "a recipe needs an \"output\"");
        cJSON_ArrayForEach(in, field(e, "inputs")) {
            if (recipe.input_count >= RECIPE_MAX_INPUTS)
                return fail(r, "a recipe has more than %d inputs", RECIPE_MAX_INPUTS);
            if (!stack_ref(r, in, &recipe.inputs[recipe.input_count++])) return false;
        }

        /* "bench" is one kind of bench, or a list of kinds that all offer the recipe. */
        recipe.level = (int)num(e, "level", 1);
        recipe.crystals = (int)num(e, "crystals", 0);
        const cJSON *bench = field(e, "bench"), *one;
        int kinds = 0;
        if (cJSON_IsString(bench)) {
            if (content.recipe_count >= MAX_RECIPES) return fail(r, "too many recipes");
            copy_name(recipe.bench, sizeof recipe.bench, bench->valuestring);
            content.recipes[content.recipe_count++] = recipe;
            kinds++;
        }
        const cJSON *list = cJSON_IsString(bench) ? NULL : bench;
        cJSON_ArrayForEach(one, list) {
            if (!cJSON_IsString(one)) continue;
            if (content.recipe_count >= MAX_RECIPES) return fail(r, "too many recipes");
            copy_name(recipe.bench, sizeof recipe.bench, one->valuestring);
            content.recipes[content.recipe_count++] = recipe;
            kinds++;
        }
        if (kinds == 0) return fail(r, "a recipe needs a \"bench\"");
    }
    return true;
}

static bool read_trees(Reader *r, const cJSON *root)
{
    const cJSON *e, *row;
    cJSON_ArrayForEach(e, field(root, "trees")) {
        const char *name = str(e, "name");
        if (!name) return fail(r, "a tree has no \"name\"");
        int id = tree_id(name);
        if (id < 0) {
            if (content.tree_count >= MAX_TREES) return fail(r, "too many trees");
            id = content.tree_count++;
        }
        TreeDef *t = &content.trees[id];
        memset(t, 0, sizeof *t);
        copy_name(t->name, sizeof t->name, name);
        if (!tile_ref(r, e, "log", &t->log) || !tile_ref(r, e, "leaves", &t->leaves) ||
            !item_ref(r, e, "fruit", &t->fruit))
            return false;
        if (t->log == TILE_AIR || t->leaves == TILE_AIR)
            return fail(r, "tree \"%s\" needs a \"log\" and \"leaves\"", name);
        t->fruit_ticks = (int)num(e, "fruit_ticks", 1800);
        t->max_height = (int)num(e, "max_height", 6);
        t->weight = (int)num(e, "weight", 1);
        t->mirror_by_column = flag(e, "mirror_by_column", false);
        const char *layer = str(e, "layer");
        t->layer = layer && strcmp(layer, "foreground") == 0 ? LAYER_FOREGROUND : LAYER_BACKGROUND;
        const cJSON *soil;
        cJSON_ArrayForEach(soil, field(e, "soil")) {
            TileType tile = cJSON_IsString(soil) ? tile_id(soil->valuestring) : -1;
            if (tile < 0) return fail(r, "tree \"%s\": \"soil\" names a tile that does not exist", name);
            if (t->soil_count < MAX_TREE_SOILS) t->soil[t->soil_count++] = tile;
        }
        cJSON_ArrayForEach(row, field(e, "rows")) {
            if (t->row_count >= MAX_TREE_ROWS) return fail(r, "tree \"%s\" has too many rows", name);
            t->rows[t->row_count++] = (TreeRow){ (int)num(row, "offset", 0), (int)num(row, "left", 0),
                                                 (int)num(row, "right", 0), flag(row, "center", false) };
        }
        content.tiles[t->log].log_of = id;
        content.tiles[t->leaves].leaves_of = id;
    }
    return true;
}

/* ---- world generation and rules ---------------------------------------- */

static bool read_worldgen(Reader *r, const cJSON *root)
{
    WorldGenDef *g = &content.worldgen;
    const cJSON *e;
    if (cJSON_IsNumber(field(root, "width"))) g->width = (int)num(root, "width", 0);
    if (cJSON_IsNumber(field(root, "height"))) g->height = (int)num(root, "height", 0);

    const cJSON *plains = field(root, "plains");
    if (plains) {
        g->plains_level = (int)num(plains, "level", 10);
        pair(plains, "gap", &g->gap_min, &g->gap_max);
        g->swell_count = 0;
        cJSON_ArrayForEach(e, field(plains, "swells")) {
            if (g->swell_count < MAX_SWELLS)
                g->swells[g->swell_count++] =
                    (Swell){ (float)num(e, "amplitude", 0), (int)num(e, "wavelength", 64) };
        }
    }

    static const char *profiles[] = { "peak", "bowl", "basin" };
    cJSON_ArrayForEach(e, field(root, "features")) {
        const char *name = str(e, "name");
        if (!name) return fail(r, "a feature has no \"name\"");
        int id = -1;
        for (int i = 0; i < content.feature_count; i++)
            if (strcmp(content.features[i].name, name) == 0) id = i;
        if (id < 0) {
            if (content.feature_count >= MAX_FEATURES) return fail(r, "too many features");
            id = content.feature_count++;
        }
        FeatureDef *f = &content.features[id];
        memset(f, 0, sizeof *f);
        copy_name(f->name, sizeof f->name, name);
        f->direction = num(e, "direction", 1) < 0 ? -1 : 1;
        f->min_size = 10;
        f->max_size = 60;
        pair(e, "size", &f->min_size, &f->max_size);
        if (f->min_size < 1 || f->max_size < f->min_size)
            return fail(r, "feature \"%s\": bad \"size\"", name);
        f->width_per_size = (float)num(field(e, "half_width"), "per_size", 1.0);
        f->width_base = (int)num(field(e, "half_width"), "base", 10);
        f->steepness = (float)num(e, "steepness", 1.0);
        f->shoulders = (float)num(e, "shoulders", 0.0);
        f->max_step = (int)num(e, "max_step", 0);
        f->jitter = (float)num(e, "jitter", 0.0);
        pair(e, "ledges", &f->ledge_min, &f->ledge_max);
        const char *profile = str(e, "profile");
        f->profile = PROFILE_BOWL;
        for (int i = 0; profile && i < 3; i++)
            if (strcmp(profile, profiles[i]) == 0) f->profile = (FeatureProfile)i;
    }

    const cJSON *caves = field(root, "caves");
    if (caves) {
        g->cave_per_100_columns = (float)num(caves, "per_100_columns", 0);
        pair(caves, "length", &g->cave_min_length, &g->cave_max_length);
        pair(caves, "depth", &g->cave_min_depth, &g->cave_max_depth);
        const cJSON *radius = field(caves, "radius");
        if (cJSON_GetArraySize(radius) == 2) {
            g->cave_min_radius = (float)cJSON_GetArrayItem(radius, 0)->valuedouble;
            g->cave_max_radius = (float)cJSON_GetArrayItem(radius, 1)->valuedouble;
        }
    }
    cJSON_ArrayForEach(e, field(root, "cave_decor")) {
        const char *name = str(e, "name");
        if (!name) return fail(r, "a cave decoration has no \"name\"");
        int id = -1;
        for (int i = 0; i < content.decor_count; i++)
            if (strcmp(content.decor[i].name, name) == 0) id = i;
        if (id < 0) {
            if (content.decor_count >= MAX_DECOR) return fail(r, "too many cave decorations");
            id = content.decor_count++;
        }
        DecorDef *d = &content.decor[id];
        memset(d, 0, sizeof *d);
        copy_name(d->name, sizeof d->name, name);
        if (!tile_ref(r, e, "tile", &d->tile)) return false;
        if (d->tile == TILE_AIR) return fail(r, "cave decoration \"%s\" needs a \"tile\"", name);
        d->per_100_columns = (float)num(e, "per_100_columns", 1.0);
        d->max_depth = 1000;
        pair(e, "depth", &d->min_depth, &d->max_depth);
        d->on_floor = flag(e, "on_floor", false);
    }

    cJSON_ArrayForEach(e, field(root, "veins")) {
        const char *name = str(e, "name");
        if (!name) return fail(r, "a vein has no \"name\"");
        int id = -1;
        for (int i = 0; i < content.vein_count; i++)
            if (strcmp(content.veins[i].name, name) == 0) id = i;
        if (id < 0) {
            if (content.vein_count >= MAX_VEINS) return fail(r, "too many veins");
            id = content.vein_count++;
        }
        VeinDef *v = &content.veins[id];
        memset(v, 0, sizeof *v);
        copy_name(v->name, sizeof v->name, name);
        if (!tile_ref(r, e, "tile", &v->tile)) return false;
        if (v->tile == TILE_AIR) return fail(r, "vein \"%s\" needs a \"tile\"", name);
        const cJSON *host;
        cJSON_ArrayForEach(host, field(e, "in")) {
            TileType t = cJSON_IsString(host) ? tile_id(host->valuestring) : -1;
            if (t < 0) return fail(r, "vein \"%s\": \"in\" names a tile that does not exist", name);
            if (v->host_count < MAX_VEIN_HOSTS) v->hosts[v->host_count++] = t;
        }
        if (v->host_count == 0) return fail(r, "vein \"%s\" needs \"in\": the blocks it forms in", name);
        v->min_size = v->max_size = 4;
        pair(e, "size", &v->min_size, &v->max_size);
        if (v->min_size < 1 || v->max_size < v->min_size || v->max_size > MAX_VEIN_SIZE)
            return fail(r, "vein \"%s\": \"size\" must be within 1 to %d", name, MAX_VEIN_SIZE);
        v->max_depth = 1000;
        pair(e, "depth", &v->min_depth, &v->max_depth);
        v->per_100_columns = (float)num(e, "per_100_columns", 1.0);
        v->buried = flag(e, "buried", false);
        v->shape = str(e, "shape") && strcmp(str(e, "shape"), "streak") == 0 ? VEIN_STREAK : VEIN_BLOB;
    }

    const cJSON *layers = field(root, "layers");
    if (layers) {
        if (!tile_ref(r, layers, "surface", &g->surface) || !tile_ref(r, layers, "soil", &g->soil) ||
            !tile_ref(r, layers, "rock", &g->rock))
            return false;
        g->soil_depth = (float)num(layers, "soil_depth", 4.6);
        g->soil_variation = (float)num(layers, "soil_variation", 0.6);
        g->bare_at = (float)num(layers, "bare_at", 0.6);
    }

    const cJSON *spawn = field(root, "spawn");
    if (spawn) {
        pair(spawn, "column", &g->spawn_min, &g->spawn_max);
        g->first_feature_at = (int)num(spawn, "first_feature_at", 80);
        g->flat_radius = (int)num(spawn, "flat_radius", 3);
        g->ease_radius = (int)num(spawn, "ease_radius", 24);
        const cJSON *portal = field(spawn, "portal");
        if (portal) {
            if (!tile_ref(r, portal, "base", &g->portal_base) ||
                !tile_ref(r, portal, "body", &g->portal_body) ||
                !tile_ref(r, portal, "top", &g->portal_top))
                return false;
            g->portal_height = (int)num(portal, "height", 3);
        }
    }

    const cJSON *trees = field(root, "tree_placement");
    if (trees) {
        g->tree_one_in = (int)num(trees, "one_in", 10);
        g->tree_spacing = (int)num(trees, "spacing", 7);
        g->tree_spawn_clearance = (int)num(trees, "spawn_clearance", 8);
        g->tree_grown_chance = (float)num(trees, "grown_chance", 0.5);
        g->tree_young_steps = (int)num(trees, "max_young_steps", 20);
    }
    return true;
}

static bool read_rules(Reader *r, const cJSON *root)
{
    RulesDef *u = &content.rules;
    const cJSON *time = field(root, "time"), *player = field(root, "player");
    const cJSON *drops = field(root, "drops"), *ui = field(root, "interface"), *e;
    if (time) {
        u->tick_rate = (int)num(time, "tick_rate", u->tick_rate);
        u->day_ticks = (int)num(time, "day_ticks", u->day_ticks);
        u->night_ticks = (int)num(time, "night_ticks", u->night_ticks);
        u->twilight_ticks = (int)num(time, "twilight_ticks", u->twilight_ticks);
        u->chunk_width = (int)num(time, "chunk_width", u->chunk_width);
        u->random_events_per_chunk =
            (int)num(time, "random_events_per_chunk", u->random_events_per_chunk);
    }
    if (player) {
        u->walk_time = (float)num(player, "walk_time", u->walk_time);
        u->fall_time = (float)num(player, "fall_time", u->fall_time);
        u->climb_time = (float)num(player, "climb_time", u->climb_time);
        u->hop_time = (float)num(player, "hop_time", u->hop_time);
        u->crawl_time = (float)num(player, "crawl_time", u->crawl_time > 0 ? u->crawl_time : 0.2);
        u->hop_height = (float)num(player, "hop_height", u->hop_height);
        u->max_wall_hop = (int)num(player, "max_wall_hop", u->max_wall_hop);
        u->place_time = (float)num(player, "place_time", u->place_time);
        u->craft_time = (float)num(player, "craft_time", u->craft_time);
        if (field(player, "start_items")) u->start_item_count = 0;
        cJSON_ArrayForEach(e, field(player, "start_items")) {
            if (u->start_item_count >= MAX_START_ITEMS) return fail(r, "too many start items");
            if (!stack_ref(r, e, &u->start_items[u->start_item_count++])) return false;
        }
    }
    if (drops) {
        u->drop_fall_speed = (float)num(drops, "fall_speed", u->drop_fall_speed);
        u->drop_pickup_delay = (float)num(drops, "pickup_delay", u->drop_pickup_delay);
        u->drop_collect_time = (float)num(drops, "collect_time", u->drop_collect_time);
    }
    if (ui) u->denial_time = (float)num(ui, "denial_time", u->denial_time);
    const cJSON *light = field(root, "light");
    if (light) {
        u->light_sun = (int)num(light, "sun", u->light_sun);
        u->light_moon = (int)num(light, "moon", u->light_moon);
        u->light_air_falloff = (int)num(light, "air_falloff", u->light_air_falloff);
        u->light_solid_falloff = (int)num(light, "solid_falloff", u->light_solid_falloff);
        u->light_minimum = (int)num(light, "minimum", u->light_minimum);
    }
    return true;
}

static bool read_trade(Reader *r, const cJSON *root)
{
    TradeDef *t = &content.trade;
    const cJSON *e;
    t->default_price = (int)num(root, "default_price", t->default_price > 0 ? t->default_price : 1);
    cJSON_ArrayForEach(e, field(root, "categories")) {
        const char *name = str(e, "name");
        if (!name) return fail(r, "a category has no \"name\"");
        int id = -1;
        for (int i = 0; i < t->category_count; i++)
            if (strcmp(t->categories[i].name, name) == 0) id = i;
        if (id < 0) {
            if (t->category_count >= MAX_CATEGORIES) return fail(r, "too many categories");
            id = t->category_count++;
        }
        copy_name(t->categories[id].name, CONTENT_NAME_LEN, name);
        copy_name(t->categories[id].label, CONTENT_NAME_LEN, str(e, "label") ? str(e, "label") : name);
    }
    if (field(root, "currencies")) t->currency_count = 0;
    cJSON_ArrayForEach(e, field(root, "currencies")) {
        if (t->currency_count >= MAX_CURRENCIES) return fail(r, "too many currencies");
        ItemId item = ITEM_NONE;
        int value = (int)num(e, "value", 0);
        if (!str(e, "item") || !item_ref(r, e, "item", &item)) return fail(r, "a currency needs an \"item\"");
        if (value < 1) return fail(r, "currency \"%s\" needs a \"value\" of 1 or more", str(e, "item"));
        /* Kept in order of value, smallest first. */
        int at = t->currency_count++;
        while (at > 0 && t->currencies[at - 1].value > value) {
            t->currencies[at] = t->currencies[at - 1];
            at--;
        }
        t->currencies[at].item = item;
        t->currencies[at].value = value;
    }
    const cJSON *crystals = field(root, "crystals");
    if (crystals) {
        t->start_crystals = (int)num(crystals, "start", 0);
        copy_name(t->crystal_icon, sizeof t->crystal_icon, str(crystals, "icon"));
    }
    return true;
}

/* ---- loading ----------------------------------------------------------- */

/* Parses a file from the data directory. A missing file is not an error: a
 * mod need only contain the files it changes. */
static cJSON *parse_file(Reader *r, const char *dir, const char *file)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, file);
    r->file = file;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = size >= 0 ? malloc((size_t)size + 1) : NULL;
    cJSON *root = NULL;
    if (text && fread(text, 1, (size_t)size, f) == (size_t)size) {
        text[size] = '\0';
        root = cJSON_Parse(text);
        if (!root) fail(r, "not valid JSON");
    } else {
        fail(r, "could not be read");
    }
    free(text);
    fclose(f);
    return root;
}

/* Ties up references that could not be settled while reading, then checks
 * for what the game cannot run without. */
static bool validate(Reader *r)
{
    const WorldGenDef *g = &content.worldgen;
    const RulesDef *u = &content.rules;
    r->file = "data";

    for (int i = 0; i < content.item_count; i++) {
        ItemDef *it = &content.items[i];
        it->category = -1;
        for (int c = 0; c < content.trade.category_count; c++)
            if (strcmp(content.trade.categories[c].name, it->category_name) == 0) it->category = c;
        if (it->category_name[0] && it->category < 0)
            return fail(r, "item \"%s\" is in category \"%s\", which does not exist", it->name,
                        it->category_name);
        /* Everything is for sale at a trade block except money itself, so an
         * item left out of the categories is a mistake, not a choice. */
        bool money = false;
        for (int c = 0; c < content.trade.currency_count; c++)
            money = money || content.trade.currencies[c].item == i;
        /* (Nor an item that turns into crystals the moment it is picked up.) */
        if (i > 0 && it->category < 0 && !money && it->crystals == 0 && content.trade.category_count > 0)
            return fail(r, "item \"%s\" has no \"category\", so it could not be bought or sold; "
                           "give it one from trade.json", it->name);
        it->plants = it->plants_name[0] ? tree_id(it->plants_name) : -1;
        if (it->plants_name[0] && it->plants < 0)
            return fail(r, "item \"%s\" plants a tree called \"%s\", which does not exist", it->name,
                        it->plants_name);
    }
    for (int i = 0; i < content.tile_count; i++) {
        const TileDef *t = &content.tiles[i];
        if (t->layer == LAYER_BACKGROUND && t->solid)
            return fail(r, "tile \"%s\" is placed in the background, so it cannot be solid", t->name);
        int slots = t->storage_columns * t->storage_rows;
        if (slots == 0 || t->storage_shared) continue;
        /* What is in it has to survive being picked up. */
        if (slots < 0 || slots > 16 || item_def(t->drop)->container_slots < slots)
            return fail(r, "tile \"%s\" stores items, so it must drop an item with at least as many "
                           "container slots (16 at most)", t->name);
    }
    for (int i = 0; i < content.recipe_count; i++) {
        bool found = false;
        for (int t = 0; t < content.tile_count && !found; t++)
            found = content.tiles[t].bench && strcmp(content.tiles[t].bench_kind, content.recipes[i].bench) == 0;
        if (!found)
            return fail(r, "a recipe is for a bench of kind \"%s\", which no tile is",
                        content.recipes[i].bench);
    }
    if (g->width < 64 || g->height < 64 || g->width > 65536 || g->height > 4096)
        return fail(r, "worldgen needs a sensible \"width\" and \"height\"");
    if (g->soil == TILE_AIR || g->rock == TILE_AIR || g->surface == TILE_AIR)
        return fail(r, "worldgen needs \"layers\" with a surface, soil and rock tile");
    if (g->portal_body == TILE_AIR || g->portal_base == TILE_AIR || g->portal_height < 2)
        return fail(r, "worldgen needs a spawn \"portal\" at least 2 tall");
    if (g->portal_top == TILE_AIR) content.worldgen.portal_top = g->portal_body;
    if (g->gap_min < 1 || g->gap_max < g->gap_min || g->spawn_max < g->spawn_min)
        return fail(r, "worldgen has a bad \"gap\" or spawn \"column\" range");
    if (u->tick_rate < 1 || u->day_ticks < 1 || u->night_ticks < 1 || u->chunk_width < 1)
        return fail(r, "rules need a tick rate, day and night length and chunk width");
    /* Light that lost nothing as it went would never stop spreading. */
    if (u->light_sun <= 0) content.rules.light_sun = 15;
    if (u->light_air_falloff < 1) content.rules.light_air_falloff = 1;
    if (u->light_solid_falloff < 1) content.rules.light_solid_falloff = 3;
    if (u->walk_time <= 0 || u->fall_time <= 0 || u->climb_time <= 0 || u->hop_time <= 0)
        return fail(r, "rules need positive player step times");
    return true;
}

bool content_load(const char *dir, char *error, size_t error_size)
{
    static const char *files[] = { "tiles.json", "items.json", "recipes.json", "trees.json",
                                   "worldgen.json", "rules.json", "trade.json" };
    enum { FILE_COUNT = sizeof files / sizeof files[0] };
    Reader r = { "", error, error_size, false };
    cJSON *roots[FILE_COUNT];
    if (error_size > 0) error[0] = '\0';
    if (content.tile_count == 0) content_reset();

    for (int i = 0; i < FILE_COUNT; i++) roots[i] = parse_file(&r, dir, files[i]);

    /* Names first, across every file, then the definitions that use them. */
    for (int i = 0; i < FILE_COUNT && !r.failed; i++) {
        r.file = files[i];
        if (roots[i]) register_names(&r, roots[i]);
    }
    static bool (*const readers[FILE_COUNT])(Reader *, const cJSON *) = {
        read_tiles, read_items, read_recipes, read_trees, read_worldgen, read_rules, read_trade,
    };
    for (int i = 0; i < FILE_COUNT && !r.failed; i++) {
        r.file = files[i];
        if (roots[i]) readers[i](&r, roots[i]);
    }
    for (int i = 0; i < FILE_COUNT; i++) cJSON_Delete(roots[i]);
    if (!r.failed) validate(&r);
    return !r.failed;
}
