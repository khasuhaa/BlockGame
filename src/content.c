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

static void read_boons(const cJSON *b, Boons *out);

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

static int element_ref(const char *name);

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
        t->trade_level = (int)num(e, "trade_level", 0);
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
        static const char *const roles[NET_ROLE_COUNT] = { "", "cable", "controller", "drive", "terminal", "import", "export", "storage_bus",
                                                            "access", "cpu", "coprocessor", "assembler", "bridge", "break", "place" };
        const char *role = str(e, "network");
        t->net_role = NET_NONE;
        for (int k = 1; role && k < NET_ROLE_COUNT; k++)
            if (strcmp(role, roles[k]) == 0) t->net_role = k;
        if (role && t->net_role == NET_NONE) return fail(r, "tile \"%s\": \"network\" is not a part a network has", t->name);
        t->net_range = (int)num(e, "network_range", 0);
        t->endless = flag(e, "endless", false);
        t->conveyor = (int)num(e, "conveyor", 0);
        t->extractor = flag(e, "extractor", false);
        t->draw = (int)num(e, "draw", 0);
        t->miner_ticks = (int)num(e, "miner", 0);
        const cJSON *automat = field(e, "automat");
        t->automat = cJSON_IsObject(automat);
        copy_name(t->automat_kind, sizeof t->automat_kind, str(automat, "bench"));
        t->automat_inputs = (int)num(automat, "inputs", 1);
        t->automat_speed = (float)num(automat, "speed", 1.0);
        t->sorts = (int)num(e, "sorts", 0);
        t->fuel_count = 0;
        const cJSON *burns;
        cJSON_ArrayForEach(burns, field(e, "fuels")) {
            if (t->fuel_count >= 4 || !item_ref(r, burns, "item", &t->fuels[t->fuel_count].item) || t->fuels[t->fuel_count].item == ITEM_NONE)
                return fail(r, "tile \"%s\": \"fuels\" lists at most 4 items that exist, each with its \"ticks\"", t->name);
            t->fuels[t->fuel_count++].ticks = (int)num(burns, "ticks", 0);
        }
        if (t->conveyor < 0 || t->draw < 0 || t->miner_ticks < 0 || t->sorts < 0 || (t->automat && (t->automat_inputs < 1 || t->automat_speed <= 0.0f)))
            return fail(r, "tile \"%s\": a conveyor, a draw, a miner, an automat's inputs and speed cannot be less than nothing", t->name);
        t->craft_speed = (float)num(field(e, "quickens"), "speed", 0.0);
        t->refines = (float)num(field(e, "quickens"), "refines", 0.0);
        {
            static const char *crafts[3] = { "alchemy", "formation", "smithing" };
            t->comprehension = -1;
            for (int k = 0; str(e, "comprehension") && k < 3; k++)
                if (strcmp(str(e, "comprehension"), crafts[k]) == 0) t->comprehension = 5 + k;
            t->bestows_qi = (float)num(e, "bestows_qi", 0.0);
            t->tile_needs_stage = (int)num(e, "needs_stage", 0);
        }
        t->craft_radius = (int)num(field(e, "quickens"), "radius", 0);
        if (field(e, "quickens") && (t->craft_speed <= 0.0f || t->craft_radius < 1 || t->craft_radius > 12))
            return fail(r, "tile \"%s\": \"quickens\" needs a \"speed\" above 0 and a \"radius\" of 1 to 12", t->name);
        t->solid_foot = flag(e, "solid_foot", false);
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
        t->window = cJSON_IsObject(field(e, "window")) || flag(e, "window", false);
        t->window_dark = flag(field(e, "window"), "dark", false);
        if (t->window && (t->solid || t->layer != LAYER_BACKGROUND))
            return fail(r, "tile \"%s\": a window goes in the background, on a backwall", t->name);
        t->burns = flag(e, "burns", false);
        t->afloat = flag(e, "afloat", false);
        t->rail = flag(e, "rail", false);
        t->model = 0;
        if (str(e, "model")) {
            static const char *models[] = { "", "block", "box", "frame", "body", "panel", "cross" };
            for (int k = 1; k < 7; k++)
                if (strcmp(str(e, "model"), models[k]) == 0) t->model = (int8_t)k;
            if (t->model == 0) return fail(r, "tile \"%s\": a \"model\" is block, box, frame, body, panel or cross", t->name);
        }
        copy_name(t->texture_ramp, sizeof t->texture_ramp, str(e, "texture_ramp"));
        t->needs_tool = flag(e, "needs_tool", false);
        copy_name(t->leads_to, sizeof t->leads_to, str(e, "leads_to"));
        t->leads_dim = -1;
        t->opens_above = (int)num(e, "opens_above", 0);
        t->opens_below = (int)num(e, "opens_below", 0);
        t->hardness = (int)num(e, "hardness", 0);
        t->village = (int)num(e, "village", 0);
        {
            const cJSON *form = field(e, "formation");
            const char *shape = str(form, "pattern");
            t->qi = (float)num(e, "qi", 0.0);
            t->qi_reach = (int)num(e, "qi_reach", 6);
            t->needs_qi = (float)num(e, "needs_qi", 0.0);
            t->grow_one_in = (int)num(e, "grow_one_in", 0);
            t->buried_treasure = flag(e, "buried_treasure", false);
            {
                const cJSON *gate = field(e, "dungeon_gate");
                t->dungeon_gate = gate != NULL && !cJSON_IsFalse(gate);
                t->gate_reach = (int)num(gate, "reach", 6);
                copy_name(t->gate_beside, sizeof t->gate_beside, cJSON_IsObject(gate) ? str(gate, "beside") : NULL);
                t->loot_forge = flag(e, "loot_forge", false);
                t->dungeon_door = flag(e, "dungeon_door", false);
                t->dungeon_exit = flag(e, "dungeon_exit", false);
                t->dungeon_spawn = (int)num(e, "dungeon_spawn", 0);
            }
            {
                static const char *const stats[SHIP_STATS] = { "burden", "weight", "sail", "thrust", "strength", "reach", "depth", "cable", "light", "pump", "guns" };
                const cJSON *ship = field(e, "ship"), *keel = field(e, "keel");
                for (int k = 0; k < SHIP_STATS; k++) t->ship[k] = (int)num(ship, stats[k], 0);
                t->keel_level = (int)num(keel, "level", 0);
                t->keel_long = (int)num(keel, "long", 0);
                t->keel_above = (int)num(keel, "above", 0);
                t->keel_below = (int)num(keel, "below", 0);
                if (keel && (t->keel_level < 1 || t->keel_long < 2)) return fail(r, "tile \"%s\": a \"keel\" has a \"level\" of 1 or more and is 2 or more \"long\"", t->name);
                t->catches = flag(e, "catches", false);
                t->helm = flag(e, "helm", false);
                t->trawl = (int)num(e, "trawl", 0);
                if (!tile_ref(r, e, "holed", &t->holed)) return false;
                t->spring = (int)num(e, "spring", 0);
                const cJSON *under;
                t->needs_under_count = 0;
                cJSON_ArrayForEach(under, field(e, "needs_under")) {
                    if (!cJSON_IsString(under) || tile_id(under->valuestring) < 0 || t->needs_under_count >= 8) return fail(r, "tile \"%s\": \"needs_under\" lists at most 8 blocks that exist", t->name);
                    t->needs_under[t->needs_under_count++] = tile_id(under->valuestring);
                }
            }
            t->rocket_fuel = (int)num(field(e, "rocket"), "fuel", 0);
            t->rocket_thrust = (int)num(field(e, "rocket"), "thrust", 0);
            t->rocket_reliability = (float)num(field(e, "rocket"), "reliability", 1.0);
            t->rocket_escape = flag(field(e, "rocket"), "escape", false);
            t->launch_fuel = (int)num(field(e, "launch"), "fuel", 0);
            t->launch_thrust = (int)num(field(e, "launch"), "thrust", 0);
            t->pad = flag(e, "pad", false);
            t->gives_air = flag(e, "air", false);
            t->needs_air = flag(e, "needs_air", false);
            t->insight = (int)num(e, "insight", 0);
            t->formation = form != NULL;
            t->formation_gate = flag(form, "gate", false);
            t->formation_bars = flag(form, "bars", false);
            t->formation_wanders = flag(form, "wanders", false);
            t->formation_flags = TILE_AIR;
            if (!tile_ref(r, form, "flags", &t->formation_flags)) return false;
            t->formation_pattern = !shape || strcmp(shape, "cross") == 0 ? 0 : strcmp(shape, "square") == 0 ? 1 : strcmp(shape, "double") == 0 ? 3 : 2;
            t->formation_min = 2, t->formation_max = 6;
            pair(form, "distance", &t->formation_min, &t->formation_max);
            t->formation_qi = (float)num(form, "qi", 0.0);
            t->formation_grows = (float)num(form, "grows", 0.0);
            t->formation_harms = (float)num(form, "harms", 0.0);
            read_boons(field(form, "aura"), &t->aura);
            if (t->formation && (t->formation_flags == TILE_AIR || t->formation_min < 1 || t->formation_max > 24))
                return fail(r, "tile \"%s\": a formation needs its \"flags\", set 1 to 24 off", t->name);
        }
        {
            /* Signals, and the machinery that goes by them. A direction is [across, down]. */
            const cJSON *sig = field(e, "signal"), *one, *piston = field(e, "piston"), *reverts = field(e, "reverts"), *walk = field(e, "walkway");
            int dir[2];
            t->signal_wire = flag(sig, "wire", false);
            t->signal_daylight = flag(sig, "daylight", false);
            t->signal_invert = flag(sig, "invert", false);
            t->signal_and = flag(sig, "and", false);
            t->signal_out = (int)num(sig, "out", 0);
            t->signal_clock = (int)num(sig, "clock", 0);
            t->signal_delay = (int)num(sig, "delay", 0);
            t->signal_in_count = 0;
            cJSON_ArrayForEach(one, field(sig, "in")) {
                if (t->signal_in_count >= 3 || cJSON_GetArraySize(one) != 2) return fail(r, "tile \"%s\": a signal comes \"in\" from at most 3 sides, each [across, down]", t->name);
                t->signal_in[t->signal_in_count][0] = (int8_t)cJSON_GetArrayItem(one, 0)->valuedouble;
                t->signal_in[t->signal_in_count++][1] = (int8_t)cJSON_GetArrayItem(one, 1)->valuedouble;
            }
            dir[0] = dir[1] = 0;
            pair(sig, "toward", &dir[0], &dir[1]);
            t->signal_toward[0] = (int8_t)dir[0], t->signal_toward[1] = (int8_t)dir[1];
            t->powered_becomes = t->unpowered_becomes = t->pulse_becomes = t->use_becomes = t->reverts_to = t->stood_becomes = t->clear_becomes = t->piston_head = t->turns_to = TILE_AIR;
            if (!tile_ref(r, e, "turns_to", &t->turns_to)) return false;
            if (!tile_ref(r, e, "powered_becomes", &t->powered_becomes) || !tile_ref(r, e, "unpowered_becomes", &t->unpowered_becomes) ||
                !tile_ref(r, e, "pulse_becomes", &t->pulse_becomes) || !tile_ref(r, e, "use_becomes", &t->use_becomes) ||
                !tile_ref(r, reverts, "to", &t->reverts_to) || !tile_ref(r, e, "stood_becomes", &t->stood_becomes) ||
                !tile_ref(r, e, "clear_becomes", &t->clear_becomes) || !tile_ref(r, piston, "head", &t->piston_head)) return false;
            t->revert_ticks = reverts ? (int)num(reverts, "ticks", 20) : 0;
            t->stood_by_items = flag(e, "stood_by_items", false);
            t->piston = piston != NULL;
            t->piston_out = flag(piston, "out", false);
            t->piston_sticky = flag(piston, "sticky", false);
            dir[0] = 1, dir[1] = 0;
            pair(piston, "dir", &dir[0], &dir[1]);
            t->piston_dir[0] = (int8_t)dir[0], t->piston_dir[1] = (int8_t)dir[1];
            t->sticky = flag(e, "sticky", false);
            t->immovable = flag(e, "immovable", false);
            const cJSON *acts = field(e, "breaks") ? field(e, "breaks") : field(e, "deploys") ? field(e, "deploys") : field(e, "spout") ? field(field(e, "spout"), "dir") : NULL;
            t->breaks = field(e, "breaks") != NULL;
            t->deploys = field(e, "deploys") != NULL;
            t->spout = (int)num(field(e, "spout"), "amount", 0);
            t->blast = (int)num(e, "blast", 0);
            t->act_dir[0] = cJSON_GetArraySize(acts) == 2 ? (int8_t)cJSON_GetArrayItem(acts, 0)->valuedouble : 1;
            t->act_dir[1] = cJSON_GetArraySize(acts) == 2 ? (int8_t)cJSON_GetArrayItem(acts, 1)->valuedouble : 0;
            t->walkway = (int)num(walk, "dir", 0);
            t->walk_ticks = (int)num(walk, "ticks", 4);
            t->device = t->signal_wire || t->signal_daylight || t->signal_out > 0 || t->signal_clock > 0 || t->signal_in_count > 0 ||
                        t->powered_becomes != TILE_AIR || t->unpowered_becomes != TILE_AIR || t->pulse_becomes != TILE_AIR || t->reverts_to != TILE_AIR ||
                        t->stood_becomes != TILE_AIR || t->clear_becomes != TILE_AIR || t->piston || t->breaks || t->deploys || t->spout != 0 ||
                        t->blast > 0 || t->walkway != 0;
            if ((t->piston && abs(t->piston_dir[0]) + abs(t->piston_dir[1]) != 1) || t->signal_out < 0 || t->signal_out > 15)
                return fail(r, "tile \"%s\": a piston pushes one square along or up or down; a signal is 0 to 15", t->name);
        }
        t->harm = (float)num(e, "harm", 0.0);
        const cJSON *extends = field(e, "extends"), *ext_cost;
        t->extends = extends != NULL;
        t->extend_cost_count = 0;
        t->extend_crystals = (int)num(extends, "crystals", 0);
        cJSON_ArrayForEach(ext_cost, field(extends, "cost")) {
            if (t->extend_cost_count >= RECIPE_MAX_INPUTS) return fail(r, "tile \"%s\": raising to it costs too many kinds of item", t->name);
            if (!stack_ref(r, ext_cost, &t->extend_cost[t->extend_cost_count++])) return false;
        }
        t->on_floor = flag(e, "on_floor", false);
        t->station = flag(e, "station", false);
        t->lays = TILE_AIR;
        if (!tile_ref(r, e, "lays", &t->lays)) return false;
        t->written = flag(e, "written", false);
        t->pair = t->pair_of = TILE_AIR;
        if (!tile_ref(r, e, "pair", &t->pair)) return false;
        t->pair_floats = flag(e, "pair_floats", false);
        t->sleep_speed = (float)num(field(e, "bed"), "speed", 0.0);
        t->sleep_over = flag(field(e, "bed"), "over", false);
        const cJSON *gen = field(e, "generator");
        const char *needs = str(gen, "needs");
        t->gen_power = (int)num(gen, "power", 0);
        t->gen_needs = !needs ? POWER_NEEDS_NOTHING : strcmp(needs, "sun") == 0 ? POWER_NEEDS_SUN : strcmp(needs, "fuel") == 0 ? POWER_NEEDS_FUEL :
                       strcmp(needs, "sky") == 0 ? POWER_NEEDS_SKY : strcmp(needs, "heat") == 0 ? POWER_NEEDS_HEAT : -1;
        if (t->gen_needs < 0) return fail(r, "tile \"%s\": a generator \"needs\" \"sun\", \"fuel\", \"sky\" or \"heat\"", t->name);
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
            l->area = flag(entry, "area", false);
            cJSON_ArrayForEach(choice, field(entry, "one_of")) {
                if (l->choice_count >= MAX_LOOT_CHOICES || !str(choice, "item") ||
                    !item_ref(r, choice, "item", &l->choices[l->choice_count].item))
                    return fail(r, "tile \"%s\": \"one_of\" lists at most %d items that exist", t->name, MAX_LOOT_CHOICES);
                l->choices[l->choice_count++].weight = (int)num(choice, "weight", 1);
            }
            if ((l->area ? l->item != ITEM_NONE || l->choice_count > 0 : (l->item == ITEM_NONE) == (l->choice_count == 0)) || l->min < 0 || l->max < l->min)
                return fail(r, "tile \"%s\": loot is an \"item\", a \"one_of\" or \"area\", with \"min\" no more than \"max\"", t->name);
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
        t->upgrade_crystals = 0;
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
            t->upgrade_crystals = (int)num(upgrade, "crystals", 0);
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

/* "boons": what a thing does for whoever has it on. */
static void read_boons(const cJSON *b, Boons *out)
{
    memset(out, 0, sizeof *out);
    if (!b) return;
    out->walk = (float)num(b, "walk", 0.0), out->mine = (float)num(b, "mine", 0.0), out->craft = (float)num(b, "craft", 0.0);
    out->regen = (float)num(b, "regen", 0.0), out->lung = (float)num(b, "lung", 0.0);
    out->appetite = (float)num(b, "appetite", 0.0), out->stamina = (float)num(b, "stamina", 0.0);
    out->armor = (float)num(b, "armor", 0.0), out->warmth = (float)num(b, "warmth", 0.0), out->luck = (float)num(b, "luck", 0.0);
    out->strength = (int)num(b, "strength", 0.0), out->thorns = (int)num(b, "thorns", 0.0);
    out->qi = (float)num(b, "qi", 0.0), out->breakthrough = (float)num(b, "breakthrough", 0.0), out->ward = (float)num(b, "ward", 0.0);
    out->glide = (float)num(b, "glide", 0.0), out->sight = (float)num(b, "sight", 0.0), out->unseen = (float)num(b, "unseen", 0.0);
    out->magnet = (float)num(b, "magnet", 0.0), out->fireproof = (float)num(b, "fireproof", 0.0), out->flight = (float)num(b, "flight", 0.0);
}

static bool boons_ok(const Boons *b)
{
    return b->walk >= 0.0f && b->mine >= 0.0f && b->craft >= 0.0f && b->regen >= 0.0f && b->lung >= 0.0f && b->appetite >= 0.0f && b->stamina >= 0.0f &&
           b->armor >= 0.0f && b->armor <= 1.0f && b->warmth >= 0.0f && b->warmth <= 1.0f && b->luck >= 0.0f && b->luck <= 1.0f && b->strength >= 0 && b->thorns >= 0;
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
        it->cooked_said = (int8_t)(field(e, "cooked") ? flag(e, "cooked", false) : -1);
        it->cooked = it->cooked_said == 1;
        it->food_oxygen = (float)num(food, "oxygen", 0.0);
        if (it->food < 0.0f || it->food > 1.0f)
            return fail(r, "item \"%s\": \"food\" is a share of the hunger bar, 0 to 1", it->name);
        /* The category is matched to the trade file once that has been read. */
        copy_name(it->category_name, sizeof it->category_name, str(e, "category"));
        it->price = (int)num(e, "price", 0);
        it->tier = (int)num(e, "tier", 0);
        if (it->tier < 0) return fail(r, "item \"%s\": a \"tier\" is 1 or more", it->name);
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
        it->cages_kept = flag(e, "cages_kept", false);
        it->feeds = (int)num(e, "feeds", 0);
        it->natal = flag(e, "natal", false);
        it->flight_qi = (float)num(e, "flight_qi", 0.0);
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
        /* "cast": a spell. */
        const cJSON *cast = field(e, "cast");
        it->casts = cJSON_IsObject(cast);
        it->cast_element = element_ref(str(cast, "element"));
        it->cast_damage = (int)num(cast, "damage", 0);
        it->cast_bane = (int)num(cast, "bane", it->cast_damage);
        it->cast_splash = (int)num(cast, "splash", 0);
        it->cast_cooldown = (float)num(cast, "cooldown", 0.0);
        it->cast_scales = (float)num(cast, "scales", 0.0);
        if (str(cast, "class")) it->loot_class = strcmp(str(cast, "class"), "warrior") == 0 ? 1 : strcmp(str(cast, "class"), "mage") == 0 ? 2 : 0;
        it->cast_cooks = flag(cast, "cooks", false);
        it->cast_water = (int)num(cast, "water", 0);
        it->cast_dig = (int)num(cast, "dig", 0);
        it->cast_shield = (float)num(cast, "shield", 0.0);
        it->cast_grow = (int)num(cast, "grow", 0);
        it->cast_freeze = (int)num(cast, "freeze", 0);
        it->cast_drain = (int)num(cast, "drain", 0);
        it->cast_hold = (int)num(cast, "hold", 0);
        it->cast_blink = flag(cast, "blink", false);
        it->hook = (int)num(e, "hook", 0);
        const char *sky = str(cast, "weather");
        it->cast_weather = !sky ? 0 : strcmp(sky, "rain") == 0 ? 1 : 2;
        it->cast_weather_ticks = (int)num(cast, "weather_ticks", 0);
        it->cast_places = TILE_AIR;
        if (!tile_ref(r, cast, "places", &it->cast_places)) return false;
        {
            const cJSON *effect = field(e, "effect");
            read_boons(field(effect, "boons"), &it->effect);
            it->effect_ticks = field(effect, "forever") && flag(effect, "forever", false) ? -1 : (int)num(effect, "ticks", 0);
            it->cures = flag(e, "cures", false);
            it->qi_gain = (float)num(e, "qi", 0.0);
            it->toxin = (float)num(e, "toxin", 0.0);
            it->opens_meridian = flag(e, "opens_meridian", false);
            it->washes_roots = flag(e, "washes_roots", false);
            it->calls_tribulation = flag(e, "calls_tribulation", false);
            const cJSON *manual = field(e, "manual"), *technique = field(e, "technique");
            it->manual = manual != NULL;
            it->manual_rate = (float)num(manual, "rate", 1.0);
            it->manual_element = (int)num(manual, "element", -1);
            it->technique = technique != NULL;
            it->technique_qi = (float)num(technique, "qi", 0.0);
            it->technique_stage = (int)num(technique, "stage", 0);
            it->technique_insight = (int)num(technique, "insight", 0);
            it->technique_element = (int)num(technique, "element", -1);
            it->tames = (int)num(e, "tames", 0);
            it->insight_gain = (int)num(e, "insight", 0);
            it->boards = flag(e, "boards", false);
            {
                const cJSON *roll = field(e, "rolled");
                const char *cls = str(roll, "class");
                it->rolled = roll != NULL;
                it->roll_power = (long long)num(roll, "power", 0);
                it->roll_ups = (long long)num(roll, "upgrades", 0);
                it->roll_health = (long long)num(roll, "health", 0);
                it->roll_gain = num(roll, "gain", 0.0);
                it->roll_health_gain = num(roll, "health_gain", 0.0);
                if (roll) it->loot_class = cls && strcmp(cls, "warrior") == 0 ? 1 : cls && strcmp(cls, "mage") == 0 ? 2 : 0;
                it->grade = (int)num(e, "grade", 0);
                if (it->grade < 0 || it->grade >= MAX_GRADES) return fail(r, "item \"%s\": \"grade\" is 1 to %d", it->name, MAX_GRADES - 1);
                if (it->rolled) {
                    /* (How it fell is what its meter reads: 0 for not yet. One is never like the next, so they are held singly.) */
                    it->holds_water = false;
                    it->meter_max = 60000;
                    it->meter_start = 0;
                    it->max_stack = 1;
                }
            }
            it->treasure_map = flag(e, "treasure_map", false);
            if (it->treasure_map) {
                /* (Which buried thing it is of, is what its meter reads: the column, plus one; 0 for none yet.) */
                it->holds_water = false;
                it->meter_max = 32000;
                it->meter_start = 0;
            }
            it->meridian = (int)num(e, "meridian", 0);
            if (it->meridian > 0) it->opens_meridian = true;
            it->needs_stage = (int)num(e, "needs_stage", 0);
            it->recalls = flag(e, "recalls", false);
            it->leaves = ITEM_NONE;
            if (!item_ref(r, e, "leaves", &it->leaves)) return false;
        }
        const char *brings = str(cast, "time");
        it->cast_time = !brings ? -1 : strcmp(brings, "sunrise") == 0 ? 0 : strcmp(brings, "moonrise") == 0 ? 1 : -2;
        if (it->casts && ((it->uses < 1 && it->cast_cooldown <= 0.0f) || it->uses > 120 || it->cast_time == -2 || it->cast_dig < 0 || it->cast_dig > 12 ||
                          it->cast_splash < 0 || it->cast_water < 0 || it->cast_shield < 0.0f))
            return fail(r, "item \"%s\": a spell needs \"uses\", 1 to 120 (or a \"cooldown\"); its \"time\" is \"sunrise\" or \"moonrise\"; its \"dig\" is at most 12", it->name);
        if (it->casts && it->uses > 0) {
            it->holds_water = false;
            it->meter_max = it->uses;
            it->meter_start = it->uses;
        }
        it->cell_bytes = (int)num(e, "cell", 0);
        it->remote = flag(e, "remote", false);
        if (it->cell_bytes < 0 || (it->cell_bytes > 0 && it->cell_bytes < 128)) return fail(r, "item \"%s\": a \"cell\" holds 128 bytes or more", it->name);
        if (it->cell_bytes > 0) {
            /* (Which of the world's cells it is, is what its meter reads; 0 for one with nothing in it yet.) */
            it->holds_water = false;
            it->meter_max = MAX_CELLS - 1;
            it->meter_start = 0;
        }
        copy_name(it->worn, sizeof it->worn, str(e, "worn"));
        it->walk_speed = (float)num(e, "walk_speed", 0.0);
        it->lifts = flag(e, "lifts", false);
        it->pack = flag(e, "pack", false);
        it->glides = flag(e, "glides", false);
        it->power = (int)num(e, "power", 0);
        it->cooling = (float)num(e, "cooling", 0.0);
        it->reveals_all = flag(e, "reveals_all", false);
        it->identifies = flag(e, "identifies", false);
        it->speciality = flag(e, "speciality", false);
        it->turns = flag(e, "turns", false);
        it->relic = flag(e, "relic", false);
        read_boons(field(e, "boons"), &it->boons);
        if (!boons_ok(&it->boons)) return fail(r, "item \"%s\": its \"boons\" are not less than nothing, and shares are no more than 1", it->name);
        copy_name(it->set_name, sizeof it->set_name, str(e, "set"));
        it->shelters = flag(e, "shelters", false);
        const cJSON *vehicle = field(e, "vehicle");
        memset(&it->vehicle, 0, sizeof it->vehicle);
        if (vehicle) {
            it->vehicle.is = true;
            it->vehicle.width = (int)num(vehicle, "width", 3);
            it->vehicle.engine = flag(vehicle, "engine", false);
            it->vehicle.hand = flag(vehicle, "hand", false);
            it->vehicle.cargo_slots = (int)num(vehicle, "cargo_slots", 0);
            it->vehicle.seats = (int)num(vehicle, "seats", 0);
            it->vehicle.speed = (float)num(vehicle, "speed", 6.0);
            it->vehicle.accel = (float)num(vehicle, "accel", 2.5);
            it->vehicle.climb = (float)num(vehicle, "climb", it->vehicle.accel);
            it->vehicle.gravity = (float)num(vehicle, "gravity", 5.0);
            it->vehicle.drag = (float)num(vehicle, "drag", 0.5);
            it->vehicle.brake = (float)num(vehicle, "brake", 10.0);
            if (it->vehicle.accel <= 0.0f || it->vehicle.climb < 0.0f || it->vehicle.gravity < 0.0f || it->vehicle.drag < 0.0f || it->vehicle.brake <= 0.0f)
                return fail(r, "item \"%s\": a vehicle's \"accel\" and \"brake\" are above nothing, and nothing else about how it goes is below it", it->name);
            copy_name(it->vehicle.texture, sizeof it->vehicle.texture, str(vehicle, "texture"));
            if (it->vehicle.width != 1 && it->vehicle.width != 3)
                return fail(r, "item \"%s\": a vehicle is 1 or 3 squares long", it->name);
            if (it->vehicle.seats < 0 || it->vehicle.seats > 2 || it->vehicle.cargo_slots < 0 || it->vehicle.cargo_slots > 16 ||
                it->vehicle.speed <= 0.0f)
                return fail(r, "item \"%s\": a vehicle has up to 2 seats, up to 16 slots, and a speed above nothing", it->name);
        }
        it->warmth = (float)num(e, "warmth", 0.0);
        if (it->warmth < 0.0f || it->warmth > 1.0f) return fail(r, "item \"%s\": \"warmth\" is a share, 0 to 1", it->name);
        it->tireless = flag(e, "tireless", false);
        it->sated = flag(e, "sated", false);
        if (it->walk_speed < 0.0f) return fail(r, "item \"%s\": \"walk_speed\" is not less than nothing", it->name);
        it->cooks_into = ITEM_NONE;
        if (str(e, "cooks_into") && !item_ref(r, e, "cooks_into", &it->cooks_into)) return false;
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
    /* "sets": things that go together, and what having enough of one on gives besides. (One named again is replaced.) */
    cJSON_ArrayForEach(e, field(root, "sets")) {
        const char *name = str(e, "name");
        if (!name) return fail(r, "a set has no \"name\"");
        int at = -1;
        for (int i = 0; i < content.set_count; i++)
            if (strcmp(content.sets[i].name, name) == 0) at = i;
        if (at < 0) {
            if (content.set_count >= MAX_SETS) return fail(r, "too many sets");
            at = content.set_count++;
        }
        SetDef *set = &content.sets[at];
        memset(set, 0, sizeof *set);
        copy_name(set->name, sizeof set->name, name);
        copy_name(set->label, sizeof set->label, str(e, "label") ? str(e, "label") : name);
        const cJSON *tier;
        cJSON_ArrayForEach(tier, field(e, "bonuses")) {
            if (set->tier_count >= MAX_SET_TIERS) return fail(r, "set \"%s\" has more than %d bonuses", name, MAX_SET_TIERS);
            set->tiers[set->tier_count].pieces = (int)num(tier, "pieces", 0);
            read_boons(field(tier, "boons"), &set->tiers[set->tier_count].boons);
            if (set->tiers[set->tier_count].pieces < 1 || !boons_ok(&set->tiers[set->tier_count].boons))
                return fail(r, "set \"%s\": a bonus is for so many \"pieces\", 1 or more, and its \"boons\" are not less than nothing", name);
            set->tier_count++;
        }
    }
    return true;
}

static char island_tree_names[MAX_ISLAND_TREES][CONTENT_NAME_LEN];

/* The element of a name, noted the first time it is met. -1 for no name, or no room for another. */
static int element_ref(const char *name)
{
    if (!name || !name[0]) return -1;
    for (int i = 0; i < content.element_count; i++)
        if (strcmp(content.elements[i], name) == 0) return i;
    if (content.element_count >= MAX_ELEMENTS) return -1;
    copy_name(content.elements[content.element_count], CONTENT_NAME_LEN, name);
    return content.element_count++;
}

/* ---- recipes and trees ------------------------------------------------- */

static bool read_recipes(Reader *r, const cJSON *root)
{
    const cJSON *e, *in;
    cJSON_ArrayForEach(e, field(root, "recipes")) {
        Recipe recipe = { 0 };
        /* One that is "optional" is left out, and nothing said, if anything it names is not there: a recipe for
         * what another mod has, which is no matter when that mod is not on. */
        if (flag(e, "optional", false)) {
            bool all = !str(field(e, "output"), "item") || item_id(str(field(e, "output"), "item")) > 0;
            const cJSON *named;
            cJSON_ArrayForEach(named, field(e, "inputs")) all = all && str(named, "item") && item_id(str(named, "item")) > 0;
            cJSON_ArrayForEach(named, field(field(e, "output"), "one_of")) all = all && str(named, "item") && item_id(str(named, "item")) > 0;
            if (str(e, "at")) all = all && tile_id(str(e, "at")) > 0;
            if (!all) continue;
        }
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
        copy_name(recipe.at, sizeof recipe.at, str(e, "at"));
        recipe.crystals = (int)num(e, "crystals", 0);
        recipe.characters = (int)num(e, "characters", 0);
        recipe.time = (float)num(e, "time", 0.0);
        recipe.formula = ITEM_NONE;
        if (!item_ref(r, e, "formula", &recipe.formula)) return false;
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
    {
        const cJSON *ocean = field(root, "ocean"), *one;
        if (ocean) {
            g->ocean = true;
            g->isle_gap_min = 40, g->isle_gap_max = 140, g->isle_min = 14, g->isle_max = 110, g->great_min = 160, g->great_max = 260, g->sea_depth_min = 10, g->sea_depth_max = 34;
            pair(ocean, "gap", &g->isle_gap_min, &g->isle_gap_max);
            pair(ocean, "length", &g->isle_min, &g->isle_max);
            pair(ocean, "great_length", &g->great_min, &g->great_max);
            pair(ocean, "depth", &g->sea_depth_min, &g->sea_depth_max);
            g->isle_peak = (int)num(ocean, "peak", 14);
            g->great_peak = (int)num(ocean, "great_peak", 46);
            g->great_every = (int)num(ocean, "great_every", 7);
            g->wreck_rate = (float)num(ocean, "wrecks_per_1000_columns", 0.0);
            g->buried_rate = (float)num(ocean, "buried_per_1000_columns", 0.0);
            copy_name(g->buried_name, CONTENT_NAME_LEN, str(ocean, "buried"));
            g->wreck_count = 0;
            cJSON_ArrayForEach(one, field(ocean, "wrecks")) {
                if (!cJSON_IsString(one) || g->wreck_count >= 16) return fail(r, "\"ocean\": \"wrecks\" lists at most 16 structures, by name");
                copy_name(g->wrecks[g->wreck_count++], CONTENT_NAME_LEN, one->valuestring);
            }
            g->isle_biome_count = 0;
            cJSON_ArrayForEach(one, field(ocean, "biomes")) {
                if (!cJSON_IsString(one) || g->isle_biome_count >= 16) return fail(r, "\"ocean\": \"biomes\" lists at most 16 biomes, by name");
                copy_name(g->isle_biomes[g->isle_biome_count++], CONTENT_NAME_LEN, one->valuestring);
            }
            if (g->isle_gap_min < 4 || g->isle_min < 6 || g->isle_max < g->isle_min || g->sea_depth_min < 2) return fail(r, "\"ocean\": islands are 6 or more long, 4 or more apart, in a sea 2 or more deep");
        }
    }
    {
        static const char *const needs[SEA_NEEDS] = { "keel", "way", "freeboard", "strength", "cable", "depth", "light", "guns", "reach" };
        const cJSON *adrift = field(root, "adrift"), *e, *one;
        if (adrift) {
            g->adrift = true;
            g->adrift_waterline = (int)num(adrift, "waterline", 60);
            g->drift_ticks = (int)num(adrift, "drift_ticks", 12);
            g->drift_ahead = (int)num(adrift, "ahead", 40);
            g->keel_long = (int)num(field(adrift, "keel"), "long", 10);
            g->keel_above = (int)num(field(adrift, "keel"), "above", 3);
            g->keel_below = (int)num(field(adrift, "keel"), "below", 1);
            copy_name(g->adrift_start, CONTENT_NAME_LEN, str(adrift, "start"));
            if (g->drift_ticks < 2 || g->drift_ahead < 4 || g->keel_long < 2) return fail(r, "\"adrift\": things take 2 ticks or more a square, appear 4 or more ahead, and the ship is 2 or more long");
        }
        const cJSON *sky = field(root, "skyblock");
        if (sky) {
            g->skyblock = true;
            g->sky_gap_min = 40, g->sky_gap_max = 90;
            pair(sky, "gap", &g->sky_gap_min, &g->sky_gap_max);
            copy_name(g->sky_island, CONTENT_NAME_LEN, str(sky, "island"));
            if (!tile_ref(r, sky, "floor", &g->sky_floor)) return false;
            g->sky_islet_count = 0;
            cJSON_ArrayForEach(one, field(sky, "islets")) {
                if (!cJSON_IsString(one) || g->sky_islet_count >= 16) return fail(r, "\"skyblock\": \"islets\" lists at most 16 structures, by name");
                copy_name(g->sky_islets[g->sky_islet_count++], CONTENT_NAME_LEN, one->valuestring);
            }
            if (!g->sky_island[0] || g->sky_gap_min < 8 || g->sky_gap_max < g->sky_gap_min) return fail(r, "\"skyblock\" needs an \"island\", and a \"gap\" of 8 or more");
        }
        cJSON_ArrayForEach(e, field(root, "seas")) {
            const char *sea_name = str(e, "name");
            if (!sea_name) return fail(r, "a sea has no \"name\"");
            SeaDef *sea = NULL;
            for (int i = 0; i < g->sea_count; i++)
                if (strcmp(g->seas[i].name, sea_name) == 0) sea = &g->seas[i];
            if (!sea) {
                if (g->sea_count >= MAX_SEAS) return fail(r, "too many seas (%d at most)", MAX_SEAS);
                sea = &g->seas[g->sea_count++];
            }
            memset(sea, 0, sizeof *sea);
            copy_name(sea->name, CONTENT_NAME_LEN, sea_name);
            copy_name(sea->label, CONTENT_NAME_LEN, str(e, "label") ? str(e, "label") : sea_name);
            for (int k = 0; k < SEA_NEEDS; k++) sea->needs[k] = (int)num(field(e, "needs"), needs[k], 0);
            sea->passage_ticks = (int)num(e, "passage_ticks", 600);
            sea->drift_rate = (float)num(e, "drift_per_1000_ticks", 20.0);
            sea->alongside_every = (int)num(e, "alongside_every", 2400);
            sea->swell = (float)num(e, "swell", 1.0);
            sea->dark = flag(e, "dark", false);
            cJSON_ArrayForEach(one, field(e, "drift")) {
                if (sea->drift_count >= MAX_SEA_DRIFT) return fail(r, "sea \"%s\": at most %d things adrift", sea_name, MAX_SEA_DRIFT);
                const char *when = str(one, "when");
                if (!item_ref(r, one, "item", &sea->drift[sea->drift_count].item)) return false;
                if (sea->drift[sea->drift_count].item == ITEM_NONE) return fail(r, "sea \"%s\": something adrift has no \"item\"", sea_name);
                sea->drift[sea->drift_count].weight = (int)num(one, "weight", 1);
                sea->drift[sea->drift_count++].when = !when ? 0 : strcmp(when, "day") == 0 ? 1 : strcmp(when, "night") == 0 ? 2 : 0;
            }
            cJSON_ArrayForEach(one, field(e, "bed")) {
                if (sea->bed_count >= MAX_SEA_DRIFT) return fail(r, "sea \"%s\": at most %d things on the bed", sea_name, MAX_SEA_DRIFT);
                if (!item_ref(r, one, "item", &sea->bed[sea->bed_count].item)) return false;
                if (sea->bed[sea->bed_count].item == ITEM_NONE) return fail(r, "sea \"%s\": something on the bed has no \"item\"", sea_name);
                sea->bed[sea->bed_count].weight = (int)num(one, "weight", 1);
                sea->bed[sea->bed_count++].depth = (int)num(one, "depth", 0);
            }
            cJSON_ArrayForEach(one, field(e, "alongside")) {
                if (sea->alongside_count >= MAX_SEA_ALONGSIDE) return fail(r, "sea \"%s\": at most %d things alongside", sea_name, MAX_SEA_ALONGSIDE);
                if (!str(one, "structure")) return fail(r, "sea \"%s\": something alongside has no \"structure\"", sea_name);
                copy_name(sea->alongside[sea->alongside_count].structure, CONTENT_NAME_LEN, str(one, "structure"));
                copy_name(sea->alongside[sea->alongside_count].label, CONTENT_NAME_LEN, str(one, "label") ? str(one, "label") : str(one, "structure"));
                sea->alongside[sea->alongside_count].weight = (int)num(one, "weight", 1);
                sea->alongside[sea->alongside_count].cable = (int)num(one, "cable", 0);
                sea->alongside[sea->alongside_count].reach = (int)num(one, "reach", 0);
                sea->alongside[sea->alongside_count].stay = (int)num(one, "stay", 1800);
                sea->alongside[sea->alongside_count++].sunk = (int)num(one, "sunk", 1);
            }
            const cJSON *sky = field(e, "sky"), *water = field(e, "water");
            sea->has_sky = cJSON_GetArraySize(sky) == 3;
            sea->has_water = cJSON_GetArraySize(water) == 4;
            for (int k = 0; sea->has_sky && k < 3; k++) sea->sky[k] = (unsigned char)cJSON_GetArrayItem(sky, k)->valuedouble;
            for (int k = 0; sea->has_water && k < 4; k++) sea->water[k] = (unsigned char)cJSON_GetArrayItem(water, k)->valuedouble;
        }
    }
    /* "dungeons": each by name, a later one of the same name in place of the earlier. */
    {
        const cJSON *e, *one;
        cJSON_ArrayForEach(e, field(root, "dungeons")) {
            const char *name = str(e, "name");
            if (!name) return fail(r, "a dungeon has no \"name\"");
            int id = -1;
            for (int i = 0; i < content.dungeon_count; i++)
                if (strcmp(content.dungeons[i].name, name) == 0) id = i;
            if (id < 0) {
                if (content.dungeon_count >= MAX_DUNGEONS) return fail(r, "too many dungeons");
                id = content.dungeon_count++;
            }
            DungeonDef *d = &content.dungeons[id];
            memset(d, 0, sizeof *d);
            copy_name(d->name, sizeof d->name, name);
            copy_name(d->label, sizeof d->label, str(e, "label") ? str(e, "label") : name);
            d->tier = (int)num(e, "tier", 1);
            d->rooms_min = d->rooms_max = 8;
            pair(e, "rooms", &d->rooms_min, &d->rooms_max);
            d->gold = (long)num(e, "gold", 0);
            if (!tile_ref(r, e, "wall", &d->wall) || !tile_ref(r, e, "floor", &d->floor) || !tile_ref(r, e, "back", &d->back) || !tile_ref(r, e, "door", &d->door) ||
                !tile_ref(r, e, "mark", &d->mark) || !tile_ref(r, e, "boss_mark", &d->boss_mark) || !tile_ref(r, e, "chest", &d->chest) || !tile_ref(r, e, "exit", &d->exit))
                return false;
            if (str(e, "light") && !tile_ref(r, e, "light", &d->light)) return false;
            if (str(e, "stone") && !item_ref(r, e, "stone", &d->stone)) return false;
            d->first_hard = (int)num(e, "difficulties_from", 0);
            if (d->first_hard < 0 || d->first_hard >= MAX_DIFFICULTIES) return fail(r, "dungeon \"%s\": \"difficulties_from\" is which difficulty, from 0", name);
            int which = 0;
            cJSON_ArrayForEach(one, field(e, "spells")) {
                if (which >= MAX_DIFFICULTIES) return fail(r, "dungeon \"%s\": \"spells\" has a pair for each difficulty and no more", name);
                for (int k = 0; k < 2 && k < cJSON_GetArraySize(one); k++) {
                    const cJSON *spell = cJSON_GetArrayItem(one, k);
                    if (!cJSON_IsString(spell) || item_id(spell->valuestring) <= 0) return fail(r, "dungeon \"%s\": a spell that is not an item", name);
                    d->spells[which][k] = item_id(spell->valuestring);
                }
                which++;
            }
            cJSON_ArrayForEach(one, field(e, "decor")) {
                if (!cJSON_IsString(one) || d->decor_count >= 8 || tile_id(one->valuestring) == TILE_AIR) return fail(r, "dungeon \"%s\": \"decor\" is at most 8 blocks, by name", name);
                d->decor[d->decor_count++] = tile_id(one->valuestring);
            }
            cJSON_ArrayForEach(one, field(e, "hanging")) {
                if (!cJSON_IsString(one) || d->hanging_count >= 4 || tile_id(one->valuestring) == TILE_AIR) return fail(r, "dungeon \"%s\": \"hanging\" is at most 4 blocks, by name", name);
                d->hanging[d->hanging_count++] = tile_id(one->valuestring);
            }
            cJSON_ArrayForEach(one, field(e, "mobs")) {
                if (!cJSON_IsString(one) || d->mob_count >= 8) return fail(r, "dungeon \"%s\": \"mobs\" is at most 8 creatures, by name, the boss last", name);
                copy_name(d->mobs[d->mob_count++], CONTENT_NAME_LEN, one->valuestring);
            }
            cJSON_ArrayForEach(one, field(e, "loot")) {
                if (d->loot_count >= MAX_DUNGEON_LOOT) return fail(r, "dungeon \"%s\": more than %d things to find", name, MAX_DUNGEON_LOOT);
                if (!item_ref(r, one, "item", &d->loot[d->loot_count].item)) return false;
                d->loot[d->loot_count++].rank = (float)num(one, "rank", 0.0);
            }
            if (d->mob_count < 2 || d->rooms_min < 3 || d->rooms_max < d->rooms_min) return fail(r, "dungeon \"%s\": it wants two creatures or more (the last is its boss) and three rooms or more", name);
        }
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
        b->cold = flag(e, "cold", b->cold);
        b->hot = flag(e, "hot", false);
        b->qi = (float)num(e, "qi", 0.0);
        copy_name(b->dimension_name, sizeof b->dimension_name, str(e, "dimension"));
        b->dimension = -1;
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
        cJSON_ArrayForEach(one, field(e, "spikes")) {
            if (b->spike_count >= MAX_BIOME_SPIKES) return fail(r, "biome \"%s\" has too many spikes", name);
            if (!tile_ref(r, one, "tile", &b->spikes[b->spike_count].tile) || b->spikes[b->spike_count].tile == TILE_AIR)
                return fail(r, "biome \"%s\": a spike needs a \"tile\"", name);
            b->spikes[b->spike_count].one_in = (int)num(one, "one_in", 20);
            b->spikes[b->spike_count].min = 2, b->spikes[b->spike_count].max = 5;
            pair(one, "height", &b->spikes[b->spike_count].min, &b->spikes[b->spike_count].max);
            b->spikes[b->spike_count].hang = flag(one, "hang", false);
            b->spikes[b->spike_count++].arch = flag(one, "arch", false);
        }
        const cJSON *treasure = field(e, "treasure");
        if (treasure && !tile_ref(r, treasure, "tile", &b->treasure)) return false;
        b->treasure_one_in = (int)num(treasure, "one_in", 0);
    }

    /* "biome_plants": more for biomes that are already there to grow - what a mod adds to one without saying it all over again. */
    cJSON_ArrayForEach(e, field(root, "biome_plants")) {
        const char *where = str(e, "biome");
        BiomeDef *b = NULL;
        for (int i = 0; where && i < content.biome_count; i++)
            if (strcmp(content.biomes[i].name, where) == 0) b = &content.biomes[i];
        TileType tile = TILE_AIR;
        if (!b || !tile_ref(r, e, "tile", &tile) || tile == TILE_AIR)
            return fail(r, "\"biome_plants\": each needs a \"biome\" that exists and a \"tile\"");
        if (b->plant_count >= MAX_BIOME_PLANTS) return fail(r, "biome \"%s\" has too many plants", b->name);
        b->plants[b->plant_count].tile = tile;
        b->plants[b->plant_count++].one_in = (int)num(e, "one_in", 20);
    }

    /* "loot_pools": places with finds of their own. (One named again is added to, not replaced: a mod may put more
     * into a pool another made.) */
    cJSON_ArrayForEach(e, field(root, "loot_pools")) {
        const char *name = str(e, "name");
        const cJSON *one;
        if (!name) return fail(r, "a loot pool has no \"name\"");
        int at = -1;
        for (int i = 0; i < content.pool_count; i++)
            if (strcmp(content.pools[i].name, name) == 0) at = i;
        if (at < 0) {
            if (content.pool_count >= MAX_POOLS) return fail(r, "too many loot pools");
            at = content.pool_count++;
            memset(&content.pools[at], 0, sizeof content.pools[at]);
            copy_name(content.pools[at].name, sizeof content.pools[at].name, name);
            copy_name(content.pools[at].label, sizeof content.pools[at].label, name);
            content.pools[at].y_min = -100000, content.pools[at].y_max = 100000;
        }
        PoolDef *pool = &content.pools[at];
        if (str(e, "label")) copy_name(pool->label, sizeof pool->label, str(e, "label"));
        cJSON_ArrayForEach(one, field(e, "biomes")) {
            if (!cJSON_IsString(one)) continue;
            if (pool->biome_name_count >= MAX_POOL_BIOMES) return fail(r, "loot pool \"%s\" names too many biomes", name);
            copy_name(pool->biome_names[pool->biome_name_count++], CONTENT_NAME_LEN, one->valuestring);
        }
        if (field(e, "y")) pair(e, "y", &pool->y_min, &pool->y_max);
        cJSON_ArrayForEach(one, field(e, "loot")) {
            if (pool->loot_count >= MAX_POOL_LOOT || !str(one, "item") || !item_ref(r, one, "item", &pool->loot[pool->loot_count].item))
                return fail(r, "loot pool \"%s\" lists at most %d items that exist", name, MAX_POOL_LOOT);
            pool->loot[pool->loot_count++].weight = (int)num(one, "weight", 1);
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
        copy_name(d->dimension_name, sizeof d->dimension_name, str(e, "dimension"));
        d->dimension = -1;
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
        copy_name(v->dimension_name, sizeof v->dimension_name, str(e, "dimension"));
        v->dimension = -1;
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
        v->rare = TILE_AIR;
        v->rare_one_in = 0;
        const char *shape = str(e, "shape");
        v->shape = shape && strcmp(shape, "streak") == 0 ? VEIN_STREAK : shape && strcmp(shape, "scar") == 0 ? VEIN_SCAR : VEIN_BLOB;
        if (shape && v->shape == VEIN_BLOB && strcmp(shape, "blob") != 0)
            return fail(r, "vein \"%s\": \"shape\" is \"blob\", \"streak\" or \"scar\"", name);
        v->min_thick = v->max_thick = 3;
        pair(e, "thickness", &v->min_thick, &v->max_thick);
        if (v->min_thick < 1 || v->max_thick < v->min_thick)
            return fail(r, "vein \"%s\": \"thickness\" is a range of 1 or more", name);
    }

    /* "vein_rares": now and then a block of a vein that is already there is something else. */
    cJSON_ArrayForEach(e, field(root, "vein_rares")) {
        const char *which = str(e, "vein");
        VeinDef *v = NULL;
        for (int i = 0; which && i < content.vein_count; i++)
            if (strcmp(content.veins[i].name, which) == 0) v = &content.veins[i];
        TileType tile = TILE_AIR;
        if (!v || !tile_ref(r, e, "tile", &tile) || tile == TILE_AIR || num(e, "one_in", 0) < 1)
            return fail(r, "\"vein_rares\": each needs a \"vein\" that exists, a \"tile\" and a \"one_in\"");
        v->rare = tile;
        v->rare_one_in = (int)num(e, "one_in", 0);
    }

    /* "poles": markers round the world. */
    cJSON_ArrayForEach(e, field(root, "poles")) {
        const char *name = str(e, "name");
        if (!name) return fail(r, "a pole has no \"name\"");
        int id = -1;
        for (int i = 0; i < g->pole_count; i++)
            if (strcmp(g->poles[i].name, name) == 0) id = i;
        if (id < 0) {
            if (g->pole_count >= MAX_POLES) return fail(r, "too many poles");
            id = g->pole_count++;
        }
        memset(&g->poles[id], 0, sizeof g->poles[id]);
        copy_name(g->poles[id].name, CONTENT_NAME_LEN, name);
        copy_name(g->poles[id].label, CONTENT_NAME_LEN, str(e, "label") ? str(e, "label") : name);
        g->poles[id].at = (float)num(e, "at", 0.0);
        g->poles[id].height = (int)num(e, "height", 10);
        if (!tile_ref(r, e, "tile", &g->poles[id].tile) || !tile_ref(r, e, "top", &g->poles[id].top)) return false;
        if (g->poles[id].tile == TILE_AIR || g->poles[id].height < 1 || g->poles[id].height > 64 || g->poles[id].at < -0.5f || g->poles[id].at > 0.5f)
            return fail(r, "pole \"%s\" needs a \"tile\", a \"height\" of 1 to 64 and an \"at\" from -0.5 to 0.5", name);
    }

    /* "cave_biomes": kinds of cave. */
    cJSON_ArrayForEach(e, field(root, "cave_biomes")) {
        const char *name = str(e, "name");
        if (!name) return fail(r, "a cave biome has no \"name\"");
        int id = -1;
        for (int i = 0; i < content.cave_biome_count; i++)
            if (strcmp(content.cave_biomes[i].name, name) == 0) id = i;
        if (id < 0) {
            if (content.cave_biome_count >= MAX_CAVE_BIOMES) return fail(r, "too many cave biomes");
            id = content.cave_biome_count++;
        }
        CaveBiomeDef *c = &content.cave_biomes[id];
        memset(c, 0, sizeof *c);
        copy_name(c->name, sizeof c->name, name);
        copy_name(c->label, sizeof c->label, str(e, "label") ? str(e, "label") : name);
        c->max_depth = 1000;
        pair(e, "depth", &c->min_depth, &c->max_depth);
        c->wavelength = (int)num(e, "wavelength", 160);
        c->share = (float)num(e, "share", 0.2);
        if (!tile_ref(r, e, "lining", &c->lining)) return false;
        if (c->share <= 0.0f || c->share > 1.0f || c->wavelength < 8 || c->max_depth < c->min_depth)
            return fail(r, "cave biome \"%s\": \"share\" is above 0 and at most 1, \"wavelength\" at least 8, and \"depth\" a range", name);
        const cJSON *one;
        for (int roof = 0; roof < 2; roof++) {
            cJSON_ArrayForEach(one, field(e, roof ? "roof" : "floor")) {
                int *count = roof ? &c->roof_count : &c->floor_count;
                TileType tile = TILE_AIR;
                if (*count >= MAX_CAVE_GROWTH || !tile_ref(r, one, "tile", &tile) || tile == TILE_AIR)
                    return fail(r, "cave biome \"%s\": at most %d things on its floor and as many on its roof, each with a \"tile\"", name, MAX_CAVE_GROWTH);
                (roof ? c->roof : c->floor)[*count].tile = tile;
                (roof ? c->roof : c->floor)[*count].one_in = (int)num(one, "one_in", 10);
                (roof ? c->roof : c->floor)[*count].hang_min = (roof ? c->roof : c->floor)[*count].hang_max = 1;
                pair(one, "hang", &(roof ? c->roof : c->floor)[*count].hang_min, &(roof ? c->roof : c->floor)[*count].hang_max);
                (*count)++;
            }
        }
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

    /* Things built: each a picture in rows of characters, with a legend that says what each character is. */
    cJSON_ArrayForEach(e, field(root, "structures")) {
        const char *name = str(e, "name");
        const cJSON *rows = field(e, "rows"), *legend = field(e, "legend"), *row;
        if (!name || !cJSON_IsArray(rows) || !legend) return fail(r, "a structure needs a \"name\", its \"rows\" and a \"legend\"");
        int id = -1;
        for (int i = 0; i < content.structure_count; i++)
            if (strcmp(content.structures[i].name, name) == 0) id = i;
        if (id < 0) {
            if (content.structure_count >= MAX_STRUCTURES) return fail(r, "too many structures");
            id = content.structure_count++;
        }
        StructureDef *s = &content.structures[id];
        memset(s, 0, sizeof *s);
        copy_name(s->name, sizeof s->name, name);
        s->height = cJSON_GetArraySize(rows);
        if (s->height < 1 || s->height > STRUCTURE_MAX_HEIGHT) return fail(r, "structure \"%s\" is 1 to %d rows tall", name, STRUCTURE_MAX_HEIGHT);
        int y = 0;
        cJSON_ArrayForEach(row, rows) {
            const char *text = cJSON_IsString(row) ? row->valuestring : "";
            int wide = (int)strlen(text);
            if (wide > STRUCTURE_MAX_WIDTH) return fail(r, "structure \"%s\" is at most %d wide", name, STRUCTURE_MAX_WIDTH);
            if (wide > s->width) s->width = wide;
            for (int x = 0; x < STRUCTURE_MAX_WIDTH; x++) {
                char key[2] = { x < wide ? text[x] : '~', '\0' };
                /* ('~' is the world left as it was; a space is open air; anything else is in the legend.) */
                if (key[0] == '~') {
                    s->cells[y][x].keep = true;
                    continue;
                }
                if (key[0] == ' ') continue;
                const cJSON *what = field(legend, key);
                if (!what) return fail(r, "structure \"%s\": '%s' is not in its legend", name, key);
                if (!tile_ref(r, what, "front", &s->cells[y][x].front) || !tile_ref(r, what, "back", &s->cells[y][x].back) ||
                    !tile_ref(r, what, "wall", &s->cells[y][x].wall)) return false;
            }
            y++;
        }
    }
    cJSON_ArrayForEach(e, field(root, "villages")) {
        const char *name = str(e, "name"), *where = str(e, "where");
        const cJSON *one;
        if (!name) return fail(r, "a village has no \"name\"");
        int id = -1;
        for (int i = 0; i < content.village_count; i++)
            if (strcmp(content.villages[i].name, name) == 0) id = i;
        if (id < 0) {
            if (content.village_count >= MAX_VILLAGE_KINDS) return fail(r, "too many kinds of village");
            id = content.village_count++;
        }
        VillageDef *v = &content.villages[id];
        memset(v, 0, sizeof *v);
        copy_name(v->name, sizeof v->name, name);
        v->where = !where || strcmp(where, "ground") == 0 ? VILLAGE_GROUND : strcmp(where, "cave") == 0 ? VILLAGE_CAVE : strcmp(where, "island") == 0 ? VILLAGE_ISLAND : -1;
        if (v->where < 0) return fail(r, "village \"%s\" is \"where\": \"ground\", \"cave\" or \"island\"", name);
        v->count_min = (int)num(e, "at_least", 1);
        v->per_1000_columns = (float)num(e, "per_1000_columns", 0.0);
        v->houses_min = 3, v->houses_max = 6;
        pair(e, "houses", &v->houses_min, &v->houses_max);
        if (!tile_ref(r, e, "street", &v->street) || !tile_ref(r, e, "ground", &v->ground)) return false;
        if (v->ground == TILE_AIR) return fail(r, "village \"%s\" needs \"ground\" to stand on", name);
        if (v->street == TILE_AIR) v->street = v->ground;
        v->centre = -1;
        for (int i = 0; i < content.structure_count; i++)
            if (str(e, "centre") && strcmp(content.structures[i].name, str(e, "centre")) == 0) v->centre = i;
        if (v->centre < 0) return fail(r, "village \"%s\" needs a \"centre\" that is a structure", name);
        cJSON_ArrayForEach(one, field(e, "buildings")) {
            int found = -1;
            for (int i = 0; cJSON_IsString(one) && i < content.structure_count; i++)
                if (strcmp(content.structures[i].name, one->valuestring) == 0) found = i;
            if (found < 0 || v->house_count >= MAX_VILLAGE_HOUSES) return fail(r, "village \"%s\" lists at most %d buildings that are structures", name, MAX_VILLAGE_HOUSES);
            v->houses[v->house_count++] = found;
        }
        if (v->house_count == 0) return fail(r, "village \"%s\" needs some \"buildings\"", name);
    }

    cJSON_ArrayForEach(e, field(root, "dimensions")) {
        const char *name = str(e, "name"), *kind = str(e, "generator");
        if (!name) return fail(r, "a dimension has no \"name\"");
        int id = -1;
        for (int i = 0; i < content.dimension_count; i++)
            if (strcmp(content.dimensions[i].name, name) == 0) id = i;
        if (id < 0) {
            if (content.dimension_count >= MAX_DIMENSIONS) return fail(r, "too many dimensions");
            id = content.dimension_count++;
        }
        DimensionDef *d = &content.dimensions[id];
        memset(d, 0, sizeof *d);
        copy_name(d->name, sizeof d->name, name);
        copy_name(d->label, sizeof d->label, str(e, "label") ? str(e, "label") : name);
        d->generator = !kind || strcmp(kind, "islands") == 0 ? DIM_ISLANDS : strcmp(kind, "caverns") == 0 ? DIM_CAVERNS : strcmp(kind, "void") == 0 ? DIM_VOID :
                       strcmp(kind, "grove") == 0 ? DIM_GROVE : strcmp(kind, "surface") == 0 ? DIM_SURFACE : strcmp(kind, "dungeon") == 0 ? DIM_DUNGEON : -1;
        if (d->generator < 0) return fail(r, "dimension \"%s\": its \"generator\" is \"islands\", \"caverns\", \"void\", \"grove\", \"surface\" or \"dungeon\"", name);
        {
            const cJSON *land = field(e, "surface");
            d->gravity = (float)num(e, "gravity", 1.0);
            d->airless = flag(e, "airless", false);
            d->surface_level = (float)num(land, "level", 0.4);
            d->surface_relief = (int)num(land, "relief", 10);
            d->crater_rate = (float)num(land, "craters", 0.0);
            d->tube_rate = (float)num(land, "tubes", 0.0);
            d->crater_min = 4, d->crater_max = 14;
            pair(land, "crater_radius", &d->crater_min, &d->crater_max);
            d->canyon_width = (int)num(land, "canyon_width", 0);
            d->canyon_depth = (int)num(land, "canyon_depth", 0);
        }
        d->qi = (float)num(e, "qi", 0.0);
        {
            const cJSON *grove = field(e, "grove"), *built = field(e, "structures"), *one;
            if (!tile_ref(r, grove, "trunk", &d->trunk) || !tile_ref(r, grove, "bough", &d->bough) || !tile_ref(r, grove, "leaf", &d->leaf) ||
                !tile_ref(r, grove, "vine", &d->vine) || !tile_ref(r, e, "bridge", &d->bridge)) return false;
            d->void_length = (int)num(field(e, "void"), "length", 15);
            d->void_depth = (int)num(field(e, "void"), "depth", 5);
            if (d->void_length < 4 || d->void_depth < 2) return fail(r, "dimension \"%s\": the island of a \"void\" is 4 or more long and 2 or more deep", name);
            d->trunk_width = (int)num(grove, "width", 7);
            d->trunk_gap = (int)num(grove, "gap", 60);
            d->structure_share = (float)num(e, "structure_share", 0.0);
            cJSON_ArrayForEach(one, built) {
                int found = -1;
                for (int i = 0; cJSON_IsString(one) && i < content.structure_count; i++)
                    if (strcmp(content.structures[i].name, one->valuestring) == 0) found = i;
                if (found < 0 || d->structure_count >= 24) return fail(r, "dimension \"%s\" lists at most 24 structures that exist", name);
                d->structures[d->structure_count++] = found;
            }
            if (d->generator == DIM_GROVE && (d->trunk == TILE_AIR || d->bough == TILE_AIR)) return fail(r, "dimension \"%s\": a grove needs a \"trunk\" and a \"bough\"", name);
        }
        d->width = (int)num(e, "width", 0);
        d->height = (int)num(e, "height", 200);
        if (d->height < 32 || d->width < 0) return fail(r, "dimension \"%s\" is at least 32 rows tall", name);
        d->sky = flag(e, "sky", d->generator != DIM_CAVERNS);
        d->falls_home = flag(e, "falls_home", false);
        d->climate_wavelength = (int)num(e, "climate_wavelength", 160);
        if (!tile_ref(r, e, "rock", &d->rock) || !tile_ref(r, e, "edge", &d->edge) || !tile_ref(r, e, "platform", &d->platform) ||
            !tile_ref(r, e, "cloud", &d->cloud) || !tile_ref(r, e, "magma", &d->magma)) return false;
        if (d->rock == TILE_AIR || d->edge == TILE_AIR) return fail(r, "dimension \"%s\" needs a \"rock\" and an \"edge\"", name);
        if (d->platform == TILE_AIR) d->platform = d->rock;
        const cJSON *isl = field(e, "islands"), *cav = field(e, "caverns"), *layer;
        d->island_rate = (float)num(isl, "per_100_columns", 6.0);
        d->island_min_length = 6; d->island_max_length = 40; d->island_min_depth = 4; d->island_max_depth = 16;
        pair(isl, "length", &d->island_min_length, &d->island_max_length);
        pair(isl, "depth", &d->island_min_depth, &d->island_max_depth);
        d->cavern_rate = (float)num(cav, "per_100_columns", 5.0);
        d->cavern_min_length = 40; d->cavern_max_length = 160;
        pair(cav, "length", &d->cavern_min_length, &d->cavern_max_length);
        d->cavern_min_radius = (float)num(cav, "radius_min", 5.0);
        d->cavern_max_radius = (float)num(cav, "radius_max", 14.0);
        d->lake_share = (float)num(cav, "lakes", 0.2);
        d->magma_rows = (int)num(e, "magma_rows", 0);
        cJSON_ArrayForEach(layer, field(e, "strata")) {
            if (d->strata_count >= MAX_STRATA) return fail(r, "dimension \"%s\" has too many strata", name);
            if (!tile_ref(r, layer, "tile", &d->strata[d->strata_count].tile)) return false;
            d->strata[d->strata_count++].from = (float)num(layer, "from", 0.5);
        }
        const cJSON *geode;
        cJSON_ArrayForEach(geode, field(e, "geodes")) {
            if (d->geode_count >= MAX_GEODES) return fail(r, "dimension \"%s\" has too many kinds of geode", name);
            if (!tile_ref(r, geode, "shell", &d->geodes[d->geode_count].shell) || !tile_ref(r, geode, "gem", &d->geodes[d->geode_count].gem)) return false;
            if (d->geodes[d->geode_count].shell == TILE_AIR) return fail(r, "dimension \"%s\": a geode needs a \"shell\"", name);
            d->geodes[d->geode_count].per_100_columns = (float)num(geode, "per_100_columns", 1.0);
            d->geodes[d->geode_count].min_radius = 4, d->geodes[d->geode_count].max_radius = 8;
            pair(geode, "radius", &d->geodes[d->geode_count].min_radius, &d->geodes[d->geode_count].max_radius);
            d->geodes[d->geode_count].max_depth = d->height;
            pair(geode, "depth", &d->geodes[d->geode_count].min_depth, &d->geodes[d->geode_count].max_depth);
            d->geode_count++;
        }
        d->ambient = (int)num(e, "ambient", 0);
        const cJSON *day = field(e, "sky_day"), *night = field(e, "sky_night");
        d->has_sky_color = cJSON_GetArraySize(day) == 3 && cJSON_GetArraySize(night) == 3;
        d->stars = flag(e, "stars", false);
        for (int k = 0; d->has_sky_color && k < 3; k++) {
            d->sky_day[k] = (unsigned char)cJSON_GetArrayItem(day, k)->valuedouble;
            d->sky_night[k] = (unsigned char)cJSON_GetArrayItem(night, k)->valuedouble;
        }
        copy_name(d->tree_name, sizeof d->tree_name, str(e, "tree"));
        d->tree = -1;
    }

    /* (Islands name trees, which are read after this: the names are kept and matched up at the end.) */
    const cJSON *isles = field(root, "islands"), *one_tree, *pick;
    if (isles) {
        g->island_rate = (float)num(isles, "per_100_columns", 0.0);
        g->island_min_y = 80;
        g->island_max_y = 180;
        pair(isles, "height", &g->island_min_y, &g->island_max_y);
        g->island_min_length = 15, g->island_max_length = 30, g->island_min_depth = 10, g->island_max_depth = 15;
        g->island_min_soil = 2, g->island_max_soil = 3;
        pair(isles, "length", &g->island_min_length, &g->island_max_length);
        pair(isles, "depth", &g->island_min_depth, &g->island_max_depth);
        pair(isles, "soil_depth", &g->island_min_soil, &g->island_max_soil);
        g->island_jag = (int)num(isles, "jag", 0);
        g->island_min_cave = g->island_max_cave = 0;
        pair(isles, "cave", &g->island_min_cave, &g->island_max_cave);
        if (g->island_jag < 0 || g->island_jag > 8 || g->island_min_cave < 0 || g->island_max_cave < g->island_min_cave)
            return fail(r, "\"islands\": \"jag\" is 0 to 8, and \"cave\" a least and a most height");
        if (!tile_ref(r, isles, "surface", &g->island_surface) || !tile_ref(r, isles, "soil", &g->island_soil) ||
            !tile_ref(r, isles, "base", &g->island_base))
            return false;
        if (g->island_rate > 0.0f && (g->island_surface == TILE_AIR || g->island_soil == TILE_AIR || g->island_base == TILE_AIR ||
                                       g->island_min_length < 3 || g->island_max_length < g->island_min_length ||
                                       g->island_min_depth < 1 || g->island_max_depth < g->island_min_depth))
            return fail(r, "\"islands\" need a \"surface\", a \"soil\" and a \"base\" block, a \"length\" and a \"depth\"");
        g->island_tree_chance = (float)num(isles, "tree_chance", 0.0);
        g->island_tree_count = 0;
        cJSON_ArrayForEach(one_tree, field(isles, "trees")) {
            if (g->island_tree_count >= MAX_ISLAND_TREES || !str(one_tree, "tree"))
                return fail(r, "\"islands\" list at most %d trees, each with a \"tree\"", MAX_ISLAND_TREES);
            copy_name(island_tree_names[g->island_tree_count], CONTENT_NAME_LEN, str(one_tree, "tree"));
            g->island_trees[g->island_tree_count].weight = (int)num(one_tree, "weight", 1);
            g->island_trees[g->island_tree_count++].min_y = (int)num(one_tree, "above", 0);
        }
    }
    /* "island_trees": more trees for the islands there already are. */
    cJSON_ArrayForEach(one_tree, field(root, "island_trees")) {
        if (g->island_tree_count >= MAX_ISLAND_TREES || !str(one_tree, "tree"))
            return fail(r, "\"island_trees\": there can be %d in all, each with a \"tree\"", MAX_ISLAND_TREES);
        copy_name(island_tree_names[g->island_tree_count], CONTENT_NAME_LEN, str(one_tree, "tree"));
        g->island_trees[g->island_tree_count].weight = (int)num(one_tree, "weight", 1);
        g->island_trees[g->island_tree_count++].min_y = (int)num(one_tree, "above", 0);
    }
    const cJSON *options = field(root, "options");
    if (options) {
        g->size_count = g->scale_count = 0;
        cJSON_ArrayForEach(pick, field(options, "sizes")) {
            if (g->size_count >= MAX_WORLD_CHOICES || !str(pick, "label") || num(pick, "width", 0) < 64)
                return fail(r, "a world size needs a \"label\" and a \"width\" of 64 or more; there can be %d", MAX_WORLD_CHOICES);
            copy_name(g->sizes[g->size_count].label, CONTENT_NAME_LEN, str(pick, "label"));
            g->sizes[g->size_count++].width = (int)num(pick, "width", 0);
        }
        cJSON_ArrayForEach(pick, field(options, "scales")) {
            if (g->scale_count >= MAX_WORLD_CHOICES || !str(pick, "label") || num(pick, "scale", 0) <= 0)
                return fail(r, "a scale needs a \"label\" and a \"scale\" above 0; there can be %d", MAX_WORLD_CHOICES);
            copy_name(g->scales[g->scale_count].label, CONTENT_NAME_LEN, str(pick, "label"));
            g->scales[g->scale_count++].scale = (float)num(pick, "scale", 1.0);
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

/* "cultivation": the realms, and the rules of the road through them. */
static bool read_dao(Reader *r, const cJSON *road)
{
    DaoDef *d = &content.dao;
    const cJSON *e, *need, *one;
    if (!road) return true;
    memset(d, 0, sizeof *d);
    copy_name(d->immortal, sizeof d->immortal, str(road, "immortal") ? str(road, "immortal") : "Immortal");
    read_boons(field(road, "immortal_boons"), &d->immortal_boons);
    d->stage_gain = (float)num(road, "stage_gain", 0.02);
    d->rate = (float)num(road, "rate", 0.05);
    d->chance_first = (float)num(road, "chance_first", 0.95);
    d->chance_last = (float)num(road, "chance_last", 0.6);
    d->fail_loss = (float)num(road, "fail_loss", 0.3);
    d->toxin_fade = (float)num(road, "toxin_fade", 0.00005);
    cJSON_ArrayForEach(e, field(road, "realms")) {
        if (d->realm_count >= MAX_REALMS) return fail(r, "too many realms");
        RealmDef *realm = &d->realms[d->realm_count++];
        copy_name(realm->label, sizeof realm->label, str(e, "label") ? str(e, "label") : "Realm");
        realm->stages = (int)num(e, "stages", 10);
        realm->qi_first = (float)num(e, "qi", 100.0);
        realm->qi_growth = (float)num(e, "growth", 1.3);
        read_boons(field(e, "boons"), &realm->boons);
        if (!item_ref(r, e, "pill", &realm->pill)) return false;
        realm->bolts = (int)num(e, "bolts", 0);
        realm->bolt = (float)num(e, "bolt", 0.2);
        realm->revives = flag(e, "revives", false);
        realm->temper_qi = (float)num(e, "temper_qi", 0.0);
        realm->core = flag(e, "core", false);
        realm->domain = (int)num(e, "domain", 0);
        realm->soul = flag(e, "soul", false);
        if (realm->stages < 1 || realm->qi_first <= 0.0f) return fail(r, "a realm has at least one stage, and holds some qi");
    }
    int total = 0;
    for (int i = 0; i < d->realm_count; i++) total += d->realms[i].stages;
    if (total + 2 > MAX_DAO_STAGES) return fail(r, "the road has more than %d stages", MAX_DAO_STAGES - 2);
    cJSON_ArrayForEach(need, field(road, "needs")) {
        int stage = (int)num(need, "stage", 0);
        if (stage < 1 || stage > total) return fail(r, "\"needs\": a \"stage\" is 1 to %d", total);
        d->need_count[stage] = 0;
        d->need_insight[stage] = (int)num(need, "insight", 0);
        cJSON_ArrayForEach(one, field(need, "items")) {
            if (d->need_count[stage] >= 3) return fail(r, "stage %d needs more than 3 kinds of thing", stage);
            if (!stack_ref(r, one, &d->needs[stage][d->need_count[stage]++])) return false;
        }
    }
    d->manual_costs = flag(road, "manual_costs_stage", false);
    d->death_costs = flag(road, "death_costs_qi", false);
    d->scorch = TILE_AIR;
    if (!tile_ref(r, road, "scorch", &d->scorch)) return false;
    const cJSON *called = field(road, "called");
    d->called_bolts = (int)num(called, "bolts", 0);
    d->called_bolt = (float)num(called, "bolt", 0.3);
    if (field(called, "leaves") && !stack_ref(r, field(called, "leaves"), &d->called_leaves)) return false;
    static const char *usual[DAO_ELEMENTS] = { "Metal", "Wood", "Water", "Fire", "Earth" };
    for (int i = 0; i < DAO_ELEMENTS; i++) {
        const cJSON *named = cJSON_GetArrayItem(field(road, "elements"), i);
        copy_name(d->elements[i], CONTENT_NAME_LEN, cJSON_IsString(named) ? named->valuestring : usual[i]);
    }
    return true;
}

static bool read_rules(Reader *r, const cJSON *root)
{
    if (!read_dao(r, field(root, "cultivation"))) return false;
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
            u->seasons[u->season_count].cold = flag(season, "cold", flag(season, "snows", false));
            u->seasons[u->season_count].freezes = flag(season, "freezes", false);
            u->seasons[u->season_count++].thaws = flag(season, "thaws", false);
        }
    }
    const cJSON *env = field(root, "environment");
    if (env) {
        u->env_fall_ticks = (int)num(env, "fall_ticks", u->env_fall_ticks);
        u->env_rise_ticks = (int)num(env, "rise_ticks", u->env_rise_ticks);
        u->env_harm_ticks = (int)num(env, "harm_ticks", u->env_harm_ticks);
        u->env_clothed = (float)num(env, "clothed", u->env_clothed);
    }
    if (field(root, "happiness")) {
        u->happy_slow = (float)num(field(root, "happiness"), "slow", u->happy_slow);
        u->happy_boost = (float)num(field(root, "happiness"), "boost", u->happy_boost);
        if (u->happy_boost < 0.0f) return fail(r, "\"happiness\": \"boost\" is a share of speed gained: not less than nothing");
        if (u->happy_slow < 0.0f || u->happy_slow >= 1.0f) return fail(r, "\"happiness\": \"slow\" is a share of speed lost, 0 up to but not 1");
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
        u->flail_time = (float)num(player, "flail_time", u->flail_time);
        u->max_drop = (int)num(player, "max_drop", u->max_drop);
        u->fly_speed = (float)num(player, "fly_speed", u->fly_speed > 0.0f ? u->fly_speed : 2.5);
        if (u->fly_speed <= 0.0f) return fail(r, "\"fly_speed\" is so many times the pace of walking: more than nothing");
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
        /* ("also": more that it brings, on top of whatever it brought before: how a mod adds to the list.) */
        cJSON_ArrayForEach(e, field(meditation, "also")) {
            ItemId item = ITEM_NONE;
            if (u->loot_count >= MAX_LOOT || !str(e, "item") || !item_ref(r, e, "item", &item)) return fail(r, "\"also\": something meditation brings needs an \"item\", and there is room for %d", MAX_LOOT);
            u->loot[u->loot_count].item = item;
            u->loot[u->loot_count++].weight = (int)num(e, "weight", 1) > 0 ? (int)num(e, "weight", 1) : 1;
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
        u->dungeon_health = (int)num(combat, "health", u->dungeon_health);
        u->attack_time = (float)num(combat, "attack_time", u->attack_time);
        u->poison_damage = (float)num(combat, "poison_damage", u->poison_damage);
    }
    /* "dungeons": how hard they may be made, each harder than the last and giving better. */
    const cJSON *dungeons = field(root, "dungeons");
    if (dungeons) {
        const cJSON *e, *one;
        u->upgrade_cost = (float)num(dungeons, "upgrade_cost", u->upgrade_cost);
        u->upgrade_steps = (int)num(dungeons, "upgrade_steps", u->upgrade_steps);
        int grade = 1;
        cJSON_ArrayForEach(e, field(dungeons, "grades")) {
            if (grade >= MAX_GRADES) return fail(r, "\"dungeons\": at most %d grades", MAX_GRADES - 1);
            copy_name(u->grade_labels[grade], CONTENT_NAME_LEN, str(e, "label"));
            int k = 0;
            cJSON_ArrayForEach(one, field(e, "color"))
                if (k < 3) u->grade_colors[grade][k++] = (unsigned char)one->valuedouble;
            grade++;
        }
        if (field(dungeons, "difficulties")) u->difficulty_count = 0;
        cJSON_ArrayForEach(e, field(dungeons, "difficulties")) {
            if (u->difficulty_count >= MAX_DIFFICULTIES) return fail(r, "\"dungeons\": at most %d difficulties", MAX_DIFFICULTIES);
            DifficultyDef *d = &u->difficulties[u->difficulty_count++];
            memset(d, 0, sizeof *d);
            copy_name(d->label, sizeof d->label, str(e, "label"));
            d->health = (float)num(e, "health", 1.0);
            d->damage = (float)num(e, "damage", 1.0);
            d->from = (float)num(e, "from", 0.0);
            d->to = (float)num(e, "to", 1.0);
            d->stone = (float)num(e, "stone", 0.0);
            d->gold = (float)num(e, "gold", 1.0);
            d->more = (int)num(e, "more", 0);
            d->loot_min = d->loot_max = 3;
            pair(e, "loot", &d->loot_min, &d->loot_max);
            int k = 1;
            cJSON_ArrayForEach(one, field(e, "grades"))
                if (k < MAX_GRADES) d->grades[k++] = (int)one->valuedouble;
        }
    }
    if (field(root, "questbook")) u->questbook = flag(root, "questbook", false);
    const cJSON *teleport = field(root, "teleport");
    if (teleport) {
        copy_name(u->teleport_bench, sizeof u->teleport_bench, str(teleport, "bench"));
        u->teleport_level = (int)num(teleport, "level", 1);
        u->teleport_cost = (int)num(teleport, "cost", 0);
        u->teleport_free = (int)num(teleport, "free_distance", 0);
        u->teleport_per = (int)num(teleport, "per_blocks", 0);
    }
    if (str(field(root, "players"), "skin")) copy_name(u->player_skin, sizeof u->player_skin, str(field(root, "players"), "skin"));
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
        u->light_glow = (int)num(light, "glow", u->light_glow);
        u->light_darkness = num(light, "darkness", u->light_darkness);
        u->light_contrast = num(light, "contrast", u->light_contrast);
        u->light_bloom = num(light, "bloom", u->light_bloom);
        if (u->light_darkness < 0.0f || u->light_darkness > 1.0f || u->light_contrast < 0.0f || u->light_bloom < 0.0f || u->light_bloom > 1.0f)
            return fail(r, "rules: light \"darkness\" and \"bloom\" are 0 to 1, and \"contrast\" is not less than 0");
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
        m->spawn_sky = flag(field(e, "spawn"), "sky", false);
        m->spawn_cave = flag(field(e, "spawn"), "cave", false);
        const char *when = str(field(e, "spawn"), "time");
        m->spawn_time = !when ? 0 : strcmp(when, "day") == 0 ? 1 : strcmp(when, "night") == 0 ? 2 : -1;
        if (m->spawn_time < 0) return fail(r, "mob \"%s\": the \"time\" it spawns at is \"day\" or \"night\"", name);
        m->flies = flag(e, "flies", false);
        m->tame = (int)num(e, "tame", 0);
        {
            const cJSON *ship = field(e, "ship"), *boarding = field(e, "boarding");
            m->gun_damage = (int)num(ship, "guns", 0);
            m->gun_range = ship ? (int)num(ship, "range", 8) : 0;
            m->wreck = m->deck = TILE_AIR;
            if (!tile_ref(r, ship, "wreck", &m->wreck) || !tile_ref(r, boarding, "deck", &m->deck)) return false;
            copy_name(m->crew, sizeof m->crew, str(boarding, "crew"));
            copy_name(m->captain, sizeof m->captain, str(boarding, "captain"));
            m->crew_count = (int)num(boarding, "count", 0);
        }
        m->mount = field(e, "mount") != NULL;
        m->mount_speed = (float)num(field(e, "mount"), "speed", 1.5);
        m->strike_share = (float)num(field(e, "strike"), "share", 0.0);
        m->strike_range = (int)num(field(e, "strike"), "range", 6);
        m->strike_rest = (int)num(field(e, "strike"), "rest_ticks", 60);
        m->strike_power = (long long)num(field(e, "strike"), "power", 0);
        m->sting_power = (long long)num(field(e, "sting"), "power", 0);
        m->boss = flag(e, "boss", false);
        {
            const cJSON *model = field(e, "model"), *one;
            static const char *moves[] = { "still", "leg", "leg2", "wing", "wing2", "tail", "head", "wave", "sway" };
            m->model_part_count = 0;
            copy_name(m->model_skin, sizeof m->model_skin, str(model, "skin"));
            pair(model, "atlas", &m->model_w, &m->model_h);
            m->model_bob = (float)num(model, "bob", 0.0);
            m->model_hop = (float)num(model, "hop", 0.0);
            m->model_turn = (float)num(model, "turn", 12.0);
            cJSON_ArrayForEach(one, field(model, "parts")) {
                if (m->model_part_count >= MAX_MODEL_PARTS) return fail(r, "creature \"%s\": its model has more than %d parts", m->name, MAX_MODEL_PARTS);
                ModelPart *part = &m->model_parts[m->model_part_count++];
                const cJSON *at = field(one, "at"), *box = field(one, "box"), *uv = field(one, "uv");
                memset(part, 0, sizeof *part);
                if (cJSON_GetArraySize(at) != 3 || cJSON_GetArraySize(box) != 6 || cJSON_GetArraySize(uv) != 2)
                    return fail(r, "creature \"%s\": a part of its model wants \"at\" [x, y, z], \"box\" [x, y, z, wide, high, deep] and \"uv\" [u, v]", m->name);
                for (int k = 0; k < 3; k++) {
                    part->pivot[k] = (float)cJSON_GetArrayItem(at, k)->valuedouble;
                    part->corner[k] = (float)cJSON_GetArrayItem(box, k)->valuedouble;
                    part->size[k] = (float)cJSON_GetArrayItem(box, 3 + k)->valuedouble;
                }
                part->u = (short)cJSON_GetArrayItem(uv, 0)->valuedouble;
                part->v = (short)cJSON_GetArrayItem(uv, 1)->valuedouble;
                part->mirror = flag(one, "mirror", false);
                part->by = (float)num(one, "by", 0.0);
                part->lag = (float)num(one, "lag", 0.0);
                const char *move = str(one, "move");
                for (int k = 0; move && k < 9; k++)
                    if (strcmp(move, moves[k]) == 0) part->move = (unsigned char)k;
            }
        }
        m->companion = field(e, "companion") != NULL;
        m->companion_bite = (int)num(field(e, "companion"), "bite", 2);
        m->companion_sight = (int)num(field(e, "companion"), "sight", 8);
        m->immune = flag(e, "immune", false);
        m->weak_to = element_ref(str(e, "weak_to"));
        cJSON_ArrayForEach(one, field(field(e, "spawn"), "above")) {
            TileType t = cJSON_IsString(one) ? tile_id(one->valuestring) : -1;
            if (t < 0) return fail(r, "mob \"%s\": it spawns above a tile that does not exist", name);
            if (m->spawn_above_count < MAX_MOB_SPAWN_TILES) m->spawn_above[m->spawn_above_count++] = t;
        }
        m->cap = (int)num(e, "cap", 0);
        if (str(e, "caged") && !item_ref(r, e, "caged", &m->caged)) return false;
        m->health = (int)num(e, "health", 1);
        m->health_unit = (long long)num(e, "health_unit", 1);
        if (m->health_unit < 1) m->health_unit = 1;
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
        {
            const cJSON *sea;
            m->spawn_seas = 0;
            cJSON_ArrayForEach(sea, field(field(e, "spawn"), "seas")) {
                int found = -1;
                for (int i = 0; cJSON_IsString(sea) && i < content.worldgen.sea_count; i++)
                    if (strcmp(content.worldgen.seas[i].name, sea->valuestring) == 0) found = i;
                if (found < 0) return fail(r, "mob \"%s\" spawns in a sea there is not", name);
                m->spawn_seas |= 1u << found;
            }
        }
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
        t->categories[id].free_only = flag(e, "free_only", t->categories[id].free_only);
        const cJSON *also;
        cJSON_ArrayForEach(also, field(e, "also")) {
            if (!cJSON_IsString(also)) continue;
            if (t->categories[id].also_count >= MAX_CATEGORY_NAMES) return fail(r, "category \"%s\" goes by too many names", name);
            copy_name(t->categories[id].also[t->categories[id].also_count++], CONTENT_NAME_LEN, also->valuestring);
        }
    }
    if (field(root, "tier_prices")) t->tier_count = 0;
    cJSON_ArrayForEach(e, field(root, "tier_prices")) {
        if (t->tier_count >= MAX_TRADE_TIERS) return fail(r, "too many tiers");
        t->tier_prices[t->tier_count++] = (int)e->valuedouble;
    }
    const cJSON *villages = field(root, "villages");
    if (villages) {
        t->villages_only = flag(villages, "only", t->villages_only);
        t->village_swing = (float)num(villages, "swing", t->village_swing);
        t->village_share = (float)num(villages, "share", t->village_share);
        t->village_days = (int)num(villages, "days", t->village_days);
        t->village_specialities = (int)num(villages, "specialities", t->village_specialities);
        if (t->village_swing < 0.0f || t->village_swing >= 1.0f || t->village_share < 0.0f || t->village_share > 1.0f)
            return fail(r, "\"villages\": \"swing\" and \"share\" are shares, 0 to 1");
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
/* Which dimension a name is: -1 for none given. False if it names one there is not. */
static bool dimension_named(const char *name, int *out)
{
    *out = -1;
    if (!name[0]) return true;
    for (int i = 0; i < content.dimension_count; i++)
        if (strcmp(content.dimensions[i].name, name) == 0) *out = i;
    return *out >= 0;
}

/* The levels mods put on the end of a kind of bench: each is hung on whatever is highest when its turn comes. */
static bool extend_benches(Reader *r)
{
    for (int i = 1; i < content.tile_count; i++) {
        TileDef *t = &content.tiles[i];
        if (!t->extends) continue;
        if (!t->bench && !t->trade) return fail(r, "tile \"%s\" extends nothing: it is neither a bench nor a block to trade at", t->name);
        /* (Undoing what an earlier pass over the same tables did.) */
        for (int k = 1; k < content.tile_count; k++)
            if (content.tiles[k].upgrade_to == i) {
                content.tiles[k].upgrade_to = TILE_AIR;
                content.tiles[k].upgrade_cost_count = 0;
                content.tiles[k].upgrade_crystals = 0;
            }
        int top = -1;
        for (int k = 1; k < content.tile_count; k++) {
            const TileDef *o = &content.tiles[k];
            if (k == i || (o->extends && k > i) || o->upgrade_to != TILE_AIR) continue;
            if (t->bench ? !(o->bench && strcmp(o->bench_kind, t->bench_kind) == 0) : !(o->trade && !o->trade_free && !o->bench)) continue;
            int level = t->bench ? o->bench_level : o->trade_level;
            if (top < 0 || level > (t->bench ? content.tiles[top].bench_level : content.tiles[top].trade_level)) top = k;
        }
        if (top < 0) return fail(r, "tile \"%s\" extends a kind of bench there is none of", t->name);
        TileDef *under = &content.tiles[top];
        under->upgrade_to = (TileType)i;
        under->upgrade_cost_count = t->extend_cost_count;
        memcpy(under->upgrade_cost, t->extend_cost, sizeof t->extend_cost);
        under->upgrade_crystals = t->extend_crystals;
        if (t->bench) t->bench_level = under->bench_level + 1;
        else t->trade_level = under->trade_level + 1;
    }
    for (int i = 0; i < content.recipe_count; i++) {
        Recipe *rc = &content.recipes[i];
        if (!rc->at[0]) continue;
        TileType at = tile_id(rc->at);
        if (at <= 0 || !tile_def(at)->bench) return fail(r, "a recipe is made at \"%s\", which is not a bench", rc->at);
        copy_name(rc->bench, sizeof rc->bench, tile_def(at)->bench_kind);
        rc->level = tile_def(at)->bench_level;
    }
    return true;
}

static bool validate(Reader *r)
{
    const WorldGenDef *g = &content.worldgen;
    const RulesDef *u = &content.rules;
    r->file = "data";

    if (!extend_benches(r)) return false;
    /* A piston's head is a thing of the piston's: it knows which way it was pushed out, and is watched like the rest. */
    for (int i = 1; i < content.tile_count; i++) content.tiles[i].is_piston_head = false;
    for (int i = 1; i < content.tile_count; i++) {
        const TileDef *p = &content.tiles[i];
        if (!p->piston || p->piston_head <= TILE_AIR || p->piston_head >= content.tile_count) continue;
        TileDef *head = &content.tiles[p->piston_head];
        head->is_piston_head = true;
        head->device = true;
        head->piston_dir[0] = p->piston_dir[0], head->piston_dir[1] = p->piston_dir[1];
    }
    for (int i = 0; i < content.biome_count; i++)
        if (!dimension_named(content.biomes[i].dimension_name, &content.biomes[i].dimension))
            return fail(r, "biome \"%s\" is of the dimension \"%s\", which does not exist", content.biomes[i].name, content.biomes[i].dimension_name);
    for (int i = 0; i < content.vein_count; i++)
        if (!dimension_named(content.veins[i].dimension_name, &content.veins[i].dimension))
            return fail(r, "vein \"%s\" is of a dimension that does not exist", content.veins[i].name);
    for (int i = 0; i < content.decor_count; i++)
        if (!dimension_named(content.decor[i].dimension_name, &content.decor[i].dimension))
            return fail(r, "cave decoration \"%s\" is of a dimension that does not exist", content.decor[i].name);
    for (int i = 0; i < content.tile_count; i++)
        if (!dimension_named(content.tiles[i].leads_to, &content.tiles[i].leads_dim))
            return fail(r, "tile \"%s\" leads to \"%s\", which is not a dimension", content.tiles[i].name, content.tiles[i].leads_to);
    for (int i = 0; i < content.dimension_count; i++) {
        DimensionDef *d = &content.dimensions[i];
        d->tree = d->tree_name[0] ? tree_id(d->tree_name) : -1;
        if (d->tree_name[0] && d->tree < 0) return fail(r, "dimension \"%s\" grows \"%s\", which is not a tree", d->name, d->tree_name);
    }

    for (int i = 0; i < content.item_count; i++) {
        ItemDef *it = &content.items[i];
        it->category = -1;
        for (int c = 0; c < content.trade.category_count; c++)
            if (strcmp(content.trade.categories[c].name, it->category_name) == 0) it->category = c;
        /* (Failing that, a category that also goes by the name.) */
        for (int c = 0; c < content.trade.category_count && it->category < 0 && it->category_name[0]; c++)
            for (int a = 0; a < content.trade.categories[c].also_count; a++)
                if (strcmp(content.trade.categories[c].also[a], it->category_name) == 0) it->category = c;
        /* With no tier of its own, the highest whose price it reaches. */
        it->trade_tier = it->tier > 0 ? it->tier : 1;
        for (int k = 0; it->tier == 0 && k < content.trade.tier_count; k++)
            if ((it->price > 0 ? it->price : content.trade.default_price) >= content.trade.tier_prices[k]) it->trade_tier = k + 1;
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
        it->cooked = it->cooked_said == 1;
        it->set = -1;
        for (int k = 0; k < content.set_count; k++)
            if (strcmp(content.sets[k].name, it->set_name) == 0) it->set = k;
        if (it->set_name[0] && it->set < 0) return fail(r, "item \"%s\" is of the set \"%s\", which does not exist", it->name, it->set_name);
        if (it->relic && it->max_stack != 1) return fail(r, "item \"%s\" is a relic: it must have a \"max_stack\" of 1", it->name);
        if (i == 0) content.has_relics = false;
        content.has_relics = content.has_relics || it->relic;
        it->plants = it->plants_name[0] ? tree_id(it->plants_name) : -1;
        if (it->plants_name[0] && it->plants < 0)
            return fail(r, "item \"%s\" plants a tree called \"%s\", which does not exist", it->name,
                        it->plants_name);
    }
    /* What is part of a tree: trunks and leaves, and what they decay into. */
    for (int i = 0; i < content.tile_count; i++) content.tiles[i].of_tree = content.tiles[i].log_of >= 0 || content.tiles[i].leaves_of >= 0;
    for (int pass = 0; pass < 3; pass++)
        for (int i = 0; i < content.tile_count; i++)
            if (content.tiles[i].of_tree && content.tiles[i].decays_into != TILE_AIR) content.tiles[content.tiles[i].decays_into].of_tree = true;
    /* A loot pool's biomes, by name: one the game has not got (another mod's, which is not on) is left out, and a
     * pool that named some and has none of them left is nowhere. */
    for (int i = 0; i < content.pool_count; i++) {
        PoolDef *pool = &content.pools[i];
        pool->biome_count = 0;
        pool->any_biome = pool->biome_name_count == 0;
        for (int k = 0; k < pool->biome_name_count; k++)
            for (int b = 0; b < content.biome_count; b++)
                if (strcmp(content.biomes[b].name, pool->biome_names[k]) == 0) pool->biomes[pool->biome_count++] = b;
    }
    /* Food that some recipe makes is cooked, unless the data has said otherwise of it. */
    for (int i = 0; i < content.recipe_count; i++) {
        ItemDef *made = &content.items[content.recipes[i].output.id];
        if (content.recipes[i].output.id > 0 && content.recipes[i].output.id < content.item_count && made->cooked_said < 0 && made->food > 0.0f) made->cooked = true;
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
    for (int i = 0; i < content.worldgen.island_tree_count; i++) {
        content.worldgen.island_trees[i].tree = tree_id(island_tree_names[i]);
        if (content.worldgen.island_trees[i].tree < 0)
            return fail(r, "the islands name a tree that does not exist: \"%s\"", island_tree_names[i]);
    }
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

/* "quests.json": tabs, and the quests in them. Either of the same name as one already there takes its place. */
static bool read_quests(Reader *r, const cJSON *root)
{
    const cJSON *e, *one;
    cJSON_ArrayForEach(e, field(root, "tabs")) {
        const char *name = str(e, "name");
        if (!name) return fail(r, "a tab has no \"name\"");
        int id = -1;
        for (int i = 0; i < content.quest_tab_count; i++)
            if (strcmp(content.quest_tabs[i].name, name) == 0) id = i;
        if (id < 0) {
            if (content.quest_tab_count >= MAX_QUEST_TABS) return fail(r, "too many tabs of quests");
            id = content.quest_tab_count++;
        }
        QuestTabDef *t = &content.quest_tabs[id];
        memset(t, 0, sizeof *t);
        copy_name(t->name, sizeof t->name, name);
        copy_name(t->label, sizeof t->label, str(e, "label") ? str(e, "label") : name);
        t->order = (int)num(e, "order", 100);
        if (str(e, "icon") && !item_ref(r, e, "icon", &t->icon)) return false;
    }
    cJSON_ArrayForEach(e, field(root, "quests")) {
        const char *name = str(e, "name"), *tab = str(e, "tab");
        if (!name || !tab) return fail(r, "a quest has no \"name\" or no \"tab\"");
        int id = -1;
        for (int i = 0; i < content.quest_count; i++)
            if (strcmp(content.quests[i].name, name) == 0) id = i;
        if (id < 0) {
            if (content.quest_count >= MAX_QUESTS) return fail(r, "too many quests");
            id = content.quest_count++;
        }
        QuestDef *q = &content.quests[id];
        memset(q, 0, sizeof *q);
        copy_name(q->name, sizeof q->name, name);
        snprintf(q->label, sizeof q->label, "%s", str(e, "label") ? str(e, "label") : name);
        snprintf(q->text, sizeof q->text, "%s", str(e, "text") ? str(e, "text") : "");
        q->tab = -1;
        for (int i = 0; i < content.quest_tab_count; i++)
            if (strcmp(content.quest_tabs[i].name, tab) == 0) q->tab = i;
        if (q->tab < 0) return fail(r, "quest \"%s\" is in the tab \"%s\", which does not exist", name, tab);
        cJSON_ArrayForEach(one, field(e, "needs")) {
            if (!cJSON_IsString(one) || q->need_count >= MAX_QUEST_NEEDS) return fail(r, "quest \"%s\": \"needs\" is at most %d quests, by name", name, MAX_QUEST_NEEDS);
            copy_name(q->need_names[q->need_count++], CONTENT_NAME_LEN, one->valuestring);
        }
        const cJSON *task = field(e, "task");
        q->count = (int)num(task, "count", 1);
        if (q->count < 1) q->count = 1;
        ItemId item = ITEM_NONE;
        if (str(task, "have")) {
            if (!item_ref(r, task, "have", &item)) return false;
            q->kind = QUEST_HAVE, q->target = item;
        } else if (str(task, "kill")) {
            q->kind = QUEST_KILL, q->target = mob_id(str(task, "kill"));
            if (q->target < 0) return fail(r, "quest \"%s\": there is no creature \"%s\"", name, str(task, "kill"));
        } else if (str(task, "near")) {
            q->kind = QUEST_NEAR, q->target = tile_id(str(task, "near"));
            if (q->target == TILE_AIR) return fail(r, "quest \"%s\": there is no block \"%s\"", name, str(task, "near"));
        } else if (str(task, "in")) {
            q->kind = QUEST_IN, q->target = -1;
            for (int i = 0; i < content.dimension_count; i++)
                if (strcmp(content.dimensions[i].name, str(task, "in")) == 0) q->target = i;
            if (q->target < 0) return fail(r, "quest \"%s\": there is no dimension \"%s\"", name, str(task, "in"));
        } else if (str(task, "biome")) {
            q->kind = QUEST_BIOME, q->target = -1;
            for (int i = 0; i < content.biome_count; i++)
                if (strcmp(content.biomes[i].name, str(task, "biome")) == 0) q->target = i;
            if (q->target < 0) return fail(r, "quest \"%s\": there is no biome \"%s\"", name, str(task, "biome"));
        } else if (field(task, "stage")) {
            q->kind = QUEST_STAGE, q->target = (int)num(task, "stage", 1);
        } else if (flag(task, "check", false)) {
            q->kind = QUEST_CHECK;
        } else {
            return fail(r, "quest \"%s\": its \"task\" is one of \"have\", \"kill\", \"near\", \"in\", \"biome\", \"stage\" or \"check\"", name);
        }
        cJSON_ArrayForEach(one, field(e, "rewards")) {
            if (q->reward_count >= MAX_QUEST_REWARDS) return fail(r, "quest \"%s\": at most %d rewards", name, MAX_QUEST_REWARDS);
            if (!item_ref(r, one, "item", &q->rewards[q->reward_count].item)) return false;
            q->rewards[q->reward_count++].count = (int)num(one, "count", 1);
        }
        q->pick = flag(e, "pick", false) && q->reward_count > 1;
        q->hub = flag(e, "hub", false);
        q->crystals = (int)num(e, "crystals", 0);
        pair(e, "at", &q->x, &q->y);
        q->icon = q->kind == QUEST_HAVE ? item : q->reward_count > 0 ? q->rewards[0].item : ITEM_NONE;
        if (str(e, "icon") && !item_ref(r, e, "icon", &q->icon)) return false;
    }
    /* What each waits on, now that they are all there. */
    for (int i = 0; i < content.quest_count; i++) {
        QuestDef *q = &content.quests[i];
        for (int k = 0; k < q->need_count; k++) {
            q->needs[k] = -1;
            for (int o = 0; o < content.quest_count; o++)
                if (o != i && strcmp(content.quests[o].name, q->need_names[k]) == 0) q->needs[k] = o;
            if (q->needs[k] < 0) return fail(r, "quest \"%s\" needs \"%s\", which does not exist", q->name, q->need_names[k]);
        }
    }
    return true;
}

bool content_load(const char *dir, char *error, size_t error_size)
{
    static const char *files[] = { "tiles.json", "items.json", "recipes.json", "trees.json",
                                   "worldgen.json", "rules.json", "trade.json", "mobs.json", "quests.json" };
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
        read_mobs, read_quests,
    };
    for (int i = 0; i < FILE_COUNT && !r.failed; i++) {
        r.file = files[i];
        if (roots[i]) readers[i](&r, roots[i]);
    }
    for (int i = 0; i < FILE_COUNT; i++) cJSON_Delete(roots[i]);
    if (!r.failed) validate(&r);
    return !r.failed;
}
