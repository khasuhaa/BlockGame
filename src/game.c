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

static bool can_step(const World *w, TilePos feet, Step s)
{
    if (s.dy == 0) return body_fits(w, feet.x + s.dx, feet.y);
    if (s.dy == -1) {
        /* Jumping or climbing a ledge needs headroom above the player. */
        if (world_is_solid(w, feet.x, feet.y - 2)) return false;
        return body_fits(w, feet.x + s.dx, feet.y - 1);
    }
    return s.dx == 0 && !world_is_solid(w, feet.x, feet.y + 1);
}

/* Where a player at `feet` comes to rest once gravity is done. */
static TilePos settle(const World *w, TilePos feet)
{
    while (!is_supported(w, feet)) feet.y++;
    return feet;
}

bool game_in_mine_range(TilePos feet, TilePos target)
{
    int dx = target.x - feet.x;
    int dy = target.y - feet.y;
    if (dx < -1 || dx > 1 || dy < -2 || dy > 1) return false;
    return !(dx == 0 && (dy == 0 || dy == -1));
}

/* ---- planning ---------------------------------------------------------- */

static bool is_goal(const World *w, TilePos feet, Action a)
{
    (void)w;
    if (a.type == ACTION_MINE) return game_in_mine_range(feet, a.target);
    return feet.x == a.target.x && (feet.y == a.target.y || feet.y - 1 == a.target.y);
}

/* Breadth-first search over resting positions for the fewest steps that
 * accomplish `a` from `start` (which must be at rest). On success writes the
 * steps to `out` (if non-NULL), their count, and the final resting position. */
static bool plan_action(Game *g, const World *w, TilePos start, Action a,
                        Step *out, int *out_count, TilePos *end)
{
    if (!world_in_bounds(w, a.target.x, a.target.y)) return false;
    bool solid = world_is_solid(w, a.target.x, a.target.y);
    if ((a.type == ACTION_MINE) != solid) return false;

    int width = w->width;
    int start_idx = start.y * width + start.x;
    int goal_idx = -1;
    int head = 0, tail = 0;

    memset(g->bfs_parent, -1, sizeof(int32_t) * (size_t)width * w->height);
    g->bfs_parent[start_idx] = start_idx;
    g->bfs_queue[tail++] = start_idx;

    while (head < tail) {
        int idx = g->bfs_queue[head++];
        TilePos pos = { idx % width, idx / width };
        if (is_goal(w, pos, a)) {
            goal_idx = idx;
            break;
        }
        for (int dx = -1; dx <= 1; dx += 2) {
            Step s = { (int8_t)dx, 0 };
            if (!can_step(w, pos, s)) {
                s.dy = -1;
                if (!can_step(w, pos, s)) continue;
            }
            TilePos next = settle(w, (TilePos){ pos.x + s.dx, pos.y + s.dy });
            int next_idx = next.y * width + next.x;
            if (g->bfs_parent[next_idx] != -1) continue;
            g->bfs_parent[next_idx] = idx;
            g->bfs_step[next_idx] = s;
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
    if (end) *end = (TilePos){ goal_idx % width, goal_idx / width };
    return true;
}

/* Plays an action out instantly on the scratch world. */
static bool sim_action(Game *g, TilePos *pos, Action a)
{
    TilePos end;
    if (!plan_action(g, &g->sim, *pos, a, NULL, NULL, &end)) return false;
    if (a.type == ACTION_MINE) {
        world_set(&g->sim, a.target.x, a.target.y, TILE_AIR);
        end = settle(&g->sim, end);
    }
    *pos = end;
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
    TilePos pos = p->activity == ACTIVITY_STEPPING ? p->step_to : p->feet;
    pos = settle(&g->sim, pos);
    if (g->has_current) {
        for (int i = g->step_index; i < g->step_count; i++) {
            pos.x += g->steps[i].dx;
            pos.y += g->steps[i].dy;
            pos = settle(&g->sim, pos);
        }
        if (g->current.type == ACTION_MINE) {
            world_set(&g->sim, g->current.target.x, g->current.target.y, TILE_AIR);
            pos = settle(&g->sim, pos);
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

static void begin_step(Game *g, Step s)
{
    Player *p = &g->player;
    p->activity = ACTIVITY_STEPPING;
    p->step_to = (TilePos){ p->feet.x + s.dx, p->feet.y + s.dy };
    p->timer = 0.0f;
    p->duration = s.dy > 0 ? FALL_TIME : WALK_TIME;
}

static void finish_activity(Game *g)
{
    Player *p = &g->player;
    if (p->activity == ACTIVITY_STEPPING) {
        p->feet = p->step_to;
    } else if (p->activity == ACTIVITY_MINING) {
        world_set(&g->world, g->current.target.x, g->current.target.y, TILE_AIR);
        g->has_current = false;
    }
    p->activity = ACTIVITY_IDLE;
}

/* Picks what the idle player does next. Returns false if nothing. */
static bool start_next_activity(Game *g)
{
    Player *p = &g->player;
    const World *w = &g->world;

    if (!is_supported(w, p->feet)) {
        begin_step(g, (Step){ 0, 1 });
        return true;
    }

    for (;;) {
        if (g->has_current) {
            if (g->step_index < g->step_count) {
                Step s = g->steps[g->step_index];
                if (can_step(w, p->feet, s)) {
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
        if (plan_action(g, w, p->feet, a, g->steps, &g->step_count, NULL)) {
            g->current = a;
            g->step_index = 0;
            g->has_current = true;
        } else {
            deny(g, a.target);
        }
    }

    if (g->move_dx != 0) {
        Step s = { (int8_t)g->move_dx, 0 };
        if (!can_step(w, p->feet, s)) s.dy = -1;
        if (can_step(w, p->feet, s)) {
            begin_step(g, s);
            return true;
        }
    }
    if (g->move_dy < 0 && can_step(w, p->feet, (Step){ 0, -1 })) {
        begin_step(g, (Step){ 0, -1 });
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
}

/* ---- setup ------------------------------------------------------------- */

bool game_init(Game *g, int width, int height, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    size_t cells = (size_t)width * height;
    bool ok = world_init(&g->world, width, height, seed) &&
              world_init(&g->sim, width, height, seed);
    g->steps = malloc(sizeof(Step) * cells);
    g->bfs_step = malloc(sizeof(Step) * cells);
    g->bfs_parent = malloc(sizeof(int32_t) * cells);
    g->bfs_queue = malloc(sizeof(int32_t) * cells);
    if (!ok || !g->steps || !g->bfs_step || !g->bfs_parent || !g->bfs_queue) {
        game_free(g);
        return false;
    }

    g->player.feet = settle(&g->world, (TilePos){ g->world.spawn_x, 1 });
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
