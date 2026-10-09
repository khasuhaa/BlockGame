#include "workers.h"

#include <stdlib.h>

#include "mob.h"
#include "tree.h"

enum { IDLE_NONE, IDLE_WORKING, IDLE_HUNGRY, IDLE_DONE };

static ItemStack *slots_of(Game *g, const Storage *st, const TileDef *def, int *count)
{
    *count = def->storage_columns * def->storage_rows;
    if (*count > CONTAINER_MAX_SLOTS) *count = CONTAINER_MAX_SLOTS;
    return st->container ? g->inventory.containers[st->container].slots : NULL;
}

static bool hired(Game *g, const Storage *st, const TileDef *def)
{
    int n;
    ItemStack *s = slots_of(g, st, def, &n);
    for (int i = 0; s && i < n; i++)
        if (s[i].count > 0 && item_def((ItemId)s[i].id)->hires) return true;
    return false;
}

/* The best tool in the chest for a block: how fast it is, 1 for a block that wants none, 0 if there is none that will do. */
static float tool_for(Game *g, const Storage *st, const TileDef *def, const TileDef *block)
{
    int n;
    ItemStack *s = slots_of(g, st, def, &n);
    float best = block->tool_class < 0 ? 1.0f : 0.0f;
    for (int i = 0; s && i < n; i++)
        if (s[i].count > 0 && block->tool_class >= 0 && item_def((ItemId)s[i].id)->tool_class == block->tool_class && item_def((ItemId)s[i].id)->tool_speed > best)
            best = item_def((ItemId)s[i].id)->tool_speed;
    return best;
}

/* Brings what lies dropped round a square into the post's chest: what there is no room for stays where it is. */
static bool sweep(Game *g, const Storage *st, int x, int y, int rx, int up, int down)
{
    bool any = false;
    for (int i = 0; i < MAX_DROPS; i++) {
        Drop *d = &g->drops[i];
        if (!d->active || d->attached || d->collect > 0.0f || d->container != 0) continue;
        int dx = world_dx(&g->world, x, d->x), dy = (int)d->y - y;
        if (abs(dx) > rx || dy < -up || dy > down) continue;
        if (!game_store_at(g, st->x, st->y, (ItemId)d->item)) continue;
        d->active = false;
        any = true;
    }
    return any;
}

/* Shows the post's worker on a square: the one it has, or a new one. */
static void show(Game *g, Storage *st, const TileDef *def, int x, int y)
{
    int kind = mob_id(def->work_mob);
    if (kind < 0) return;
    int at = st->given - 1;
    if (at < 0 || at >= MAX_MOBS || !g->mobs[at].active || g->mobs[at].kind != kind || !g->mobs[at].owned) {
        at = mob_spawn(g, kind, x, y, true, false);
        st->given = (int16_t)(at + 1);
        if (at < 0) return;
    }
    Mob *m = &g->mobs[at];
    if (m->x != x) m->dir = world_dx(&g->world, m->x, x) < 0 ? -1 : 1;
    m->x = m->to_x = world_wrap_x(&g->world, x);
    m->y = m->to_y = y;
    m->step_left = 0;
}

static bool dig(Game *g, Storage *st, const TileDef *def, int x, int y)
{
    if (!game_break_block(g, x, y)) return false;
    sweep(g, st, x, y, 1, 2, 2);
    show(g, st, def, x, y);
    return true;
}

static int mine(Game *g, Storage *st, const TileDef *def)
{
    World *w = &g->world;
    int sx = st->x + 1;
    for (int level = 1;; level++) {
        int top = st->y + 1 + 2 * level, bottom = top + 1;
        if (bottom >= w->height - 1) return IDLE_DONE; /* (the bottom of the world: nothing more to do) */
        /* The shaft, down to this level: dug, and laddered so that there is always a way back. */
        for (int y = st->y + 1; y <= bottom; y++) {
            if (world_is_solid(w, sx, y)) {
                const TileDef *block = tile_def(world_get(w, sx, y));
                if (!block->removable || tool_for(g, st, def, block) <= 0.0f || !dig(g, st, def, sx, y)) return IDLE_DONE;
                if (def->work_ladder != TILE_AIR) world_set_at(w, tile_def(def->work_ladder)->layer, sx, y, def->work_ladder);
                return IDLE_WORKING;
            }
            if (def->work_ladder != TILE_AIR && world_get_at(w, tile_def(def->work_ladder)->layer, sx, y) == TILE_AIR) world_set_at(w, tile_def(def->work_ladder)->layer, sx, y, def->work_ladder);
        }
        /* The tunnel: two high, as far as the tools will dig. */
        bool stopped = false;
        for (int x = sx + 1; x <= sx + def->work_reach && !stopped; x++)
            for (int y = top; y <= bottom && !stopped; y++) {
                if (!world_is_solid(w, x, y)) continue;
                const TileDef *block = tile_def(world_get(w, x, y));
                if (!block->removable || tool_for(g, st, def, block) <= 0.0f) stopped = true;
                else if (dig(g, st, def, x, y)) return IDLE_WORKING;
                else stopped = true;
            }
    }
}

static int farm(Game *g, Storage *st, const TileDef *def)
{
    World *w = &g->world;
    int n, side[2] = { 0, 0 };
    ItemStack *s = slots_of(g, st, def, &n);
    /* The patch: from the post out to the furthest marker on each side. */
    for (int d = 1; d <= def->work_reach; d++)
        for (int k = 0; k < 2; k++) {
            int x = st->x + (k ? d : -d);
            if (tile_def(world_get(w, x, st->y))->work_marker || tile_def(world_get_at(w, LAYER_BACKGROUND, x, st->y))->work_marker) side[k] = d;
        }
    for (int x = st->x - side[0]; x <= st->x + side[1]; x++) {
        if (x == st->x || !world_is_solid(w, x, st->y + 1) || world_is_solid(w, x, st->y)) continue;
        TileType grown = world_get_at(w, LAYER_BACKGROUND, x, st->y);
        const TileDef *crop = tile_def(grown);
        if (crop->work_marker) continue;
        if (grown != TILE_AIR && crop->soil_count > 0 && crop->grows_into == TILE_AIR) {
            /* Ripe: reaped. */
            if (!game_break_block(g, x, st->y)) {
                world_set_at(w, LAYER_BACKGROUND, x, st->y, TILE_AIR);
                for (int k = 0; crop->drop != ITEM_NONE && k < (crop->drop_count > 0 ? crop->drop_count : 1); k++)
                    if (!game_store_at(g, st->x, st->y, crop->drop)) game_drop_item(g, crop->drop, x, st->y);
            }
            sweep(g, st, x, st->y, 1, 2, 1);
            show(g, st, def, x, st->y);
            return IDLE_WORKING;
        }
        if (grown != TILE_AIR || world_get(w, x, st->y) != TILE_AIR) continue;
        /* Bare: sown with the first thing in the chest that will grow there. */
        TileType ground = world_get(w, x, st->y + 1);
        for (int i = 0; s && i < n; i++) {
            const ItemDef *seed = item_def((ItemId)s[i].id);
            if (s[i].count == 0 || seed->sows == TILE_AIR) continue;
            bool grows = false;
            for (int k = 0; k < tile_def(seed->sows)->soil_count; k++) grows = grows || tile_def(seed->sows)->soil[k] == ground;
            if (!grows) continue;
            world_set_at(w, tile_def(seed->sows)->layer, x, st->y, seed->sows);
            if (--s[i].count == 0) s[i] = (ItemStack){ 0 };
            show(g, st, def, x, st->y);
            return IDLE_WORKING;
        }
    }
    return IDLE_DONE;
}

static int lumber(Game *g, Storage *st, const TileDef *def)
{
    World *w = &g->world;
    if (sweep(g, st, st->x, st->y, def->work_reach, 12, 2)) return IDLE_WORKING;
    for (int d = 1; d <= def->work_reach; d++)
        for (int k = -1; k <= 1; k += 2) {
            int x = st->x + k * d;
            /* (A tree with three of trunk and more is grown enough.) */
            if (tree_trunk_at(w, x, st->y) < 0 || tree_trunk_at(w, x, st->y - 1) < 0 || tree_trunk_at(w, x, st->y - 2) < 0) continue;
            int layer = tile_def(world_get_at(w, LAYER_BACKGROUND, x, st->y))->drop != ITEM_NONE && world_get(w, x, st->y) == TILE_AIR ? LAYER_BACKGROUND : LAYER_FOREGROUND;
            const TileDef *trunk = tile_def(world_get_at(w, layer, x, st->y));
            if (tool_for(g, st, def, trunk) <= 0.0f) continue;
            game_decay_tree(g, x, st->y);
            world_set_at(w, layer, x, st->y, TILE_AIR);
            if (trunk->drop != ITEM_NONE && !game_store_at(g, st->x, st->y, trunk->drop)) game_drop_item(g, trunk->drop, x, st->y);
            show(g, st, def, x, st->y);
            return IDLE_WORKING;
        }
    return IDLE_DONE;
}

static int breed(Game *g, Storage *st, const TileDef *def)
{
    World *w = &g->world;
    int flock = 0;
    for (int i = 0; i < MAX_MOBS; i++) {
        const Mob *m = &g->mobs[i];
        flock += m->active && !content.mobs[m->kind].worker && abs(world_dx(w, st->x, m->x)) <= def->work_reach && abs(m->y - st->y) <= 4;
    }
    if (sweep(g, st, st->x, st->y, def->work_reach, 4, 2)) return IDLE_WORKING;
    /* (Until there is a flock the eggs are left to hatch.) */
    if (flock < def->work_flock) return IDLE_DONE;
    for (int dx = -def->work_reach; dx <= def->work_reach; dx++)
        for (int dy = -4; dy <= 2; dy++)
            for (int layer = LAYER_FOREGROUND; layer <= LAYER_BACKGROUND; layer++) {
                int x = st->x + dx, y = st->y + dy;
                if (!world_in_bounds(w, x, y)) continue;
                const TileDef *egg = tile_def(world_get_at(w, layer, x, y));
                if (egg->hatches < 0 || world_get_at(w, layer, x, y) == TILE_AIR || egg->drop == ITEM_NONE) continue;
                if (!game_store_at(g, st->x, st->y, egg->drop)) return IDLE_DONE;
                world_set_at(w, layer, x, y, TILE_AIR);
                show(g, st, def, x, y);
                return IDLE_WORKING;
            }
    return IDLE_DONE;
}

static int fish(Game *g, Storage *st, const TileDef *def)
{
    World *w = &g->world;
    int total = 0;
    for (int k = 0; k < def->work_catch_count; k++) total += def->work_catch[k].weight;
    if (total <= 0) return IDLE_DONE;
    for (int d = 1; d <= def->work_reach; d++)
        for (int k = -1; k <= 1; k += 2)
            for (int dy = 0; dy <= 6; dy++) {
                if (!world_is_water(w, st->x + k * d, st->y + dy)) continue;
                int roll = (int)(game_random(g) % (uint32_t)total);
                for (int c = 0; c < def->work_catch_count; c++) {
                    if (roll < def->work_catch[c].weight) {
                        if (!game_store_at(g, st->x, st->y, def->work_catch[c].item)) return IDLE_DONE;
                        break;
                    }
                    roll -= def->work_catch[c].weight;
                }
                /* (At the water's edge: on the last dry square toward it.) */
                show(g, st, def, st->x + k * (d - 1), st->y);
                return IDLE_WORKING;
            }
    return IDLE_DONE;
}

static int guard(Game *g, Storage *st, const TileDef *def)
{
    World *w = &g->world;
    int n, damage = 2;
    ItemStack *s = slots_of(g, st, def, &n);
    for (int i = 0; s && i < n; i++)
        if (s[i].count > 0 && item_def((ItemId)s[i].id)->damage > damage) damage = item_def((ItemId)s[i].id)->damage;
    for (int i = 0; i < MAX_MOBS; i++) {
        const Mob *m = &g->mobs[i];
        if (!m->active || m->owned) continue;
        const MobDef *kind = &content.mobs[m->kind];
        if (!kind->hunts && kind->sting_ticks <= 0 && kind->strike_power <= 0 && kind->sting_power <= 0) continue;
        if (abs(world_dx(w, st->x, m->x)) > def->work_reach || abs(m->y - st->y) > def->work_reach) continue;
        show(g, st, def, m->x - (world_dx(w, st->x, m->x) < 0 ? -1 : 1), m->y);
        mob_hurt(g, i, damage);
        return IDLE_WORKING;
    }
    return IDLE_DONE;
}

/* A meal out of the chest, every so many times the post has worked. False if one is due and there is nothing to eat. */
static bool fed(Game *g, Storage *st, const TileDef *def)
{
    int every = content.rules.worker_meal_steps, n;
    if (every <= 0) return true;
    if (st->recipe < every) return true;
    ItemStack *s = slots_of(g, st, def, &n);
    for (int i = 0; s && i < n; i++) {
        if (s[i].count == 0 || item_def((ItemId)s[i].id)->food <= 0.0f || item_def((ItemId)s[i].id)->hires) continue;
        if (--s[i].count == 0) s[i] = (ItemStack){ 0 };
        st->recipe = 0;
        return true;
    }
    return false;
}

static int work(Game *g, Storage *st, const TileDef *def)
{
    if (!hired(g, st, def)) return IDLE_NONE;
    if (!fed(g, st, def)) return IDLE_HUNGRY;
    int did = IDLE_DONE;
    switch (def->work_kind) {
    case POST_MINE: did = mine(g, st, def); break;
    case POST_FARM: did = farm(g, st, def); break;
    case POST_LUMBER: did = lumber(g, st, def); break;
    case POST_BREED: did = breed(g, st, def); break;
    case POST_FISH: did = fish(g, st, def); break;
    case POST_GATHER: did = sweep(g, st, st->x, st->y, def->work_reach, 8, 3) ? IDLE_WORKING : IDLE_DONE; break;
    case POST_GUARD: did = guard(g, st, def); break;
    default: break;
    }
    if (did == IDLE_WORKING && st->recipe < 30000) st->recipe++;
    return did;
}

int game_worker_state(const Game *g, int x, int y)
{
    for (int i = 0; i < MAX_STORAGES; i++)
        if (g->storages[i].used && g->storages[i].x == x && g->storages[i].y == y && tile_def(game_bench_at(g, x, y))->work_kind != POST_NONE) return g->storages[i].made;
    return IDLE_NONE;
}

void workers_tick(Game *g)
{
    if (!content.has_workers || g->tick % 10 != 0) return;
    static bool kept[MAX_MOBS];
    for (int i = 0; i < MAX_MOBS; i++) kept[i] = false;
    for (int i = 0; i < MAX_STORAGES; i++) {
        Storage *st = &g->storages[i];
        if (!st->used) continue;
        const TileDef *def = tile_def(game_bench_at(g, st->x, st->y));
        if (def->work_kind == POST_NONE) continue;
        st->work = (int16_t)(st->work + 10);
        if (st->work >= def->work_ticks) {
            st->work = 0;
            int state = work(g, st, def);
            st->made = (int16_t)state;
            /* With nothing to do - or nothing to eat - the worker stands by the post; with nobody hired there is none. */
            if (state == IDLE_HUNGRY || state == IDLE_DONE) show(g, st, def, st->x, st->y);
            if (state == IDLE_NONE) st->given = 0;
        }
        if (st->given > 0 && st->given <= MAX_MOBS) kept[st->given - 1] = true;
    }
    /* A worker whose post has gone, or has let them go, goes too. */
    for (int i = 0; i < MAX_MOBS; i++)
        if (g->mobs[i].active && content.mobs[g->mobs[i].kind].worker && !kept[i]) g->mobs[i].active = false;
}
