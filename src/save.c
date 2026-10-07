#include "save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_MAGIC 0x574B4C42u /* "BLKW" */
#define SAVE_VERSION 27u

/* Fixed-size part of a save. After it come the names of the tiles and items
 * the save's ids stand for, the tile layers, the inventory and the drops. */
typedef struct {
    uint32_t magic, version;
    int32_t width, height;
    uint32_t seed;
    int32_t y_top, spawn_x, spawn_y, surface_y;
    int32_t feet_x, feet_y, climbing, facing;
    uint64_t tick;
    int32_t tick_rate;
    uint32_t rng, next_drop_id;
    uint32_t inventory_size, drops_size;
} SaveHeader;

/* From version 6 a second fixed-size part follows the first, with room to
 * grow, so adding to it does not move what comes after. */
typedef struct {
    int32_t sea_row; /* -1 for a world with no sea */
    int32_t fog;     /* fog of war: 1 on, 2 off, 0 not recorded */
    int32_t started; /* whether the world has had its first character: 1 yes, 2 no, 0 not recorded (yes) */
    int32_t random_events; /* random blocks hit per chunk per tick, plus one; 0 in a save from before it was a setting */
    int32_t quick_craft;   /* everything made takes a tick: 1 yes, 2 no, 0 not recorded (no) */
    int32_t reserved[3];
} SaveExtra;

/* Up to version 15 an item's id was one byte. Everything that held a stack
 * was laid out round that, and is read as it was and widened. */
typedef struct {
    uint8_t id;
    uint8_t count;
    uint16_t container;
} StackV1;

typedef struct {
    uint8_t used;
    StackV1 slots[CONTAINER_MAX_SLOTS];
} ContainerV1;

typedef struct {
    StackV1 slots[INVENTORY_SLOTS];
    StackV1 armor[ARMOR_SLOTS];
    ContainerV1 containers[MAX_CONTAINERS];
    int sel_slot, sel_inner;
    int8_t choice[INVENTORY_SLOTS]; /* (from version 13) */
} InventoryV1;

typedef struct {
    bool active;
    uint8_t item;
    int x;
    float y;
    float age;
    float collect;
    bool attached;
    int ttl;
    uint32_t id;
    uint16_t container;
    bool held_off;
} DropV1;

static ItemStack widen(StackV1 s)
{
    return (ItemStack){ s.id, s.count, s.container };
}

/* Reads the inventory and drops of a version 3 to 15 save into today's layout. */
static bool read_v1_state(FILE *f, Game *g, size_t inventory_size)
{
    static InventoryV1 inv;
    static DropV1 drops[MAX_DROPS];
    memset(&inv, 0, sizeof inv);
    memset(inv.choice, -1, sizeof inv.choice);
    if (inventory_size > sizeof inv || fread(&inv, inventory_size, 1, f) != 1 || fread(drops, sizeof drops, 1, f) != 1)
        return false;
    Inventory *to = &g->inventory;
    memset(to, 0, sizeof *to);
    for (int i = 0; i < INVENTORY_SLOTS; i++) to->slots[i] = widen(inv.slots[i]);
    for (int i = 0; i < ARMOR_SLOTS; i++) to->armor[i] = widen(inv.armor[i]);
    for (int i = 0; i < MAX_CONTAINERS; i++) {
        to->containers[i].used = inv.containers[i].used;
        for (int j = 0; j < CONTAINER_MAX_SLOTS; j++) to->containers[i].slots[j] = widen(inv.containers[i].slots[j]);
    }
    to->sel_slot = inv.sel_slot;
    to->sel_inner = inv.sel_inner;
    memcpy(to->choice, inv.choice, sizeof to->choice);
    for (int i = 0; i < MAX_DROPS; i++) {
        const DropV1 *d = &drops[i];
        g->drops[i] = (Drop){ d->active, d->item, d->x, d->y, d->age, d->collect, d->attached, d->ttl,
                              d->id, d->container, d->held_off };
    }
    return true;
}

/* How the inventory and drops were laid out before version 3, when a
 * container had four slots and a drop could not carry contents. */
typedef struct {
    bool used;
    StackV1 slots[4];
} OldContainer;

typedef struct {
    StackV1 slots[INVENTORY_SLOTS];
    StackV1 armor[ARMOR_SLOTS];
    OldContainer containers[MAX_CONTAINERS];
    int sel_slot, sel_inner;
} OldInventory;

typedef struct {
    bool active;
    uint8_t item;
    int x;
    float y;
    float age;
    float collect;
    bool attached;
    int ttl;
    uint32_t id;
} OldDrop;

/* Version 3 kept only a block's contents, not its fuel. */
typedef struct {
    bool used;
    int x, y;
    uint16_t container;
} OldStorage;

/* Before version 27 a block kept no note of what a machine was making. */
typedef struct {
    bool used;
    int x, y;
    uint16_t container;
    int fuel;
    int power;
    int16_t made, given;
} Storage26;

static bool read_storages_26(FILE *f, Game *g)
{
    static Storage26 old[MAX_STORAGES];
    if (fread(old, sizeof old, 1, f) != 1) return false;
    for (int i = 0; i < MAX_STORAGES; i++)
        g->storages[i] = (Storage){ .used = old[i].used, .x = old[i].x, .y = old[i].y, .container = old[i].container,
                                    .fuel = old[i].fuel, .power = old[i].power, .made = old[i].made, .given = old[i].given };
    return true;
}

static bool read_old_storages(FILE *f, Game *g)
{
    static OldStorage old[MAX_STORAGES];
    if (fread(old, sizeof old, 1, f) != 1) return false;
    for (int i = 0; i < MAX_STORAGES; i++)
        g->storages[i] = (Storage){ .used = old[i].used, .x = old[i].x, .y = old[i].y,
                                    .container = old[i].container };
    return true;
}

/* Reads the inventory and drops of a version 1 or 2 save into today's layout. */
static bool read_old_state(FILE *f, Game *g)
{
    static OldInventory inv;
    static OldDrop drops[MAX_DROPS];
    if (fread(&inv, sizeof inv, 1, f) != 1 || fread(drops, sizeof drops, 1, f) != 1) return false;

    memset(&g->inventory, 0, sizeof g->inventory);
    for (int i = 0; i < INVENTORY_SLOTS; i++) g->inventory.slots[i] = widen(inv.slots[i]);
    for (int i = 0; i < ARMOR_SLOTS; i++) g->inventory.armor[i] = widen(inv.armor[i]);
    g->inventory.sel_slot = inv.sel_slot;
    g->inventory.sel_inner = inv.sel_inner;
    for (int i = 0; i < MAX_CONTAINERS; i++) {
        g->inventory.containers[i].used = inv.containers[i].used;
        for (int j = 0; j < 4; j++) g->inventory.containers[i].slots[j] = widen(inv.containers[i].slots[j]);
    }
    for (int i = 0; i < MAX_DROPS; i++) {
        const OldDrop *d = &drops[i];
        g->drops[i] = (Drop){ d->active, d->item, d->x, d->y, d->age, d->collect, d->attached, d->ttl,
                              d->id, 0, false };
    }
    return true;
}

/* Version 1 saves predate the data files and have no name tables: their ids
 * were these, in this order. Trees stood in the foreground. */
static const char *const legacy_tiles[] = { "air", "dirt", "workbench", "spawn_portal", "grass",
                                            "stone", "portal_base", "oak_log", "oak_leaves",
                                            "cherry_log", "cherry_leaves", "wood" };
static const char *const legacy_items[] = { "none", "basket", "dirt", "workbench", "stone",
                                            "stick", "wood", "acorn", "cherry" };

/* Ids are only good for one run of the game, so a save records which name
 * each id stood for and they are matched up again by name on loading. */
static bool write_names(FILE *f, int count, const char *first, size_t stride)
{
    uint32_t n = (uint32_t)count;
    if (fwrite(&n, sizeof n, 1, f) != 1) return false;
    for (int i = 0; i < count; i++)
        if (fwrite(first + (size_t)i * stride, CONTENT_NAME_LEN, 1, f) != 1) return false;
    return true;
}

/* Reads a name table into a map from saved id to current id. A name that no
 * longer exists maps to 0: the block becomes air, the item vanishes. */
static bool read_names(FILE *f, uint8_t map[256], int (*lookup)(const char *))
{
    uint32_t n;
    if (fread(&n, sizeof n, 1, f) != 1 || n > 256) return false;
    memset(map, 0, 256);
    for (uint32_t i = 0; i < n; i++) {
        char name[CONTENT_NAME_LEN];
        if (fread(name, sizeof name, 1, f) != 1) return false;
        name[sizeof name - 1] = '\0';
        int id = lookup(name);
        map[i] = (uint8_t)(id > 0 ? id : 0);
    }
    return true;
}

/* The same for blocks, whose ids take two bytes from version 18 and whose
 * table can be as long as there are kinds of block. */
static bool read_tile_names(FILE *f, uint16_t map[MAX_TILES])
{
    uint32_t n;
    if (fread(&n, sizeof n, 1, f) != 1 || n > MAX_TILES) return false;
    memset(map, 0, sizeof(uint16_t) * MAX_TILES);
    for (uint32_t i = 0; i < n; i++) {
        char name[CONTENT_NAME_LEN];
        if (fread(name, sizeof name, 1, f) != 1) return false;
        name[sizeof name - 1] = '\0';
        int id = tile_id_or_alias(name);
        map[i] = (uint16_t)(id > 0 ? id : 0);
    }
    return true;
}

/* Reads one layer of blocks: a byte to a block before version 18, two since. */
static bool read_layer(FILE *f, uint16_t *out, size_t cells, uint32_t version)
{
    if (version >= 18) return fread(out, sizeof(uint16_t), cells, f) == cells;
    uint8_t *narrow = malloc(cells);
    bool ok = narrow && fread(narrow, 1, cells, f) == cells;
    for (size_t i = 0; ok && i < cells; i++) out[i] = narrow[i];
    free(narrow);
    return ok;
}

/* A character other than the first, as saved from version 14. (The first is
 * in the header and the inventory, where the only character always was.) */
typedef struct {
    int32_t feet_x, feet_y, climbing, facing;
    ItemStack slots[INVENTORY_SLOTS];
    ItemStack armor[ARMOR_SLOTS];
    int32_t sel_slot, sel_inner;
    int8_t choice[INVENTORY_SLOTS];
    Vitals vitals;
    int32_t reserved[8];
} SavedCharacter;

/* And as they were in versions 14 and 15, with one-byte item ids. */
typedef struct {
    int32_t feet_x, feet_y, climbing, facing;
    StackV1 slots[INVENTORY_SLOTS];
    StackV1 armor[ARMOR_SLOTS];
    int32_t sel_slot, sel_inner;
    int8_t choice[INVENTORY_SLOTS];
    Vitals vitals;
    int32_t reserved[8];
} SavedCharacterV1;

/* A creature as saved before version 10. */
typedef struct {
    bool active;
    uint8_t kind;
    bool owned;
    bool baby;
    int x, y;
    int to_x, to_y;
    int step_left, step_total;
    int dir;
    int age;
    uint32_t id;
} OldMob;

static bool read_old_mobs(FILE *f, Game *g)
{
    static OldMob old[MAX_MOBS];
    if (fread(old, sizeof old, 1, f) != 1) return false;
    for (int i = 0; i < MAX_MOBS; i++) {
        const OldMob *m = &old[i];
        g->mobs[i] = (Mob){ .active = m->active, .kind = m->kind, .owned = m->owned, .baby = m->baby,
                            .x = m->x, .y = m->y, .to_x = m->to_x, .to_y = m->to_y,
                            .step_left = m->step_left, .step_total = m->step_total, .dir = m->dir,
                            .age = m->age, .id = m->id };
    }
    return true;
}

static int biome_id(const char *name)
{
    for (int i = 0; i < content.biome_count; i++)
        if (strcmp(content.biomes[i].name, name) == 0) return i;
    return 0;
}

/* The same for items, whose ids take two bytes and whose table can be longer. */
static bool read_item_names(FILE *f, uint16_t map[MAX_ITEMS])
{
    uint32_t n;
    if (fread(&n, sizeof n, 1, f) != 1 || n > MAX_ITEMS) return false;
    memset(map, 0, sizeof(uint16_t) * MAX_ITEMS);
    for (uint32_t i = 0; i < n; i++) {
        char name[CONTENT_NAME_LEN];
        if (fread(name, sizeof name, 1, f) != 1) return false;
        name[sizeof name - 1] = '\0';
        int id = item_id(name);
        map[i] = (uint16_t)(id > 0 ? id : 0);
    }
    return true;
}

/* Which saved creature kinds still exist, filled in as their names are read. */
static bool mob_id_by_saved_name[256];
static int mob_names_read;

static int mob_id_or_zero(const char *name)
{
    int id = mob_id(name);
    if (mob_names_read < 256) mob_id_by_saved_name[mob_names_read++] = id >= 0;
    return id > 0 ? id : 0;
}

static void legacy_names(uint16_t map[MAX_TILES], const char *const *names, int count,
                         int (*lookup)(const char *))
{
    memset(map, 0, sizeof(uint16_t) * MAX_TILES);
    for (int i = 0; i < count; i++) {
        int id = lookup(names[i]);
        map[i] = (uint16_t)(id > 0 ? id : 0);
    }
}

static void remap_stack(ItemStack *s, const uint16_t items[MAX_ITEMS])
{
    s->id = s->id < MAX_ITEMS ? items[s->id] : 0;
    if (s->id == ITEM_NONE) *s = (ItemStack){ 0 };
}

bool save_game(const Game *g, const char *path)
{
    char tmp[1024];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp) return false;
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;

    /* The first character is the one written in the header and the inventory. */
    Game *mine = (Game *)g;
    int played = g->controlled;
    game_use_character(mine, 0);

    const World *w = &g->world;
    size_t cells = (size_t)w->width * w->height;
    SaveHeader h = {
        SAVE_MAGIC, SAVE_VERSION, w->width, w->height, w->seed,
        w->y_top, w->spawn_x, w->spawn_y, w->surface_y,
        g->player.feet.x, g->player.feet.y, g->player.climbing, g->player.facing,
        g->tick, g->tick_rate, g->rng, g->next_drop_id,
        (uint32_t)sizeof g->inventory, (uint32_t)sizeof g->drops,
    };
    /* A step in progress is saved as already taken. */
    if (g->player.activity == ACTIVITY_STEPPING) {
        h.feet_x = world_wrap_x(w, g->player.step_to.x);
        h.feet_y = g->player.step_to.y;
        h.climbing = g->player.step.climb;
    }

    SaveExtra extra = { .sea_row = w->sea_row, .fog = g->fog ? 1 : 2, .started = g->started ? 1 : 2,
                        .random_events = g->random_events + 1, .quick_craft = g->quick_craft ? 1 : 2 };
    bool ok = fwrite(&h, sizeof h, 1, f) == 1 && fwrite(&extra, sizeof extra, 1, f) == 1 &&
              write_names(f, content.tile_count, content.tiles[0].name, sizeof content.tiles[0]) &&
              write_names(f, content.item_count, content.items[0].name, sizeof content.items[0]);
    for (int layer = 0; layer < LAYER_COUNT && ok; layer++)
        ok = fwrite(w->layers[layer], sizeof(uint16_t), cells, f) == cells;
    ok = ok && fwrite(&g->inventory, sizeof g->inventory, 1, f) == 1 &&
         fwrite(g->drops, sizeof g->drops, 1, f) == 1 &&
         fwrite(g->storages, sizeof g->storages, 1, f) == 1 &&
         write_names(f, content.mob_count, content.mobs[0].name, sizeof content.mobs[0]) &&
         fwrite(g->mobs, sizeof g->mobs, 1, f) == 1 && fwrite(&g->next_mob_id, sizeof g->next_mob_id, 1, f) == 1 &&
         fwrite(g->explored, 1, cells, f) == cells && fwrite(&g->vitals, sizeof g->vitals, 1, f) == 1 &&
         write_names(f, content.biome_count, content.biomes[0].name, sizeof content.biomes[0]) &&
         fwrite(w->biome, 1, (size_t)w->width, f) == (size_t)w->width &&
         fwrite(w->water, 1, cells, f) == cells && fwrite(w->water_awake, 1, (size_t)w->width, f) == (size_t)w->width;
    /* The other characters, and which of them all was being played. */
    uint32_t people[2] = { (uint32_t)g->character_count, (uint32_t)played };
    ok = ok && fwrite(people, sizeof people, 1, f) == 1;
    for (int c = 1; c < g->character_count && ok; c++) {
        const Character *who = &g->characters[c];
        SavedCharacter sc = { .feet_x = who->player.feet.x, .feet_y = who->player.feet.y,
                              .climbing = who->player.climbing, .facing = who->player.facing,
                              .sel_slot = who->sel_slot, .sel_inner = who->sel_inner, .vitals = who->vitals };
        /* A step in progress is saved as already taken. */
        if (who->player.activity == ACTIVITY_STEPPING) {
            sc.feet_x = world_wrap_x(w, who->player.step_to.x);
            sc.feet_y = who->player.step_to.y;
            sc.climbing = who->player.step.climb;
        }
        memcpy(sc.slots, who->slots, sizeof sc.slots);
        memcpy(sc.armor, who->armor, sizeof sc.armor);
        memcpy(sc.choice, who->choice, sizeof sc.choice);
        ok = fwrite(&sc, sizeof sc, 1, f) == 1;
    }
    /* What each is called, the first included. */
    for (int c = 0; c < g->character_count && ok; c++) {
        char name[CHARACTER_NAME_MAX] = "";
        snprintf(name, sizeof name, "%s", game_character_name(g, c));
        ok = fwrite(name, sizeof name, 1, f) == 1;
    }
    /* The numbers kept with squares: few, so only the ones there are, each with where it is. */
    uint32_t kept = 0;
    for (size_t i = 0; i < cells; i++) kept += w->meta[i] != 0;
    ok = ok && fwrite(&kept, sizeof kept, 1, f) == 1;
    for (size_t i = 0; i < cells && ok; i++) {
        if (w->meta[i] == 0) continue;
        uint32_t record[2] = { (uint32_t)i, w->meta[i] };
        ok = fwrite(record, sizeof record, 1, f) == 1;
    }
    /* The snow lying on each column. */
    ok = ok && fwrite(g->snow, 1, (size_t)w->width, f) == (size_t)w->width;
    /* What is written on the signs: each text that is in use, with its number. */
    uint32_t texts = 0;
    for (int id = 1; id < MAX_SIGN_TEXTS; id++) texts += g->signs[id].used;
    ok = ok && fwrite(&texts, sizeof texts, 1, f) == 1;
    for (uint32_t id = 1; id < MAX_SIGN_TEXTS && ok; id++)
        if (g->signs[id].used) ok = fwrite(&id, sizeof id, 1, f) == 1 && fwrite(g->signs[id].text, sizeof g->signs[id].text, 1, f) == 1;
    /* Paint: few squares have any, so only those, each with where it is. */
    uint32_t painted = 0;
    for (size_t i = 0; i < cells; i++) painted += w->paint[0][i] != 0 || w->paint[1][i] != 0;
    ok = ok && fwrite(&painted, sizeof painted, 1, f) == 1;
    for (size_t i = 0; i < cells && ok; i++) {
        if (w->paint[0][i] == 0 && w->paint[1][i] == 0) continue;
        uint32_t record[2] = { (uint32_t)i, (uint32_t)w->paint[0][i] | (uint32_t)w->paint[1][i] << 8 };
        ok = fwrite(record, sizeof record, 1, f) == 1;
    }
    /* Which items have been found here, in the order of the names written above. */
    uint32_t known = (uint32_t)content.item_count;
    ok = ok && fwrite(&known, sizeof known, 1, f) == 1 && fwrite(g->discovered, 1, known, f) == known;
    game_use_character(mine, played);
    /* What is left of each character's shield. */
    float shields[MAX_CHARACTERS] = { 0 };
    for (int c = 0; c < g->character_count && c < MAX_CHARACTERS; c++) shields[c] = c == g->live ? g->shield : g->characters[c].shield;
    ok = ok && fwrite(shields, sizeof shields, 1, f) == 1;
    ok = fclose(f) == 0 && ok;
    if (!ok || rename(tmp, path) != 0) {
        remove(tmp);
        return false;
    }
    return true;
}

bool load_game(Game *g, const char *path)
{
    memset(g, 0, sizeof *g);
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    SaveHeader h;
    static uint16_t tiles[MAX_TILES];
    static uint16_t items[MAX_ITEMS];
    bool ok = fread(&h, sizeof h, 1, f) == 1 && h.magic == SAVE_MAGIC &&
              h.version >= 1 && h.version <= SAVE_VERSION &&
              h.inventory_size == (h.version < 3 ? sizeof(OldInventory)
                                   : h.version < 13 ? sizeof(InventoryV1) - INVENTORY_SLOTS
                                   : h.version < 16 ? sizeof(InventoryV1) : sizeof g->inventory) &&
              h.drops_size == (h.version < 3 ? sizeof(OldDrop) * MAX_DROPS
                               : h.version < 16 ? sizeof(DropV1) * MAX_DROPS : sizeof g->drops) &&
              h.width > 0 && h.height > 0 && h.width <= 1 << 16 && h.height <= 1 << 12;
    /* A world from before there was a sea has none: nothing floods. */
    SaveExtra extra = { .sea_row = -1 };
    if (ok && h.version >= 6) ok = fread(&extra, sizeof extra, 1, f) == 1;
    if (ok && h.version == 1) {
        legacy_names(tiles, legacy_tiles, (int)(sizeof legacy_tiles / sizeof legacy_tiles[0]),
                     tile_id_or_alias);
        memset(items, 0, sizeof items);
        for (int i = 0; i < (int)(sizeof legacy_items / sizeof legacy_items[0]); i++) {
            int id = item_id(legacy_items[i]);
            items[i] = (uint16_t)(id > 0 ? id : 0);
        }
    } else if (ok) {
        ok = read_tile_names(f, tiles) && read_item_names(f, items);
    }
    ok = ok && game_init(g, h.width, h.height, h.seed, WORLD_BLANK);

    if (ok) {
        World *w = &g->world;
        size_t cells = (size_t)w->width * w->height;
        /* Saves from before version 5 have no backwall layer. */
        int saved_layers = h.version < 5 ? BLOCK_LAYERS : LAYER_COUNT;
        for (int layer = 0; layer < saved_layers && ok; layer++)
            ok = read_layer(f, w->layers[layer], cells, h.version);
        if (ok && h.version < 3) {
            ok = read_old_state(f, g);
        } else {
            /* (Before version 16 item ids were a byte, and everything holding
             * them was smaller; before 13 there were no baskets' choices.) */
            ok = ok && (h.version < 16 ? read_v1_state(f, g, h.inventory_size)
                                       : fread(&g->inventory, sizeof g->inventory, 1, f) == 1 &&
                                             fread(g->drops, sizeof g->drops, 1, f) == 1) &&
                 (h.version == 3 ? read_old_storages(f, g)
                  : h.version < 27 ? read_storages_26(f, g)
                                   : fread(g->storages, sizeof g->storages, 1, f) == 1);
        }
        /* Creatures, from version 7: kinds by name, like blocks and items. */
        if (ok && h.version >= 7) {
            uint8_t kinds[256];
            mob_names_read = 0;
            memset(mob_id_by_saved_name, 0, sizeof mob_id_by_saved_name);
            ok = read_names(f, kinds, mob_id_or_zero) &&
                 (h.version < 10 ? read_old_mobs(f, g) : fread(g->mobs, sizeof g->mobs, 1, f) == 1) &&
                 fread(&g->next_mob_id, sizeof g->next_mob_id, 1, f) == 1;
            for (int i = 0; ok && i < MAX_MOBS; i++) {
                /* (A kind that no longer exists maps to 0 like any other; a
                 * creature of it is only kept if 0 really is its kind.) */
                bool known = mob_id_by_saved_name[g->mobs[i].kind];
                if (!known) g->mobs[i].active = false;
                g->mobs[i].kind = kinds[g->mobs[i].kind];
            }
        }
        /* What has been uncovered, from version 8. An older world starts
         * dark, but for where the player is standing. */
        memset(g->explored, 0, cells);
        if (ok && h.version >= 8) ok = fread(g->explored, 1, cells, f) == cells;
        /* The player's needs, from version 9; before that, everything full.
         * Nobody is saved asleep. */
        if (ok && h.version >= 9) ok = fread(&g->vitals, sizeof g->vitals, 1, f) == 1;
        /* The biome of each column, by name, from version 10. */
        bool biomes_read = false;
        if (ok && h.version >= 10) {
            uint8_t kinds[256];
            ok = read_names(f, kinds, biome_id) && fread(w->biome, 1, (size_t)w->width, f) == (size_t)w->width;
            for (int x = 0; ok && x < w->width; x++) w->biome[x] = kinds[w->biome[x]];
            biomes_read = ok;
        }
        /* The water, from version 12. Before that it was not kept: it was
         * simply wherever the sky was open below sea level, and is put there. */
        bool water_read = false;
        if (ok && h.version >= 12) {
            ok = fread(w->water, 1, cells, f) == cells && fread(w->water_awake, 1, (size_t)w->width, f) == (size_t)w->width;
            for (int x = 0; ok && x < w->width; x++) w->water_stirring = w->water_stirring || w->water_awake[x];
            water_read = ok;
        } else {
            g->vitals.oxygen = 1.0f;
        }
        g->vitals.rest = REST_NONE;
        g->vitals.meditation = 0.0f;
        /* The other characters, from version 14. */
        uint32_t people[2] = { 1, 0 };
        if (ok && h.version >= 14)
            ok = fread(people, sizeof people, 1, f) == 1 && people[0] <= MAX_CHARACTERS && (people[0] >= 1 || h.version >= 15);
        /* Before version 15 nobody could die: everyone has their death bar whole. */
        if (h.version < 15) {
            g->vitals.death = 1.0f;
            g->vitals.respawn = 0;
        }
        for (uint32_t c = 1; ok && c < people[0]; c++) {
            SavedCharacter sc;
            if (h.version < 16) {
                SavedCharacterV1 old;
                ok = fread(&old, sizeof old, 1, f) == 1;
                sc = (SavedCharacter){ .feet_x = old.feet_x, .feet_y = old.feet_y, .climbing = old.climbing,
                                       .facing = old.facing, .sel_slot = old.sel_slot, .sel_inner = old.sel_inner,
                                       .vitals = old.vitals };
                for (int i = 0; i < INVENTORY_SLOTS; i++) sc.slots[i] = widen(old.slots[i]);
                for (int i = 0; i < ARMOR_SLOTS; i++) sc.armor[i] = widen(old.armor[i]);
                memcpy(sc.choice, old.choice, sizeof sc.choice);
            } else {
                ok = fread(&sc, sizeof sc, 1, f) == 1;
            }
            ok = ok && world_in_bounds(w, 0, sc.feet_y);
            if (!ok) break;
            int made = game_add_character(g, (TilePos){ sc.feet_x, sc.feet_y });
            if (made < 0) continue; /* (the rules now allow fewer: this one is left out) */
            Character *who = &g->characters[made];
            who->player.climbing = sc.climbing != 0;
            who->player.facing = sc.facing < 0 ? -1 : 1;
            for (int i = 0; i < INVENTORY_SLOTS; i++) {
                who->slots[i] = sc.slots[i];
                remap_stack(&who->slots[i], items);
            }
            for (int i = 0; i < ARMOR_SLOTS; i++) {
                who->armor[i] = sc.armor[i];
                remap_stack(&who->armor[i], items);
            }
            who->sel_slot = sc.sel_slot >= 0 && sc.sel_slot < INVENTORY_SLOTS ? sc.sel_slot : 0;
            who->sel_inner = sc.sel_inner;
            memcpy(who->choice, sc.choice, sizeof who->choice);
            who->vitals = sc.vitals;
            who->vitals.rest = REST_NONE;
            who->vitals.meditation = 0.0f;
            if (h.version < 15) {
                who->vitals.death = 1.0f;
                who->vitals.respawn = 0;
            }
        }
        /* Their names, from version 15; before that they had none of their own. */
        for (uint32_t c = 0; ok && h.version >= 15 && c < people[0]; c++) {
            char name[CHARACTER_NAME_MAX];
            ok = fread(name, sizeof name, 1, f) == 1;
            name[sizeof name - 1] = '\0';
            if (ok && (int)c < g->character_count && name[0])
                snprintf((int)c == g->live ? g->name : g->characters[c].name, CHARACTER_NAME_MAX, "%s", name);
        }
        if (ok && h.version >= 17) {
            uint32_t kept = 0, record[2];
            ok = fread(&kept, sizeof kept, 1, f) == 1 && kept <= cells;
            for (uint32_t i = 0; ok && i < kept; i++) {
                ok = fread(record, sizeof record, 1, f) == 1 && record[0] < cells;
                if (ok) w->meta[record[0]] = (uint16_t)record[1];
            }
        }
        if (ok && h.version >= 19) ok = fread(g->snow, 1, (size_t)w->width, f) == (size_t)w->width;
        if (extra.random_events > 0) g->random_events = extra.random_events - 1;
        g->quick_craft = extra.quick_craft == 1;
        if (ok && h.version >= 22) {
            uint32_t texts = 0, id = 0;
            ok = fread(&texts, sizeof texts, 1, f) == 1 && texts < MAX_SIGN_TEXTS;
            for (uint32_t i = 0; ok && i < texts; i++) {
                /* (In version 22 a sign had three lines of 20 letters: they are run together into the one it has now.) */
                char old[3][21] = { { 0 } };
                ok = fread(&id, sizeof id, 1, f) == 1 && id >= 1 && id < MAX_SIGN_TEXTS &&
                     (h.version >= 23 ? fread(g->signs[id].text, sizeof g->signs[id].text, 1, f) == 1 : fread(old, sizeof old, 1, f) == 1);
                if (!ok) break;
                g->signs[id].used = true;
                if (h.version < 23) {
                    char joined[72] = "";
                    for (int k = 0; k < 3; k++) {
                        old[k][20] = '\0';
                        if (!old[k][0]) continue;
                        size_t n = strlen(joined);
                        snprintf(joined + n, sizeof joined - n, "%s%s", n ? " " : "", old[k]);
                    }
                    snprintf(g->signs[id].text, sizeof g->signs[id].text, "%s", joined);
                }
                g->signs[id].text[SIGN_TEXT_MAX] = '\0';
            }
        }
        if (ok && h.version >= 21) {
            uint32_t painted = 0, record[2];
            ok = fread(&painted, sizeof painted, 1, f) == 1 && painted <= cells;
            for (uint32_t i = 0; ok && i < painted; i++) {
                ok = fread(record, sizeof record, 1, f) == 1 && record[0] < cells;
                if (!ok) break;
                w->paint[0][record[0]] = (uint8_t)(record[1] & 0xFF);
                w->paint[1][record[0]] = (uint8_t)(record[1] >> 8 & 0xFF);
            }
        }
        if (ok && h.version >= 20) {
            static uint8_t found[MAX_ITEMS];
            uint32_t known = 0;
            ok = fread(&known, sizeof known, 1, f) == 1 && known <= MAX_ITEMS && fread(found, 1, known, f) == known;
            for (uint32_t i = 0; ok && i < known; i++)
                if (found[i] && items[i] != 0) g->discovered[items[i]] = 1;
        }
        if (ok && h.version >= 26) {
            float shields[MAX_CHARACTERS] = { 0 };
            ok = fread(shields, sizeof shields, 1, f) == 1;
            for (int c = 0; ok && c < MAX_CHARACTERS; c++) {
                float left = shields[c] >= 0.0f && shields[c] <= 16.0f ? shields[c] : 0.0f;
                if (c == g->live) g->shield = left;
                else g->characters[c].shield = left;
            }
        }
        bool nobody = ok && h.version >= 15 && people[0] == 0;
        g->started = extra.started != 2;
        int played = (int)people[1] < g->character_count ? (int)people[1] : 0;

        /* Turn the save's ids into this run's. */
        for (int layer = 0; layer < LAYER_COUNT && ok; layer++)
            for (size_t i = 0; i < cells; i++) w->layers[layer][i] = w->layers[layer][i] < MAX_TILES ? tiles[w->layers[layer][i]] : 0;
        if (ok) {
            Inventory *inv = &g->inventory;
            for (int i = 0; i < INVENTORY_SLOTS; i++) remap_stack(&inv->slots[i], items);
            for (int i = 0; i < ARMOR_SLOTS; i++) remap_stack(&inv->armor[i], items);
            for (int i = 0; i < MAX_CONTAINERS; i++) {
                /* (One that records meters holds readings, not items.) */
                if (inv->containers[i].used == CONTAINER_METERS) continue;
                for (int j = 0; j < CONTAINER_MAX_SLOTS; j++) remap_stack(&inv->containers[i].slots[j], items);
            }
            if (h.version < 13) memset(inv->choice, -1, sizeof inv->choice);
            for (int i = 0; i < MAX_DROPS; i++) {
                /* (Before version 11 this was unused space in the record.) */
                if (h.version < 11) g->drops[i].held_off = false;
                g->drops[i].item = g->drops[i].item < MAX_ITEMS ? items[g->drops[i].item] : 0;
                if (g->drops[i].item == ITEM_NONE) g->drops[i].active = false;
            }
        }
        /* A block that has since been moved to the background - a golden chest, say - goes there in an older world,
         * where nothing is in its way. */
        for (size_t i = 0; ok && h.version < 25 && i < cells; i++) {
            TileType front = w->layers[LAYER_FOREGROUND][i];
            if (front == TILE_AIR || tile_def(front)->layer != LAYER_BACKGROUND || tile_def(front)->pair_of != TILE_AIR ||
                tile_def(front)->pair != TILE_AIR || tile_def(front)->height > 1 || w->layers[LAYER_BACKGROUND][i] != TILE_AIR)
                continue;
            w->layers[LAYER_BACKGROUND][i] = (uint16_t)front;
            w->layers[LAYER_FOREGROUND][i] = TILE_AIR;
        }
        /* The look a door keeps is a block: that is an id of the save's too. Before version 24
         * a door looked like whatever it stood between at the time: each keeps the look it had. */
        for (int y = 0; ok && y < w->height; y++) {
            for (int x = 0; x < w->width; x++) {
                if (tile_def(world_get(w, x, y))->mimics == MIMIC_NONE) continue;
                size_t cell = (size_t)y * w->width + x;
                if (h.version >= 24) w->meta[cell] = w->meta[cell] < MAX_TILES ? tiles[w->meta[cell]] : 0;
                else w->meta[cell] = 0;
            }
        }
        for (int y = 0; ok && h.version < 24 && y < w->height; y++)
            for (int x = 0; x < w->width; x++) world_fix_look(w, x, y);
        /* Before version 23 a sign was one block wide: each gets its other half, where there is room for it. */
        for (int y = 0; ok && h.version == 22 && y < w->height; y++) {
            for (int x = 0; x < w->width; x++) {
                const TileDef *def = tile_def(world_get(w, x, y));
                if (def->written && def->pair != TILE_AIR && world_get(w, x + 1, y) == TILE_AIR) world_set(w, x + 1, y, def->pair);
            }
        }
        /* Old saves kept trees in the foreground; move each to its own layer. */
        for (size_t i = 0; ok && h.version == 1 && i < cells; i++) {
            const TileDef *def = tile_def(w->layers[LAYER_FOREGROUND][i]);
            int kind = def->log_of >= 0 ? def->log_of : def->leaves_of;
            if (kind < 0 || content.trees[kind].layer == LAYER_FOREGROUND) continue;
            if (w->layers[content.trees[kind].layer][i] == TILE_AIR)
                w->layers[content.trees[kind].layer][i] = w->layers[LAYER_FOREGROUND][i];
            w->layers[LAYER_FOREGROUND][i] = TILE_AIR;
        }

        /* An old world gets a backwall behind every block still standing. What
         * was already dug out stays open to the sky. */
        for (size_t i = 0; ok && h.version < 5 && i < cells; i++)
            if (tile_def(w->layers[LAYER_FOREGROUND][i])->wall)
                w->layers[LAYER_WALL][i] = w->layers[LAYER_FOREGROUND][i];
        /* A world from when the spawn portal stood taller: trim it to the
         * height it has now. */
        const WorldGenDef *gen = &content.worldgen;
        for (int i = gen->portal_height; ok && i < gen->portal_height + 4; i++) {
            const TileDef *above = tile_def(world_get(w, h.spawn_x, h.spawn_y - i));
            if (!above->bench || above->removable ||
                strcmp(above->bench_kind, tile_def(gen->portal_body)->bench_kind) != 0)
                break;
            world_set(w, h.spawn_x, h.spawn_y - i, TILE_AIR);
        }
        /* The shared container belongs to the player, not this world. */
        memset(&g->inventory.containers[SHARED_CONTAINER], 0, sizeof(Container));

        w->y_top = h.y_top;
        w->sea_row = extra.sea_row;
        /* An older world did not record its biomes: they are worked out from the land. */
        if (ok && !biomes_read) world_guess_biomes(w);
        if (ok && !water_read) water_make_sea(w);
        if (extra.fog != 0) g->fog = extra.fog == 1;
        w->spawn_x = h.spawn_x;
        w->spawn_y = h.spawn_y;
        w->surface_y = h.surface_y;
        g->player.feet = g->player.step_to = (TilePos){ world_wrap_x(w, h.feet_x), h.feet_y };
        if (ok && world_in_bounds(w, 0, h.feet_y)) game_reveal(g, g->player.feet.x, g->player.feet.y - 1);
        g->player.climbing = h.climbing != 0;
        g->player.facing = h.facing < 0 ? -1 : 1;
        g->tick = h.tick;
        g->tick_rate = h.tick_rate < 1 ? 1 : h.tick_rate > MAX_TICK_RATE ? MAX_TICK_RATE : h.tick_rate;
        g->rng = h.rng;
        g->next_drop_id = h.next_drop_id;
        ok = ok && world_in_bounds(w, 0, h.feet_y);
        if (ok) game_control(g, played);
        /* A world can be saved with nobody in it. */
        if (nobody) g->character_count = 0;
        if (!ok) game_free(g);
    }
    fclose(f);
    if (!ok) memset(g, 0, sizeof *g);
    return ok;
}

bool save_read_preview(const char *path, WorldPreview *out)
{
    memset(out, 0, sizeof *out);
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    SaveHeader h;
    static uint16_t tiles[MAX_TILES];
    static uint16_t items[MAX_ITEMS];
    bool ok = fread(&h, sizeof h, 1, f) == 1 && h.magic == SAVE_MAGIC && h.version >= 1 &&
              h.version <= SAVE_VERSION && h.width > 0 && h.height > 0 && h.width <= 1 << 16 &&
              h.height <= 1 << 12;
    SaveExtra extra;
    if (ok && h.version >= 6) ok = fread(&extra, sizeof extra, 1, f) == 1;
    if (ok && h.version == 1)
        legacy_names(tiles, legacy_tiles, (int)(sizeof legacy_tiles / sizeof legacy_tiles[0]),
                     tile_id_or_alias);
    else if (ok)
        ok = read_tile_names(f, tiles) && read_item_names(f, items);

    /* The layers follow, one after another, each a row at a time. */
    long layers_at = ok ? ftell(f) : -1;
    int saved_layers = h.version < 5 ? BLOCK_LAYERS : LAYER_COUNT;
    /* (A byte to a block before version 18, two since.) */
    size_t wide = h.version >= 18 ? 2 : 1;
    uint8_t *row = ok ? malloc((size_t)h.width * wide) : NULL;
    ok = ok && row && layers_at >= 0;
    for (int layer = 0; layer < saved_layers && ok; layer++) {
        for (int dy = 0; dy < PREVIEW_SIZE && ok; dy++) {
            int y = h.spawn_y - 1 - PREVIEW_RADIUS + dy;
            if (y < 0 || y >= h.height) continue;
            long at = layers_at + ((long)layer * h.height + y) * h.width * (long)wide;
            ok = fseek(f, at, SEEK_SET) == 0 && fread(row, wide, (size_t)h.width, f) == (size_t)h.width;
            for (int dx = 0; dx < PREVIEW_SIZE && ok; dx++) {
                int x = ((h.spawn_x - PREVIEW_RADIUS + dx) % h.width + h.width) % h.width;
                unsigned saved = wide == 2 ? (unsigned)row[x * 2] | (unsigned)row[x * 2 + 1] << 8 : row[x];
                out->tiles[layer][dy * PREVIEW_SIZE + dx] = saved < MAX_TILES ? tiles[saved] : 0;
            }
        }
    }
    /* As on loading: old worlds had trees in front, and no backwalls. */
    for (int i = 0; ok && i < PREVIEW_SIZE * PREVIEW_SIZE; i++) {
        TileType front = out->tiles[LAYER_FOREGROUND][i];
        const TileDef *def = tile_def(front);
        int kind = def->log_of >= 0 ? def->log_of : def->leaves_of;
        if (h.version == 1 && kind >= 0 && content.trees[kind].layer != LAYER_FOREGROUND) {
            out->tiles[content.trees[kind].layer][i] = (uint16_t)front;
            out->tiles[LAYER_FOREGROUND][i] = TILE_AIR;
        } else if (h.version < 5 && def->wall) {
            out->tiles[LAYER_WALL][i] = (uint16_t)front;
        }
    }
    out->tick = h.tick;
    free(row);
    fclose(f);
    return ok;
}

void profile_load(const char *path, Game *g)
{
    Container *shared = &g->inventory.containers[SHARED_CONTAINER];
    memset(shared, 0, sizeof *shared);
    shared->used = true;
    g->crystals = content.trade.start_crystals;

    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128], name[CONTENT_NAME_LEN];
    int a, b;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "crystals %d", &a) == 1 && a >= 0) g->crystals = a;
        /* Items are written by name; one that no longer exists is dropped. */
        if (sscanf(line, "shared %d %31s %d", &a, name, &b) == 3 && a >= 0 && a < CONTAINER_MAX_SLOTS &&
            b > 0 && b <= 255 && item_id(name) > 0)
            shared->slots[a] = (ItemStack){ (uint16_t)item_id(name), (uint8_t)b, 0 };
    }
    fclose(f);
}

bool profile_save(const char *path, const Game *g)
{
    FILE *f = fopen(path, "w");
    if (!f) return false;
    bool ok = fprintf(f, "crystals %d\n", g->crystals) > 0;
    const Container *shared = &g->inventory.containers[SHARED_CONTAINER];
    for (int i = 0; i < CONTAINER_MAX_SLOTS; i++) {
        const ItemStack *s = &shared->slots[i];
        if (s->id == ITEM_NONE || s->count == 0) continue;
        ok = fprintf(f, "shared %d %s %d\n", i, item_def(s->id)->name, s->count) > 0 && ok;
    }
    return fclose(f) == 0 && ok;
}
