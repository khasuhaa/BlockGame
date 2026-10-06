#include "save.h"

#include <stdio.h>
#include <string.h>

#define SAVE_MAGIC 0x574B4C42u /* "BLKW" */
#define SAVE_VERSION 1u

/* Fixed-size part of a save. The tile layers follow it, then the inventory
 * and the drops. Saves are only meant to be read back by the same build. */
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

    bool ok = fwrite(&h, sizeof h, 1, f) == 1 && fwrite(w->tiles, 1, cells, f) == cells &&
              fwrite(w->background, 1, cells, f) == cells &&
              fwrite(&g->inventory, sizeof g->inventory, 1, f) == 1 &&
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
    bool ok = fread(&h, sizeof h, 1, f) == 1 && h.magic == SAVE_MAGIC &&
              h.version == SAVE_VERSION && h.inventory_size == sizeof g->inventory &&
              h.drops_size == sizeof g->drops && h.width > 0 && h.height > 0 &&
              h.width <= 1 << 16 && h.height <= 1 << 12 &&
              game_init(g, h.width, h.height, h.seed, WORLD_BLANK);
    if (ok) {
        World *w = &g->world;
        size_t cells = (size_t)w->width * w->height;
        ok = fread(w->tiles, 1, cells, f) == cells && fread(w->background, 1, cells, f) == cells &&
             fread(&g->inventory, sizeof g->inventory, 1, f) == 1 &&
             fread(g->drops, sizeof g->drops, 1, f) == 1;
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
