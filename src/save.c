#include "save.h"

#include <stdio.h>
#include <string.h>

#define SAVE_MAGIC 0x574B4C42u /* "BLKW" */
#define SAVE_VERSION 2u

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

    bool ok = fwrite(&h, sizeof h, 1, f) == 1 &&
              write_names(f, content.tile_count, content.tiles[0].name, sizeof content.tiles[0]) &&
              write_names(f, content.item_count, content.items[0].name, sizeof content.items[0]);
    for (int layer = 0; layer < LAYER_COUNT && ok; layer++)
        ok = fwrite(w->layers[layer], 1, cells, f) == cells;
    ok = ok && fwrite(&g->inventory, sizeof g->inventory, 1, f) == 1 &&
         fwrite(g->drops, sizeof g->drops, 1, f) == 1;
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
              (h.version == 1 || h.version == SAVE_VERSION) &&
              h.inventory_size == sizeof g->inventory && h.drops_size == sizeof g->drops &&
              h.width > 0 && h.height > 0 && h.width <= 1 << 16 && h.height <= 1 << 12;
    if (ok && h.version == 1) {
        legacy_names(tiles, legacy_tiles, (int)(sizeof legacy_tiles / sizeof legacy_tiles[0]), tile_id);
        legacy_names(items, legacy_items, (int)(sizeof legacy_items / sizeof legacy_items[0]), item_id);
    } else if (ok) {
        ok = read_names(f, tiles, tile_id) && read_names(f, items, item_id);
    }
    ok = ok && game_init(g, h.width, h.height, h.seed, WORLD_BLANK);

    if (ok) {
        World *w = &g->world;
        size_t cells = (size_t)w->width * w->height;
        for (int layer = 0; layer < LAYER_COUNT && ok; layer++)
            ok = fread(w->layers[layer], 1, cells, f) == cells;
        ok = ok && fread(&g->inventory, sizeof g->inventory, 1, f) == 1 &&
             fread(g->drops, sizeof g->drops, 1, f) == 1;

        /* Turn the save's ids into this run's. */
        for (int layer = 0; layer < LAYER_COUNT && ok; layer++)
            for (size_t i = 0; i < cells; i++) w->layers[layer][i] = tiles[w->layers[layer][i]];
        if (ok) {
            Inventory *inv = &g->inventory;
            for (int i = 0; i < INVENTORY_SLOTS; i++) remap_stack(&inv->slots[i], items);
            for (int i = 0; i < ARMOR_SLOTS; i++) remap_stack(&inv->armor[i], items);
            for (int i = 0; i < MAX_CONTAINERS; i++)
                for (int j = 0; j < BASKET_SLOTS; j++) remap_stack(&inv->containers[i].slots[j], items);
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

        w->y_top = h.y_top;
        w->spawn_x = h.spawn_x;
        w->spawn_y = h.spawn_y;
        w->surface_y = h.surface_y;
        g->player.feet = g->player.step_to = (TilePos){ world_wrap_x(w, h.feet_x), h.feet_y };
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
