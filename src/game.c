#include "game.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- movement rules ---------------------------------------------------- */

static bool body_fits(const World *w, int x, int y)
{
    return !world_is_solid(w, x, y) && !world_is_solid(w, x, y - 1);
}

static bool is_supported(const World *w, TilePos feet)
{
    return world_is_solid(w, feet.x, feet.y + 1);
}

/* A wall on side `d` that a player at `feet` can hold on to. */
static bool can_cling_side(const World *w, TilePos feet, int d)
{
    return world_is_solid(w, feet.x + d, feet.y) || world_is_solid(w, feet.x + d, feet.y - 1);
}

/* Something climbable behind the player's body, such as a tree. */
static bool can_hold(const World *w, TilePos feet)
{
    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        if (tile_def(world_get_at(w, layer, feet.x, feet.y))->climbable ||
            tile_def(world_get_at(w, layer, feet.x, feet.y - 1))->climbable)
            return true;
    }
    return false;
}

static bool can_cling(const World *w, TilePos feet)
{
    return can_cling_side(w, feet, -1) || can_cling_side(w, feet, 1) || can_hold(w, feet);
}

/* Pull up onto the block beside the feet: a one-block ledge when standing,
 * the top of the wall when climbing. */
static bool can_mantle(const World *w, TilePos feet, int d)
{
    return world_is_solid(w, feet.x + d, feet.y) && !world_is_solid(w, feet.x, feet.y - 2) &&
           body_fits(w, feet.x + d, feet.y - 1);
}

static bool can_climb_up(const World *w, TilePos feet)
{
    return !world_is_solid(w, feet.x, feet.y - 2) &&
           can_cling(w, (TilePos){ feet.x, feet.y - 1 });
}

/* Leap from one wall to a facing wall on side `d`. Returns the distance, or 0. */
static int wall_hop_distance(const World *w, TilePos feet, int d)
{
    for (int k = 1; k <= MAX_WALL_HOP; k++) {
        TilePos to = { feet.x + d * k, feet.y };
        if (!body_fits(w, to.x, to.y)) return 0;
        if (can_cling_side(w, to, d)) return k;
    }
    return 0;
}

/* Where a pose ends up once gravity is done with it. */
static Pose pose_rest(const World *w, Pose p)
{
    if (is_supported(w, p.feet)) {
        p.climbing = false;
    } else if (!p.climbing || !can_cling(w, p.feet)) {
        p.climbing = false;
        while (!is_supported(w, p.feet)) p.feet.y++;
    }
    return p;
}

static Pose pose_after(const World *w, Pose p, Step s)
{
    p.feet.x = world_wrap_x(w, p.feet.x + s.dx);
    p.feet.y += s.dy;
    p.climbing = s.climb;
    return pose_rest(w, p);
}

/* Every step open to a player at rest in pose `p`. `out` needs MAX_MOVES. */
static int list_moves(const World *w, Pose p, Step *out)
{
    int n = 0;
    int x = p.feet.x, y = p.feet.y;

    if (p.climbing) {
        if (can_climb_up(w, p.feet)) out[n++] = (Step){ 0, -1, true };
        out[n++] = (Step){ 0, 1, true };
        for (int d = -1; d <= 1; d += 2) {
            int k;
            if (can_mantle(w, p.feet, d)) out[n++] = (Step){ (int8_t)d, -1, false };
            else if (body_fits(w, x + d, y) && can_hold(w, (TilePos){ x + d, y }))
                out[n++] = (Step){ (int8_t)d, 0, true }; /* sideways across a tree */
            else if ((k = wall_hop_distance(w, p.feet, d)) > 0)
                out[n++] = (Step){ (int8_t)(d * k), 0, true };
        }
        return n;
    }

    for (int d = -1; d <= 1; d += 2) {
        if (!body_fits(w, x + d, y)) {
            if (can_mantle(w, p.feet, d)) out[n++] = (Step){ (int8_t)d, -1, false };
        } else if (is_supported(w, (TilePos){ x + d, y })) {
            out[n++] = (Step){ (int8_t)d, 0, false };
        } else {
            /* An edge: hop a one-block gap, step down one block, or go over
             * the side and hang on the wall of a deeper drop. */
            if (body_fits(w, x + 2 * d, y) && is_supported(w, (TilePos){ x + 2 * d, y }))
                out[n++] = (Step){ (int8_t)(2 * d), 0, false };
            if (world_is_solid(w, x + d, y + 2)) out[n++] = (Step){ (int8_t)d, 0, false };
            else out[n++] = (Step){ (int8_t)d, 1, true };
        }
    }
    /* Jump and catch a wall too tall to step onto. */
    if (can_climb_up(w, p.feet)) out[n++] = (Step){ 0, -1, true };
    return n;
}

static bool move_allowed(const World *w, Pose p, Step s)
{
    Step moves[MAX_MOVES];
    int n = list_moves(w, p, moves);
    for (int i = 0; i < n; i++) {
        if (moves[i].dx == s.dx && moves[i].dy == s.dy && moves[i].climb == s.climb) return true;
    }
    return false;
}

/* The step held movement keys ask for, or a zero step for none. Holding a
 * direction carries the player over whatever is in the way: across one-block
 * gaps, down and up walls, and from wall to wall across a pit. */
static Step key_step(const World *w, Pose p, int dx, int dy)
{
    const Step none = { 0, 0, false };
    int x = p.feet.x, y = p.feet.y;

    if (p.climbing) {
        if (dy < 0 && can_climb_up(w, p.feet)) return (Step){ 0, -1, true };
        if (dy > 0) return (Step){ 0, 1, true };
        if (dx == 0) {
            /* W at the top of a wall pulls up onto it. */
            for (int d = -1; d <= 1 && dy < 0; d += 2)
                if (can_mantle(w, p.feet, d)) return (Step){ (int8_t)d, -1, false };
            return none;
        }
        if (can_mantle(w, p.feet, dx)) return (Step){ (int8_t)dx, -1, false };
        if (can_cling_side(w, p.feet, dx))
            return can_climb_up(w, p.feet) ? (Step){ 0, -1, true } : none;
        if (body_fits(w, x + dx, y) && can_hold(w, (TilePos){ x + dx, y }))
            return (Step){ (int8_t)dx, 0, true };
        int k = wall_hop_distance(w, p.feet, dx);
        if (k > 0) return (Step){ (int8_t)(dx * k), 0, true };
        return dy < 0 ? none : (Step){ 0, 1, true };
    }

    if (dx != 0) {
        if (!body_fits(w, x + dx, y)) {
            if (can_mantle(w, p.feet, dx)) return (Step){ (int8_t)dx, -1, false };
            if (can_cling_side(w, (TilePos){ x, y - 1 }, dx) && !world_is_solid(w, x, y - 2))
                return (Step){ 0, -1, true };
            return none;
        }
        if (is_supported(w, (TilePos){ x + dx, y })) return (Step){ (int8_t)dx, 0, false };
        /* Holding down as well goes into a gap instead of hopping it. */
        if (dy <= 0 && body_fits(w, x + 2 * dx, y) && is_supported(w, (TilePos){ x + 2 * dx, y }))
            return (Step){ (int8_t)(2 * dx), 0, false };
        if (world_is_solid(w, x + dx, y + 2)) return (Step){ (int8_t)dx, 0, false };
        return (Step){ (int8_t)dx, 1, true };
    }
    if (dy < 0 && !world_is_solid(w, x, y - 2)) return (Step){ 0, -1, true };
    return none;
}

bool game_in_mine_range(TilePos feet, TilePos target)
{
    int dx = target.x - feet.x;
    int dy = target.y - feet.y;
    if (dx < -1 || dx > 1 || dy < -2 || dy > 1) return false;
    return !(dx == 0 && (dy == 0 || dy == -1));
}

/* ---- planning ---------------------------------------------------------- */

static bool on_body(TilePos feet, TilePos t)
{
    return t.x == feet.x && (t.y == feet.y || t.y == feet.y - 1);
}

/* game_in_mine_range, measured the short way round the world. */
static bool in_reach(const World *w, TilePos feet, TilePos target)
{
    return game_in_mine_range((TilePos){ 0, feet.y },
                              (TilePos){ world_dx(w, feet.x, target.x), target.y });
}

static bool in_use_range(const World *w, TilePos feet, TilePos bench)
{
    return on_body(feet, bench) || in_reach(w, feet, bench);
}

/* Which layer a mine action on a tile takes its block from: the foreground
 * if there is a block there, otherwise the background; -1 if neither has
 * anything that can be mined. */
static int mine_layer(const World *w, TilePos t)
{
    TileType front = world_get(w, t.x, t.y);
    if (front != TILE_AIR) return tile_def(front)->removable ? LAYER_FOREGROUND : -1;
    return tile_def(world_get_at(w, LAYER_BACKGROUND, t.x, t.y))->removable ? LAYER_BACKGROUND : -1;
}

static bool is_goal(const World *w, Pose p, Action a)
{
    switch (a.type) {
    case ACTION_MINE:
        /* A background block can be mined from right in front of it. */
        if (!world_is_solid(w, a.target.x, a.target.y)) return in_use_range(w, p.feet, a.target);
        return in_reach(w, p.feet, a.target);
    case ACTION_PLACE:
    case ACTION_PLANT: return in_reach(w, p.feet, a.target);
    case ACTION_USE:
    case ACTION_COLLECT: return in_use_range(w, p.feet, a.target);
    default: return on_body(p.feet, a.target);
    }
}

static bool beside_block(const World *w, TilePos t)
{
    static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (int i = 0; i < 4; i++) {
        int x = t.x + dirs[i][0], y = t.y + dirs[i][1];
        if (world_in_bounds(w, x, y) && world_is_solid(w, x, y)) return true;
    }
    return false;
}

/* Whether an item can be planted on a block: it grows a tree, the block is
 * soil for that tree, and there is room above for the first trunk block. */
static bool can_plant(const World *w, ItemId item, TilePos t)
{
    int kind = item_def(item)->plants;
    if (kind < 0 || !world_in_bounds(w, t.x, t.y) || !world_in_bounds(w, t.x, t.y - 1)) return false;
    const TreeDef *def = &content.trees[kind];
    TileType ground = world_get(w, t.x, t.y);
    bool soil = false;
    for (int i = 0; i < def->soil_count; i++) soil = soil || def->soil[i] == ground;
    return soil && world_get_at(w, def->layer, t.x, t.y - 1) == TILE_AIR &&
           !world_is_solid(w, t.x, t.y - 1);
}

/* Whether the target tile is still something the action can be done to. */
static bool target_valid(const World *w, Action a)
{
    if (!world_in_bounds(w, a.target.x, a.target.y)) return false;
    if (a.type == ACTION_COLLECT) return true; /* the drop itself is checked on arrival */
    TileType t = world_get(w, a.target.x, a.target.y);
    switch (a.type) {
    case ACTION_MINE: return mine_layer(w, a.target) >= 0;
    case ACTION_PLACE:
        return t == TILE_AIR && item_def((ItemId)a.item)->places != TILE_AIR &&
               beside_block(w, a.target);
    case ACTION_PLANT: return can_plant(w, (ItemId)a.item, a.target);
    case ACTION_USE: return tile_def(t)->bench || tile_def(t)->storage_columns > 0;
    default: return !tile_def(t)->solid;
    }
}

static int pose_index(const World *w, Pose p)
{
    return (p.feet.y * w->width + p.feet.x) * 2 + (p.climbing ? 1 : 0);
}

static Pose pose_at_index(const World *w, int idx)
{
    int cell = idx / 2;
    return (Pose){ { cell % w->width, cell / w->width }, idx % 2 != 0 };
}

/* Breadth-first search over resting poses (standing or hanging on a wall)
 * for the fewest steps that accomplish `a` from `start`, which must be at
 * rest. On success writes the steps to `out` (if non-NULL), their count, and
 * the final pose. */
static bool plan_action(Game *g, const World *w, Pose start, Action a,
                        Step *out, int *out_count, Pose *end)
{
    if (!target_valid(w, a)) return false;

    int start_idx = pose_index(w, start);
    int goal_idx = -1;
    int head = 0, tail = 0;

    /* Stamping poses with the search number saves clearing them every time. */
    uint32_t gen = ++g->bfs_gen;
    g->bfs_seen[start_idx] = gen;
    g->bfs_queue[tail++] = start_idx;

    while (head < tail) {
        int idx = g->bfs_queue[head++];
        Pose pose = pose_at_index(w, idx);
        if (is_goal(w, pose, a)) {
            goal_idx = idx;
            break;
        }
        Step moves[MAX_MOVES];
        int n = list_moves(w, pose, moves);
        for (int i = 0; i < n; i++) {
            int next_idx = pose_index(w, pose_after(w, pose, moves[i]));
            if (g->bfs_seen[next_idx] == gen) continue;
            g->bfs_seen[next_idx] = gen;
            g->bfs_parent[next_idx] = idx;
            g->bfs_step[next_idx] = moves[i];
            g->bfs_queue[tail++] = next_idx;
        }
    }
    if (goal_idx < 0) return false;

    int count = 0;
    for (int idx = goal_idx; idx != start_idx; idx = g->bfs_parent[idx]) count++;
    if (out) {
        int i = count;
        for (int idx = goal_idx; idx != start_idx; idx = g->bfs_parent[idx])
            out[--i] = g->bfs_step[idx];
    }
    if (out_count) *out_count = count;
    if (end) *end = pose_at_index(w, goal_idx);
    return true;
}

/* What a finished action does to the scratch world. */
static Pose sim_effect(Game *g, Pose pose, Action a)
{
    int layer = mine_layer(&g->sim, a.target);
    if (a.type == ACTION_MINE && layer >= 0)
        world_set_at(&g->sim, layer, a.target.x, a.target.y, TILE_AIR);
    else if (a.type == ACTION_PLACE)
        world_set(&g->sim, a.target.x, a.target.y, item_def((ItemId)a.item)->places);
    else if (a.type == ACTION_PLANT)
        tree_plant(&g->sim, a.target.x, a.target.y - 1, item_def((ItemId)a.item)->plants);
    return pose_rest(&g->sim, pose);
}

/* Plays an action out instantly on the scratch world. */
static bool sim_action(Game *g, Pose *pose, Action a)
{
    Pose end;
    if (!plan_action(g, &g->sim, *pose, a, NULL, NULL, &end)) return false;
    *pose = sim_effect(g, end, a);
    return true;
}

static void deny(Game *g, TilePos pos)
{
    Denial *slot = &g->denials[0];
    for (int i = 0; i < MAX_DENIALS; i++) {
        if (g->denials[i].ttl < slot->ttl) slot = &g->denials[i];
    }
    slot->pos = pos;
    slot->ttl = DENIAL_TIME;
}

/* Fast-forwards the scratch world through everything already pending and
 * returns the pose the player will be left in. */
static Pose sim_pending(Game *g)
{
    const Player *p = &g->player;
    for (int layer = 0; layer < LAYER_COUNT; layer++)
        memcpy(g->sim.layers[layer], g->world.layers[layer],
               (size_t)g->world.width * g->world.height);
    Pose pos = { p->feet, p->climbing };
    if (p->activity == ACTIVITY_STEPPING)
        pos = (Pose){ { world_wrap_x(&g->world, p->step_to.x), p->step_to.y }, p->step.climb };
    pos = pose_rest(&g->sim, pos);
    if (g->has_current) {
        for (int i = g->step_index; i < g->step_count; i++)
            pos = pose_after(&g->sim, pos, g->steps[i]);
        pos = sim_effect(g, pos, g->current);
    }
    for (int i = 0; i < g->queue_count; i++) sim_action(g, &pos, g->queue[i]);
    return pos;
}

static bool uses_item(Action a)
{
    return a.type == ACTION_PLACE || a.type == ACTION_PLANT;
}

/* How many of an item the pending actions will use up. */
static int pending_places(const Game *g, ItemId item)
{
    int n = g->has_current && uses_item(g->current) && g->current.item == item;
    for (int i = 0; i < g->queue_count; i++)
        if (uses_item(g->queue[i]) && g->queue[i].item == item) n++;
    return n;
}

/* Queues `a` if it can be done from `pos` in the scratch world. */
static bool queue_checked(Game *g, Pose pos, Action a)
{
    bool ok = g->queue_count < MAX_QUEUED_ACTIONS;
    if (ok && uses_item(a))
        ok = inventory_count(&g->inventory, (ItemId)a.item) > pending_places(g, (ItemId)a.item);
    if (!ok || !sim_action(g, &pos, a)) {
        deny(g, a.target);
        return false;
    }
    g->queue[g->queue_count++] = a;
    return true;
}

bool game_queue_action(Game *g, ActionType type, int tx, int ty)
{
    Action a = { .type = type, .target = { world_wrap_x(&g->world, tx), ty } };
    if (uses_item(a)) a.item = inventory_selected(&g->inventory)->id;
    return queue_checked(g, sim_pending(g), a);
}

float game_mine_time(const Game *g, TileType t)
{
    const TileDef *def = tile_def(t);
    return def->mine_time / inventory_tool_speed(&g->inventory, def->tool_class);
}

static TilePos drop_tile(const Drop *d)
{
    return (TilePos){ d->x, (int)(d->y + 0.5f) };
}

bool game_queue_collect(Game *g, int drop)
{
    if (drop < 0 || drop >= MAX_DROPS) return false;
    const Drop *d = &g->drops[drop];
    if (!d->active || d->collect > 0.0f) return false;
    Action a = { ACTION_COLLECT, drop_tile(d), 0, drop, d->id };
    return queue_checked(g, sim_pending(g), a);
}

void game_drop_center(const Game *g, int drop, float *x, float *y)
{
    const Drop *d = &g->drops[drop];
    *x = (float)d->x + 0.5f;
    /* Fruit hangs in the middle of its leaf; anything else rests on the ground. */
    *y = d->y + (d->attached ? 0.5f : 0.7f);
}

int game_drop_at(const Game *g, float x, float y)
{
    int best = -1;
    float best_d2 = 0.5f * 0.5f;
    for (int i = 0; i < MAX_DROPS; i++) {
        const Drop *d = &g->drops[i];
        if (!d->active || d->collect > 0.0f) continue;
        float cx, cy;
        game_drop_center(g, i, &cx, &cy);
        float dx = (float)world_dx(&g->world, 0, (int)floorf(x) - d->x) + (x - floorf(x)) - 0.5f;
        float dy = y - cy;
        float d2 = dx * dx + dy * dy;
        if (d2 < best_d2) {
            best_d2 = d2;
            best = i;
        }
    }
    return best;
}

bool game_click_tile(Game *g, int tx, int ty)
{
    Pose pos = sim_pending(g);
    tx = world_wrap_x(&g->world, tx);
    Action a = { .type = ACTION_MOVE, .target = { tx, ty } };
    const ItemStack *held = inventory_selected(&g->inventory);

    Action plant = { .type = ACTION_PLANT, .target = a.target, .item = held->id };
    if (held->count > 0 && can_plant(&g->sim, (ItemId)held->id, a.target)) {
        /* Holding something that grows: soil with room above is where it is planted. */
        a = plant;
    } else if (world_in_bounds(&g->world, tx, ty) && mine_layer(&g->sim, a.target) >= 0) {
        /* A block here, in front or behind, is mined. */
        a.type = ACTION_MINE;
    } else if (held->count > 0 && item_def((ItemId)held->id)->places != TILE_AIR) {
        /* Holding a block: air beside a block is where it gets placed. */
        Action place = { .type = ACTION_PLACE, .target = a.target, .item = held->id };
        if (target_valid(&g->sim, place)) a = place;
    }
    return queue_checked(g, pos, a);
}

/* ---- simulation -------------------------------------------------------- */

/* Leaves an item lying in a tile, with whatever is inside it. With no room
 * for another drop it goes straight to the inventory instead. */
static Drop *spawn_drop_stack(Game *g, ItemStack one, TilePos at)
{
    if (one.id == ITEM_NONE) return NULL;
    for (int i = 0; i < MAX_DROPS; i++) {
        if (g->drops[i].active) continue;
        g->drops[i] = (Drop){ true, one.id, at.x, (float)at.y, 0.0f, 0.0f, false, 0,
                              ++g->next_drop_id, one.container };
        return &g->drops[i];
    }
    one.count = 1;
    if (inventory_add_stack(&g->inventory, one) != 0) container_free(&g->inventory, one.container);
    return NULL;
}

static Drop *spawn_drop(Game *g, ItemId item, TilePos at)
{
    return spawn_drop_stack(g, (ItemStack){ (uint8_t)item, 1, 0 }, at);
}

/* Puts a drop in the inventory, contents and all, if there is room. */
static bool collect_drop(Game *g, Drop *d)
{
    if (inventory_add_stack(&g->inventory, (ItemStack){ d->item, 1, d->container }) != 0) return false;
    d->container = 0;
    d->collect = 0.001f;
    return true;
}

/* ---- storage blocks ---------------------------------------------------- */

static Storage *storage_at(Game *g, TilePos t)
{
    for (int i = 0; i < MAX_STORAGES; i++)
        if (g->storages[i].used && g->storages[i].x == t.x && g->storages[i].y == t.y)
            return &g->storages[i];
    return NULL;
}

/* Whether a tile keeps anything a Storage record is needed for. */
static bool needs_storage(const TileDef *def)
{
    return def->storage_columns > 0 || def->fuel_capacity > 0;
}

/* Sets up the record of a newly placed block. A storage block takes over the
 * contents of the item it came from; anything else has none. */
static bool storage_create(Game *g, TilePos t, const TileDef *def, ItemStack from)
{
    for (int i = 0; i < MAX_STORAGES; i++) {
        if (g->storages[i].used) continue;
        uint16_t c = 0;
        if (def->storage_columns > 0) {
            c = from.container ? from.container : container_alloc(&g->inventory);
            if (c == 0) return false;
        } else {
            container_free(&g->inventory, from.container);
        }
        g->storages[i] = (Storage){ .used = true, .x = t.x, .y = t.y, .container = c };
        return true;
    }
    return false;
}

/* Takes the contents out of a storage block that is being removed: the
 * container to hand to its item, or 0 if there was nothing in it. */
static uint16_t storage_release(Game *g, TilePos t)
{
    Storage *st = storage_at(g, t);
    if (!st) return 0;
    uint16_t c = st->container;
    st->used = false;
    if (container_is_empty(&g->inventory, c)) {
        container_free(&g->inventory, c);
        c = 0;
    }
    return c;
}

uint16_t game_open_storage(const Game *g)
{
    if (!g->bench_open) return 0;
    Storage *st = storage_at((Game *)g, g->bench_pos);
    return st ? st->container : 0;
}

static void update_drops(Game *g, float dt)
{
    TilePos feet = g->player.feet;
    for (int i = 0; i < MAX_DROPS; i++) {
        Drop *d = &g->drops[i];
        if (!d->active) continue;
        if (d->collect > 0.0f) {
            d->collect += dt / DROP_COLLECT_TIME;
            if (d->collect >= 1.0f) d->active = false;
            continue;
        }
        d->age += dt;
        if (d->attached) continue;

        int rest = (int)d->y;
        while (!world_is_solid(&g->world, d->x, rest + 1)) rest++;
        d->y += DROP_FALL_SPEED * dt;
        if (d->y > (float)rest) d->y = (float)rest;

        /* Collected from the tiles the player occupies or touches. */
        int dx = world_dx(&g->world, feet.x, d->x), dy = (int)(d->y + 0.5f) - feet.y;
        if (d->age < DROP_PICKUP_DELAY || dx < -1 || dx > 1 || dy < -2 || dy > 1) continue;
        collect_drop(g, d);
    }
}

static void begin_step(Game *g, Step s)
{
    Player *p = &g->player;
    TilePos to = { p->feet.x + s.dx, p->feet.y + s.dy };
    /* Going hand over hand across a tree is a climb; letting go of a wall is a hop. */
    bool hop = s.dy == 0 && (s.dx > 1 || s.dx < -1 || (p->climbing && !can_hold(&g->world, to)));
    p->activity = ACTIVITY_STEPPING;
    p->step = s;
    p->step_hop = hop;
    p->step_to = (TilePos){ p->feet.x + s.dx, p->feet.y + s.dy };
    p->timer = 0.0f;
    if (hop) p->duration = HOP_TIME;
    else if (s.climb || p->climbing) p->duration = CLIMB_TIME;
    else p->duration = s.dy > 0 ? FALL_TIME : WALK_TIME;
    if (s.dx != 0) p->facing = s.dx > 0 ? 1 : -1;
}

static void begin_work(Game *g, Activity activity, float duration)
{
    g->player.activity = activity;
    g->player.timer = 0.0f;
    g->player.duration = duration;
}

static void finish_activity(Game *g)
{
    Player *p = &g->player;
    if (p->activity == ACTIVITY_STEPPING) {
        /* A step across the seam lands on the far side of the world. */
        p->feet = (TilePos){ world_wrap_x(&g->world, p->step_to.x), p->step_to.y };
        p->step_to = p->feet;
        p->climbing = p->step.climb;
    } else if (p->activity == ACTIVITY_MINING) {
        TilePos t = g->current.target;
        int layer = mine_layer(&g->world, t);
        if (layer >= 0) {
            /* A storage block's contents go into the item it leaves behind. */
            const TileDef *def = tile_def(world_get_at(&g->world, layer, t.x, t.y));
            uint16_t inside = layer == LAYER_FOREGROUND ? storage_release(g, t) : 0;
            spawn_drop_stack(g, (ItemStack){ (uint8_t)def->drop, 1, inside }, t);
            world_set_at(&g->world, layer, t.x, t.y, TILE_AIR);
        }
        g->has_current = false;
    } else if (p->activity == ACTIVITY_PLACING) {
        TilePos t = g->current.target;
        ItemId item = (ItemId)g->current.item;
        TileType tile = item_def(item)->places;
        ItemStack one;
        if (!inventory_take_one(&g->inventory, item, &one)) {
            deny(g, t);
        } else if (g->current.type == ACTION_PLANT) {
            container_free(&g->inventory, one.container);
            tree_plant(&g->world, t.x, t.y - 1, item_def(item)->plants);
        } else if (!needs_storage(tile_def(tile))) {
            container_free(&g->inventory, one.container);
            world_set(&g->world, t.x, t.y, tile);
        } else if (storage_create(g, t, tile_def(tile), one)) {
            /* What was inside the item is now inside the block. */
            world_set(&g->world, t.x, t.y, tile);
        } else {
            inventory_add_stack(&g->inventory, one);
            deny(g, t);
        }
        g->has_current = false;
    }
    p->activity = ACTIVITY_IDLE;
}

/* Picks what the idle player does next. Returns false if nothing. */
static bool start_next_activity(Game *g)
{
    Player *p = &g->player;
    const World *w = &g->world;

    if (is_supported(w, p->feet)) {
        p->climbing = false;
    } else if (!p->climbing || !can_cling(w, p->feet)) {
        p->climbing = false;
        begin_step(g, (Step){ 0, 1, false });
        return true;
    }
    Pose pose = { p->feet, p->climbing };

    for (;;) {
        if (g->has_current) {
            if (g->step_index < g->step_count) {
                Step s = g->steps[g->step_index];
                if (move_allowed(w, pose, s)) {
                    g->step_index++;
                    begin_step(g, s);
                    return true;
                }
            } else if (target_valid(w, g->current) && is_goal(w, pose, g->current)) {
                switch (g->current.type) {
                case ACTION_MINE:
                    begin_work(g, ACTIVITY_MINING,
                               game_mine_time(g, world_get_at(w, mine_layer(w, g->current.target),
                                                              g->current.target.x,
                                                              g->current.target.y)));
                    return true;
                case ACTION_PLACE:
                case ACTION_PLANT:
                    if (inventory_count(&g->inventory, (ItemId)g->current.item) < 1) break;
                    begin_work(g, ACTIVITY_PLACING, PLACE_TIME);
                    return true;
                case ACTION_COLLECT: {
                    Drop *d = &g->drops[g->current.drop];
                    bool there = d->active && d->id == g->current.drop_id && d->collect == 0.0f;
                    /* Someone else's hands, or a despawn, is not worth a complaint. */
                    if (there && (!in_use_range(w, p->feet, drop_tile(d)) || !collect_drop(g, d)))
                        break;
                    g->has_current = false;
                    continue;
                }
                case ACTION_USE:
                    game_close_bench(g);
                    g->bench_open = true;
                    g->bench_pos = g->current.target;
                    /* fall through */
                default:
                    g->has_current = false;
                    continue;
                }
            }
            /* The world no longer matches the plan. */
            deny(g, g->current.target);
            g->has_current = false;
            continue;
        }

        if (g->queue_count == 0) break;
        Action a = g->queue[0];
        g->queue_count--;
        memmove(g->queue, g->queue + 1, sizeof(Action) * (size_t)g->queue_count);
        if (plan_action(g, w, pose, a, g->steps, &g->step_count, NULL)) {
            g->current = a;
            g->step_index = 0;
            g->has_current = true;
        } else {
            deny(g, a.target);
        }
    }

    Step s = key_step(w, pose, g->move_dx, g->move_dy);
    if (s.dx != 0 || s.dy != 0) {
        begin_step(g, s);
        return true;
    }
    return false;
}

bool game_craft(Game *g, const Recipe *r, int count)
{
    if (!g->bench_open || g->craft_remaining > 0 || count < 1) return false;
    /* Only what this bench makes, at the level it has reached. */
    TileType bench = world_get(&g->world, g->bench_pos.x, g->bench_pos.y);
    if (strcmp(r->bench, tile_def(bench)->bench_kind) != 0 || recipe_locked(bench, r)) return false;
    if (game_bench_fuel(g, NULL) == 0) return false; /* out of fuel */
    int max = recipe_max_crafts(&g->inventory, r);
    if (max < 1) return false;
    g->craft_recipe = r;
    g->craft_total = g->craft_remaining = count < max ? count : max;
    g->craft_timer = 0.0f;
    return true;
}

int game_bench_fuel(const Game *g, int *capacity)
{
    if (!g->bench_open) return -1;
    const TileDef *def = tile_def(world_get(&g->world, g->bench_pos.x, g->bench_pos.y));
    Storage *st = storage_at((Game *)g, g->bench_pos);
    if (def->fuel_capacity <= 0 || !st) return -1;
    if (capacity) *capacity = def->fuel_capacity;
    return st->fuel;
}

bool game_add_fuel(Game *g, ItemId item)
{
    int capacity = 0, fuel = game_bench_fuel(g, &capacity);
    if (fuel < 0 || fuel >= capacity || item_def(item)->fuel <= 0) return false;
    if (inventory_remove(&g->inventory, item, 1) != 1) return false;
    fuel += item_def(item)->fuel;
    storage_at(g, g->bench_pos)->fuel = fuel > capacity ? capacity : fuel;
    return true;
}

bool game_upgrade_bench(Game *g)
{
    if (!g->bench_open) return false;
    const TileDef *def = tile_def(world_get(&g->world, g->bench_pos.x, g->bench_pos.y));
    if (def->upgrade_to == TILE_AIR ||
        !inventory_has_all(&g->inventory, def->upgrade_cost, def->upgrade_cost_count))
        return false;
    inventory_remove_all(&g->inventory, def->upgrade_cost, def->upgrade_cost_count);
    world_set(&g->world, g->bench_pos.x, g->bench_pos.y, def->upgrade_to);
    return true;
}

void game_close_bench(Game *g)
{
    g->bench_open = false;
    g->craft_remaining = 0;
}

static void update_bench(Game *g, float dt)
{
    if (!g->bench_open) return;
    /* It shuts if it is removed or the player leaves it. */
    const TileDef *open = tile_def(world_get(&g->world, g->bench_pos.x, g->bench_pos.y));
    if ((!open->bench && open->storage_columns == 0) || !in_use_range(&g->world, g->player.feet, g->bench_pos)) {
        game_close_bench(g);
        return;
    }

    /* The fire going out stops the work. */
    if (game_bench_fuel(g, NULL) == 0) g->craft_remaining = 0;
    if (g->craft_remaining <= 0) return;
    g->craft_timer += dt;
    while (g->craft_remaining > 0 && g->craft_timer >= CRAFT_TIME) {
        g->craft_timer -= CRAFT_TIME;
        int left = recipe_craft(&g->inventory, g->craft_recipe);
        if (left < 0) {
            g->craft_remaining = 0;
            break;
        }
        for (int i = 0; i < left; i++) spawn_drop(g, g->craft_recipe->output.id, g->player.feet);
        g->craft_remaining--;
    }
}

/* ---- world time -------------------------------------------------------- */

static uint32_t game_random(Game *g)
{
    uint32_t x = g->rng ? g->rng : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g->rng = x;
}

static Drop *attached_drop_at(Game *g, int x, int y)
{
    for (int i = 0; i < MAX_DROPS; i++) {
        Drop *d = &g->drops[i];
        if (d->active && d->attached && d->x == x && (int)d->y == y) return d;
    }
    return NULL;
}

/* Something happens to one block: a trunk grows its tree, a leaf bears fruit. */
static void random_event(Game *g, int x, int y)
{
    World *w = &g->world;

    if (tree_trunk_at(w, x, y) >= 0) {
        TreeGrowth growth = tree_grow(w, x, y, &g->rng);
        if (!growth.extended) return;
        /* Fruit rides up with the leaf it hangs on. */
        for (int i = 0; i < MAX_DROPS; i++) {
            Drop *d = &g->drops[i];
            if (!d->active || !d->attached || abs(world_dx(w, growth.x, d->x)) > MAX_TREE_ROWS) continue;
            if ((int)d->y < growth.top - 2 || (int)d->y > growth.top + MAX_TREE_ROWS) continue;
            if (tree_leaves_at(w, d->x, (int)d->y - 1) >= 0) d->y -= 1.0f;
        }
        return;
    }

    int kind = tree_leaves_at(w, x, y);
    if (kind < 0 || content.trees[kind].fruit == ITEM_NONE || attached_drop_at(g, x, y)) return;
    Drop *fruit = spawn_drop(g, content.trees[kind].fruit, (TilePos){ x, y });
    if (!fruit) return;
    fruit->attached = true;
    fruit->ttl = content.trees[kind].fruit_ticks;
}

void game_tick(Game *g)
{
    World *w = &g->world;
    g->tick++;

    for (int chunk = 0; chunk * CHUNK_WIDTH < w->width; chunk++) {
        for (int n = 0; n < content.rules.random_events_per_chunk; n++) {
            uint32_t r = game_random(g);
            int x = chunk * CHUNK_WIDTH + (int)(r % (uint32_t)CHUNK_WIDTH);
            if (x < w->width) random_event(g, x, (int)((r >> 8) % (uint32_t)w->height));
        }
    }

    /* Fuel burns down whether or not anything is being made. */
    for (int i = 0; i < MAX_STORAGES; i++)
        if (g->storages[i].used && g->storages[i].fuel > 0) g->storages[i].fuel--;

    for (int i = 0; i < MAX_DROPS; i++) {
        Drop *d = &g->drops[i];
        if (!d->active) continue;
        /* Fruit whose leaf is gone falls like anything else. */
        if (d->attached && tree_leaves_at(w, d->x, (int)d->y) < 0)
            d->attached = false;
        if (d->ttl > 0 && --d->ttl == 0 && d->collect == 0.0f) d->active = false;
    }
}

float game_day_phase(const Game *g, bool *is_day)
{
    int t = (int)(g->tick % (DAY_TICKS + NIGHT_TICKS));
    if (is_day) *is_day = t < DAY_TICKS;
    return t < DAY_TICKS ? (float)t / DAY_TICKS : (float)(t - DAY_TICKS) / NIGHT_TICKS;
}

float game_daylight(const Game *g)
{
    /* Dusk is centred on the end of the day, dawn on the end of the night. */
    const float half = TWILIGHT_TICKS / 2.0f;
    int cycle = DAY_TICKS + NIGHT_TICKS;
    float t = (float)(g->tick % (uint64_t)cycle);
    float night;
    if (t < half) night = 0.5f - t / TWILIGHT_TICKS;
    else if (t > (float)cycle - half) night = 0.5f + ((float)cycle - t) / TWILIGHT_TICKS;
    else night = fminf(fmaxf((t - DAY_TICKS) / TWILIGHT_TICKS + 0.5f, 0.0f), 1.0f);
    return 1.0f - night * night * (3.0f - 2.0f * night);
}

void game_update(Game *g, float dt)
{
    Player *p = &g->player;
    float frame = dt;

    g->tick_timer += dt * (float)g->tick_rate;
    for (int n = 0; g->tick_timer >= 1.0f && n < MAX_TICK_RATE; n++) {
        g->tick_timer -= 1.0f;
        game_tick(g);
    }
    if (g->tick_timer > 1.0f) g->tick_timer = 0.0f;

    for (int i = 0; i < MAX_DENIALS; i++) {
        if (g->denials[i].ttl > 0.0f) g->denials[i].ttl -= dt;
    }

    for (;;) {
        if (p->activity != ACTIVITY_IDLE) {
            float remaining = p->duration - p->timer;
            if (dt < remaining) {
                p->timer += dt;
                break;
            }
            dt -= remaining;
            finish_activity(g);
        }
        if (!start_next_activity(g)) break;
    }
    update_drops(g, frame);
    update_bench(g, frame);
}

void game_set_move_input(Game *g, int dx, int dy)
{
    g->move_dx = dx;
    g->move_dy = dy;
}

void game_cancel_actions(Game *g)
{
    g->queue_count = 0;
    g->has_current = false;
    if (g->player.activity == ACTIVITY_MINING || g->player.activity == ACTIVITY_PLACING)
        g->player.activity = ACTIVITY_IDLE;
}

void game_player_visual_pos(const Game *g, float *x, float *y)
{
    const Player *p = &g->player;
    float t = p->activity == ACTIVITY_STEPPING ? p->timer / p->duration : 0.0f;
    *x = (float)p->feet.x + (float)(p->step_to.x - p->feet.x) * t;
    *y = (float)p->feet.y + (float)(p->step_to.y - p->feet.y) * t;
    if (p->activity == ACTIVITY_STEPPING && p->step_hop) *y -= HOP_HEIGHT * 4.0f * t * (1.0f - t);
}

int game_player_grip_side(const Game *g)
{
    const Player *p = &g->player;
    /* While moving along a wall, read the grip from where the step ends. */
    bool moving = p->activity == ACTIVITY_STEPPING;
    if (!(moving ? p->step.climb && !p->step_hop : p->climbing)) return 0;
    TilePos at = moving ? p->step_to : p->feet;
    if (can_cling_side(&g->world, at, p->facing)) return p->facing;
    if (can_cling_side(&g->world, at, -p->facing)) return -p->facing;
    return 0;
}

/* ---- setup ------------------------------------------------------------- */

bool game_init(Game *g, int width, int height, uint32_t seed, WorldKind kind)
{
    memset(g, 0, sizeof *g);
    size_t cells = (size_t)width * height;
    bool ok = world_init(&g->world, width, height, seed, kind) &&
              world_init(&g->sim, width, height, seed, WORLD_FLAT);
    g->steps = malloc(sizeof(Step) * cells);
    /* The search visits two poses per tile: standing and climbing. */
    g->bfs_step = malloc(sizeof(Step) * cells * 2);
    g->bfs_parent = malloc(sizeof(int32_t) * cells * 2);
    g->bfs_queue = malloc(sizeof(int32_t) * cells * 2);
    g->bfs_seen = calloc(cells * 2, sizeof(uint32_t));
    if (!ok || !g->steps || !g->bfs_step || !g->bfs_parent || !g->bfs_queue ||
        !g->bfs_seen) {
        game_free(g);
        return false;
    }

    g->player.feet = (TilePos){ g->world.spawn_x, g->world.spawn_y };
    g->player.facing = 1;
    inventory_init(&g->inventory);
    g->tick_rate = DEFAULT_TICK_RATE;
    g->rng = seed ^ 0x5EED1E57u;
    g->player.step_to = g->player.feet;
    return true;
}

void game_free(Game *g)
{
    world_free(&g->world);
    world_free(&g->sim);
    free(g->steps);
    free(g->bfs_step);
    free(g->bfs_parent);
    free(g->bfs_queue);
    free(g->bfs_seen);
    memset(g, 0, sizeof *g);
}
