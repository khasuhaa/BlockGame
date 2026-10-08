#include "signal.h"

#include "water.h"

#include <stdlib.h>
#include <string.h>

static const int around[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

/* The block of a square that has to do with signals, and which layer it is in: the one in front first. */
static const TileDef *device_in(const World *w, int x, int y, int *layer)
{
    for (int l = 0; l < BLOCK_LAYERS; l++) {
        const TileDef *def = tile_def(world_get_at(w, l, x, y));
        if (!def->device) continue;
        if (layer) *layer = l;
        return def;
    }
    return NULL;
}

static size_t cell_of(const World *w, int x, int y)
{
    return (size_t)y * w->width + world_wrap_x(w, x);
}

/* Whether a block makes a signal of its own (rather than carrying one, or only acting on one). */
static bool makes(const TileDef *def)
{
    return def->signal_out > 0 || def->signal_clock > 0 || def->signal_daylight || def->signal_in_count > 0;
}

/* What the square at (x, y) offers the square a step (dx, dy) on from it, out of a table of what every square
 * has: a wire gives what it carries, and a source what it makes - if it makes it that way. */
static int offers(const Game *g, const uint8_t *levels, int x, int y, int dx, int dy)
{
    const World *w = &g->world;
    if (!world_in_bounds(w, x, y)) return 0;
    const TileDef *def = device_in(w, x, y, NULL);
    if (!def) return 0;
    int level = levels[cell_of(w, x, y)];
    if (def->signal_wire) return level;
    if (!makes(def)) return 0;
    if (def->signal_toward[0] != 0 || def->signal_toward[1] != 0) return def->signal_toward[0] == dx && def->signal_toward[1] == dy ? level : 0;
    /* (One that reads some sides gives nothing back to them.) */
    for (int i = 0; i < def->signal_in_count; i++)
        if (def->signal_in[i][0] == dx && def->signal_in[i][1] == dy) return 0;
    return level;
}

int game_signal_at(const Game *g, int x, int y)
{
    const World *w = &g->world;
    if (!g->signal || !world_in_bounds(w, x, y) || !device_in(w, x, y, NULL)) return 0;
    return g->signal[cell_of(w, x, y)];
}

/* The most that reaches a square from the four beside it. */
static int reaching(const Game *g, const uint8_t *levels, int x, int y)
{
    int most = 0;
    for (int i = 0; i < 4; i++) {
        int got = offers(g, levels, x + around[i][0], y + around[i][1], -around[i][0], -around[i][1]);
        if (got > most) most = got;
    }
    return most;
}

/* ---- pistons ------------------------------------------------------------- */

/* Whether the block in front of a square can be pushed at all. */
static bool movable(const World *w, int x, int y)
{
    TileType t = world_get(w, x, y);
    const TileDef *def = tile_def(t);
    if (t == TILE_AIR || !world_in_bounds(w, x, y)) return false;
    return def->removable && !def->immovable && !def->piston && def->height <= 1 && !def->bench && !def->trade && def->storage_columns == 0 &&
           def->pair == TILE_AIR && def->pair_of == TILE_AIR && !def->written && def->fuel_capacity == 0;
}

enum { MOST_PUSHED = 24 };

/* Pushes whatever is in the square (x, y), and all that has to go with it, one square along (dx, dy). False,
 * with nothing moved, if something in the way cannot go. Nothing there at all is a push that worked. */
static bool push_from(Game *g, int x, int y, int dx, int dy, int px, int py)
{
    World *w = &g->world;
    TilePos set[MOST_PUSHED];
    int count = 0;
    if (!world_in_bounds(w, x, y)) return false;
    if (world_get(w, x, y) == TILE_AIR) return true;
    if (!movable(w, x, y)) return false;
    set[count++] = (TilePos){ world_wrap_x(w, x), y };
    for (int i = 0; i < count; i++) {
        /* What is in front of it has to go too; and a sticky block takes what is beside it. */
        TilePos next[4];
        int wanted = 0;
        next[wanted++] = (TilePos){ world_wrap_x(w, set[i].x + dx), set[i].y + dy };
        bool sticky = tile_def(world_get(w, set[i].x, set[i].y))->sticky;
        for (int k = 0; sticky && k < 4; k++) {
            if (around[k][0] == dx && around[k][1] == dy) continue;
            next[wanted++] = (TilePos){ world_wrap_x(w, set[i].x + around[k][0]), set[i].y + around[k][1] };
        }
        for (int k = 0; k < wanted; k++) {
            bool must = k == 0, already = next[k].x == world_wrap_x(w, px) && next[k].y == py;
            for (int j = 0; j < count && !already; j++) already = set[j].x == next[k].x && set[j].y == next[k].y;
            if (already) continue;
            if (!world_in_bounds(w, next[k].x, next[k].y)) {
                if (must) return false;
                continue;
            }
            if (world_get(w, next[k].x, next[k].y) == TILE_AIR) continue;
            if (!movable(w, next[k].x, next[k].y)) {
                if (must) return false;
                continue;
            }
            /* (What a sticky block would take along comes only if it is free to: nothing is held back for its sake.) */
            if (!must) {
                int fx = world_wrap_x(w, next[k].x + dx), fy = next[k].y + dy;
                bool free = world_in_bounds(w, fx, fy) && world_get(w, fx, fy) == TILE_AIR && !(fx == world_wrap_x(w, px) && fy == py);
                for (int j = 0; j < count && !free; j++) free = set[j].x == fx && set[j].y == fy;
                if (!free || count >= MOST_PUSHED) continue;
            }
            if (count >= MOST_PUSHED) return false;
            set[count++] = next[k];
        }
    }
    /* The furthest along goes first, so that nothing lands on what has not gone yet. */
    for (int i = 0; i < count; i++)
        for (int j = i + 1; j < count; j++)
            if (set[j].x * dx + set[j].y * dy > set[i].x * dx + set[i].y * dy ||
                /* (Round the join of the world, the further is the one a step on from the other.) */
                (dx != 0 && world_dx(w, set[i].x, set[j].x) * dx > 0 && set[j].x * dx <= set[i].x * dx)) {
                TilePos swap = set[i];
                set[i] = set[j];
                set[j] = swap;
            }
    for (int i = 0; i < count; i++) {
        int tx = set[i].x + dx, ty = set[i].y + dy;
        TileType t = world_get(w, set[i].x, set[i].y);
        int kept = world_meta(w, set[i].x, set[i].y), paint = world_paint(w, false, set[i].x, set[i].y);
        /* Whoever is where it is going is moved on by it; whoever was standing on it goes with it. */
        game_shove(g, tx, ty, dx, dy);
        if (dy == 0) game_shove(g, set[i].x, set[i].y - 1, dx, 0);
        world_set(w, set[i].x, set[i].y, TILE_AIR);
        world_set(w, tx, ty, t);
        if (kept) world_set_meta(w, tx, ty, kept);
        if (paint) world_set_paint(w, false, tx, ty, paint);
    }
    return true;
}

/* A piston that has just got a signal: what is in front goes along, and its head goes out after it. */
static void piston_out(Game *g, const TileDef *def, int layer, int x, int y)
{
    World *w = &g->world;
    int dx = def->piston_dir[0], dy = def->piston_dir[1];
    if (def->powered_becomes == TILE_AIR || !world_in_bounds(w, x + dx, y + dy)) return;
    if (!push_from(g, x + dx, y + dy, dx, dy, x, y)) return;
    game_shove(g, x + dx, y + dy, dx, dy);
    world_set_at(w, layer, x, y, def->powered_becomes);
    if (def->piston_head != TILE_AIR) world_set(w, x + dx, y + dy, def->piston_head);
}

/* One that has lost it: the head comes back, and with it, for a sticky one, the block that was against it. */
static void piston_in(Game *g, const TileDef *def, int layer, int x, int y)
{
    World *w = &g->world;
    int dx = def->piston_dir[0], dy = def->piston_dir[1];
    if (def->unpowered_becomes == TILE_AIR) return;
    if (def->piston_head != TILE_AIR && world_get(w, x + dx, y + dy) == def->piston_head) world_set(w, x + dx, y + dy, TILE_AIR);
    world_set_at(w, layer, x, y, def->unpowered_becomes);
    if (!def->piston_sticky || !movable(w, x + 2 * dx, y + 2 * dy)) return;
    TileType t = world_get(w, x + 2 * dx, y + 2 * dy);
    int kept = world_meta(w, x + 2 * dx, y + 2 * dy), paint = world_paint(w, false, x + 2 * dx, y + 2 * dy);
    game_shove(g, x + dx, y + dy, -dx, -dy);
    world_set(w, x + 2 * dx, y + 2 * dy, TILE_AIR);
    world_set(w, x + dx, y + dy, t);
    if (kept) world_set_meta(w, x + dx, y + dy, kept);
    if (paint) world_set_paint(w, false, x + dx, y + dy, paint);
}

/* ---- the tick ------------------------------------------------------------ */

bool game_use_block(Game *g, int x, int y)
{
    World *w = &g->world;
    for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
        const TileDef *def = tile_def(world_get_at(w, layer, x, y));
        if (def->use_becomes == TILE_AIR) continue;
        /* (A piston that is out is not turned: it is in the middle of something.) */
        if (def->piston_out) return false;
        world_set_at(w, layer, x, y, def->use_becomes);
        if (tile_def(def->use_becomes)->revert_ticks > 0) world_set_meta(w, x, y, tile_def(def->use_becomes)->revert_ticks);
        return true;
    }
    return false;
}

void signal_tick(Game *g)
{
    World *w = &g->world;
    if (!w->devices_known) world_find_devices(w);
    if (w->device_count == 0) return;
    size_t cells = (size_t)w->width * w->height;
    if (g->signal_cells != cells) {
        free(g->signal);
        free(g->signal_was);
        g->signal = calloc(cells, 1);
        g->signal_was = calloc(cells, 1);
        g->signal_cells = g->signal && g->signal_was ? cells : 0;
        if (!g->signal_cells) return;
    }
    /* (What is here now: blocks come and go as this runs.) */
    int count = w->device_count;
    int32_t *list = malloc(sizeof(int32_t) * (size_t)count);
    if (!list) return;
    memcpy(list, w->devices, sizeof(int32_t) * (size_t)count);
    uint8_t *now = g->signal, *was = g->signal_was;
    bool day = false;
    game_day_phase(g, &day);

    /* What happens by itself: a button comes back up, a plate goes down under whoever is on it. */
    for (int i = 0; i < count; i++) {
        int x = list[i] % w->width, y = list[i] / w->width, layer = 0;
        const TileDef *def = device_in(w, x, y, &layer);
        if (!def) continue;
        if (def->reverts_to != TILE_AIR) {
            int left = world_meta(w, x, y);
            if (left <= 1) world_set_at(w, layer, x, y, def->reverts_to);
            else world_set_meta(w, x, y, left - 1);
            continue;
        }
        if (def->stood_becomes != TILE_AIR || def->clear_becomes != TILE_AIR) {
            bool on = game_square_stood(g, x, y, def->stood_by_items);
            if (on && def->stood_becomes != TILE_AIR) world_set_at(w, layer, x, y, def->stood_becomes);
            if (!on && def->clear_becomes != TILE_AIR) world_set_at(w, layer, x, y, def->clear_becomes);
        }
    }

    /* What every source makes this tick; wires start at nothing. One that reads its sides goes by what they had last tick. */
    for (int i = 0; i < count; i++) {
        int x = list[i] % w->width, y = list[i] / w->width;
        const TileDef *def = device_in(w, x, y, NULL);
        now[list[i]] = 0;
        if (!def || def->signal_wire || !makes(def)) continue;
        int level = def->signal_out;
        if (def->signal_clock > 0) level = (g->tick / (uint64_t)def->signal_clock) % 2 == 0 ? 15 : 0;
        if (def->signal_daylight) level = day ? 15 : 0;
        if (def->signal_in_count > 0) {
            int on = 0;
            for (int k = 0; k < def->signal_in_count; k++)
                on += offers(g, was, x + def->signal_in[k][0], y + def->signal_in[k][1], -def->signal_in[k][0], -def->signal_in[k][1]) > 0;
            bool want = (def->signal_and ? on == def->signal_in_count : on > 0) != def->signal_invert, had = was[list[i]] > 0;
            /* (It follows its inputs only when they have stood as long as it takes.) */
            if (want != had && def->signal_delay > 1) {
                int waited = world_meta(w, x, y) + 1;
                if (waited < def->signal_delay) {
                    world_set_meta(w, x, y, waited);
                    want = had;
                } else {
                    world_set_meta(w, x, y, 0);
                }
            } else if (def->signal_delay > 1 && world_meta(w, x, y) != 0) {
                world_set_meta(w, x, y, 0);
            }
            level = want ? 15 : 0;
        }
        now[list[i]] = (uint8_t)level;
    }
    /* Along the wires, strongest first: each a point weaker than what it had it from. */
    for (int i = 0; i < count; i++) {
        int x = list[i] % w->width, y = list[i] / w->width;
        const TileDef *def = device_in(w, x, y, NULL);
        if (!def || !def->signal_wire) continue;
        int most = 0;
        for (int k = 0; k < 4; k++) {
            int nx = x + around[k][0], ny = y + around[k][1];
            const TileDef *by = world_in_bounds(w, nx, ny) ? device_in(w, nx, ny, NULL) : NULL;
            if (!by || by->signal_wire) continue;
            int got = offers(g, now, nx, ny, -around[k][0], -around[k][1]);
            if (got > most) most = got;
        }
        now[list[i]] = (uint8_t)most;
    }
    for (int level = 15; level > 1; level--) {
        for (int i = 0; i < count; i++) {
            if (now[list[i]] != level) continue;
            int x = list[i] % w->width, y = list[i] / w->width;
            const TileDef *def = device_in(w, x, y, NULL);
            if (!def || !def->signal_wire) continue;
            for (int k = 0; k < 4; k++) {
                int nx = x + around[k][0], ny = y + around[k][1];
                if (!world_in_bounds(w, nx, ny)) continue;
                const TileDef *by = device_in(w, nx, ny, NULL);
                size_t at = cell_of(w, nx, ny);
                if (by && by->signal_wire && now[at] < level - 1) now[at] = (uint8_t)(level - 1);
            }
        }
    }

    /* What acts on a signal: powered if one reaches its square, or is in it. */
    for (int i = 0; i < count; i++) {
        int x = list[i] % w->width, y = list[i] / w->width, layer = 0;
        const TileDef *def = device_in(w, x, y, &layer);
        if (!def || def->signal_wire || makes(def)) continue;
        bool acts = def->powered_becomes != TILE_AIR || def->unpowered_becomes != TILE_AIR || def->pulse_becomes != TILE_AIR || def->piston ||
                    def->breaks || def->deploys || def->spout != 0 || def->blast > 0;
        if (!acts) continue;
        /* (A wire behind it, in its own square, counts.) */
        const TileDef *behind = tile_def(world_get_at(w, LAYER_BACKGROUND, x, y));
        bool powered = reaching(g, now, x, y) > 0 || (layer == LAYER_FOREGROUND && behind->signal_wire && now[list[i]] > 0);
        bool had = was[list[i]] > 0, arrived = powered && !had;
        int ax = x + def->act_dir[0], ay = y + def->act_dir[1];
        if (!behind->signal_wire || layer != LAYER_FOREGROUND) now[list[i]] = powered ? 1 : 0;
        if (def->piston) {
            if (powered && !def->piston_out) piston_out(g, def, layer, x, y);
            else if (!powered && def->piston_out) piston_in(g, def, layer, x, y);
            continue;
        }
        if (arrived && def->breaks) game_break_block(g, ax, ay);
        if (arrived && def->deploys) game_deploy_block(g, x, y, ax, ay);
        if (powered && def->spout != 0 && world_in_bounds(w, ax, ay) && !world_is_solid(w, ax, ay) && g->tick % 4 == 0) {
            if (def->spout > 0) water_add(w, ax, ay, def->spout);
            else world_set_water(w, ax, ay, world_water(w, ax, ay) + def->spout > 0 ? world_water(w, ax, ay) + def->spout : 0);
            water_wake(w, ax);
        }
        if (arrived && def->blast > 0) {
            world_set_at(w, layer, x, y, TILE_AIR);
            game_blast(g, x, y, def->blast);
            continue;
        }
        if (arrived && def->pulse_becomes != TILE_AIR) world_set_at(w, layer, x, y, def->pulse_becomes);
        else if (powered && def->powered_becomes != TILE_AIR) world_set_at(w, layer, x, y, def->powered_becomes);
        else if (!powered && def->unpowered_becomes != TILE_AIR) world_set_at(w, layer, x, y, def->unpowered_becomes);
    }

    /* Walkways: whoever is on one is carried along it. */
    for (int i = 0; i < count; i++) {
        int x = list[i] % w->width, y = list[i] / w->width;
        const TileDef *def = tile_def(world_get(w, x, y));
        if (def->walkway == 0 || def->walk_ticks < 1 || g->tick % (uint64_t)def->walk_ticks != 0) continue;
        game_shove(g, x, y - 1, def->walkway, 0);
    }

    for (int i = 0; i < count; i++) was[list[i]] = now[list[i]];
    free(list);
}
