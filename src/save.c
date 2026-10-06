#include "save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_MAGIC 0x574B4C42u /* "BLKW" */
#define SAVE_VERSION 10u

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
    int32_t reserved[6];
} SaveExtra;

/* How the inventory and drops were laid out before version 3, when a
 * container had four slots and a drop could not carry contents. */
typedef struct {
    bool used;
    ItemStack slots[4];
} OldContainer;

typedef struct {
    ItemStack slots[INVENTORY_SLOTS];
    ItemStack armor[ARMOR_SLOTS];
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
    memcpy(g->inventory.slots, inv.slots, sizeof inv.slots);
    memcpy(g->inventory.armor, inv.armor, sizeof inv.armor);
    g->inventory.sel_slot = inv.sel_slot;
    g->inventory.sel_inner = inv.sel_inner;
    for (int i = 0; i < MAX_CONTAINERS; i++) {
        g->inventory.containers[i].used = inv.containers[i].used;
        memcpy(g->inventory.containers[i].slots, inv.containers[i].slots, sizeof inv.containers[i].slots);
    }
    for (int i = 0; i < MAX_DROPS; i++) {
        const OldDrop *d = &drops[i];
        g->drops[i] = (Drop){ d->active, d->item, d->x, d->y, d->age, d->collect, d->attached, d->ttl,
                              d->id, 0 };
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

/* Which saved creature kinds still exist, filled in as their names are read. */
static bool mob_id_by_saved_name[256];
static int mob_names_read;

static int mob_id_or_zero(const char *name)
{
    int id = mob_id(name);
    if (mob_names_read < 256) mob_id_by_saved_name[mob_names_read++] = id >= 0;
    return id > 0 ? id : 0;
}

static void legacy_names(uint8_t map[256], const char *const *names, int count,
                         int (*lookup)(const char *))
{
    memset(map, 0, 256);
    for (int i = 0; i < count; i++) {
        int id = lookup(names[i]);
        map[i] = (uint8_t)(id > 0 ? id : 0);
    }
}

static void remap_stack(ItemStack *s, const uint8_t items[256])
{
    s->id = items[s->id];
    if (s->id == ITEM_NONE) *s = (ItemStack){ 0 };
}

bool save_game(const Game *g, const char *path)
{
    char tmp[1024];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp) return false;
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;

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

    SaveExtra extra = { .sea_row = w->sea_row, .fog = g->fog ? 1 : 2 };
    bool ok = fwrite(&h, sizeof h, 1, f) == 1 && fwrite(&extra, sizeof extra, 1, f) == 1 &&
              write_names(f, content.tile_count, content.tiles[0].name, sizeof content.tiles[0]) &&
              write_names(f, content.item_count, content.items[0].name, sizeof content.items[0]);
    for (int layer = 0; layer < LAYER_COUNT && ok; layer++)
        ok = fwrite(w->layers[layer], 1, cells, f) == cells;
    ok = ok && fwrite(&g->inventory, sizeof g->inventory, 1, f) == 1 &&
         fwrite(g->drops, sizeof g->drops, 1, f) == 1 &&
         fwrite(g->storages, sizeof g->storages, 1, f) == 1 &&
         write_names(f, content.mob_count, content.mobs[0].name, sizeof content.mobs[0]) &&
         fwrite(g->mobs, sizeof g->mobs, 1, f) == 1 && fwrite(&g->next_mob_id, sizeof g->next_mob_id, 1, f) == 1 &&
         fwrite(g->explored, 1, cells, f) == cells && fwrite(&g->vitals, sizeof g->vitals, 1, f) == 1 &&
         write_names(f, content.biome_count, content.biomes[0].name, sizeof content.biomes[0]) &&
         fwrite(w->biome, 1, (size_t)w->width, f) == (size_t)w->width;
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
    uint8_t tiles[256], items[256];
    bool ok = fread(&h, sizeof h, 1, f) == 1 && h.magic == SAVE_MAGIC &&
              h.version >= 1 && h.version <= SAVE_VERSION &&
              h.inventory_size == (h.version < 3 ? sizeof(OldInventory) : sizeof g->inventory) &&
              h.drops_size == (h.version < 3 ? sizeof(OldDrop) * MAX_DROPS : sizeof g->drops) &&
              h.width > 0 && h.height > 0 && h.width <= 1 << 16 && h.height <= 1 << 12;
    /* A world from before there was a sea has none: nothing floods. */
    SaveExtra extra = { .sea_row = -1 };
    if (ok && h.version >= 6) ok = fread(&extra, sizeof extra, 1, f) == 1;
    if (ok && h.version == 1) {
        legacy_names(tiles, legacy_tiles, (int)(sizeof legacy_tiles / sizeof legacy_tiles[0]),
                     tile_id_or_alias);
        legacy_names(items, legacy_items, (int)(sizeof legacy_items / sizeof legacy_items[0]), item_id);
    } else if (ok) {
        ok = read_names(f, tiles, tile_id_or_alias) && read_names(f, items, item_id);
    }
    ok = ok && game_init(g, h.width, h.height, h.seed, WORLD_BLANK);

    if (ok) {
        World *w = &g->world;
        size_t cells = (size_t)w->width * w->height;
        /* Saves from before version 5 have no backwall layer. */
        int saved_layers = h.version < 5 ? BLOCK_LAYERS : LAYER_COUNT;
        for (int layer = 0; layer < saved_layers && ok; layer++)
            ok = fread(w->layers[layer], 1, cells, f) == cells;
        if (ok && h.version < 3) {
            ok = read_old_state(f, g);
        } else {
            ok = ok && fread(&g->inventory, sizeof g->inventory, 1, f) == 1 &&
                 fread(g->drops, sizeof g->drops, 1, f) == 1 &&
                 (h.version == 3 ? read_old_storages(f, g)
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
        g->vitals.rest = REST_NONE;
        g->vitals.meditation = 0.0f;

        /* Turn the save's ids into this run's. */
        for (int layer = 0; layer < LAYER_COUNT && ok; layer++)
            for (size_t i = 0; i < cells; i++) w->layers[layer][i] = tiles[w->layers[layer][i]];
        if (ok) {
            Inventory *inv = &g->inventory;
            for (int i = 0; i < INVENTORY_SLOTS; i++) remap_stack(&inv->slots[i], items);
            for (int i = 0; i < ARMOR_SLOTS; i++) remap_stack(&inv->armor[i], items);
            for (int i = 0; i < MAX_CONTAINERS; i++)
                for (int j = 0; j < CONTAINER_MAX_SLOTS; j++)
                    remap_stack(&inv->containers[i].slots[j], items);
            for (int i = 0; i < MAX_DROPS; i++) {
                g->drops[i].item = items[g->drops[i].item];
                if (g->drops[i].item == ITEM_NONE) g->drops[i].active = false;
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
    uint8_t tiles[256], items[256];
    bool ok = fread(&h, sizeof h, 1, f) == 1 && h.magic == SAVE_MAGIC && h.version >= 1 &&
              h.version <= SAVE_VERSION && h.width > 0 && h.height > 0 && h.width <= 1 << 16 &&
              h.height <= 1 << 12;
    SaveExtra extra;
    if (ok && h.version >= 6) ok = fread(&extra, sizeof extra, 1, f) == 1;
    if (ok && h.version == 1)
        legacy_names(tiles, legacy_tiles, (int)(sizeof legacy_tiles / sizeof legacy_tiles[0]),
                     tile_id_or_alias);
    else if (ok)
        ok = read_names(f, tiles, tile_id_or_alias) && read_names(f, items, item_id);

    /* The layers follow, one after another, each a row at a time. */
    long layers_at = ok ? ftell(f) : -1;
    int saved_layers = h.version < 5 ? BLOCK_LAYERS : LAYER_COUNT;
    uint8_t *row = ok ? malloc((size_t)h.width) : NULL;
    ok = ok && row && layers_at >= 0;
    for (int layer = 0; layer < saved_layers && ok; layer++) {
        for (int dy = 0; dy < PREVIEW_SIZE && ok; dy++) {
            int y = h.spawn_y - 1 - PREVIEW_RADIUS + dy;
            if (y < 0 || y >= h.height) continue;
            long at = layers_at + ((long)layer * h.height + y) * h.width;
            ok = fseek(f, at, SEEK_SET) == 0 && fread(row, 1, (size_t)h.width, f) == (size_t)h.width;
            for (int dx = 0; dx < PREVIEW_SIZE && ok; dx++) {
                int x = ((h.spawn_x - PREVIEW_RADIUS + dx) % h.width + h.width) % h.width;
                out->tiles[layer][dy * PREVIEW_SIZE + dx] = tiles[row[x]];
            }
        }
    }
    /* As on loading: old worlds had trees in front, and no backwalls. */
    for (int i = 0; ok && i < PREVIEW_SIZE * PREVIEW_SIZE; i++) {
        TileType front = out->tiles[LAYER_FOREGROUND][i];
        const TileDef *def = tile_def(front);
        int kind = def->log_of >= 0 ? def->log_of : def->leaves_of;
        if (h.version == 1 && kind >= 0 && content.trees[kind].layer != LAYER_FOREGROUND) {
            out->tiles[content.trees[kind].layer][i] = (uint8_t)front;
            out->tiles[LAYER_FOREGROUND][i] = TILE_AIR;
        } else if (h.version < 5 && def->wall) {
            out->tiles[LAYER_WALL][i] = (uint8_t)front;
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
            shared->slots[a] = (ItemStack){ (uint8_t)item_id(name), (uint8_t)b, 0 };
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
