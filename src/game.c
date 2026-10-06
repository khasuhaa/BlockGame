#include "game.h"

#include "mob.h"
#include "trade.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- movement rules ---------------------------------------------------- */

/* The player is two tiles tall standing or climbing, and one tile tall when
 * crawling. Crawling is not a choice: it is what the player does wherever
 * there is a block where their head would be. */

/* Room for the whole body. */
static bool body_fits(const World *w, int x, int y)
{
    return !world_is_solid(w, x, y) && !world_is_solid(w, x, y - 1);
}

/* Room to be in at all, if only on hands and knees. */
static bool low_fits(const World *w, int x, int y)
{
    return !world_is_solid(w, x, y);
}

static bool is_crawling(const World *w, TilePos feet)
{
    return world_is_solid(w, feet.x, feet.y - 1);
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

/* Deep enough water round the body to swim in. */
static bool in_water(const World *w, TilePos feet)
{
    return world_is_water(w, feet.x, feet.y) || world_is_water(w, feet.x, feet.y - 1);
}

/* Something climbable behind the player's body, such as a tree - or water,
 * which holds a swimmer up the same way wherever they are in it. */
static bool can_hold(const World *w, TilePos feet)
{
    if (in_water(w, feet)) return true;
    for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
        if (tile_def(world_get_at(w, layer, feet.x, feet.y))->climbable ||
            tile_def(world_get_at(w, layer, feet.x, feet.y - 1))->climbable)
            return true;
    }
    return false;
}

/* A block just overhead to hang from, like a monkey bar. */
static bool can_hang(const World *w, TilePos feet)
{
    return world_is_solid(w, feet.x, feet.y - 2);
}

/* Whether a player can hang on at `feet` without standing on anything. */
static bool can_cling(const World *w, TilePos feet)
{
    if (!body_fits(w, feet.x, feet.y)) return false;
    return can_cling_side(w, feet, -1) || can_cling_side(w, feet, 1) || can_hold(w, feet) ||
           can_hang(w, feet);
}

/* Pull up onto the block beside the feet: a one-block ledge when standing,
 * the top of the wall when climbing. The top may be a gap only high enough
 * to crawl into. */
static bool can_mantle(const World *w, TilePos feet, int d)
{
    return world_is_solid(w, feet.x + d, feet.y) && body_fits(w, feet.x, feet.y - 1) &&
           low_fits(w, feet.x + d, feet.y - 1);
}

static bool can_climb_up(const World *w, TilePos feet)
{
    return body_fits(w, feet.x, feet.y - 1) && can_cling(w, (TilePos){ feet.x, feet.y - 1 });
}

/* Move one column while climbing without letting go: across a tree, or hand
 * over hand along the underside of a row of blocks. */
static bool can_climb_across(const World *w, TilePos feet, int d)
{
    TilePos to = { feet.x + d, feet.y };
    return body_fits(w, to.x, to.y) && (can_hold(w, to) || can_hang(w, to));
}

/* From hanging under a block, round its end and up onto its side. */
static bool can_swing_up(const World *w, TilePos feet, int d)
{
    return body_fits(w, feet.x + d, feet.y) && !world_is_solid(w, feet.x + d, feet.y - 2) &&
           can_cling_side(w, (TilePos){ feet.x + d, feet.y - 1 }, -d);
}

/* The other way: from the side of a block, down and under it. */
static bool can_swing_under(const World *w, TilePos feet, int d)
{
    return !world_is_solid(w, feet.x, feet.y + 1) && body_fits(w, feet.x + d, feet.y + 1) &&
           world_is_solid(w, feet.x + d, feet.y - 1);
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
            else if (can_climb_across(w, p.feet, d)) out[n++] = (Step){ (int8_t)d, 0, true };
            else if ((k = wall_hop_distance(w, p.feet, d)) > 0)
                out[n++] = (Step){ (int8_t)(d * k), 0, true };
            if (can_swing_up(w, p.feet, d)) out[n++] = (Step){ (int8_t)d, -1, true };
            if (can_swing_under(w, p.feet, d)) out[n++] = (Step){ (int8_t)d, 1, true };
        }
        /* In water there is nothing to go round: a swimmer goes on the slant
         * too, wherever the squares either side of the slant are clear. */
        for (int d = -1; in_water(w, p.feet) && d <= 1; d += 2) {
            for (int v = -1; v <= 1 && n < MAX_MOVES; v += 2) {
                TilePos to = { x + d, y + v };
                if (in_water(w, to) && body_fits(w, to.x, to.y) && body_fits(w, x + d, y) && body_fits(w, x, y + v))
                    out[n++] = (Step){ (int8_t)d, (int8_t)v, true };
            }
        }
        return n;
    }

    bool upright = !is_crawling(w, p.feet);
    for (int d = -1; d <= 1; d += 2) {
        if (!low_fits(w, x + d, y)) {
            if (can_mantle(w, p.feet, d)) out[n++] = (Step){ (int8_t)d, -1, false };
        } else if (is_supported(w, (TilePos){ x + d, y })) {
            /* Walking, or crawling if there is only a one-block gap to go through. */
            out[n++] = (Step){ (int8_t)d, 0, false };
        } else {
            /* An edge: hop a one-block gap, step down one block, or go over
             * the side and hang on the wall of a deeper drop. */
            if (upright && body_fits(w, x + d, y) && body_fits(w, x + 2 * d, y) &&
                is_supported(w, (TilePos){ x + 2 * d, y }))
                out[n++] = (Step){ (int8_t)(2 * d), 0, false };
            if (world_is_solid(w, x + d, y + 2)) out[n++] = (Step){ (int8_t)d, 0, false };
            else out[n++] = (Step){ (int8_t)d, 1, true };
        }
    }
    /* Jump and catch a wall too tall to step onto, or something overhead. */
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
 * gaps, through gaps one block high, down and up walls, from wall to wall
 * across a pit, and along the underside of a row of blocks. */
static Step key_step(const World *w, Pose p, int dx, int dy)
{
    const Step none = { 0, 0, false };
    int x = p.feet.x, y = p.feet.y;

    if (p.climbing) {
        if (dy < 0 && can_climb_up(w, p.feet)) return (Step){ 0, -1, true };
        if (dy > 0) {
            /* Rather than drop off the bottom of a block, swing in under it. */
            TilePos below = { x, y + 1 };
            for (int d = -1; d <= 1; d += 2)
                if (!is_supported(w, below) && !can_cling(w, below) && can_swing_under(w, p.feet, d))
                    return (Step){ (int8_t)d, 1, true };
            return (Step){ 0, 1, true };
        }
        if (dx == 0) {
            /* W with nowhere higher to climb: onto the top of a wall, or
             * round the end of the block being hung from. */
            for (int d = -1; d <= 1 && dy < 0; d += 2)
                if (can_mantle(w, p.feet, d)) return (Step){ (int8_t)d, -1, false };
            for (int d = -1; d <= 1 && dy < 0; d += 2)
                if (can_swing_up(w, p.feet, d)) return (Step){ (int8_t)d, -1, true };
            return none;
        }
        if (can_mantle(w, p.feet, dx)) return (Step){ (int8_t)dx, -1, false };
        if (can_cling_side(w, p.feet, dx))
            return can_climb_up(w, p.feet) ? (Step){ 0, -1, true } : none;
        if (can_climb_across(w, p.feet, dx)) return (Step){ (int8_t)dx, 0, true };
        int k = wall_hop_distance(w, p.feet, dx);
        if (k > 0) return (Step){ (int8_t)(dx * k), 0, true };
        /* Nothing further that way to hold: down, or off the end of the bars. */
        return dy < 0 ? none : (Step){ 0, 1, true };
    }

    bool upright = !is_crawling(w, p.feet);
    if (dx != 0) {
        if (!low_fits(w, x + dx, y)) {
            if (can_mantle(w, p.feet, dx)) return (Step){ (int8_t)dx, -1, false };
            if (can_cling_side(w, (TilePos){ x, y - 1 }, dx) && can_climb_up(w, p.feet))
                return (Step){ 0, -1, true };
            return none;
        }
        if (is_supported(w, (TilePos){ x + dx, y })) return (Step){ (int8_t)dx, 0, false };
        /* Holding down as well goes into a gap instead of hopping it. */
        if (dy <= 0 && upright && body_fits(w, x + dx, y) && body_fits(w, x + 2 * dx, y) &&
            is_supported(w, (TilePos){ x + 2 * dx, y }))
            return (Step){ (int8_t)(2 * dx), 0, false };
        if (world_is_solid(w, x + dx, y + 2)) return (Step){ (int8_t)dx, 0, false };
        return (Step){ (int8_t)dx, 1, true };
    }
    if (dy < 0 && body_fits(w, x, y - 1)) return (Step){ 0, -1, true };
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
/* Whether a player at `feet` can get a hand to `target`: the tiles around
 * their body - ten when upright, eight when crawling - measured the short way
 * round the world. A tile at a corner cannot be reached past two blocks that
 * meet in front of it. */
static bool in_reach(const World *w, TilePos feet, TilePos target)
{
    int dx = world_dx(w, feet.x, target.x), dy = target.y - feet.y;
    bool crawling = is_crawling(w, feet);
    if (crawling ? (dx < -1 || dx > 1 || dy < -1 || dy > 1 || (dx == 0 && dy == 0))
                 : !game_in_mine_range((TilePos){ 0, 0 }, (TilePos){ dx, dy }))
        return false;
    if (dx == 0 || (dy != 1 && dy != (crawling ? -1 : -2))) return true;
    /* A corner: the two tiles the reach passes between, beside the body and
     * above or below it. */
    int beside_y = dy > 0 ? feet.y : target.y + 1;
    return !world_is_solid(w, feet.x + dx, beside_y) || !world_is_solid(w, feet.x, target.y);
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

/* The backwall at a tile, if it is bare: nothing in front of it in either
 * block layer. TILE_AIR otherwise. */
static TileType bare_wall(const World *w, TilePos t)
{
    if (world_get(w, t.x, t.y) != TILE_AIR || world_get_at(w, LAYER_BACKGROUND, t.x, t.y) != TILE_AIR)
        return TILE_AIR;
    return world_get_at(w, LAYER_WALL, t.x, t.y);
}

/* The layer a particular mine action works on. */
static int action_layer(const World *w, Action a)
{
    if (a.wall) return bare_wall(w, a.target) != TILE_AIR ? LAYER_WALL : -1;
    return mine_layer(w, a.target);
}

/* The block at a position that can be opened - a bench or a storage block -
 * looking in front first, then behind. TILE_AIR if there is none. */
static TileType usable_at(const World *w, int x, int y, int *layer)
{
    for (int l = 0; l < BLOCK_LAYERS; l++) {
        TileType t = world_get_at(w, l, x, y);
        if (!tile_def(t)->bench && !tile_def(t)->trade && tile_def(t)->storage_columns == 0) continue;
        if (layer) *layer = l;
        return t;
    }
    return TILE_AIR;
}

TileType game_bench_at(const Game *g, int x, int y)
{
    return usable_at(&g->world, x, y, NULL);
}

static bool is_goal(const World *w, Pose p, Action a)
{
    switch (a.type) {
    case ACTION_MINE:
        /* A background block can be mined from right in front of it. */
        if (!world_is_solid(w, a.target.x, a.target.y)) return in_use_range(w, p.feet, a.target);
        return in_reach(w, p.feet, a.target);
    case ACTION_PLACE:
    case ACTION_BUCKET:
    case ACTION_PLANT: return in_reach(w, p.feet, a.target);
    case ACTION_USE:
    case ACTION_COLLECT:
    case ACTION_ATTACK: return in_use_range(w, p.feet, a.target);
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
    const ItemDef *it = item_def(item);
    if (!world_in_bounds(w, t.x, t.y) || !world_in_bounds(w, t.x, t.y - 1)) return false;
    TileType ground = world_get(w, t.x, t.y);
    bool soil = false, wet = world_is_water(w, t.x, t.y - 1);
    int layer;
    if (it->plants >= 0) {
        const TreeDef *def = &content.trees[it->plants];
        for (int i = 0; i < def->soil_count; i++) soil = soil || def->soil[i] == ground;
        if (def->needs_water && !wet) return false;
        layer = def->layer;
    } else if (it->sows != TILE_AIR) {
        /* A plant: its first block goes on top of soil it can live on. */
        const TileDef *def = tile_def(it->sows);
        for (int i = 0; i < def->soil_count; i++) soil = soil || def->soil[i] == ground;
        if (def->needs_water && !wet) return false;
        layer = def->layer;
    } else {
        return false;
    }
    return soil && world_get_at(w, layer, t.x, t.y - 1) == TILE_AIR && !world_is_solid(w, t.x, t.y - 1);
}

/* Puts down what a planted item grows into: a tree's first trunk block, or a
 * plant's first stage. */
static void plant_item(World *w, ItemId item, TilePos soil)
{
    const ItemDef *it = item_def(item);
    if (it->plants >= 0) tree_plant(w, soil.x, soil.y - 1, it->plants);
    else if (it->sows != TILE_AIR) world_set_at(w, tile_def(it->sows)->layer, soil.x, soil.y - 1, it->sows);
}

static void place_tile(World *w, TilePos t, TileType tile);
static void replace_block(World *w, int layer, TilePos t, TileType with);
static bool room_for(const World *w, TilePos t, TileType tile);

/* Whether the target tile is still something the action can be done to. */
static bool target_valid(const World *w, Action a)
{
    if (!world_in_bounds(w, a.target.x, a.target.y)) return false;
    /* A drop or a creature is checked on arrival, not here. */
    if (a.type == ACTION_COLLECT || a.type == ACTION_ATTACK) return true;
    TileType t = world_get(w, a.target.x, a.target.y);
    switch (a.type) {
    case ACTION_MINE: return action_layer(w, a) >= 0;
    case ACTION_PLACE:
        /* Something that hangs in the background can go up against a backwall. */
        if (item_def((ItemId)a.item)->places == TILE_AIR ||
            !room_for(w, a.target, item_def((ItemId)a.item)->places))
            return false;
        if (tile_def(item_def((ItemId)a.item)->places)->layer == LAYER_BACKGROUND &&
            bare_wall(w, a.target) != TILE_AIR)
            return true;
        return beside_block(w, a.target);
    case ACTION_PLANT: return can_plant(w, (ItemId)a.item, a.target);
    case ACTION_USE: return usable_at(w, a.target.x, a.target.y, NULL) != TILE_AIR;
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
/* Afloat: in water, held up by it and by nothing else. */
static bool pose_afloat(const World *w, Pose p)
{
    return p.climbing && in_water(w, p.feet) && !is_supported(w, p.feet);
}

/* The straight line through the water from one pose to another, as steps:
 * whichever of across and up-or-down is the longer way is gone one square at
 * a time, the other spread evenly along it. False if anything is in the way,
 * or it would take more than `most` steps. */
static bool swim_line(const World *w, Pose from, Pose to, int most, Step *out, int *count)
{
    int dx = world_dx(w, from.feet.x, to.feet.x), dy = to.feet.y - from.feet.y;
    int len = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if (len == 0 || len > most) return false;
    Pose at = from;
    int px = 0, py = 0;
    for (int k = 1; k <= len; k++) {
        int nx = (int)lroundf((float)(k * dx) / (float)len), ny = (int)lroundf((float)(k * dy) / (float)len);
        Step s = { (int8_t)(nx - px), (int8_t)(ny - py), true };
        if (!move_allowed(w, at, s)) return false;
        at = pose_after(w, at, s);
        if (at.feet.x != world_wrap_x(w, from.feet.x + nx) || at.feet.y != from.feet.y + ny) return false;
        if (k < len && !pose_afloat(w, at)) return false;
        out[k - 1] = s;
        px = nx;
        py = ny;
    }
    if (at.climbing != to.climbing) return false;
    *count = len;
    return true;
}

/* A route is found square by square, which through open water comes out as
 * a dog-leg. Wherever it swims from one place to another with nothing in the
 * way, this puts the straight line there instead. */
#define SWIM_LOOKAHEAD 48
static void straighten_swim(const World *w, Pose start, Step *steps, int *count)
{
    int n = *count;
    if (n < 2) return;
    Pose *at = malloc(sizeof(Pose) * (size_t)(n + 1));
    Step *made = malloc(sizeof(Step) * (size_t)n);
    if (!at || !made) {
        free(at);
        free(made);
        return;
    }
    at[0] = start;
    for (int i = 0; i < n; i++) at[i + 1] = pose_after(w, at[i], steps[i]);
    int kept = 0;
    for (int i = 0; i < n;) {
        Step line[SWIM_LOOKAHEAD];
        int length = 0, reach = -1;
        if (pose_afloat(w, at[i])) {
            int far = i + SWIM_LOOKAHEAD < n ? i + SWIM_LOOKAHEAD : n;
            for (int j = far; j > i + 1 && reach < 0; j--)
                if (in_water(w, at[j].feet) && swim_line(w, at[i], at[j], j - i, line, &length)) reach = j;
        }
        if (reach < 0) {
            made[kept++] = steps[i++];
            continue;
        }
        for (int k = 0; k < length; k++) made[kept++] = line[k];
        i = reach;
    }
    memcpy(steps, made, sizeof(Step) * (size_t)kept);
    *count = kept;
    free(at);
    free(made);
}

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

    /* A route this hard to find is as good as none; do not hold the game up. */
    while (head < tail && head < MAX_SEARCH) {
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
    if (out) straighten_swim(w, start, out, &count);
    if (out_count) *out_count = count;
    if (end) *end = pose_at_index(w, goal_idx);
    return true;
}

/* What a finished action does to the scratch world. */
static Pose sim_effect(Game *g, Pose pose, Action a)
{
    int layer = action_layer(&g->sim, a);
    if (a.type == ACTION_MINE && layer >= 0)
        replace_block(&g->sim, layer, a.target, TILE_AIR);
    else if (a.type == ACTION_PLACE)
        place_tile(&g->sim, a.target, item_def((ItemId)a.item)->places);
    else if (a.type == ACTION_PLANT)
        plant_item(&g->sim, (ItemId)a.item, a.target);
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
    g->sim.sea_row = g->world.sea_row;
    /* (The plan takes the water as it lies now.) */
    memcpy(g->sim.water, g->world.water, (size_t)g->world.width * g->world.height);
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
    game_wake(g);
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
    Action a = { .type = ACTION_COLLECT, .target = drop_tile(d), .drop = drop, .drop_id = d->id };
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

/* Whether an action is the one marked on a tile. */
static bool action_on_tile(Action a, TilePos t)
{
    if (a.type == ACTION_COLLECT) return false; /* marked on the item, not a tile */
    if (a.target.x != t.x) return false;
    return a.target.y == t.y || (a.type == ACTION_PLANT && a.target.y - 1 == t.y);
}

/* Drops whichever pending action `match` picks, latest first. */
static bool unqueue(Game *g, bool (*match)(Action, TilePos, int), TilePos t, int drop)
{
    for (int i = g->queue_count - 1; i >= 0; i--) {
        if (!match(g->queue[i], t, drop)) continue;
        memmove(g->queue + i, g->queue + i + 1, sizeof(Action) * (size_t)(g->queue_count - i - 1));
        g->queue_count--;
        return true;
    }
    if (!g->has_current || !match(g->current, t, drop)) return false;
    /* The one being carried out: a step already begun still finishes. */
    g->has_current = false;
    if (g->player.activity == ACTIVITY_MINING || g->player.activity == ACTIVITY_PLACING)
        g->player.activity = ACTIVITY_IDLE;
    return true;
}

static bool match_tile(Action a, TilePos t, int drop)
{
    (void)drop;
    return action_on_tile(a, t);
}

static bool match_drop(Action a, TilePos t, int drop)
{
    (void)t;
    return a.type == ACTION_COLLECT && a.drop == drop;
}

static bool match_mob(Action a, TilePos t, int mob)
{
    (void)t;
    return a.type == ACTION_ATTACK && a.drop == mob;
}

bool game_click_mob(Game *g, int mob)
{
    if (mob < 0 || mob >= MAX_MOBS || !g->mobs[mob].active) return false;
    if (unqueue(g, match_mob, (TilePos){ 0, 0 }, mob)) return true;
    const Mob *m = &g->mobs[mob];
    Action a = { .type = ACTION_ATTACK, .target = { m->x, m->y }, .drop = mob, .drop_id = m->id };
    return queue_checked(g, sim_pending(g), a);
}

bool game_unqueue_tile(Game *g, int tx, int ty)
{
    return unqueue(g, match_tile, (TilePos){ world_wrap_x(&g->world, tx), ty }, 0);
}

bool game_click_drop(Game *g, int drop)
{
    if (unqueue(g, match_drop, (TilePos){ 0, 0 }, drop)) return true;
    return game_queue_collect(g, drop);
}

bool game_click_tile(Game *g, int tx, int ty)
{
    if (game_unqueue_tile(g, tx, ty)) return true;

    Pose pos = sim_pending(g);
    tx = world_wrap_x(&g->world, tx);
    Action a = { .type = ACTION_MOVE, .target = { tx, ty } };
    const ItemStack *held = inventory_selected(&g->inventory);

    Action plant = { .type = ACTION_PLANT, .target = a.target, .item = held->id };
    /* Holding a bucket: water is scooped up into it, and poured out where there is room for it. */
    int holds = held->count > 0 && item_def((ItemId)held->id)->holds_water ? inventory_meter(&g->inventory, held) : -1;
    int wet = world_water(&g->world, tx, ty);
    if (holds >= 0 && world_in_bounds(&g->world, tx, ty) && !world_is_solid(&g->sim, tx, ty) &&
        ((wet > 0 && holds < item_def((ItemId)held->id)->meter_max) || holds > 0)) {
        a.type = ACTION_BUCKET;
        a.item = held->id;
    } else if (held->count > 0 && can_plant(&g->sim, (ItemId)held->id, a.target)) {
        /* Holding something that grows: soil with room above is where it is planted. */
        a = plant;
    } else if (world_in_bounds(&g->world, tx, ty) && mine_layer(&g->sim, a.target) >= 0) {
        /* A block here, in front or behind, is mined. */
        a.type = ACTION_MINE;
    } else if (held->count > 0 && bare_wall(&g->sim, a.target) != TILE_AIR &&
               item_def((ItemId)held->id)->tool_class >= 0 &&
               item_def((ItemId)held->id)->tool_class == tile_def(bare_wall(&g->sim, a.target))->tool_class) {
        /* Bare backwall, with the right kind of tool in hand: it is taken down. */
        a.type = ACTION_MINE;
        a.wall = true;
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
static void vitals_tick(Game *g);
static bool work_now(Game *g, Work work, int arg, int amount);

static Drop *spawn_drop_stack(Game *g, ItemStack one, TilePos at)
{
    if (one.id == ITEM_NONE) return NULL;
    for (int i = 0; i < MAX_DROPS; i++) {
        if (g->drops[i].active) continue;
        g->drops[i] = (Drop){ true, one.id, at.x, (float)at.y, 0.0f, 0.0f, false, 0,
                              ++g->next_drop_id, one.container, false };
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
    /* Some things are never held: they turn straight into crystals. */
    if (item_def(d->item)->crystals > 0) {
        g->crystals += item_def(d->item)->crystals;
        d->collect = 0.001f;
        return true;
    }
    if (inventory_add_stack(&g->inventory, (ItemStack){ d->item, 1, d->container }) != 0) return false;
    d->container = 0;
    d->collect = 0.001f;
    return true;
}

int game_explored(const Game *g, int x, int y)
{
    if (!g->fog || !g->explored) return 255;
    if (!world_in_bounds(&g->world, x, y)) return 0;
    return g->explored[(size_t)y * g->world.width + world_wrap_x(&g->world, x)];
}

void game_reveal(Game *g, int x, int y)
{
    if (!g->explored) return;
    const World *w = &g->world;
    int radius = content.rules.fog_radius, fade = content.rules.fog_fade;
    if (fade < 1) fade = 1;
    for (int dy = -radius; dy <= radius; dy++) {
        int ty = y + dy;
        if (ty < 0 || ty >= w->height) continue;
        for (int dx = -radius; dx <= radius; dx++) {
            /* Clear near the middle, thinning out over the last few blocks,
             * so the edge of what is known fades into the dark. */
            float d = sqrtf((float)(dx * dx + dy * dy));
            float clear = ((float)radius - d) / (float)fade;
            if (clear <= 0.0f) continue;
            int level = clear >= 1.0f ? 255 : (int)(clear * clear * (3.0f - 2.0f * clear) * 255.0f);
            uint8_t *cell = &g->explored[(size_t)ty * w->width + world_wrap_x(w, x + dx)];
            if (level > *cell) *cell = (uint8_t)level;
        }
    }
}

void game_drop_item(Game *g, ItemId id, int x, int y)
{
    spawn_drop(g, id, (TilePos){ world_wrap_x(&g->world, x), y });
}

void game_give_items(Game *g, ItemId id, int count)
{
    int left = inventory_add(&g->inventory, id, count);
    for (int i = 0; i < left; i++) spawn_drop(g, id, g->player.feet);
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
    /* (A shared store keeps its contents with the player, not the block.) */
    return (def->storage_columns > 0 && !def->storage_shared) || def->fuel_capacity > 0;
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

/* Puts so many of an item into a container, a stack to a slot, where there is room. */
static void container_put(Inventory *inv, uint16_t c, ItemId item, int count)
{
    int most = item_def(item)->max_stack;
    for (int i = 0; i < CONTAINER_MAX_SLOTS && count > 0; i++) {
        ItemStack *s = &inv->containers[c].slots[i];
        if (s->count > 0) continue;
        int n = count < most ? count : most;
        *s = (ItemStack){ (uint8_t)item, (uint8_t)n, 0 };
        count -= n;
    }
}

/* A storage block that the world came with has no record yet. The first time
 * it is opened or dug up it gets one, holding whatever its kind is found with. */
static void storage_ensure(Game *g, TilePos t, const TileDef *def)
{
    if (!needs_storage(def) || storage_at(g, t)) return;
    if (!storage_create(g, t, def, (ItemStack){ 0 })) return;
    uint16_t c = storage_at(g, t)->container;
    for (int i = 0; c != 0 && i < def->loot_count; i++) {
        const LootDef *l = &def->loot[i];
        int count = l->min + (l->max > l->min ? (int)(game_random(g) % (uint32_t)(l->max - l->min + 1)) : 0);
        ItemId item = l->item;
        if (item == ITEM_NONE) {
            int total = 0;
            for (int k = 0; k < l->choice_count; k++) total += l->choices[k].weight;
            int roll = total > 0 ? (int)(game_random(g) % (uint32_t)total) : 0;
            for (int k = 0; k < l->choice_count && item == ITEM_NONE; k++) {
                if (roll < l->choices[k].weight) item = l->choices[k].item;
                roll -= l->choices[k].weight;
            }
        }
        if (item != ITEM_NONE && count > 0) container_put(&g->inventory, c, item, count);
    }
}

/* A tree cut through: every block of trunk above the cut decays, and so do
 * the leaves that hung on the trunk from the cut upward - those beside it,
 * and whatever leaves reach on from those sideways or upward. What is below
 * the cut is left alone. */
static void decay_tree(World *w, int layer, TilePos cut)
{
    TileType log = world_get_at(w, layer, cut.x, cut.y);
    int kind = tile_def(log)->log_of;
    if (kind < 0) return;
    const TreeDef *tree = &content.trees[kind];
    TileType leaves = tree->leaves, dead_log = tile_def(log)->decays_into;
    TileType dead_leaves = leaves != TILE_AIR ? tile_def(leaves)->decays_into : TILE_AIR;
    if (dead_log == TILE_AIR && dead_leaves == TILE_AIR) return;
    /* No further out than this tree's own crown reaches. */
    int reach = 1;
    for (int i = 0; i < tree->row_count; i++) {
        const TreeRow *row = &tree->rows[i];
        if (row->left > reach) reach = row->left;
        if (row->right > reach) reach = row->right;
        for (int k = 0; k < row->cell_count; k++)
            if (abs(row->cells[k]) > reach) reach = abs(row->cells[k]);
    }

    TilePos open[128];
    int n = 0;
#define DECAY_LEAF(px, py)                                                              \
    do {                                                                                \
        if (dead_leaves != TILE_AIR && abs((px) - cut.x) <= reach &&                    \
            world_get_at(w, layer, (px), (py)) == leaves) {                             \
            world_set_at(w, layer, (px), (py), dead_leaves);                            \
            if (n < 128) open[n++] = (TilePos){ (px), (py) };                           \
        }                                                                               \
    } while (0)
    int y = cut.y;
    DECAY_LEAF(cut.x - 1, y);
    DECAY_LEAF(cut.x + 1, y);
    for (y = cut.y - 1; y >= 0 && world_get_at(w, layer, cut.x, y) == log; y--) {
        if (dead_log != TILE_AIR) world_set_at(w, layer, cut.x, y, dead_log);
        DECAY_LEAF(cut.x - 1, y);
        DECAY_LEAF(cut.x + 1, y);
    }
    DECAY_LEAF(cut.x, y); /* the leaf that sat on top of the trunk */
    while (n > 0) {
        TilePos p = open[--n];
        DECAY_LEAF(p.x - 1, p.y);
        DECAY_LEAF(p.x + 1, p.y);
        DECAY_LEAF(p.x, p.y - 1);
    }
#undef DECAY_LEAF
}

void game_decay_tree(Game *g, int x, int y)
{
    for (int layer = 0; layer < BLOCK_LAYERS; layer++)
        if (tile_def(world_get_at(&g->world, layer, x, y))->log_of >= 0) decay_tree(&g->world, layer, (TilePos){ x, y });
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
    if (tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y))->storage_shared) return SHARED_CONTAINER;
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
        /* Nothing lies about for ever, however it came to be there. */
        if (DROP_LIFETIME > 0.0f && d->age >= DROP_LIFETIME) {
            container_free(&g->inventory, d->container);
            d->active = false;
            continue;
        }

        int rest = (int)d->y;
        while (!world_is_solid(&g->world, d->x, rest + 1)) rest++;
        d->y += DROP_FALL_SPEED * dt;
        if (d->y > (float)rest) d->y = (float)rest;

        /* Collected from the tiles the player occupies or touches. */
        int dx = world_dx(&g->world, feet.x, d->x), dy = (int)(d->y + 0.5f) - feet.y;
        bool in_reach = dx >= -1 && dx <= 1 && dy >= -2 && dy <= 1;
        /* Something the player put down stays down until they have been away. */
        if (d->held_off && !in_reach) d->held_off = false;
        if (d->age < DROP_PICKUP_DELAY || !in_reach || d->held_off) continue;
        collect_drop(g, d);
    }
}

static void begin_step(Game *g, Step s)
{
    Player *p = &g->player;
    TilePos to = { p->feet.x + s.dx, p->feet.y + s.dy };
    /* Going hand over hand across a tree is a climb; letting go of a wall is a hop. */
    bool hop = s.dy == 0 && (s.dx > 1 || s.dx < -1 ||
                             (p->climbing && !can_hold(&g->world, to) && !can_hang(&g->world, to)));
    bool crawl = is_crawling(&g->world, p->feet) || is_crawling(&g->world, to);
    p->activity = ACTIVITY_STEPPING;
    p->step = s;
    p->step_hop = hop;
    p->step_to = (TilePos){ p->feet.x + s.dx, p->feet.y + s.dy };
    p->timer = 0.0f;
    bool swim = in_water(&g->world, p->feet) && in_water(&g->world, to) && !is_supported(&g->world, to) &&
                content.rules.swim_time > 0.0f;
    p->step_swim = swim;
    if (hop) p->duration = HOP_TIME;
    else if (swim) p->duration = content.rules.swim_time * (s.dx != 0 && s.dy != 0 ? 1.4142f : 1.0f);
    else if (s.climb || p->climbing) p->duration = CLIMB_TIME;
    else if (s.dy > 0) p->duration = FALL_TIME;
    else p->duration = crawl ? CRAWL_TIME : WALK_TIME;
    if (s.dx != 0) p->facing = s.dx > 0 ? 1 : -1;
}

/* Puts a block in the world, with a backwall behind it if it leaves one and
 * there is none there already. */
static void place_tile(World *w, TilePos t, TileType tile)
{
    /* A tall block goes up as a column of itself from where it is put. */
    for (int i = 0; i < tile_def(tile)->height; i++)
        world_set_at(w, tile_def(tile)->layer, t.x, t.y - i, tile);
    if (tile_def(tile)->wall && world_get_at(w, LAYER_WALL, t.x, t.y) == TILE_AIR)
        world_set_at(w, LAYER_WALL, t.x, t.y, tile);
}

/* Changes every tile of the block at a position - all of it, if it is a tall
 * one - to another tile: TILE_AIR to remove it. */
static void replace_block(World *w, int layer, TilePos t, TileType with)
{
    int bottom, count = world_structure(w, layer, t.x, t.y, &bottom);
    for (int i = 0; i < count; i++) world_set_at(w, layer, t.x, bottom - i, with);
}

/* Whether there is room for a block: nothing in front, and nothing in its
 * own layer, for every tile of its height. */
static bool room_for(const World *w, TilePos t, TileType tile)
{
    for (int i = 0; i < tile_def(tile)->height; i++) {
        if (!world_in_bounds(w, t.x, t.y - i) || world_get(w, t.x, t.y - i) != TILE_AIR ||
            world_get_at(w, tile_def(tile)->layer, t.x, t.y - i) != TILE_AIR)
            return false;
    }
    return true;
}

/* Leaves what a removed block leaves: `count` of its main drop, the first
 * carrying `inside`, and whatever else it gives. */
static void drop_block_items(Game *g, const TileDef *def, TilePos at, uint16_t inside, int count)
{
    for (int i = 0; i < count; i++) {
        /* Something rotten does not always leave anything. (What a block
         * holds inside it is never lost that way.) */
        bool kept = def->drop_chance >= 1.0f || (i == 0 && inside != 0) ||
                    (float)(game_random(g) >> 8) / 16777216.0f < def->drop_chance;
        if (kept) spawn_drop_stack(g, (ItemStack){ (uint8_t)def->drop, 1, i == 0 ? inside : 0 }, at);
    }
    for (int k = 0; k < def->extra_drop_count; k++)
        for (int i = 0; i < def->extra_drops[k].count; i++) spawn_drop(g, def->extra_drops[k].id, at);
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
        game_reveal(g, p->feet.x, p->feet.y - 1);
        p->step_to = p->feet;
        p->climbing = p->step.climb;
    } else if (p->activity == ACTIVITY_MINING) {
        TilePos t = g->current.target;
        int layer = action_layer(&g->world, g->current);
        if (layer == LAYER_WALL) {
            /* Backwall comes away with nothing to show for it. */
            world_set_at(&g->world, layer, t.x, t.y, TILE_AIR);
        } else if (layer >= 0) {
            /* A storage block's contents go into the item it leaves behind. */
            const TileDef *def = tile_def(world_get_at(&g->world, layer, t.x, t.y));
            storage_ensure(g, t, def);
            uint16_t inside = needs_storage(def) ? storage_release(g, t) : 0;
            /* Ore comes out several times over with the right pickaxe. */
            int count = def->drop_count > 0 ? def->drop_count : 1;
            if (def->ore) count *= inventory_ore_multiplier(&g->inventory, def->tool_class);
            /* A tool that wears is the worse for it. */
            inventory_wear_tool(&g->inventory, def->tool_class);
            /* A tall block comes down whole, and lands at its foot. */
            int bottom;
            world_structure(&g->world, layer, t.x, t.y, &bottom);
            /* Something like kelp comes away from the cut upward: every block
             * of it stacked above goes too, each leaving what it leaves. */
            TileType tile = world_get_at(&g->world, layer, t.x, t.y);
            for (int y = t.y - 1; def->breaks_above && world_get_at(&g->world, layer, t.x, y) == tile; y--) {
                drop_block_items(g, def, (TilePos){ t.x, y }, 0, 1);
                world_set_at(&g->world, layer, t.x, y, TILE_AIR);
            }
            /* A tree cut through dies from the cut upward. */
            decay_tree(&g->world, layer, t);
            drop_block_items(g, def, (TilePos){ t.x, bottom }, inside, count);
            replace_block(&g->world, layer, t, TILE_AIR);
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
            plant_item(&g->world, item, t);
        } else if (!needs_storage(tile_def(tile))) {
            container_free(&g->inventory, one.container);
            place_tile(&g->world, t, tile);
        } else if (!storage_at(g, t) && storage_create(g, t, tile_def(tile), one)) {
            /* What was inside the item is now inside the block. (One tile
             * keeps one record, so not where a block behind already has one.) */
            place_tile(&g->world, t, tile);
        } else {
            inventory_add_stack(&g->inventory, one);
            deny(g, t);
        }
        g->has_current = false;
    }
    p->activity = ACTIVITY_IDLE;
}

/* A bucket at a square of the world. If there is water there and room in the
 * bucket, it takes as much as it has room for and no more than is there;
 * otherwise it is emptied there, all it holds. Either way the water only
 * changes places. */
static void use_bucket(Game *g, ItemId item, TilePos at)
{
    World *w = &g->world;
    ItemStack *bucket = inventory_find(&g->inventory, item);
    if (!bucket || !item_def(item)->holds_water || world_is_solid(w, at.x, at.y)) return;
    int held = inventory_meter(&g->inventory, bucket), most = item_def(item)->meter_max;
    int there = world_water(w, at.x, at.y);
    int taken = there < most - held ? there : most - held;
    if (taken > 0) {
        world_set_water(w, at.x, at.y, there - taken);
        inventory_set_meter(&g->inventory, bucket, held + taken);
        return;
    }
    /* Emptied: all of it goes in there, and finds its own level from then on. */
    if (held <= 0) return;
    inventory_set_meter(&g->inventory, bucket, water_add(w, at.x, at.y, held));
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
    /* Asleep or meditating, the player stays put - on the ground, that is. */
    if (g->vitals.rest != REST_NONE) {
        if (!p->climbing) return false;
        game_wake(g);
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
            } else if (g->current.type == ACTION_ATTACK) {
                /* The quarry will not have stood still: go by where it is now. */
                Mob *m = &g->mobs[g->current.drop];
                if (!m->active || m->id != g->current.drop_id) {
                    g->has_current = false;
                    continue;
                }
                TilePos at = { m->x, m->y };
                if (in_use_range(w, p->feet, at)) {
                    mob_kill(g, g->current.drop);
                    g->has_current = false;
                    continue;
                }
                g->current.target = at;
                if (g->current.tries++ < MAX_ATTACK_TRIES &&
                    plan_action(g, w, pose, g->current, g->steps, &g->step_count, NULL) && g->step_count > 0) {
                    g->step_index = 0;
                    continue;
                }
            } else if (target_valid(w, g->current) && is_goal(w, pose, g->current)) {
                switch (g->current.type) {
                case ACTION_MINE:
                    begin_work(g, ACTIVITY_MINING,
                               game_mine_time(g, world_get_at(w, action_layer(w, g->current),
                                                              g->current.target.x,
                                                              g->current.target.y)));
                    return true;
                case ACTION_PLACE:
                case ACTION_PLANT:
                    if (inventory_count(&g->inventory, (ItemId)g->current.item) < 1) break;
                    begin_work(g, ACTIVITY_PLACING, PLACE_TIME);
                    return true;
                case ACTION_BUCKET:
                    use_bucket(g, (ItemId)g->current.item, g->current.target);
                    g->has_current = false;
                    continue;
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
                    storage_ensure(g, g->bench_pos, tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y)));
                    /* What the player came to do there, if they came to do anything. */
                    if (g->current.work != WORK_NONE)
                        work_now(g, (Work)g->current.work, g->current.arg, g->current.amount);
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
    return r != NULL && game_bench_work(g, WORK_CRAFT, (int)(r - content.recipes), count);
}

static bool craft_now(Game *g, const Recipe *r, int count)
{
    if (!g->bench_open || g->craft_remaining > 0 || count < 1) return false;
    /* Only what this bench makes, at the level it has reached. */
    TileType bench = game_bench_at(g, g->bench_pos.x, g->bench_pos.y);
    if (strcmp(r->bench, tile_def(bench)->bench_kind) != 0 || recipe_locked(bench, r)) return false;
    if (game_bench_fuel(g, NULL) == 0) return false; /* out of fuel */
    int max = recipe_max_crafts(&g->inventory, r, g->crystals);
    if (max < 1) return false;
    g->craft_recipe = r;
    g->craft_total = g->craft_remaining = count < max ? count : max;
    g->craft_timer = 0.0f;
    return true;
}

int game_fuel_at(const Game *g, int x, int y)
{
    Storage *st = storage_at((Game *)g, (TilePos){ world_wrap_x(&g->world, x), y });
    return st ? st->fuel : 0;
}

int game_bench_fuel(const Game *g, int *capacity)
{
    if (!g->bench_open) return -1;
    const TileDef *def = tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y));
    Storage *st = storage_at((Game *)g, g->bench_pos);
    if (def->fuel_capacity <= 0 || !st) return -1;
    if (capacity) *capacity = def->fuel_capacity;
    return st->fuel;
}

bool game_add_fuel(Game *g, ItemId item)
{
    return game_bench_work(g, WORK_FUEL, item, 1);
}

static bool add_fuel_now(Game *g, ItemId item)
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
    return game_bench_work(g, WORK_UPGRADE, 0, 0);
}

static bool upgrade_now(Game *g)
{
    if (!g->bench_open) return false;
    int layer = LAYER_FOREGROUND;
    const TileDef *def = tile_def(usable_at(&g->world, g->bench_pos.x, g->bench_pos.y, &layer));
    if (def->upgrade_to == TILE_AIR ||
        !inventory_has_all(&g->inventory, def->upgrade_cost, def->upgrade_cost_count))
        return false;
    inventory_remove_all(&g->inventory, def->upgrade_cost, def->upgrade_cost_count);
    replace_block(&g->world, layer, g->bench_pos, def->upgrade_to);
    return true;
}

/* ---- working a bench, from near or far ---------------------------------- */

static bool work_now(Game *g, Work work, int arg, int amount)
{
    switch (work) {
    case WORK_CRAFT:
        return arg >= 0 && arg < content.recipe_count && craft_now(g, &content.recipes[arg], amount);
    case WORK_UPGRADE: return upgrade_now(g);
    case WORK_FUEL: return add_fuel_now(g, arg);
    case WORK_BUY: return trade_buy_now(g, arg, amount);
    case WORK_SELL: return trade_sell_now(g, arg, amount);
    case WORK_CONVERT: return trade_convert_now(g, arg, amount);
    default: return false;
    }
}

bool game_open_bench(Game *g, int x, int y)
{
    x = world_wrap_x(&g->world, x);
    const TileDef *def = tile_def(game_bench_at(g, x, y));
    if (!def->bench && !def->trade) return false;
    if (g->bench_open && g->bench_pos.x == x && g->bench_pos.y == y) return true;
    game_close_bench(g);
    g->bench_open = true;
    g->bench_pos = (TilePos){ x, y };
    return true;
}

bool game_at_bench(const Game *g)
{
    const Player *p = &g->player;
    return g->bench_open && p->activity != ACTIVITY_STEPPING && in_use_range(&g->world, p->feet, g->bench_pos);
}

bool game_bench_work(Game *g, Work work, int arg, int amount)
{
    if (!g->bench_open || work == WORK_NONE) return false;
    /* Standing at it with nothing else to do first: done here and now. */
    if (game_at_bench(g) && !g->has_current && g->queue_count == 0) return work_now(g, work, arg, amount);
    /* Otherwise the player goes there, and does it on arriving. */
    Action a = { .type = ACTION_USE, .target = g->bench_pos, .work = (uint8_t)work, .arg = arg, .amount = amount };
    return queue_checked(g, sim_pending(g), a);
}

int game_teleport_cost(const Game *g)
{
    const RulesDef *u = &content.rules;
    if (!g->bench_open || !u->teleport_bench[0]) return -1;
    const TileDef *def = tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y));
    if (!def->bench || strcmp(def->bench_kind, u->teleport_bench) != 0 || def->bench_level < u->teleport_level) return -1;
    float dx = (float)world_dx(&g->world, g->player.feet.x, g->bench_pos.x), dy = (float)(g->bench_pos.y - g->player.feet.y);
    int far = (int)sqrtf(dx * dx + dy * dy) - u->teleport_free;
    return u->teleport_cost + (far > 0 && u->teleport_per > 0 ? far / u->teleport_per : 0);
}

bool game_teleport(Game *g)
{
    World *w = &g->world;
    Player *p = &g->player;
    int cost = game_teleport_cost(g);
    if (cost < 0 || g->crystals < cost || in_use_range(w, p->feet, g->bench_pos)) return false;
    /* To the foot of the portal, or failing that beside it. */
    int layer = LAYER_FOREGROUND, bottom = g->bench_pos.y;
    usable_at(w, g->bench_pos.x, g->bench_pos.y, &layer);
    world_structure(w, layer, g->bench_pos.x, g->bench_pos.y, &bottom);
    TilePos to = { g->bench_pos.x, bottom };
    bool found = false;
    static const int beside[5] = { 0, -1, 1, -2, 2 };
    for (int i = 0; i < 5 && !found; i++) {
        for (int dy = 0; dy <= 2 && !found; dy++) {
            TilePos at = { world_wrap_x(w, g->bench_pos.x + beside[i]), bottom - dy };
            if (!body_fits(w, at.x, at.y) || !is_supported(w, at)) continue;
            to = at;
            found = true;
        }
    }
    if (!found) return false;
    g->crystals -= cost;
    game_cancel_actions(g);
    game_wake(g);
    p->feet = p->step_to = to;
    p->activity = ACTIVITY_IDLE;
    p->climbing = false;
    game_reveal(g, to.x, to.y - 1);
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
    /* It shuts if it is removed. A chest shuts when the player leaves it too;
     * a bench's menu stays up to be looked at, but nothing is made at it
     * while the player is away. */
    const TileDef *open = tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y));
    bool there = in_use_range(&g->world, g->player.feet, g->bench_pos);
    if ((!open->bench && !open->trade && open->storage_columns == 0) || (!there && !open->bench && !open->trade)) {
        game_close_bench(g);
        return;
    }
    if (!there) {
        g->craft_remaining = 0;
        return;
    }

    /* The fire going out stops the work. */
    if (game_bench_fuel(g, NULL) == 0) g->craft_remaining = 0;
    if (g->craft_remaining <= 0) return;
    g->craft_timer += dt;
    while (g->craft_remaining > 0 && g->craft_timer >= CRAFT_TIME) {
        g->craft_timer -= CRAFT_TIME;
        int left = recipe_craft(&g->inventory, g->craft_recipe, &g->crystals);
        if (left < 0) {
            g->craft_remaining = 0;
            break;
        }
        for (int i = 0; i < left; i++) spawn_drop(g, g->craft_recipe->output.id, g->player.feet);
        g->craft_remaining--;
    }
}

/* ---- world time -------------------------------------------------------- */

uint32_t game_random(Game *g)
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
void game_random_event(Game *g, int x, int y)
{
    World *w = &g->world;

    if (mobs_try_hatch(g, x, y) || mobs_try_spawn(g, x, y)) return;
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

    /* Something rotten falls apart, leaving what it leaves if it leaves anything. */
    for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
        const TileDef *def = tile_def(world_get_at(w, layer, x, y));
        if (!def->vanishes) continue;
        drop_block_items(g, def, (TilePos){ x, y }, 0, def->drop_count > 0 ? def->drop_count : 1);
        world_set_at(w, layer, x, y, TILE_AIR);
        return;
    }

    /* A plant grows on to its next stage, if it is where it can live. */
    for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
        const TileDef *def = tile_def(world_get_at(w, layer, x, y));
        if (def->grows_into == TILE_AIR) continue;
        if (!def->needs_water || world_is_water(w, x, y)) world_set_at(w, layer, x, y, def->grows_into);
        return;
    }

    int kind = tree_leaves_at(w, x, y);
    if (kind < 0 || content.trees[kind].fruit == ITEM_NONE || attached_drop_at(g, x, y)) return;
    Drop *fruit = spawn_drop(g, content.trees[kind].fruit, (TilePos){ x, y });
    if (!fruit) return;
    fruit->attached = true;
    fruit->ttl = content.trees[kind].fruit_ticks;
}

/* ---- the player's needs ------------------------------------------------- */

static float clamp01(float v)
{
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

bool game_bar_full(float value)
{
    return value >= 1.0f - content.rules.full_margin;
}

bool game_is_food(ItemId id)
{
    return item_def(id)->food > 0.0f;
}

bool game_player_underwater(const Game *g)
{
    const Player *p = &g->player;
    bool low = world_is_solid(&g->world, p->feet.x, p->feet.y - 1); /* crawling: the head is down at the feet */
    return world_is_water(&g->world, p->feet.x, low ? p->feet.y : p->feet.y - 1);
}

bool game_player_swimming(const Game *g)
{
    const Player *p = &g->player;
    return in_water(&g->world, p->feet) && !is_supported(&g->world, p->feet);
}

bool game_saturated(const Game *g)
{
    return g->vitals.saturation > 0.0f;
}

bool game_can_eat(const Game *g, ItemId id)
{
    if (!game_is_food(id)) return false;
    /* Something that gives breath can always be eaten for that, when breath is short. */
    if (item_def(id)->food_oxygen > 0.0f && g->vitals.oxygen < 1.0f) return true;
    return !game_saturated(g);
}

bool game_eat(Game *g)
{
    Vitals *v = &g->vitals;
    const ItemStack *held = inventory_selected(&g->inventory);
    if (held->count < 1 || !game_can_eat(g, held->id)) return false;
    const ItemDef *def = item_def(held->id);
    ItemStack eaten;
    if (!inventory_take_one(&g->inventory, held->id, &eaten)) return false;
    container_free(&g->inventory, eaten.container);

    v->oxygen = clamp01(v->oxygen + def->food_oxygen);
    /* On a full stomach it does not fill the bar; it keeps it full. */
    if (game_bar_full(v->hunger)) v->saturation += def->food * (float)content.rules.saturation_ticks;
    v->hunger = clamp01(v->hunger + def->food);
    float mend = def->food_health >= 0.0f ? def->food_health : def->food * content.rules.food_health;
    v->health = clamp01(v->health + mend);
    v->energy = clamp01(v->energy + def->food_energy);
    return true;
}

void game_wake(Game *g)
{
    g->vitals.rest = REST_NONE;
    g->vitals.meditation = 0.0f;
}

bool game_rest(Game *g, Rest kind)
{
    Player *p = &g->player;
    if (kind == REST_NONE) {
        game_wake(g);
        return true;
    }
    if (kind == REST_SLEEP && game_bar_full(g->vitals.energy)) return false;
    /* Only from standing on the ground, or about to be. */
    TilePos at = p->activity == ACTIVITY_STEPPING ? p->step_to : p->feet;
    bool climbing = p->activity == ACTIVITY_STEPPING ? p->step.climb : p->climbing;
    if (climbing || !is_supported(&g->world, at)) return false;
    game_cancel_actions(g);
    game_close_bench(g);
    g->move_dx = g->move_dy = 0;
    if (g->vitals.rest != (int32_t)kind) g->vitals.meditation = 0.0f;
    g->vitals.rest = (int32_t)kind;
    return true;
}

int game_tick_speed(const Game *g)
{
    int speed = g->vitals.rest != REST_NONE && content.rules.rest_speed > 1 ? content.rules.rest_speed : 1;
    return g->tick_rate * speed;
}

/* Where something that comes out of the player appears: at their head, to
 * fall to their feet. */
static TilePos toss_tile(const Game *g)
{
    return (TilePos){ g->player.feet.x, g->player.feet.y - 1 };
}

/* What meditating has brought: one thing from the list, by weight. It drops
 * out on top of the player, and is picked up like anything else at their feet. */
static void meditation_reward(Game *g)
{
    const RulesDef *u = &content.rules;
    int total = 0;
    for (int i = 0; i < u->loot_count; i++) total += u->loot[i].weight;
    if (total <= 0) return;
    int roll = (int)(game_random(g) % (uint32_t)total);
    ItemId item = ITEM_NONE;
    for (int i = 0; i < u->loot_count && item == ITEM_NONE; i++) {
        if (roll < u->loot[i].weight) item = u->loot[i].item;
        roll -= u->loot[i].weight;
    }
    spawn_drop(g, item, toss_tile(g));
}

int game_discard(Game *g, SlotRef from, int count)
{
    Inventory *inv = &g->inventory;
    ItemStack *s = inventory_slot(inv, from);
    TilePos at = toss_tile(g);
    int gone = 0;
    while (s && s->count > 0 && gone < count) {
        /* One at a time off the top, each with what is inside it or how its meter reads. */
        ItemStack one;
        if (!inventory_split(inv, s, 1, &one)) break;
        Drop *d = spawn_drop_stack(g, one, at);
        if (!d) break; /* no room in the world for more: it went back where it fits */
        d->held_off = true;
        gone++;
    }
    return gone;
}

static void vitals_tick(Game *g)
{
    const RulesDef *u = &content.rules;
    Vitals *v = &g->vitals;

    /* Meditating, the body wants nothing: neither hunger nor energy runs down. */
    bool still = v->rest == REST_MEDITATE;

    /* Hunger waits for a full stomach to wear off, then runs down. */
    if (still) {}
    else if (v->saturation > 0.0f) v->saturation = v->saturation > 1.0f ? v->saturation - 1.0f : 0.0f;
    else if (u->hunger_ticks > 0) v->hunger = clamp01(v->hunger - 1.0f / (float)u->hunger_ticks);

    /* Health mends by itself, unless there is nothing left to eat on. */
    if (v->hunger <= 0.0f && u->starve_ticks > 0) {
        if (v->health > u->starve_floor) {
            v->health -= 1.0f / (float)u->starve_ticks;
            if (v->health < u->starve_floor) v->health = u->starve_floor;
        }
    } else if (u->health_regen_ticks > 0) {
        v->health = clamp01(v->health + 1.0f / (float)u->health_regen_ticks);
    }

    /* Breath: it runs down with the head under water and comes back in the
     * air. With none left, health goes instead. */
    if (u->oxygen_ticks > 0 && game_player_underwater(g)) {
        v->oxygen = clamp01(v->oxygen - 1.0f / (float)u->oxygen_ticks);
        if (v->oxygen <= 0.0f && u->drown_ticks > 0) {
            v->health = clamp01(v->health - 1.0f / (float)u->drown_ticks);
            game_wake(g);
        }
    } else {
        v->oxygen = u->oxygen_refill_ticks > 0 ? clamp01(v->oxygen + 1.0f / (float)u->oxygen_refill_ticks) : 1.0f;
    }

    if (v->rest == REST_SLEEP) {
        if (u->sleep_ticks > 0) v->energy = clamp01(v->energy + 1.0f / (float)u->sleep_ticks);
        if (v->energy >= 1.0f || u->sleep_ticks <= 0) game_wake(g);
    } else if (u->energy_ticks > 0 && !still) {
        v->energy = clamp01(v->energy - 1.0f / (float)u->energy_ticks);
    }

    if (v->rest == REST_MEDITATE && u->meditation_ticks > 0) {
        v->meditation += 1.0f / (float)u->meditation_ticks;
        /* (A hair short of 1 is the rounding of all those small steps.) */
        if (v->meditation >= 0.9999f) {
            v->meditation = 0.0f;
            meditation_reward(g);
        }
    }
}

void game_tick(Game *g)
{
    World *w = &g->world;
    g->tick++;

    for (int chunk = 0; chunk * CHUNK_WIDTH < w->width; chunk++) {
        for (int n = 0; n < content.rules.random_events_per_chunk; n++) {
            uint32_t r = game_random(g);
            int x = chunk * CHUNK_WIDTH + (int)(r % (uint32_t)CHUNK_WIDTH);
            if (x < w->width) game_random_event(g, x, (int)((r >> 8) % (uint32_t)w->height));
        }
    }

    mobs_tick(g);
    water_step(w);
    vitals_tick(g);

    /* Fuel burns down whether or not anything is being made. */
    for (int i = 0; i < MAX_STORAGES; i++)
        if (g->storages[i].used && g->storages[i].fuel > 0) g->storages[i].fuel--;

    for (int i = 0; i < MAX_DROPS; i++) {
        Drop *d = &g->drops[i];
        if (!d->active) continue;
        /* Fruit whose leaf is gone falls like anything else. */
        if (d->attached && tree_leaves_at(w, d->x, (int)d->y) < 0) {
            d->attached = false;
            d->age = 0.0f; /* its time on the ground starts now */
        }
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

    g->tick_timer += dt * (float)game_tick_speed(g);
    for (int n = 0; g->tick_timer >= 1.0f && n < MAX_TICKS_PER_UPDATE; n++) {
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
    if (dx != 0 || dy != 0) game_wake(g);
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
    int dx = p->step_to.x - p->feet.x, dy = p->step_to.y - p->feet.y;
    /* A diagonal step goes round the corner between the two squares, by
     * whichever way the body has room: up and then across to get onto
     * something, across and then down to get off it. */
    bool rise_first = dy < 0;
    if (dx != 0 && dy != 0) {
        bool by_rise = body_fits(&g->world, p->feet.x, p->feet.y + dy);
        bool by_side = body_fits(&g->world, p->feet.x + dx, p->feet.y);
        if (rise_first && !by_rise && by_side) rise_first = false;
        else if (!rise_first && !by_side && by_rise) rise_first = true;
    }
    float ox, oy;
    game_step_offset(dx, dy, rise_first, t, &ox, &oy);
    if (p->activity == ACTIVITY_STEPPING && p->step_swim) {
        /* Swimming is straight there, slant or no. */
        ox = (float)dx * t;
        oy = (float)dy * t;
    }
    *x = (float)p->feet.x + ox;
    *y = (float)p->feet.y + oy;
    if (p->activity == ACTIVITY_STEPPING && p->step_hop) *y -= HOP_HEIGHT * 4.0f * t * (1.0f - t);
}

void game_step_offset(int dx, int dy, bool rise_first, float t, float *ox, float *oy)
{
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    if (dx == 0 || dy == 0) {
        *ox = (float)dx * t;
        *oy = (float)dy * t;
        return;
    }
    /* Two legs, each eased in and out so the turn is not a jolt. */
    float a = t < 0.5f ? t * 2.0f : 1.0f, b = t < 0.5f ? 0.0f : t * 2.0f - 1.0f;
    a = a * a * (3.0f - 2.0f * a);
    b = b * b * (3.0f - 2.0f * b);
    *ox = (float)dx * (rise_first ? b : a);
    *oy = (float)dy * (rise_first ? a : b);
}

bool game_player_crawling(const Game *g)
{
    const Player *p = &g->player;
    /* Down on hands and knees from the start of a step into a low gap to the
     * end of the step out of it. */
    return is_crawling(&g->world, p->feet) ||
           (p->activity == ACTIVITY_STEPPING && is_crawling(&g->world, p->step_to));
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
    g->explored = calloc(cells, 1);
    if (!g->explored) {
        game_free(g);
        return false;
    }
    g->vitals = (Vitals){ .health = 1.0f, .hunger = 1.0f, .energy = 1.0f, .oxygen = 1.0f };
    g->fog = content.rules.fog_enabled;
    game_reveal(g, g->player.feet.x, g->player.feet.y - 1);
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
    free(g->explored);
    memset(g, 0, sizeof *g);
}
