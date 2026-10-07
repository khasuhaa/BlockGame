#include "content.h"

#include "../third_party/cjson/cJSON.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Content content;

/* ---- lookup ------------------------------------------------------------ */

int tone_pack(int body, int tip)
{
    if (body < 0 || body >= TONE_COLORS) body = 0;
    if (tip < 0 || tip >= TONE_COLORS) tip = 0;
    return 1 + body * TONE_COLORS + tip;
}

void tone_unpack(int tones, int *body, int *tip)
{
    if (tones < 1 || tones > TONE_MAX) tones = 1;
    *body = (tones - 1) / TONE_COLORS;
    *tip = (tones - 1) % TONE_COLORS;
}

int tone_nearest(int red, int green, int blue)
{
    int step = 255 / (TONE_LEVELS - 1), c[3] = { red, green, blue }, out = 0;
    for (int k = 0; k < 3; k++) {
        int level = (c[k] + step / 2) / step;
        out = out * TONE_LEVELS + (level < 0 ? 0 : level >= TONE_LEVELS ? TONE_LEVELS - 1 : level);
    }
    return out;
}

void tone_rgb(int color, unsigned char rgb[3])
{
    int step = 255 / (TONE_LEVELS - 1);
    if (color < 0 || color >= TONE_COLORS) color = 0;
    rgb[0] = (unsigned char)(color / (TONE_LEVELS * TONE_LEVELS) * step);
    rgb[1] = (unsigned char)(color / TONE_LEVELS % TONE_LEVELS * step);
    rgb[2] = (unsigned char)(color % TONE_LEVELS * step);
}

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

int mob_id(const char *name)
{
    for (int i = 0; i < content.mob_count; i++)
        if (strcmp(content.mobs[i].name, name) == 0) return i;
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
    content.tiles[0].hatches = -1;
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
    out->full = flag(obj, "full", false);
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
        t->trade_free = flag(e, "trade_free", false);
        t->removable = flag(e, "removable", false);
        t->climbable = flag(e, "climbable", false);
        t->mine_time = (float)num(e, "mine_time", 0.35);
        t->light = (int)num(e, "light", 0);
        t->height = (int)num(e, "height", 1);
        if (t->height < 1 || t->height > 8) return fail(r, "tile \"%s\": height must be 1 to 8", t->name);
        copy_name(t->texture_top, sizeof t->texture_top, str(e, "texture_top"));
        copy_name(t->texture_base, sizeof t->texture_base, str(e, "texture_base"));
        copy_name(t->texture_joined, sizeof t->texture_joined, str(e, "texture_joined"));
        t->join_count = 0;
        const cJSON *joins = field(e, "joins"), *join;
        cJSON_ArrayForEach(join, joins) {
            TileType other = cJSON_IsString(join) ? tile_id(join->valuestring) : -1;
            if (other < 0 || t->join_count >= MAX_TILE_JOINS)
                return fail(r, "tile \"%s\": \"joins\" lists at most %d blocks that exist", t->name, MAX_TILE_JOINS);
            t->joins[t->join_count++] = other;
        }
        const cJSON *alias;
        t->alias_count = 0;
        cJSON_ArrayForEach(alias, field(e, "aliases")) {
            if (t->alias_count < MAX_TILE_ALIASES && cJSON_IsString(alias))
                copy_name(t->aliases[t->alias_count++], CONTENT_NAME_LEN, alias->valuestring);
        }
        t->light_needs_fuel = flag(e, "light_needs_fuel", false);
        t->ore = flag(e, "ore", false);
        t->drop_count = (int)num(e, "drop_count", 1);
        t->needs_water = flag(e, "needs_water", false);
        t->breaks_above = flag(e, "breaks_above", false);
        t->breaks_below = flag(e, "breaks_below", false);
        t->passable = flag(e, "passable", false);
        const char *mimics = str(e, "mimics");
        t->mimics = !mimics ? MIMIC_NONE : strcmp(mimics, "upright") == 0 ? MIMIC_UPRIGHT : strcmp(mimics, "flat") == 0 ? MIMIC_FLAT : -1;
        if (t->mimics < 0) return fail(r, "tile \"%s\": \"mimics\" is \"upright\" or \"flat\"", t->name);
        const cJSON *beam = field(e, "beam");
        const char *way = str(beam, "direction");
        t->beam_dir = !beam ? 0 : way && strcmp(way, "down") == 0 ? 1 : -1;
        t->beam_light = (int)num(beam, "light", 0);
        if (beam && (!way || (strcmp(way, "up") != 0 && strcmp(way, "down") != 0) || t->beam_light < 1))
            return fail(r, "tile \"%s\": a \"beam\" needs a \"direction\" of \"up\" or \"down\" and a \"light\"", t->name);
        t->steps = flag(e, "steps", false);
        t->attaches = flag(e, "attaches", t->steps);
        t->supports = flag(e, "supports", false);
        t->floats = flag(e, "floats", false);
        t->shows_contents = flag(e, "shows_contents", false);
        t->fertile = (float)num(e, "fertile", 0.0);
        t->see_through = flag(e, "see_through", false);
        t->burns = flag(e, "burns", false);
        t->afloat = flag(e, "afloat", false);
        t->written = flag(e, "written", false);
        t->pair = t->pair_of = TILE_AIR;
        if (!tile_ref(r, e, "pair", &t->pair)) return false;
        t->sleep_speed = (float)num(field(e, "bed"), "speed", 0.0);
        t->sleep_over = flag(field(e, "bed"), "over", false);
        const cJSON *gen = field(e, "generator");
        const char *needs = str(gen, "needs");
        t->gen_power = (int)num(gen, "power", 0);
        t->gen_needs = !needs ? POWER_NEEDS_NOTHING : strcmp(needs, "sun") == 0 ? POWER_NEEDS_SUN : strcmp(needs, "fuel") == 0 ? POWER_NEEDS_FUEL : -1;
        if (t->gen_needs < 0) return fail(r, "tile \"%s\": a generator \"needs\" \"sun\" or \"fuel\"", t->name);
        t->battery = (int)num(e, "battery", 0);
        t->wire = flag(e, "wire", false);
        t->electric = (int)num(field(e, "electric"), "per_second", 0);
        t->motor_power = (int)num(field(e, "motor"), "power", 0);
        t->shaft = flag(e, "shaft", false);
        if (t->gen_power < 0 || t->battery < 0 || t->electric < 0 || t->motor_power < 0)
            return fail(r, "tile \"%s\": amounts of electricity cannot be less than nothing", t->name);
        t->hangs = flag(e, "hangs", false);
        const cJSON *mount = field(e, "texture_mount");
        copy_name(t->texture_mount[0], CONTENT_PATH_LEN, str(mount, "floor"));
        copy_name(t->texture_mount[1], CONTENT_PATH_LEN, str(mount, "ceiling"));
        copy_name(t->texture_mount[2], CONTENT_PATH_LEN, str(mount, "side"));
        if (t->steps && t->solid) return fail(r, "tile \"%s\": steps are walked through, so they cannot be solid", t->name);
        t->drop_meter = (int)num(e, "drop_meter", 0);
        t->decays_into = TILE_AIR;
        if (!tile_ref(r, e, "decays_into", &t->decays_into)) return false;
        t->vanishes = flag(e, "vanishes", false);
        t->drop_chance = (float)num(e, "drop_chance", 1.0);
        t->loot_count = 0;
        const cJSON *loot = field(e, "loot"), *entry, *choice;
        cJSON_ArrayForEach(entry, loot) {
            if (t->loot_count >= MAX_TILE_LOOT) return fail(r, "tile \"%s\" has more than %d kinds of loot", t->name, MAX_TILE_LOOT);
            LootDef *l = &t->loot[t->loot_count++];
            memset(l, 0, sizeof *l);
            l->min = (int)num(entry, "min", 1);
            l->max = (int)num(entry, "max", l->min);
            if (str(entry, "item") && !item_ref(r, entry, "item", &l->item)) return false;
            cJSON_ArrayForEach(choice, field(entry, "one_of")) {
                if (l->choice_count >= MAX_LOOT_CHOICES || !str(choice, "item") ||
                    !item_ref(r, choice, "item", &l->choices[l->choice_count].item))
                    return fail(r, "tile \"%s\": \"one_of\" lists at most %d items that exist", t->name, MAX_LOOT_CHOICES);
                l->choices[l->choice_count++].weight = (int)num(choice, "weight", 1);
            }
            if ((l->item == ITEM_NONE) == (l->choice_count == 0) || l->min < 0 || l->max < l->min)
                return fail(r, "tile \"%s\": loot is an \"item\" or a \"one_of\", with \"min\" no more than \"max\"", t->name);
        }
        /* Creatures are read later; the name is matched to one once they are. */
        copy_name(t->hatches_name, sizeof t->hatches_name, str(e, "hatches"));
        t->hatch_owned = flag(e, "hatch_owned", false);
        t->grows_into = TILE_AIR;
        if (!tile_ref(r, e, "grows_into", &t->grows_into)) return false;
        const cJSON *ground;
        t->soil_count = 0;
        cJSON_ArrayForEach(ground, field(e, "soil")) {
            TileType tile = cJSON_IsString(ground) ? tile_id(ground->valuestring) : -1;
            if (tile < 0) return fail(r, "tile \"%s\": \"soil\" names a tile that does not exist", t->name);
            if (t->soil_count < MAX_TILE_SOILS) t->soil[t->soil_count++] = tile;
        }
        t->grows_top = t->topped_by = t->cut_to = TILE_AIR;
        if (!tile_ref(r, e, "grows_top", &t->grows_top) || !tile_ref(r, e, "topped_by", &t->topped_by) ||
            !tile_ref(r, e, "cut_to", &t->cut_to))
            return false;
        /* "tones": it is two-tone; the pictures drawn in its two colours, and the colours wild ones come in. */
        const cJSON *tones = field(e, "tones"), *tone;
        t->two_tone = cJSON_IsObject(tones);
        t->tone_color_count = 0;
        t->tone_textures[0][0] = t->tone_textures[1][0] = '\0';
        int tone_pictures = 0;
        cJSON_ArrayForEach(tone, field(tones, "textures"))
            if (cJSON_IsString(tone) && tone_pictures < 2) copy_name(t->tone_textures[tone_pictures++], CONTENT_PATH_LEN, tone->valuestring);
        cJSON_ArrayForEach(tone, field(tones, "natural")) {
            if (cJSON_GetArraySize(tone) != 3 || t->tone_color_count >= MAX_TONE_COLORS)
                return fail(r, "tile \"%s\": \"natural\" lists at most %d colours, each [red, green, blue]", t->name, MAX_TONE_COLORS);
            for (int k = 0; k < 3; k++)
                t->tone_colors[t->tone_color_count][k] = (unsigned char)cJSON_GetArrayItem(tone, k)->valuedouble;
            t->tone_color_count++;
        }
        t->shade = (int)num(e, "shade", 0);
        t->lit_count = 0;
        const cJSON *lit = field(e, "texture_lit"), *frame;
        cJSON_ArrayForEach(frame, lit) {
            if (t->lit_count >= MAX_TILE_TEXTURES || !cJSON_IsString(frame))
                return fail(r, "tile \"%s\": \"texture_lit\" is a list of at most %d files", t->name, MAX_TILE_TEXTURES);
            copy_name(t->textures_lit[t->lit_count++], CONTENT_PATH_LEN, frame->valuestring);
        }
        t->lit_fps = (float)num(e, "lit_fps", 6.0);
        t->front = flag(e, "front", false);
        t->pulse = (float)num(e, "pulse", 0.0);
        t->drop = ITEM_NONE;
        if (!item_ref(r, e, "drop", &t->drop)) return false;
        /* "drops" lists several things: the first is the main drop. */
        const cJSON *each;
        int listed = 0;
        t->extra_drop_count = 0;
        cJSON_ArrayForEach(each, field(e, "drops")) {
            RecipeItem one;
            if (!stack_ref(r, each, &one)) return false;
            if (listed++ == 0) {
                t->drop = one.id;
                t->drop_count = one.count;
            } else if (t->extra_drop_count < MAX_EXTRA_DROPS) {
                /* (Anything after the first may be left only some of the time.) */
                t->extra_chance[t->extra_drop_count] = (float)num(each, "chance", 1.0);
                t->extra_drops[t->extra_drop_count++] = one;
            } else {
                return fail(r, "tile \"%s\" drops too many kinds of thing", t->name);
            }
        }
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
        t->upgrade_time = 0.0f;
        const cJSON *upgrade = field(e, "upgrade"), *cost;
        if (upgrade) {
            if (!str(upgrade, "to") || !tile_ref(r, upgrade, "to", &t->upgrade_to))
                return fail(r, "tile \"%s\": an upgrade needs a \"to\" tile", t->name);
            cJSON_ArrayForEach(cost, field(upgrade, "cost")) {
                if (t->upgrade_cost_count >= RECIPE_MAX_INPUTS)
                    return fail(r, "tile \"%s\": an upgrade costs too many kinds of item", t->name);
                if (!stack_ref(r, cost, &t->upgrade_cost[t->upgrade_cost_count++])) return false;
            }
            t->upgrade_time = (float)num(upgrade, "time", 0.0);
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
        /* "texture_seasons": a list of pictures for each season in turn; an empty one for no change. */
        const cJSON *season;
        int which = 0;
        memset(t->season_texture_count, 0, sizeof t->season_texture_count);
        cJSON_ArrayForEach(season, field(e, "texture_seasons")) {
            if (which >= MAX_SEASONS) return fail(r, "tile \"%s\": \"texture_seasons\" lists at most %d seasons", t->name, MAX_SEASONS);
            cJSON_ArrayForEach(one, season)
                if (t->season_texture_count[which] < MAX_TILE_TEXTURES && cJSON_IsString(one))
                    copy_name(t->season_textures[which][t->season_texture_count[which]++], CONTENT_PATH_LEN, one->valuestring);
            which++;
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
        /* "food" is how much hunger it fills, or says that and what it does for health. */
        const cJSON *food = field(e, "food");
        it->food = cJSON_IsNumber(food) ? (float)food->valuedouble : (float)num(food, "hunger", 0.0);
        it->food_health = (float)num(food, "health", -1.0);
        it->food_energy = (float)num(food, "energy", 0.0);
        it->energy_over = flag(food, "energy_over", false);
        it->food_oxygen = (float)num(food, "oxygen", 0.0);
        if (it->food < 0.0f || it->food > 1.0f)
            return fail(r, "item \"%s\": \"food\" is a share of the hunger bar, 0 to 1", it->name);
        /* The category is matched to the trade file once that has been read. */
        copy_name(it->category_name, sizeof it->category_name, str(e, "category"));
        it->price = (int)num(e, "price", 0);
        /* "tool": what kind it is, and how much faster it makes mining. */
        const cJSON *tool = field(e, "tool");
        if (!tool_class_ref(r, str(tool, "class"), &it->tool_class)) return false;
        it->tool_speed = (float)num(tool, "speed", 1.0);
        it->ore_multiplier = (int)num(tool, "ore_multiplier", 1);
        it->crystals = (int)num(e, "crystals", 0);
        it->becomes_player = flag(e, "becomes_player", false);
        if (tool && (it->tool_class < 0 || it->tool_speed <= 0.0f))
            return fail(r, "item \"%s\": a tool needs a \"class\" and a \"speed\" above 0", it->name);
        /* "holds_water": how many eighths; "durability": how many uses. */
        it->holds_water = cJSON_IsNumber(field(e, "holds_water"));
        it->meter_max = (int)num(e, it->holds_water ? "holds_water" : "durability", 0);
        it->meter_start = it->holds_water ? 0 : it->meter_max;
        if (it->meter_max < 0 || it->meter_max > 250)
            return fail(r, "item \"%s\": a meter reads at most 250", it->name);
        it->armor = (float)num(e, "armor", 0.0);
        if (it->armor < 0.0f || it->armor > 1.0f) return fail(r, "item \"%s\": \"armor\" is a share of an attack, 0 to 1", it->name);
        it->damage = (int)num(e, "damage", 0);
        it->range = (int)num(e, "range", 0);
        it->poison = (int)num(field(e, "poison"), "damage", 0);
        it->poison_every = (int)num(field(e, "poison"), "every", 2);
        if (it->poison_every < 1) it->poison_every = 1;
        it->tinted = flag(e, "tinted", false);
        if (it->tinted) {
            it->holds_water = false;
            it->meter_max = MAX_TINTS;
            it->meter_start = 0;
        }
        /* "tones": it is two-tone; the pictures drawn in its two colours, and the colour a new one is. */
        const cJSON *tones = field(e, "tones"), *tone;
        it->two_tone = cJSON_IsObject(tones);
        it->tone_textures[0][0] = it->tone_textures[1][0] = '\0';
        if (it->two_tone) {
            int pictures = 0, plain[3] = { 255, 255, 255 }, k = 0;
            cJSON_ArrayForEach(tone, field(tones, "textures"))
                if (cJSON_IsString(tone) && pictures < 2) copy_name(it->tone_textures[pictures++], CONTENT_PATH_LEN, tone->valuestring);
            cJSON_ArrayForEach(tone, field(tones, "default"))
                if (k < 3) plain[k++] = (int)tone->valuedouble;
            int color = tone_nearest(plain[0], plain[1], plain[2]);
            it->holds_water = it->tinted = false;
            it->meter_max = TONE_MAX;
            it->meter_start = tone_pack(color, color);
        }
        it->crosses_with = it->crosses_into = TILE_AIR;
        if (field(e, "crosses") && (!tile_ref(r, field(e, "crosses"), "with", &it->crosses_with) ||
                                    !tile_ref(r, field(e, "crosses"), "into", &it->crosses_into)))
            return false;
        if ((it->crosses_with == TILE_AIR) != (it->crosses_into == TILE_AIR))
            return fail(r, "item \"%s\": \"crosses\" needs both \"with\" and \"into\"", it->name);
        it->shears = flag(e, "shears", false);
        it->milks = flag(e, "milks", false);
        it->ignites = flag(e, "ignites", false);
        it->written = flag(e, "written", false);
        if (it->written) {
            it->meter_max = MAX_SIGN_TEXTS - 1;
            it->meter_start = 0;
        }
        it->cages = flag(e, "cages", false);
        it->releases = -1;
        it->paints = flag(e, "paints", false);
        it->strips = flag(e, "strips", false);
        it->uses = (int)num(e, "uses", 0);
        if ((it->paints || it->strips) && (it->uses < 1 || it->uses > 120))
            return fail(r, "item \"%s\": something that paints or strips needs \"uses\", 1 to 120", it->name);
        if (it->paints) it->meter_max = MAX_TINTS + PAINT_USED * it->uses;
        if (it->strips) {
            it->meter_max = it->uses;
            it->meter_start = it->uses;
        }
        it->places = TILE_AIR;
        it->sized = false;
        /* "places" is one block, or a list of them: the block for each size of a sized thing. */
        const cJSON *places = field(e, "places"), *size;
        if (cJSON_IsArray(places)) {
            int sizes = 0;
            cJSON_ArrayForEach(size, places) {
                TileType t = cJSON_IsString(size) ? tile_id(size->valuestring) : -1;
                if (t <= 0 || sizes >= MAX_ITEM_SIZES)
                    return fail(r, "item \"%s\": \"places\" lists at most %d blocks that exist", it->name, MAX_ITEM_SIZES);
                it->places_sized[sizes++] = t;
            }
            if (sizes == 0) return fail(r, "item \"%s\": \"places\" is empty", it->name);
            it->sized = true;
            it->holds_water = false;
            it->meter_max = sizes;
            it->meter_start = 1;
            it->places = it->places_sized[0];
        } else if (!tile_ref(r, e, "places", &it->places)) return false;
        it->places_on_block = TILE_AIR;
        if (!tile_ref(r, e, "places_on_block", &it->places_on_block)) return false;
        it->sows = TILE_AIR;
        if (!tile_ref(r, e, "sows", &it->sows)) return false;
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
        /* "output" is so many of an item - or of one of several, by chance ("one_of", each with a "weight"). */
        const cJSON *made = field(e, "output"), *choice;
        cJSON_ArrayForEach(choice, field(made, "one_of")) {
            if (recipe.one_of_count >= MAX_RECIPE_CHOICES || !str(choice, "item") ||
                !item_ref(r, choice, "item", &recipe.one_of[recipe.one_of_count].item))
                return fail(r, "a recipe's \"one_of\" lists at most %d items that exist", MAX_RECIPE_CHOICES);
            recipe.one_of[recipe.one_of_count++].weight = (int)num(choice, "weight", 1);
        }
        if (recipe.one_of_count > 0) {
            recipe.output = (RecipeItem){ recipe.one_of[0].item, (int)num(made, "count", 1), false };
        } else if (!cJSON_IsObject(made) || !stack_ref(r, made, &recipe.output)) {
            return fail(r, "a recipe needs an \"output\"");
        }
        cJSON_ArrayForEach(in, field(e, "inputs")) {
            if (recipe.input_count >= RECIPE_MAX_INPUTS)
                return fail(r, "a recipe has more than %d inputs", RECIPE_MAX_INPUTS);
            if (!stack_ref(r, in, &recipe.inputs[recipe.input_count++])) return false;
        }

        /* "random_inputs": so many in all of these kinds, the mix settled by each world. */
        const cJSON *random = field(e, "random_inputs"), *kind;
        recipe.random_total = (int)num(random, "count", 0);
        cJSON_ArrayForEach(kind, field(random, "of")) {
            ItemId item = cJSON_IsString(kind) ? item_id(kind->valuestring) : -1;
            if (item <= 0 || recipe.random_item_count >= RECIPE_MAX_INPUTS)
                return fail(r, "a recipe's \"random_inputs\" lists at most %d items that exist", RECIPE_MAX_INPUTS);
            recipe.random_items[recipe.random_item_count++] = item;
        }
        if (random && (recipe.input_count > 0 || recipe.random_item_count == 0 || recipe.random_total < 1))
            return fail(r, "a recipe has \"inputs\" or \"random_inputs\" (some \"of\" and a \"count\"), not both");

        /* "bench" is one kind of bench, or a list of kinds that all offer the recipe. */
        recipe.level = (int)num(e, "level", 1);
        recipe.crystals = (int)num(e, "crystals", 0);
        recipe.time = (float)num(e, "time", 0.0);
        /* "tint": what it makes comes out that colour. */
        const cJSON *tint = field(e, "tint");
        if (cJSON_GetArraySize(tint) == 3) {
            if (content.tint_count >= MAX_TINTS) return fail(r, "too many tints");
            for (int c = 0; c < 3; c++) content.tints[content.tint_count][c] = (unsigned char)cJSON_GetArrayItem(tint, c)->valuedouble;
            recipe.tint = ++content.tint_count;
        }
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
        /* Leaves are optional: kelp is all stem. */
        if (t->log == TILE_AIR) return fail(r, "tree \"%s\" needs a \"log\"", name);
        t->needs_water = flag(e, "needs_water", false);
        t->fruit_ticks = (int)num(e, "fruit_ticks", 1800);
        t->max_height = (int)num(e, "max_height", 6);
        t->weight = (int)num(e, "weight", 1);
        t->mirror_by_column = flag(e, "mirror_by_column", false);
        t->connected = flag(e, "connected", false);
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
            TreeRow *tr = &t->rows[t->row_count++];
            *tr = (TreeRow){ .offset = (int)num(row, "offset", 0), .left = (int)num(row, "left", 0),
                             .right = (int)num(row, "right", 0), .center = flag(row, "center", false) };
            const cJSON *cell;
            cJSON_ArrayForEach(cell, field(row, "cells")) {
                if (tr->cell_count < MAX_ROW_CELLS && cJSON_IsNumber(cell))
                    tr->cells[tr->cell_count++] = (int)cell->valuedouble;
            }
        }
        content.tiles[t->log].log_of = id;
        if (t->leaves != TILE_AIR) content.tiles[t->leaves].leaves_of = id;
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

    if (cJSON_IsNumber(field(root, "sea_level"))) {
        g->has_sea = true;
        g->sea_level = (int)num(root, "sea_level", 0);
    }
    const cJSON *climate = field(root, "climate");
    if (climate) {
        g->heat_wavelength = (int)num(climate, "heat_wavelength", 300);
        g->dryness_wavelength = (int)num(climate, "dryness_wavelength", 220);
        g->climate_blend = (float)num(climate, "blend", 0.2);
        g->climate_spawn_clearance = (int)num(climate, "spawn_clearance", 40);
    }
    cJSON_ArrayForEach(e, field(root, "biomes")) {
        const char *name = str(e, "name");
        if (!name) return fail(r, "a biome has no \"name\"");
        int id = -1;
        for (int i = 0; i < content.biome_count; i++)
            if (strcmp(content.biomes[i].name, name) == 0) id = i;
        if (id < 0) {
            if (content.biome_count >= MAX_BIOMES) return fail(r, "too many biomes");
            id = content.biome_count++;
        }
        BiomeDef *b = &content.biomes[id];
        memset(b, 0, sizeof *b);
        copy_name(b->name, sizeof b->name, name);
        copy_name(b->label, sizeof b->label, str(e, "label") ? str(e, "label") : name);
        if (!tile_ref(r, e, "surface", &b->surface) || !tile_ref(r, e, "soil", &b->soil)) return false;
        if (b->surface == TILE_AIR || b->soil == TILE_AIR)
            return fail(r, "biome \"%s\" needs a \"surface\" and a \"soil\"", name);
        b->soil_depth = (float)num(e, "soil_depth", 0.0);
        b->below_sea = flag(e, "below_sea", false);
        b->near_sea = (int)num(e, "near_sea", 0);
        b->buried = TILE_AIR;
        if (field(e, "buried") && !tile_ref(r, field(e, "buried"), "tile", &b->buried)) return false;
        b->buried_one_in = (int)num(field(e, "buried"), "one_in", 0);
        b->under = TILE_AIR;
        if (field(e, "under") && !tile_ref(r, field(e, "under"), "tile", &b->under)) return false;
        b->under_depth = (int)num(field(e, "under"), "depth", 0);
        b->seam = TILE_AIR;
        if (str(field(e, "under"), "seam") && !tile_ref(r, field(e, "under"), "seam", &b->seam)) return false;
        b->seam_share = (float)num(field(e, "under"), "seam_share", 0.5);
        b->tree_one_in = (int)num(e, "tree_one_in", 0);
        b->tree_spacing = (int)num(e, "tree_spacing", 0);
        b->heat_min = b->dry_min = -2.0f;
        b->heat_max = b->dry_max = 2.0f;
        const cJSON *range;
        if (cJSON_GetArraySize(range = field(e, "heat")) == 2) {
            b->has_climate = true;
            b->heat_min = (float)cJSON_GetArrayItem(range, 0)->valuedouble;
            b->heat_max = (float)cJSON_GetArrayItem(range, 1)->valuedouble;
        }
        if (cJSON_GetArraySize(range = field(e, "dryness")) == 2) {
            b->has_climate = true;
            b->dry_min = (float)cJSON_GetArrayItem(range, 0)->valuedouble;
            b->dry_max = (float)cJSON_GetArrayItem(range, 1)->valuedouble;
        }
        const cJSON *one;
        cJSON_ArrayForEach(one, field(e, "trees")) {
            int tree = str(one, "tree") ? tree_id(str(one, "tree")) : -1;
            if (tree < 0) return fail(r, "biome \"%s\" names a tree that does not exist", name);
            if (b->tree_count >= MAX_BIOME_TREES) return fail(r, "biome \"%s\" has too many trees", name);
            b->trees[b->tree_count].tree = tree;
            b->trees[b->tree_count++].weight = (int)num(one, "weight", 1);
        }
        cJSON_ArrayForEach(one, field(e, "plants")) {
            TileType tile = TILE_AIR;
            if (!tile_ref(r, one, "tile", &tile) || tile == TILE_AIR)
                return fail(r, "biome \"%s\": a plant needs a \"tile\"", name);
            if (b->plant_count >= MAX_BIOME_PLANTS) return fail(r, "biome \"%s\" has too many plants", name);
            b->plants[b->plant_count].tile = tile;
            b->plants[b->plant_count++].one_in = (int)num(one, "one_in", 20);
        }
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
        pair(e, "hang", &d->hang_min, &d->hang_max);
        if (d->hang_min < 0 || d->hang_max < d->hang_min) return fail(r, "cave decoration \"%s\": \"hang\" is a range of lengths", name);
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
        bool scar = str(e, "shape") && strcmp(str(e, "shape"), "scar") == 0; /* (its size is a length, with no such limit) */
        if (v->min_size < 1 || v->max_size < v->min_size || (v->max_size > MAX_VEIN_SIZE && !scar))
            return fail(r, "vein \"%s\": \"size\" must be within 1 to %d", name, MAX_VEIN_SIZE);
        v->max_depth = 1000;
        pair(e, "depth", &v->min_depth, &v->max_depth);
        v->per_100_columns = (float)num(e, "per_100_columns", 1.0);
        v->buried = flag(e, "buried", false);
        const char *shape = str(e, "shape");
        v->shape = shape && strcmp(shape, "streak") == 0 ? VEIN_STREAK : shape && strcmp(shape, "scar") == 0 ? VEIN_SCAR : VEIN_BLOB;
        if (shape && v->shape == VEIN_BLOB && strcmp(shape, "blob") != 0)
            return fail(r, "vein \"%s\": \"shape\" is \"blob\", \"streak\" or \"scar\"", name);
        v->min_thick = v->max_thick = 3;
        pair(e, "thickness", &v->min_thick, &v->max_thick);
        if (v->min_thick < 1 || v->max_thick < v->min_thick)
            return fail(r, "vein \"%s\": \"thickness\" is a range of 1 or more", name);
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
        u->max_tick_rate = (int)num(time, "max_tick_rate", u->max_tick_rate);
        u->day_ticks = (int)num(time, "day_ticks", u->day_ticks);
        u->night_ticks = (int)num(time, "night_ticks", u->night_ticks);
        u->twilight_ticks = (int)num(time, "twilight_ticks", u->twilight_ticks);
        u->chunk_width = (int)num(time, "chunk_width", u->chunk_width);
        u->random_events_per_chunk =
            (int)num(time, "random_events_per_chunk", u->random_events_per_chunk);
        u->random_event_base = (int)num(time, "random_event_base", u->random_event_base);
        u->max_random_events = (int)num(time, "max_random_events", u->max_random_events);
        u->season_days = (int)num(time, "season_days", u->season_days);
        const cJSON *season;
        if (field(time, "seasons")) u->season_count = 0;
        cJSON_ArrayForEach(season, field(time, "seasons")) {
            if (u->season_count >= MAX_SEASONS || !str(season, "name"))
                return fail(r, "\"seasons\" lists at most %d, each with a \"name\"", MAX_SEASONS);
            copy_name(u->seasons[u->season_count].label, CONTENT_NAME_LEN, str(season, "name"));
            u->seasons[u->season_count].snows = flag(season, "snows", false);
            u->seasons[u->season_count].freezes = flag(season, "freezes", false);
            u->seasons[u->season_count++].thaws = flag(season, "thaws", false);
        }
    }
    const cJSON *weather = field(root, "weather");
    if (weather) {
        u->weather_ticks = (int)num(weather, "period_ticks", u->weather_ticks);
        u->rain_chance = (float)num(weather, "rain_chance", u->rain_chance);
        u->snow_max = (int)num(weather, "snow_layers", u->snow_max);
        u->snow_line = (int)num(weather, "snow_line", u->snow_line);
        u->snow_chance = (float)num(weather, "snow_chance", u->snow_chance);
        u->melt_chance = (float)num(weather, "melt_chance", u->melt_chance);
        if (str(weather, "ice")) copy_name(u->ice_name, sizeof u->ice_name, str(weather, "ice"));
        u->freeze_chance = (float)num(weather, "freeze_chance", u->freeze_chance);
        u->thaw_chance = (float)num(weather, "thaw_chance", u->thaw_chance);
    }
    if (field(root, "building")) u->wall_span = (int)num(field(root, "building"), "wall_span", u->wall_span);
    if (field(root, "power")) u->lift_time = (float)num(field(root, "power"), "lift_time", u->lift_time);
    if (field(root, "building")) u->burn_time = (float)num(field(root, "building"), "burn_time", u->burn_time);
    if (field(root, "water")) u->boat_speed = (float)num(field(root, "water"), "boat_speed", u->boat_speed);
    if (field(root, "vitals")) u->energy_over = (float)num(field(root, "vitals"), "energy_over", u->energy_over);
    if (field(root, "crafting")) u->rush_seconds = (float)num(field(root, "crafting"), "rush_seconds_per_crystal", u->rush_seconds);
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
        u->drop_lifetime = (float)num(drops, "lifetime", u->drop_lifetime);
        u->drop_float_speed = (float)num(drops, "float_speed", u->drop_float_speed);
        u->drop_collect_time = (float)num(drops, "collect_time", u->drop_collect_time);
    }
    if (ui) u->denial_time = (float)num(ui, "denial_time", u->denial_time);
    if (field(root, "water")) {
        u->water_flow = (int)num(field(root, "water"), "flow", u->water_flow);
        u->oxygen_ticks = (int)num(field(root, "water"), "oxygen_ticks", u->oxygen_ticks);
        u->oxygen_refill_ticks = (int)num(field(root, "water"), "oxygen_refill_ticks", u->oxygen_refill_ticks);
        u->drown_ticks = (int)num(field(root, "water"), "drown_ticks", u->drown_ticks);
        u->swim_time = (float)num(field(root, "water"), "swim_time", u->swim_time);
    }
    const cJSON *water = field(field(root, "water"), "color");
    for (int i = 0; i < 4 && cJSON_GetArraySize(water) == 4; i++)
        u->water_color[i] = (int)cJSON_GetArrayItem(water, i)->valuedouble;
    const cJSON *fog = field(root, "fog");
    if (fog) {
        u->fog_enabled = flag(fog, "enabled", u->fog_enabled);
        u->fog_radius = (int)num(fog, "radius", u->fog_radius);
        u->fog_fade = (int)num(fog, "fade", u->fog_fade);
    }
    const cJSON *vitals = field(root, "vitals");
    if (vitals) {
        u->hunger_ticks = (int)num(vitals, "hunger_ticks", u->hunger_ticks);
        u->energy_ticks = (int)num(vitals, "energy_ticks", u->energy_ticks);
        u->sleep_ticks = (int)num(vitals, "sleep_ticks", u->sleep_ticks);
        u->health_regen_ticks = (int)num(vitals, "health_regen_ticks", u->health_regen_ticks);
        u->starve_ticks = (int)num(vitals, "starve_ticks", u->starve_ticks);
        u->starve_floor = (float)num(vitals, "starve_floor", u->starve_floor);
        u->food_health = (float)num(vitals, "food_health", u->food_health);
        u->saturation_ticks = (int)num(vitals, "saturation_ticks", u->saturation_ticks);
        u->full_margin = (float)num(vitals, "full_margin", u->full_margin);
        u->rest_speed = (int)num(vitals, "rest_speed", u->rest_speed);
    }
    const cJSON *meditation = field(root, "meditation");
    if (meditation) {
        u->meditation_ticks = (int)num(meditation, "ticks", u->meditation_ticks);
        const cJSON *loot = field(meditation, "loot");
        if (loot) u->loot_count = 0;
        cJSON_ArrayForEach(e, loot) {
            if (u->loot_count >= MAX_LOOT) return fail(r, "meditation brings more than %d kinds of thing", MAX_LOOT);
            ItemId item = ITEM_NONE;
            if (!str(e, "item") || !item_ref(r, e, "item", &item))
                return fail(r, "something meditation brings needs an \"item\"");
            u->loot[u->loot_count].item = item;
            u->loot[u->loot_count].weight = (int)num(e, "weight", 1);
            if (u->loot[u->loot_count++].weight < 1) return fail(r, "what meditation brings needs a \"weight\" of 1 or more");
        }
    }
    if (field(root, "players")) u->max_players = (int)num(field(root, "players"), "max", u->max_players);
    const cJSON *death = field(root, "death");
    if (death) {
        u->death_bar = (float)num(death, "bar", u->death_bar);
        u->respawn_ticks = (int)num(death, "respawn_ticks", u->respawn_ticks);
        u->respawn_cost = (int)num(death, "respawn_cost", u->respawn_cost);
    }
    const cJSON *combat = field(root, "combat");
    if (combat) {
        u->hand_damage = (int)num(combat, "hand_damage", u->hand_damage);
        u->attack_time = (float)num(combat, "attack_time", u->attack_time);
        u->poison_damage = (float)num(combat, "poison_damage", u->poison_damage);
    }
    const cJSON *teleport = field(root, "teleport");
    if (teleport) {
        copy_name(u->teleport_bench, sizeof u->teleport_bench, str(teleport, "bench"));
        u->teleport_level = (int)num(teleport, "level", 1);
        u->teleport_cost = (int)num(teleport, "cost", 0);
        u->teleport_free = (int)num(teleport, "free_distance", 0);
        u->teleport_per = (int)num(teleport, "per_blocks", 0);
    }
    const cJSON *look = field(root, "view");
    if (look) {
        const cJSON *steps = field(look, "zoom");
        float start = (float)num(look, "start", 0.0);
        if (cJSON_GetArraySize(steps) > 0) {
            u->zoom_count = 0;
            for (int i = 0; i < cJSON_GetArraySize(steps) && i < MAX_ZOOM_LEVELS; i++) {
                float px = (float)cJSON_GetArrayItem(steps, i)->valuedouble;
                if (px < 1.0f) px = 1.0f; /* a block cannot be drawn smaller than a pixel */
                u->zoom[u->zoom_count++] = px;
            }
            if (u->zoom_start >= u->zoom_count) u->zoom_start = 0;
        }
        for (int i = 0; i < u->zoom_count; i++)
            if (u->zoom[i] == start) u->zoom_start = i;
        u->map_from = (float)num(look, "map_from", u->map_from);
        u->map_to = (float)num(look, "map_to", u->map_to);
    }
    const cJSON *light = field(root, "light");
    if (light) {
        u->light_sun = (int)num(light, "sun", u->light_sun);
        u->light_moon = (int)num(light, "moon", u->light_moon);
        u->light_air_falloff = (int)num(light, "air_falloff", u->light_air_falloff);
        u->light_solid_falloff = (int)num(light, "solid_falloff", u->light_solid_falloff);
        u->light_minimum = (int)num(light, "minimum", u->light_minimum);
        u->light_water_depth = (int)num(light, "water_depth_per_level", u->light_water_depth);
    }
    return true;
}

static bool read_mobs(Reader *r, const cJSON *root)
{
    const cJSON *e, *one;
    if (cJSON_IsNumber(field(root, "spawn_radius"))) content.mob_spawn_radius = (int)num(root, "spawn_radius", 50);
    if (cJSON_IsNumber(field(root, "cap"))) content.mob_cap = (int)num(root, "cap", 10);
    if (cJSON_IsNumber(field(root, "water_cap"))) content.mob_water_cap = (int)num(root, "water_cap", 10);
    cJSON_ArrayForEach(e, field(root, "mobs")) {
        const char *name = str(e, "name");
        if (!name) return fail(r, "a mob has no \"name\"");
        int id = mob_id(name);
        if (id < 0) {
            if (content.mob_count >= MAX_MOB_KINDS) return fail(r, "too many kinds of mob");
            id = content.mob_count++;
        }
        MobDef *m = &content.mobs[id];
        memset(m, 0, sizeof *m);
        copy_name(m->name, sizeof m->name, name);
        copy_name(m->label, sizeof m->label, str(e, "label") ? str(e, "label") : name);
        copy_name(m->texture, sizeof m->texture, str(e, "texture"));
        copy_name(m->texture_baby, sizeof m->texture_baby,
                  str(e, "texture_baby") ? str(e, "texture_baby") : str(e, "texture"));
        cJSON_ArrayForEach(one, field(e, "walk"))
            if (cJSON_IsString(one) && m->walk_count < MAX_MOB_FRAMES)
                copy_name(m->walk[m->walk_count++], CONTENT_PATH_LEN, one->valuestring);
        cJSON_ArrayForEach(one, field(e, "walk_baby"))
            if (cJSON_IsString(one) && m->walk_baby_count < MAX_MOB_FRAMES)
                copy_name(m->walk_baby[m->walk_baby_count++], CONTENT_PATH_LEN, one->valuestring);
        m->hop = (float)num(e, "hop", 0.0);
        m->step_ticks = (int)num(e, "step_ticks", 4);
        if (m->step_ticks < 1) m->step_ticks = 1;
        m->move_chance = (float)num(e, "move_chance", 0.2);
        m->turn_chance = (float)num(e, "turn_chance", 0.25);
        m->baby_ticks = (int)num(e, "baby_ticks", 600);
        m->spawn_chance = (float)num(field(e, "spawn"), "chance", 0.0);
        cJSON_ArrayForEach(one, field(field(e, "spawn"), "above")) {
            TileType t = cJSON_IsString(one) ? tile_id(one->valuestring) : -1;
            if (t < 0) return fail(r, "mob \"%s\": it spawns above a tile that does not exist", name);
            if (m->spawn_above_count < MAX_MOB_SPAWN_TILES) m->spawn_above[m->spawn_above_count++] = t;
        }
        m->cap = (int)num(e, "cap", 0);
        if (str(e, "caged") && !item_ref(r, e, "caged", &m->caged)) return false;
        m->health = (int)num(e, "health", 1);
        m->size_min = m->size_max = 1;
        pair(e, "size", &m->size_min, &m->size_max);
        if (m->size_min < 1 || m->size_max < m->size_min || m->size_max > MAX_ITEM_SIZES)
            return fail(r, "mob \"%s\": \"size\" is a range within 1 to %d", name, MAX_ITEM_SIZES);
        /* "swim": it lives in water. */
        const cJSON *swim = field(e, "swim");
        m->swims = cJSON_IsObject(swim);
        m->hunts = flag(swim, "hunts", false);
        m->sight = (int)num(swim, "sight", 12);
        m->spawn_water = flag(field(e, "spawn"), "water", false);
        /* "biome" is one biome, or a list of them. */
        const cJSON *where = field(field(e, "spawn"), "biome"), *place;
        for (int k = 0, n = cJSON_IsString(where) ? 1 : cJSON_GetArraySize(where); where && k < n; k++) {
            place = cJSON_IsString(where) ? where : cJSON_GetArrayItem(where, k);
            int found = -1;
            for (int i = 0; cJSON_IsString(place) && i < content.biome_count; i++)
                if (strcmp(content.biomes[i].name, place->valuestring) == 0) found = i;
            if (found < 0 || m->spawn_biome_count >= MAX_MOB_BIOMES)
                return fail(r, "mob \"%s\" spawns in at most %d biomes that exist", name, MAX_MOB_BIOMES);
            m->spawn_biomes[m->spawn_biome_count++] = found;
        }
        /* "sting": it poisons a player it gets to. */
        const cJSON *sting = field(e, "sting");
        m->sting_ticks = (int)num(sting, "ticks", 0);
        m->sting_rest = (int)num(sting, "rest_ticks", 60);
        m->sting_sight = (int)num(sting, "sight", 6);
        /* "shear": what the right tool takes off it without hurting it. */
        const cJSON *shear = field(e, "shear");
        if (shear && (!str(shear, "item") || !item_ref(r, shear, "item", &m->shear_item)))
            return fail(r, "mob \"%s\": \"shear\" needs an \"item\"", name);
        m->shear_min = (int)num(shear, "min", 1);
        m->shear_max = (int)num(shear, "max", m->shear_min);
        m->shear_ticks = (int)num(shear, "regrow_ticks", 3000);
        copy_name(m->texture_shorn, sizeof m->texture_shorn, str(shear, "texture"));
        /* "milk": what the right thing in hand gets from it. */
        const cJSON *milk = field(e, "milk");
        if (milk && (!str(milk, "item") || !item_ref(r, milk, "item", &m->milk_item)))
            return fail(r, "mob \"%s\": \"milk\" needs an \"item\"", name);
        m->milk_ticks = (int)num(milk, "ticks", 600);
        /* "perch": it lives in trees and comes down at the player. */
        const cJSON *perch = field(e, "perch");
        m->perches = cJSON_IsObject(perch);
        cJSON_ArrayForEach(one, field(perch, "trees")) {
            int tree = cJSON_IsString(one) ? tree_id(one->valuestring) : -1;
            if (tree < 0) return fail(r, "mob \"%s\" perches in a tree that does not exist", name);
            if (m->perch_tree_count < MAX_MOB_TREES) m->perch_trees[m->perch_tree_count++] = tree;
        }
        m->ambush_range = (int)num(perch, "ambush_range", 2);
        m->ambush_depth = (int)num(perch, "ambush_depth", 12);
        m->bite = (float)num(perch, "bite", 0.1);
        m->chase_ticks = (int)num(perch, "chase_ticks", 100);
        m->bite_rest = (int)num(perch, "rest_ticks", 100);
        m->seek_radius = (int)num(perch, "seek_radius", 24);
        if (m->swims) {
            m->bite = (float)num(swim, "bite", 0.0);
            m->bite_rest = (int)num(swim, "rest_ticks", 40);
        }
        const cJSON *lays = field(e, "lays");
        if (lays && !tile_ref(r, lays, "tile", &m->lays)) return false;
        m->lay_chance = (float)num(lays, "chance", 0.0);
        cJSON_ArrayForEach(one, field(e, "drops")) {
            if (m->drop_count >= MAX_MOB_DROPS) return fail(r, "mob \"%s\" drops too many kinds of thing", name);
            ItemId item = ITEM_NONE;
            if (!str(one, "item") || !item_ref(r, one, "item", &item)) return fail(r, "mob \"%s\": a drop needs an \"item\"", name);
            m->drops[m->drop_count].item = item;
            m->drops[m->drop_count].min = (int)num(one, "min", 1);
            m->drops[m->drop_count].chance = (float)num(one, "chance", 1.0);
            m->drops[m->drop_count].sized = flag(one, "sized", false);
            m->drops[m->drop_count++].max = (int)num(one, "max", num(one, "min", 1));
        }
    }
    /* What each kind breeds on into, now that every kind in the file is known. */
    cJSON_ArrayForEach(e, field(root, "mobs")) {
        const cJSON *lays = field(e, "lays"), *next = field(lays, "next");
        if (!next) continue;
        MobDef *m = &content.mobs[mob_id(str(e, "name"))];
        m->lay_next_chance = (float)num(lays, "next_chance", 0.25);
        cJSON_ArrayForEach(one, next) {
            int kind = cJSON_IsString(one) ? mob_id(one->valuestring) : -1;
            if (kind < 0 || m->lay_next_count >= MAX_MOB_NEXT)
                return fail(r, "mob \"%s\": \"next\" lists at most %d mobs that exist", m->name, MAX_MOB_NEXT);
            m->lay_next[m->lay_next_count++] = kind;
        }
    }
    return true;
}

static bool read_trade(Reader *r, const cJSON *root)
{
    TradeDef *t = &content.trade;
    const cJSON *e;
    t->default_price = (int)num(root, "default_price", t->default_price > 0 ? t->default_price : 1);
    t->sell_share = (float)num(root, "sell_share", t->sell_share > 0.0f ? t->sell_share : 1.0);
    if (t->sell_share < 0.0f || t->sell_share > 1.0f) return fail(r, "\"sell_share\" is a share of the price, 0 to 1");
    t->discover = flag(root, "discover", t->discover);
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
        /* (Nor an item that is never held: one that turns into crystals the
         * moment it is picked up, or into a character the moment it is made.) */
        if (i > 0 && it->category < 0 && !money && it->crystals == 0 && !it->becomes_player &&
            content.trade.category_count > 0)
            return fail(r, "item \"%s\" has no \"category\", so it could not be bought or sold; "
                           "give it one from trade.json", it->name);
        it->plants = it->plants_name[0] ? tree_id(it->plants_name) : -1;
        if (it->plants_name[0] && it->plants < 0)
            return fail(r, "item \"%s\" plants a tree called \"%s\", which does not exist", it->name,
                        it->plants_name);
    }
    for (int i = 0; i < content.tile_count; i++) {
        TileDef *egg = &content.tiles[i];
        egg->hatches = egg->hatches_name[0] ? mob_id(egg->hatches_name) : -1;
        if (egg->hatches_name[0] && egg->hatches < 0)
            return fail(r, "tile \"%s\" hatches a mob called \"%s\", which does not exist", egg->name,
                        egg->hatches_name);
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
    if (g->portal_body == TILE_AIR || g->portal_base == TILE_AIR || g->portal_height < 1)
        return fail(r, "worldgen needs a spawn \"portal\" with a base and a body");
    if (g->portal_top == TILE_AIR) content.worldgen.portal_top = g->portal_body;
    if (g->gap_min < 1 || g->gap_max < g->gap_min || g->spawn_max < g->spawn_min)
        return fail(r, "worldgen has a bad \"gap\" or spawn \"column\" range");
    if (u->tick_rate < 1 || u->day_ticks < 1 || u->night_ticks < 1 || u->chunk_width < 1)
        return fail(r, "rules need a tick rate, day and night length and chunk width");
    /* Things that come in two halves, and things with a creature in them: each side told of the other. */
    for (TileType t = 1; t < content.tile_count; t++) content.tiles[t].pair_of = TILE_AIR;
    for (TileType t = 1; t < content.tile_count; t++)
        if (content.tiles[t].pair > TILE_AIR && content.tiles[t].pair < content.tile_count) content.tiles[content.tiles[t].pair].pair_of = t;
    for (ItemId i = 1; i < content.item_count; i++) content.items[i].releases = -1;
    for (int m = 0; m < content.mob_count; m++)
        if (content.mobs[m].caged > ITEM_NONE && content.mobs[m].caged < content.item_count) content.items[content.mobs[m].caged].releases = m;
    /* What water freezes into is named in the rules, and found now that the blocks are known. */
    content.rules.ice = u->ice_name[0] ? tile_id(u->ice_name) : TILE_AIR;
    if (u->ice < 0) return fail(r, "the weather names a block of \"ice\" that does not exist: \"%s\"", u->ice_name);
    if (u->random_event_base < 1) content.rules.random_event_base = u->random_events_per_chunk > 0 ? u->random_events_per_chunk : 1;
    if (u->max_random_events < u->random_events_per_chunk) content.rules.max_random_events = u->random_events_per_chunk;
    /* With no biomes given, the whole world is one: the plain layers, and every tree. */
    if (content.biome_count == 0) {
        BiomeDef *b = &content.biomes[content.biome_count++];
        memset(b, 0, sizeof *b);
        snprintf(b->name, sizeof b->name, "land");
        b->surface = g->surface;
        b->soil = g->soil;
        b->tree_one_in = g->tree_one_in;
        for (int i = 0; i < content.tree_count && i < MAX_BIOME_TREES; i++) {
            b->trees[b->tree_count].tree = i;
            b->trees[b->tree_count++].weight = content.trees[i].weight > 0 ? content.trees[i].weight : 1;
        }
    }
    if (content.biomes[0].has_climate || content.biomes[0].below_sea || content.biomes[0].near_sea > 0)
        return fail(r, "the first biome is the default one: it cannot depend on climate or the sea");
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
                                   "worldgen.json", "rules.json", "trade.json", "mobs.json" };
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
        read_mobs,
    };
    for (int i = 0; i < FILE_COUNT && !r.failed; i++) {
        r.file = files[i];
        if (roots[i]) readers[i](&r, roots[i]);
    }
    for (int i = 0; i < FILE_COUNT; i++) cJSON_Delete(roots[i]);
    if (!r.failed) validate(&r);
    return !r.failed;
}
