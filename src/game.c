#include "game.h"

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

static bool can_cling(const World *w, TilePos feet)
{
    return can_cling_side(w, feet, -1) || can_cling_side(w, feet, 1);
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
    p.feet.x += s.dx;
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

static bool is_goal(Pose p, Action a)
{
    TilePos feet = p.feet;
    if (a.type == ACTION_MINE) return game_in_mine_range(feet, a.target);
    return feet.x == a.target.x && (feet.y == a.target.y || feet.y - 1 == a.target.y);
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
    if (!world_in_bounds(w, a.target.x, a.target.y)) return false;
    bool solid = world_is_solid(w, a.target.x, a.target.y);
    if ((a.type == ACTION_MINE) != solid) return false;

    int start_idx = pose_index(w, start);
    int goal_idx = -1;
    int head = 0, tail = 0;

    memset(g->bfs_parent, -1, sizeof(int32_t) * 2 * (size_t)w->width * w->height);
    g->bfs_parent[start_idx] = start_idx;
    g->bfs_queue[tail++] = start_idx;

    while (head < tail) {
        int idx = g->bfs_queue[head++];
        Pose pose = pose_at_index(w, idx);
        if (is_goal(pose, a)) {
            goal_idx = idx;
            break;
        }
        Step moves[MAX_MOVES];
        int n = list_moves(w, pose, moves);
        for (int i = 0; i < n; i++) {
            int next_idx = pose_index(w, pose_after(w, pose, moves[i]));
            if (g->bfs_parent[next_idx] != -1) continue;
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

/* Plays an action out instantly on the scratch world. */
static bool sim_action(Game *g, Pose *pose, Action a)
{
    Pose end;
    if (!plan_action(g, &g->sim, *pose, a, NULL, NULL, &end)) return false;
    if (a.type == ACTION_MINE) {
        world_set(&g->sim, a.target.x, a.target.y, TILE_AIR);
        end = pose_rest(&g->sim, end);
    }
    *pose = end;
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

bool game_click_tile(Game *g, int tx, int ty)
{
    const Player *p = &g->player;
    TilePos target = { tx, ty };
    if (!world_in_bounds(&g->world, tx, ty) || g->queue_count >= MAX_QUEUED_ACTIONS) {
        deny(g, target);
        return false;
    }

    /* Fast-forward a copy of the world through everything already pending. */
    memcpy(g->sim.tiles, g->world.tiles, (size_t)g->world.width * g->world.height);
    Pose pos = { p->feet, p->climbing };
    if (p->activity == ACTIVITY_STEPPING) pos = (Pose){ p->step_to, p->step.climb };
    pos = pose_rest(&g->sim, pos);
    if (g->has_current) {
        for (int i = g->step_index; i < g->step_count; i++)
            pos = pose_after(&g->sim, pos, g->steps[i]);
        if (g->current.type == ACTION_MINE) {
            world_set(&g->sim, g->current.target.x, g->current.target.y, TILE_AIR);
            pos = pose_rest(&g->sim, pos);
        }
    }
    for (int i = 0; i < g->queue_count; i++) sim_action(g, &pos, g->queue[i]);

    Action a = { world_is_solid(&g->sim, tx, ty) ? ACTION_MINE : ACTION_MOVE, target };
    if (!sim_action(g, &pos, a)) {
        deny(g, target);
        return false;
    }
    g->queue[g->queue_count++] = a;
    return true;
}

/* ---- simulation -------------------------------------------------------- */

/* Leaves an item lying in a tile. With no room for another drop it goes
 * straight to the inventory instead. */
static void spawn_drop(Game *g, ItemId item, TilePos at)
{
    if (item == ITEM_NONE) return;
    for (int i = 0; i < MAX_DROPS; i++) {
        if (g->drops[i].active) continue;
        g->drops[i] = (Drop){ true, (uint8_t)item, at.x, (float)at.y, 0.0f, 0.0f };
        return;
    }
    inventory_add(&g->inventory, item, 1);
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

        int rest = (int)d->y;
        while (!world_is_solid(&g->world, d->x, rest + 1)) rest++;
        d->y += DROP_FALL_SPEED * dt;
        if (d->y > (float)rest) d->y = (float)rest;

        /* Collected from the tiles the player occupies or touches. */
        int dx = d->x - feet.x, dy = (int)(d->y + 0.5f) - feet.y;
        if (d->age < DROP_PICKUP_DELAY || dx < -1 || dx > 1 || dy < -2 || dy > 1) continue;
        if (inventory_add(&g->inventory, (ItemId)d->item, 1) == 0) d->collect = 0.001f;
    }
}

static void begin_step(Game *g, Step s)
{
    Player *p = &g->player;
    bool hop = s.dy == 0 && (p->climbing || s.dx > 1 || s.dx < -1);
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

static void finish_activity(Game *g)
{
    Player *p = &g->player;
    if (p->activity == ACTIVITY_STEPPING) {
        p->feet = p->step_to;
        p->climbing = p->step.climb;
    } else if (p->activity == ACTIVITY_MINING) {
        TilePos t = g->current.target;
        spawn_drop(g, item_for_tile(world_get(&g->world, t.x, t.y)), t);
        world_set(&g->world, t.x, t.y, TILE_AIR);
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
            } else if (g->current.type == ACTION_MOVE) {
                g->has_current = false;
                continue;
            } else if (world_is_solid(w, g->current.target.x, g->current.target.y) &&
                       game_in_mine_range(p->feet, g->current.target)) {
                p->activity = ACTIVITY_MINING;
                p->timer = 0.0f;
                p->duration = MINE_TIME;
                return true;
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

void game_update(Game *g, float dt)
{
    Player *p = &g->player;

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
    update_drops(g, dt);
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
    if (g->player.activity == ACTIVITY_MINING) g->player.activity = ACTIVITY_IDLE;
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

bool game_init(Game *g, int width, int height, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    size_t cells = (size_t)width * height;
    bool ok = world_init(&g->world, width, height, seed) &&
              world_init(&g->sim, width, height, seed);
    g->steps = malloc(sizeof(Step) * cells);
    /* The search visits two poses per tile: standing and climbing. */
    g->bfs_step = malloc(sizeof(Step) * cells * 2);
    g->bfs_parent = malloc(sizeof(int32_t) * cells * 2);
    g->bfs_queue = malloc(sizeof(int32_t) * cells * 2);
    if (!ok || !g->steps || !g->bfs_step || !g->bfs_parent || !g->bfs_queue) {
        game_free(g);
        return false;
    }

    g->player.feet = pose_rest(&g->world, (Pose){ { g->world.spawn_x, 1 }, false }).feet;
    g->player.facing = 1;
    inventory_init(&g->inventory);
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
    memset(g, 0, sizeof *g);
}
