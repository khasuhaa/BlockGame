#include "game.h"
#include "digital.h"
#include "industry.h"
#include "power.h"

#include "mob.h"
#include "trade.h"
#include "train.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- movement rules ---------------------------------------------------- */

/* The player is two tiles tall standing or climbing, and one tile tall when
 * crawling. Crawling is not a choice: it is what the player does wherever
 * there is a block where their head would be. */

/* Whether a square keeps a character's body out: anything solid, but for a
 * door, which lets a character through and nothing else. */
static bool blocks_body(const World *w, int x, int y)
{
    return world_is_solid(w, x, y) && !tile_def(world_get(w, x, y))->passable;
}

/* Room for the whole body. */
static bool body_fits(const World *w, int x, int y)
{
    return !blocks_body(w, x, y) && !blocks_body(w, x, y - 1);
}

/* Room to be in at all, if only on hands and knees. */
static bool low_fits(const World *w, int x, int y)
{
    return !blocks_body(w, x, y);
}

static bool is_crawling(const World *w, TilePos feet)
{
    return blocks_body(w, feet.x, feet.y - 1);
}

/* A trapdoor underfoot: a door in the floor, to be let down through. */
static bool hatch_below(const World *w, TilePos feet)
{
    const TileDef *def = tile_def(world_get(w, feet.x, feet.y + 1));
    return def->passable && def->climbable;
}

/* Something to stand on: a block, or steps, which hold up whoever is on top of them. */
static bool is_supported(const World *w, TilePos feet)
{
    return world_is_solid(w, feet.x, feet.y + 1) || world_is_step(w, feet.x, feet.y + 1);
}

/* Up a flight of steps: from beside them onto the top of them. (They can as
 * well be walked straight through.) */
static bool can_step_up(const World *w, TilePos feet, int d)
{
    return world_is_step(w, feet.x + d, feet.y) && !world_is_solid(w, feet.x + d, feet.y) &&
           body_fits(w, feet.x, feet.y - 1) && body_fits(w, feet.x + d, feet.y - 1);
}

/* Whether going up the steps beside the feet gets anywhere: at the top of
 * them there is more to go up, or a floor to walk on to. Steps that stand by
 * themselves in the way are walked past, not up and down again. */
static bool steps_lead_on(const World *w, TilePos feet, int d)
{
    TilePos top = { feet.x + d, feet.y - 1 };
    if (can_step_up(w, top, d)) return true;
    return low_fits(w, top.x + d, top.y) && !world_is_step(w, top.x + d, top.y + 1) && is_supported(w, (TilePos){ top.x + d, top.y });
}

/* The floors a running lift will take a character at `feet` to: every place
 * along its shaft, up or down, where there is room for them and something to
 * stand on. It goes through whatever blocks are between. `out` gets how far
 * each is (negative for up), nearest first each way; returns how many. */
static int lift_stops(const World *w, TilePos feet, int16_t *out)
{
    int n = 0;
    if (!world_is_shaft(w, feet.x, feet.y) || !world_lift_runs(w, feet.x, feet.y)) return 0;
    for (int way = -1; way <= 1; way += 2) {
        int found = 0;
        for (int dy = way; world_is_shaft(w, feet.x, feet.y + dy) && found < MAX_LIFT_STOPS && abs(dy) < 30000; dy += way) {
            if (!body_fits(w, feet.x, feet.y + dy) || !is_supported(w, (TilePos){ feet.x, feet.y + dy })) continue;
            out[n++] = (int16_t)dy;
            found++;
        }
    }
    return n;
}

static void set_aloft(Game *g);

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

/* Held up wherever they are: by water deep enough to swim in, or - for someone who can fly - by the air. */
static bool buoyed(const World *w, TilePos feet)
{
    if (in_water(w, feet)) return true;
    return w->aloft && world_in_bounds(w, feet.x, feet.y - 1) && !world_is_solid(w, feet.x, feet.y) && !world_is_solid(w, feet.x, feet.y - 1);
}

/* Something climbable behind the player's body, such as a tree - or water,
 * which holds a swimmer up the same way wherever they are in it. */
static bool can_hold(const World *w, TilePos feet)
{
    if (buoyed(w, feet)) return true;
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
 * to crawl into, and there need be no room to rise before going in: a gap at
 * head height under a ceiling is got into head first. (Not from another gap:
 * someone crawling has a block over them, and no way up round it.) */
static bool can_mantle(const World *w, TilePos feet, int d)
{
    return world_is_solid(w, feet.x + d, feet.y) && !blocks_body(w, feet.x, feet.y - 1) &&
           low_fits(w, feet.x + d, feet.y - 1);
}

/* Off whatever is being climbed and straight in beside the feet, onto a
 * floor there: a ledge, or a gap only high enough to crawl into. */
static bool can_step_off(const World *w, TilePos feet, int d)
{
    return low_fits(w, feet.x + d, feet.y) && is_supported(w, (TilePos){ feet.x + d, feet.y });
}

static bool can_climb_up(const World *w, TilePos feet)
{
    TilePos up = { feet.x, feet.y - 1 };
    /* (Up out of a trapdoor is onto the top of it: there is nothing to hold there, and no need.) */
    return body_fits(w, up.x, up.y) && (can_cling(w, up) || (blocks_body(w, feet.x, feet.y) != world_is_solid(w, feet.x, feet.y)));
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
static int every_move(const World *w, Pose p, Step *out)
{
    int n = 0;
    int x = p.feet.x, y = p.feet.y;

    if (p.climbing) {
        if (can_climb_up(w, p.feet)) out[n++] = (Step){ 0, -1, true };
        out[n++] = (Step){ 0, 1, true };
        for (int d = -1; d <= 1; d += 2) {
            int k;
            if (can_mantle(w, p.feet, d)) out[n++] = (Step){ (int8_t)d, -1, false };
            else if (can_step_off(w, p.feet, d)) out[n++] = (Step){ (int8_t)d, 0, false };
            else if (can_climb_across(w, p.feet, d)) out[n++] = (Step){ (int8_t)d, 0, true };
            else if ((k = wall_hop_distance(w, p.feet, d)) > 0)
                out[n++] = (Step){ (int8_t)(d * k), 0, true };
            if (can_swing_up(w, p.feet, d)) out[n++] = (Step){ (int8_t)d, -1, true };
            if (can_swing_under(w, p.feet, d)) out[n++] = (Step){ (int8_t)d, 1, true };
        }
        /* In water there is nothing to go round: a swimmer goes on the slant
         * too, wherever the squares either side of the slant are clear. */
        for (int d = -1; buoyed(w, p.feet) && d <= 1; d += 2) {
            for (int v = -1; v <= 1 && n < MAX_MOVES; v += 2) {
                TilePos to = { x + d, y + v };
                if (buoyed(w, to) && body_fits(w, to.x, to.y) && body_fits(w, x + d, y) && body_fits(w, x, y + v))
                    out[n++] = (Step){ (int8_t)d, (int8_t)v, true };
            }
        }
        return n;
    }

    bool upright = !is_crawling(w, p.feet);
    /* Someone who can fly takes off from where they stand, straight up or on the slant: these come first, so that
     * of two ways as short as each other the one through the air is the one taken. */
    if (w->aloft && buoyed(w, p.feet)) {
        if (buoyed(w, (TilePos){ x, y - 1 })) out[n++] = (Step){ 0, -1, true };
        for (int d = -1; d <= 1; d += 2) {
            TilePos to = { x + d, y - 1 };
            if (buoyed(w, to) && body_fits(w, to.x, to.y) && body_fits(w, x + d, y) && body_fits(w, x, y - 1)) out[n++] = (Step){ (int8_t)d, -1, true };
        }
    }
    for (int d = -1; d <= 1; d += 2) {
        /* (Walking past steps comes before going up them: a way that only passes them does not climb them.) */
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
            if (is_supported(w, (TilePos){ x + d, y + 1 })) out[n++] = (Step){ (int8_t)d, 0, false };
            else out[n++] = (Step){ (int8_t)d, 1, true };
        }
        if (can_step_up(w, p.feet, d)) out[n++] = (Step){ (int8_t)d, -1, false };
    }
    /* Jump and catch a wall too tall to step onto, or something overhead. */
    if (can_climb_up(w, p.feet) && !(w->aloft && buoyed(w, p.feet) && buoyed(w, (TilePos){ x, y - 1 }))) out[n++] = (Step){ 0, -1, true };
    /* Down through a trapdoor in the floor. */
    if (hatch_below(w, p.feet)) out[n++] = (Step){ 0, 1, true };
    /* A lift, to any of its floors. */
    int16_t floors[2 * MAX_LIFT_STOPS];
    int stops = lift_stops(w, p.feet, floors);
    for (int i = 0; i < stops && n < MAX_MOVES; i++)
        if (floors[i] != -1 || !can_climb_up(w, p.feet)) out[n++] = (Step){ 0, floors[i], false };
    return n;
}

/* Whether a step would leave them falling further than they will let themselves: with nothing under where it
 * takes them and nothing there to hold, and no ground within the drop the rules allow. */
static bool drops_too_far(const World *w, Pose p, Step s)
{
    int most = content.rules.max_drop;
    if (most <= 0) return false; /* (no limit set) */
    TilePos to = { p.feet.x + s.dx, p.feet.y + s.dy };
    for (int k = 0; k <= most; k++) {
        TilePos at = { to.x, to.y + k };
        if (!world_in_bounds(w, at.x, at.y)) return false;
        if (is_supported(w, at) || can_cling(w, at)) return false;
    }
    return true;
}

/* Every step open to a player at rest in pose `p`, but for any that is only a way of falling. `out` needs MAX_MOVES. */
static int list_moves(const World *w, Pose p, Step *out)
{
    int n = every_move(w, p, out), kept = 0;
    for (int i = 0; i < n; i++)
        if (!drops_too_far(w, p, out[i])) out[kept++] = out[i];
    return kept;
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
static Step key_step_any(const World *w, Pose p, int dx, int dy)
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
        if (can_step_off(w, p.feet, dx)) return (Step){ (int8_t)dx, 0, false };
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
        /* Steps are gone up; with down held as well, walked through. */
        /* (With up held, always; otherwise only where they lead on to something.) */
        if (dy <= 0 && can_step_up(w, p.feet, dx) && (dy < 0 || steps_lead_on(w, p.feet, dx))) return (Step){ (int8_t)dx, -1, false };
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
        if (is_supported(w, (TilePos){ x + dx, y + 1 })) return (Step){ (int8_t)dx, 0, false };
        return (Step){ (int8_t)dx, 1, true };
    }
    /* In a running lift, up and down are the next floor that way. */
    if (dy != 0) {
        int16_t floors[2 * MAX_LIFT_STOPS];
        int stops = lift_stops(w, p.feet, floors);
        for (int i = 0; i < stops; i++)
            if ((floors[i] < 0) == (dy < 0)) return (Step){ 0, floors[i], false };
    }
    if (dy < 0 && body_fits(w, x, y - 1)) return (Step){ 0, -1, true };
    if (dy > 0 && hatch_below(w, p.feet)) return (Step){ 0, 1, true };
    return none;
}

/* (And never one that is only a way of falling.) */
static Step key_step(const World *w, Pose p, int dx, int dy)
{
    Step s = key_step_any(w, p, dx, dy);
    return drops_too_far(w, p, s) ? (Step){ 0, 0, false } : s;
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

/* Whether something at `target` can be hit from `feet`: within reach, or -
 * with something that shoots - within its range with nothing solid in the way. */
static bool in_attack_range(const World *w, TilePos feet, TilePos target, int range)
{
    if (in_use_range(w, feet, target)) return true;
    if (range <= 0) return false;
    int dx = world_dx(w, feet.x, target.x), dy = target.y - (feet.y - 1); /* from the head */
    if (dx * dx + dy * dy > range * range) return false;
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    for (int k = 1; k < steps; k++) {
        int x = feet.x + (int)lroundf((float)(k * dx) / (float)steps), y = feet.y - 1 + (int)lroundf((float)(k * dy) / (float)steps);
        if (world_is_solid(w, x, y)) return false;
    }
    return true;
}

/* Which layer a mine action on a tile takes its block from: the foreground
 * if there is a block there, otherwise the background; -1 if neither has
 * anything that can be mined. */
static int mine_layer(const World *w, TilePos t)
{
    TileType front = world_get(w, t.x, t.y);
    /* (Wire lies over the block it is put on: it comes off first.) */
    if (front != TILE_AIR && tile_def(world_get_at(w, LAYER_BACKGROUND, t.x, t.y))->wire) return LAYER_BACKGROUND;
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
        /* (Not one that is out of sight and out of reach behind a block: something buried, till it is dug out.) */
        if (l != LAYER_FOREGROUND && tile_def(world_get(w, x, y))->solid && !tile_def(world_get(w, x, y))->see_through) continue;
        if (layer) *layer = l;
        return t;
    }
    return TILE_AIR;
}

TileType game_bench_at(const Game *g, int x, int y)
{
    return usable_at(&g->world, x, y, NULL);
}

static bool spell_on_self(const ItemDef *it);
static void spell_on_mobs(Game *g, const ItemDef *it, int x, int y);
static void wear_spell(Game *g, ItemId item);

static bool is_goal(const World *w, Pose p, Action a)
{
    switch (a.type) {
    case ACTION_MINE:
        /* A background block can be mined from right in front of it. */
        if (!world_is_solid(w, a.target.x, a.target.y)) return in_use_range(w, p.feet, a.target);
        return in_reach(w, p.feet, a.target);
    case ACTION_PLACE:
    case ACTION_BUCKET:
    case ACTION_PAINT:
    case ACTION_RELEASE:
    case ACTION_PLANT: return in_reach(w, p.feet, a.target);
    case ACTION_ATTACK: return in_attack_range(w, p.feet, a.target, a.amount);
    case ACTION_CAST: {
        /* From where they stand, for a spell on themselves; else from as far off as the spell carries. */
        if (spell_on_self(item_def((ItemId)a.item))) return true;
        int dx = world_dx(w, p.feet.x, a.target.x), dy = a.target.y - p.feet.y, far = item_def((ItemId)a.item)->range;
        return in_reach(w, p.feet, a.target) || dx * dx + dy * dy <= far * far;
    }
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

/* Whether something in the background could hold on here to something else
 * in the background: a column on the one under it, steps on the steps a
 * step down from them. */
static bool beside_background(const World *w, TilePos t, TileType tile)
{
    static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    if (!tile_def(tile)->attaches) return false;
    for (int i = 0; i < 4; i++)
        if (tile_def(world_get_at(w, LAYER_BACKGROUND, t.x + dirs[i][0], t.y + dirs[i][1]))->attaches) return true;
    for (int dx = -1; tile_def(tile)->steps && dx <= 1; dx += 2)
        for (int dy = -1; dy <= 1; dy += 2)
            if (world_is_step(w, t.x + dx, t.y + dy)) return true;
    return false;
}

/* The block an item goes down as in a square: its usual one, or - for an item
 * that has one - another where it is being put down on top of a block. */
static TileType placed_tile(const World *w, ItemId item, TilePos t)
{
    const ItemDef *it = item_def(item);
    if (it->places_on_block != TILE_AIR && world_is_solid(w, t.x, t.y + 1)) return it->places_on_block;
    return it->places;
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
    /* Something that crosses with what is growing there can go in beside it. */
    if (soil && it->crosses_with != TILE_AIR && world_get_at(w, layer, t.x, t.y - 1) == it->crosses_with) return true;
    return soil && world_get_at(w, layer, t.x, t.y - 1) == TILE_AIR && !world_is_solid(w, t.x, t.y - 1);
}

/* The colours of a flower bred from two. One of its two features, body or
 * tips, is a mix of its parents' there; the other is one parent's or the
 * other's, as it falls out. */
static int cross_tones(Game *g, int a, int b)
{
    int part[2][2], child[2];
    tone_unpack(a, &part[0][0], &part[0][1]);
    tone_unpack(b, &part[1][0], &part[1][1]);
    int crossed = (int)(game_random(g) >> 8 & 1u);
    unsigned char one[3], other[3];
    tone_rgb(part[0][crossed], one);
    tone_rgb(part[1][crossed], other);
    int mixed[3];
    for (int k = 0; k < 3; k++) {
        /* Halfway between; where that falls between two shades, either. */
        int step = 255 / (TONE_LEVELS - 1), sum = one[k] / step + other[k] / step;
        mixed[k] = (sum / 2 + (sum % 2 ? (int)(game_random(g) >> 8 & 1u) : 0)) * step;
    }
    child[crossed] = tone_nearest(mixed[0], mixed[1], mixed[2]);
    child[!crossed] = part[game_random(g) >> 8 & 1u][!crossed];
    return tone_pack(child[0], child[1]);
}

/* What the two colours of a block are: the number kept with it, or, with
 * none kept, what a new one of the item it leaves would be. */
static int tile_tones(const World *w, int layer, int x, int y)
{
    int meta = world_meta(w, x, y);
    const TileDef *def = tile_def(world_get_at(w, layer, x, y));
    if (meta != 0 || !def->two_tone) return meta;
    if (item_def(def->drop)->two_tone) return item_def(def->drop)->meter_start;
    return tone_pack(0, 0);
}

/* Puts down what a planted item grows into: a tree's first trunk block, or a
 * plant's first stage. */
static void plant_item(Game *g, World *w, ItemId item, TilePos soil, int tones)
{
    const ItemDef *it = item_def(item);
    if (it->plants >= 0) {
        tree_plant(w, soil.x, soil.y - 1, it->plants);
    } else if (it->sows != TILE_AIR) {
        int layer = tile_def(it->sows)->layer;
        if (it->crosses_with != TILE_AIR && world_get_at(w, layer, soil.x, soil.y - 1) == it->crosses_with) {
            /* In beside the other half of a cross: the two become one plant, bred from both. */
            /* (Only in the world itself: a plan of what will be does no breeding.) */
            int bred = w == &g->world ? cross_tones(g, tile_tones(w, layer, soil.x, soil.y - 1), tones) : 0;
            world_set_at(w, layer, soil.x, soil.y - 1, it->crosses_into);
            world_set_meta(w, soil.x, soil.y - 1, bred);
            return;
        }
        world_set_at(w, layer, soil.x, soil.y - 1, it->sows);
        /* A two-tone thing grows into a flower of its own colours. */
        if (it->two_tone && tile_def(it->sows)->two_tone) world_set_meta(w, soil.x, soil.y - 1, tones);
    }
}

static void place_tile(World *w, TilePos t, TileType tile);
static void replace_block(World *w, int layer, TilePos t, TileType with);
static bool room_for(const World *w, TilePos t, TileType tile);

/* What paint would go on in a square: the block in front if there is a solid
 * one (0), else the backwall if there is any (1); -1 for nothing to paint. */
static int paintable(const World *w, TilePos t)
{
    if (!world_in_bounds(w, t.x, t.y)) return -1;
    if (world_get(w, t.x, t.y) != TILE_AIR && world_is_solid(w, t.x, t.y)) return 0;
    return world_get_at(w, LAYER_WALL, t.x, t.y) != TILE_AIR ? 1 : -1;
}

/* Whether the target tile is still something the action can be done to. */
static bool target_valid(const World *w, Action a)
{
    if (!world_in_bounds(w, a.target.x, a.target.y)) return false;
    /* A drop or a creature is checked on arrival, not here. */
    if (a.type == ACTION_COLLECT || a.type == ACTION_ATTACK) return true;
    TileType t = world_get(w, a.target.x, a.target.y);
    switch (a.type) {
    case ACTION_MINE: return action_layer(w, a) >= 0;
    case ACTION_PLACE: {
        TileType placed = placed_tile(w, (ItemId)a.item, a.target);
        if (placed == TILE_AIR || !room_for(w, a.target, placed)) return false;
        /* Something that hangs goes under a block and nowhere else, backwall or no. */
        if (tile_def(placed)->hangs) return world_is_solid(w, a.target.x, a.target.y - 1);
        /* A boat goes on the top of water and nowhere else. */
        if (tile_def(placed)->afloat)
            return world_is_water(w, a.target.x, a.target.y) && !world_is_water(w, a.target.x, a.target.y - 1);
        /* A window goes on a backwall: it is a hole in one, and there has to be one to make a hole in. */
        if (tile_def(placed)->window) return bare_wall(w, a.target) != TILE_AIR;
        /* Something that lays its own track stands on that: room for it is all it needs. */
        if (tile_def(placed)->lays != TILE_AIR) return true;
        /* And something that floats, anywhere there is room for it. */
        if (tile_def(placed)->floats) return true;
        if (tile_def(placed)->layer == LAYER_BACKGROUND &&
            (bare_wall(w, a.target) != TILE_AIR || beside_background(w, a.target, placed)))
            return true;
        /* With no backwall it is fixed to a block: under it, over it or beside
         * it. A column under it does as well as a block. */
        if (tile_def(placed)->layer == LAYER_BACKGROUND &&
            tile_def(world_get_at(w, LAYER_BACKGROUND, a.target.x, a.target.y + 1))->supports)
            return true;
        /* (Something two blocks wide is held by a block beside either half of it.) */
        if (tile_def(placed)->pair != TILE_AIR && beside_block(w, (TilePos){ a.target.x + 1, a.target.y })) return true;
        return beside_block(w, a.target);
    }
    case ACTION_PLANT: return can_plant(w, (ItemId)a.item, a.target);
    case ACTION_PAINT: return paintable(w, a.target) >= 0;
    /* A creature is let out where one could stand: open, with ground under it. */
    case ACTION_RELEASE:
        return !world_is_solid(w, a.target.x, a.target.y) && world_is_solid(w, a.target.x, a.target.y + 1) && !world_is_water(w, a.target.x, a.target.y);
    case ACTION_USE: return usable_at(w, a.target.x, a.target.y, NULL) != TILE_AIR;
    case ACTION_CAST: return true;
    default: return !tile_def(t)->solid || tile_def(t)->passable; /* (somewhere to go to: a door will do) */
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
    return p.climbing && buoyed(w, p.feet) && !is_supported(w, p.feet);
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
        /* (Someone who can fly goes straight from where they stand, too, if the way starts upward.) */
        if (pose_afloat(w, at[i]) || (w->aloft && buoyed(w, at[i].feet))) {
            int far = i + SWIM_LOOKAHEAD < n ? i + SWIM_LOOKAHEAD : n;
            for (int j = far; j > i + 1 && reach < 0; j--)
                if (buoyed(w, at[j].feet) && swim_line(w, at[i], at[j], j - i, line, &length)) reach = j;
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
        place_tile(&g->sim, a.target, placed_tile(&g->sim, (ItemId)a.item, a.target));
    else if (a.type == ACTION_PLANT)
        plant_item(g, &g->sim, (ItemId)a.item, a.target, 0);
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
    /* (And the numbers kept with squares: a lift is planned for as it stands, running or not.) */
    memcpy(g->sim.meta, g->world.meta, (size_t)g->world.width * g->world.height * sizeof(uint16_t));
    for (int layer = 0; layer < LAYER_COUNT; layer++)
        memcpy(g->sim.layers[layer], g->world.layers[layer],
               (size_t)g->world.width * g->world.height * sizeof(uint16_t));
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
    if (!game_alive(g)) return false; /* nobody there to do it */
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
    if (g->ride) return false;
    set_aloft(g);
    Action a = { .type = type, .target = { world_wrap_x(&g->world, tx), ty } };
    if (uses_item(a)) a.item = inventory_selected(&g->inventory)->id;
    return queue_checked(g, sim_pending(g), a);
}

/* Whether what is in hand would set fire to the block an action is at. */
static bool sets_fire(const Game *g, const World *w, TilePos t)
{
    const ItemStack *held = inventory_selected(&g->inventory);
    int layer = mine_layer(w, t);
    return held->count > 0 && item_def((ItemId)held->id)->ignites && layer >= 0 && tile_def(world_get_at(w, layer, t.x, t.y))->burns;
}

uint16_t game_relic_box(Game *g)
{
    if (!content.has_relics) return 0;
    Inventory *inv = &g->inventory;
    if (g->relic_box > 0 && g->relic_box < MAX_CONTAINERS && inv->containers[g->relic_box].used == CONTAINER_RELICS) return g->relic_box;
    g->relic_box = container_alloc(inv);
    if (g->relic_box != 0) inv->containers[g->relic_box].used = CONTAINER_RELICS;
    return g->relic_box;
}

/* What the live character has on: armour, then relics. Returns how many. */
static int worn(const Game *g, const ItemDef *out[ARMOR_SLOTS + RELIC_SLOTS])
{
    int n = 0;
    for (int i = 0; i < ARMOR_SLOTS; i++)
        if (g->inventory.armor[i].count > 0) out[n++] = item_def((ItemId)g->inventory.armor[i].id);
    uint16_t box = g->relic_box;
    if (box == 0 || box >= MAX_CONTAINERS || g->inventory.containers[box].used != CONTAINER_RELICS) return n;
    for (int i = 0; i < RELIC_SLOTS; i++) {
        const ItemStack *s = &g->inventory.containers[box].slots[i];
        if (s->count > 0 && item_def((ItemId)s->id)->relic) out[n++] = item_def((ItemId)s->id);
    }
    return n;
}

int game_set_pieces(const Game *g, int set)
{
    const ItemDef *on[ARMOR_SLOTS + RELIC_SLOTS];
    int n = worn(g, on), pieces = 0;
    for (int i = 0; i < n; i++) pieces += set >= 0 && on[i]->set == set;
    return pieces;
}

static void add_boons(Boons *all, const Boons *b)
{
    if (b->walk > 0.0f) all->walk *= b->walk;
    if (b->mine > 0.0f) all->mine *= b->mine;
    if (b->craft > 0.0f) all->craft *= b->craft;
    if (b->regen > 0.0f) all->regen *= b->regen;
    if (b->lung > 0.0f) all->lung *= b->lung;
    if (b->appetite > 0.0f) all->appetite *= b->appetite;
    if (b->stamina > 0.0f) all->stamina *= b->stamina;
    all->armor += b->armor, all->warmth += b->warmth, all->luck += b->luck;
    all->strength += b->strength, all->thorns += b->thorns;
}

Boons game_boons(const Game *g)
{
    Boons all = { .walk = 1.0f, .mine = 1.0f, .craft = 1.0f, .regen = 1.0f, .lung = 1.0f, .appetite = 1.0f, .stamina = 1.0f };
    const ItemDef *on[ARMOR_SLOTS + RELIC_SLOTS];
    int n = worn(g, on);
    for (int i = 0; i < n; i++) add_boons(&all, &on[i]->boons);
    /* Enough of a set on: what that gives besides - every bonus the number reaches. */
    for (int s = 0; s < content.set_count && n > 0; s++) {
        int pieces = 0;
        for (int i = 0; i < n; i++) pieces += on[i]->set == s;
        for (int t = 0; t < content.sets[s].tier_count; t++)
            if (pieces >= content.sets[s].tiers[t].pieces) add_boons(&all, &content.sets[s].tiers[t].boons);
    }
    if (all.luck > 1.0f) all.luck = 1.0f;
    return all;
}

int game_boon_lines(const Boons *b, char lines[][BOON_LINE], int max)
{
    int n = 0;
#define LINE(...) do { if (n < max) snprintf(lines[n++], BOON_LINE, __VA_ARGS__); } while (0)
#define MORE(x) ((int)(((x) - 1.0f) * 100.0f + 0.5f))
#define LESS(x) ((int)((1.0f - (x)) * 100.0f + 0.5f))
    if (b->walk > 0.0f && b->walk != 1.0f) LINE("%d%% faster on foot", MORE(b->walk));
    if (b->mine > 0.0f && b->mine != 1.0f) LINE("%d%% faster digging", MORE(b->mine));
    if (b->craft > 0.0f && b->craft != 1.0f) LINE("%d%% faster at a bench", MORE(b->craft));
    if (b->strength > 0) LINE("+%d damage", b->strength);
    if (b->armor > 0.0f) LINE("%d%% of attacks stopped", (int)(b->armor * 100.0f + 0.5f));
    if (b->thorns > 0) LINE("%d thorns", b->thorns);
    if (b->warmth > 0.0f) LINE("%d%% of environment kept", (int)(b->warmth * 100.0f + 0.5f));
    if (b->regen > 0.0f && b->regen != 1.0f) LINE("%d%% faster mending", MORE(b->regen));
    if (b->lung > 0.0f && b->lung != 1.0f) LINE("%d%% more breath", MORE(b->lung));
    if (b->appetite > 0.0f && b->appetite != 1.0f) LINE("Hunger %d%% slower", LESS(b->appetite));
    if (b->stamina > 0.0f && b->stamina != 1.0f) LINE("Tiredness %d%% slower", LESS(b->stamina));
    if (b->luck > 0.0f) LINE("%d%% luck", (int)(b->luck * 100.0f + 0.5f));
#undef LINE
#undef MORE
#undef LESS
    return n;
}

int game_item_lines(ItemId item, char lines[][BOON_LINE], int max)
{
    if (item <= ITEM_NONE || item >= content.item_count) return 0;
    const ItemDef *def = item_def(item);
    /* What it does of itself, in the older fields armour has, and then its boons: all said the same way. */
    Boons all = def->boons;
    if (def->walk_speed > 0.0f) all.walk = (all.walk > 0.0f ? all.walk : 1.0f) * def->walk_speed;
    all.armor += def->armor;
    all.warmth += def->warmth;
    int n = game_boon_lines(&all, lines, max);
    if (def->sated && n < max) snprintf(lines[n++], BOON_LINE, "Never hungry");
    if (def->shelters && n < max) snprintf(lines[n++], BOON_LINE, "The weather cannot touch you");
    if (def->lifts && n < max) snprintf(lines[n++], BOON_LINE, "Flight");
    return n;
}

bool game_identifies(const Game *g)
{
    const Inventory *inv = &g->inventory;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        const ItemStack *st = &inv->slots[i];
        if (st->count == 0) continue;
        if (item_def((ItemId)st->id)->identifies) return true;
        if (st->container == 0 || st->container >= MAX_CONTAINERS || inv->containers[st->container].used != CONTAINER_ITEMS) continue;
        for (int k = 0; k < CONTAINER_MAX_SLOTS; k++)
            if (inv->containers[st->container].slots[k].count > 0 && item_def((ItemId)inv->containers[st->container].slots[k].id)->identifies) return true;
    }
    return false;
}

float game_mine_time(const Game *g, TileType t)
{
    const TileDef *def = tile_def(t);
    return def->mine_time / inventory_tool_speed(&g->inventory, def->tool_class) / game_boons(g).mine;
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
    if (g->player.activity == ACTIVITY_MINING || g->player.activity == ACTIVITY_PLACING ||
        g->player.activity == ACTIVITY_ATTACKING)
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
    /* (With a bow in hand, from as far off as it shoots. With something that
     * shears in hand, a creature that can be shorn is shorn and not hit.) */
    const ItemStack *held = inventory_selected(&g->inventory);
    bool shear = held->count > 0 && item_def((ItemId)held->id)->shears && content.mobs[m->kind].shear_item != ITEM_NONE;
    bool milk = held->count > 0 && item_def((ItemId)held->id)->milks && content.mobs[m->kind].milk_item != ITEM_NONE;
    bool cage = held->count > 0 && item_def((ItemId)held->id)->cages && content.mobs[m->kind].caged != ITEM_NONE;
    Action a = { .type = ACTION_ATTACK, .target = { m->x, m->y }, .drop = mob, .drop_id = m->id,
                 .amount = shear || milk || cage ? 0 : game_attack_range(g),
                 .arg = shear ? HAND_SHEAR : milk ? HAND_MILK : cage ? HAND_CAGE : HAND_HIT };
    return queue_checked(g, sim_pending(g), a);
}

bool game_unqueue_tile(Game *g, int tx, int ty)
{
    return unqueue(g, match_tile, (TilePos){ world_wrap_x(&g->world, tx), ty }, 0);
}

bool game_can_collect(const Game *g, int drop)
{
    static Inventory tried; /* (the whole of what they carry, to try it in: too big to keep on the stack) */
    if (drop < 0 || drop >= MAX_DROPS || !g->drops[drop].active) return false;
    const Drop *d = &g->drops[drop];
    if (item_def(d->item)->crystals > 0) return true;
    tried = g->inventory;
    return inventory_add_stack(&tried, (ItemStack){ d->item, 1, 0 }) == 0;
}

bool game_click_drop(Game *g, int drop)
{
    if (unqueue(g, match_drop, (TilePos){ 0, 0 }, drop)) return true;
    return game_queue_collect(g, drop);
}

bool game_click_tile(Game *g, int tx, int ty)
{
    /* Riding, there is nowhere to go and nothing in reach: getting out comes first. */
    if (g->ride) return false;
    /* Nor is anything to be done with what has not been seen. */
    if (game_explored(g, tx, ty) == 0) return false;
    set_aloft(g);
    if (game_unqueue_tile(g, tx, ty)) return true;
    /* Something that runs on rails, in hand, and track here near enough to reach: it is put on it. */
    if (inventory_selected(&g->inventory)->count > 0 && item_def((ItemId)inventory_selected(&g->inventory)->id)->vehicle.is &&
        train_place(g, (ItemId)inventory_selected(&g->inventory)->id, tx, ty) >= 0)
        return true;

    Pose pos = sim_pending(g);
    tx = world_wrap_x(&g->world, tx);
    Action a = { .type = ACTION_MOVE, .target = { tx, ty } };
    const ItemStack *held = inventory_selected(&g->inventory);

    Action plant = { .type = ACTION_PLANT, .target = a.target, .item = held->id };
    /* Holding a bucket: water is scooped up into it, and poured out where there is room for it. */
    int holds = held->count > 0 && item_def((ItemId)held->id)->holds_water ? inventory_meter(&g->inventory, held) : -1;
    int wet = world_water(&g->world, tx, ty);
    const ItemDef *spell = item_def(held->count > 0 ? (ItemId)held->id : ITEM_NONE);
    if (spell->casts && world_in_bounds(&g->world, tx, ty) &&
        (spell->cast_water > 0 || spell->cast_dig > 0 || spell->cast_shield > 0.0f || spell->cast_time >= 0)) {
        /* A spell in hand that is for a place, or for the one casting it: it is cast. */
        a.type = ACTION_CAST;
        a.item = held->id;
    } else if (holds >= 0 && world_in_bounds(&g->world, tx, ty) && !world_is_solid(&g->sim, tx, ty) &&
        ((wet > 0 && holds < item_def((ItemId)held->id)->meter_max) || holds > 0)) {
        a.type = ACTION_BUCKET;
        a.item = held->id;
    } else if (held->count > 0 && (item_def((ItemId)held->id)->paints || item_def((ItemId)held->id)->strips) &&
               paintable(&g->sim, a.target) >= 0) {
        /* Paint, or what takes it off, in hand: the block or the backwall here is done, not dug. */
        a.type = ACTION_PAINT;
        a.item = held->id;
    } else if (held->count > 0 && item_def((ItemId)held->id)->releases >= 0) {
        /* A creature in a cage: it is let out here, if here is somewhere it could stand. */
        Action out = { .type = ACTION_RELEASE, .target = a.target, .item = held->id };
        if (target_valid(&g->sim, out)) a = out;
    } else if (held->count > 0 && can_plant(&g->sim, (ItemId)held->id, a.target)) {
        /* Holding something that grows: soil with room above is where it is planted. */
        a = plant;
    } else if (held->count > 0 && item_def((ItemId)held->id)->crosses_with != TILE_AIR &&
               world_get_at(&g->sim, tile_def(item_def((ItemId)held->id)->crosses_with)->layer, tx, ty) ==
                   item_def((ItemId)held->id)->crosses_with &&
               can_plant(&g->sim, (ItemId)held->id, (TilePos){ tx, ty + 1 })) {
        /* Onto the very plant it crosses with: it goes into the soil under that. */
        a = plant;
        a.target.y = ty + 1;
    } else if (world_lift_runs(&g->sim, tx, ty) && !blocks_body(&g->sim, tx, ty)) {
        /* A floor of a lift that is running is somewhere to go, not something
         * to take down. (With its power off, the shaft is mined like anything.) */
    } else if (held->count > 0 && tile_def(item_def((ItemId)held->id)->places)->wire && world_in_bounds(&g->world, tx, ty) &&
               !world_is_wired(&g->sim, tx, ty) &&
               target_valid(&g->sim, (Action){ .type = ACTION_PLACE, .target = a.target, .item = held->id })) {
        /* Wire in hand: it goes on whatever is here - a block, a bench, a backwall - rather than that being dug out. */
        a.type = ACTION_PLACE;
        a.item = held->id;
    } else if (world_in_bounds(&g->world, tx, ty) && mine_layer(&g->sim, a.target) >= 0 &&
               ((tile_def(world_get_at(&g->sim, mine_layer(&g->sim, a.target), tx, ty))->needs_tool &&
                 (held->count <= 0 || item_def((ItemId)held->id)->tool_class < 0 ||
                  item_def((ItemId)held->id)->tool_class != tile_def(world_get_at(&g->sim, mine_layer(&g->sim, a.target), tx, ty))->tool_class)) ||
                train_at(g, tx, ty) >= 0)) {
        /* Something that only comes up with its tool, and that not in hand - or track with something standing on it:
         * the click is a click on the square, and they go to it. */
    } else if (world_in_bounds(&g->world, tx, ty) && mine_layer(&g->sim, a.target) >= 0) {
        /* A block here, in front or behind, is mined - or, with fire in hand and the block one that burns, burnt down. */
        a.type = ACTION_MINE;
        if (sets_fire(g, &g->sim, a.target)) a.item = held->id;
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
static TilePos character_feet(const Game *g, int c);
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
    return spawn_drop_stack(g, (ItemStack){ (uint16_t)item, 1, 0 }, at);
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

void game_poison(Game *g, int ticks)
{
    if (g->vitals.respawn > 0 || ticks <= 0) return;
    game_wake(g);
    if (g->vitals.poison < ticks) g->vitals.poison = ticks;
}

int game_attack_range(const Game *g)
{
    const ItemStack *held = inventory_selected(&g->inventory);
    return held->count > 0 ? item_def(held->id)->range : 0;
}

int game_attack_damage(const Game *g)
{
    const ItemStack *held = inventory_selected(&g->inventory);
    int with = held->count > 0 ? item_def(held->id)->damage : 0;
    return (with > content.rules.hand_damage ? with : content.rules.hand_damage) + game_boons(g).strength;
}

bool game_player_in_water(const Game *g)
{
    return in_water(&g->world, g->player.feet);
}

void game_drop_item_sized(Game *g, ItemId id, int x, int y, int size)
{
    ItemStack one = { (uint16_t)id, 1, 0 };
    if (item_def(id)->meter_max > 0) inventory_set_meter(&g->inventory, &one, size);
    spawn_drop_stack(g, one, (TilePos){ world_wrap_x(&g->world, x), y });
}

void game_drop_stack(Game *g, ItemStack stack, int x, int y)
{
    /* (A drop is one of a thing: so many of them, the first with whatever is inside.) */
    for (int i = 0; i < stack.count; i++) spawn_drop_stack(g, (ItemStack){ stack.id, 1, i == 0 ? stack.container : 0 }, (TilePos){ x, y });
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
    return (def->storage_columns > 0 && !def->storage_shared) || def->fuel_capacity > 0 ||
           /* (Anything that makes, holds or uses electricity keeps count of it.) */
           def->gen_power > 0 || def->battery > 0 || def->electric > 0 || def->motor_power > 0 || def->draw > 0 || def->extractor || def->net_role == NET_BRIDGE || def->net_role == NET_ACCESS;
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
        *s = (ItemStack){ (uint16_t)item, (uint8_t)n, 0 };
        count -= n;
    }
}

int game_area_pools(const Game *g, int x, int y, int *out)
{
    const World *w = &g->world;
    int n = 0, biome = world_biome(w, x), height = world_row_to_y(w, y);
    for (int i = 0; i < content.pool_count; i++) {
        const PoolDef *pool = &content.pools[i];
        bool here = pool->any_biome;
        for (int k = 0; k < pool->biome_count && !here; k++) here = pool->biomes[k] == biome;
        if (!here || height < pool->y_min || height > pool->y_max || pool->loot_count == 0) continue;
        if (out) out[n] = i;
        n++;
    }
    return n;
}

ItemId game_area_find(Game *g, int x, int y)
{
    int pools[MAX_POOLS], n = game_area_pools(g, x, y, pools), total = 0;
    for (int i = 0; i < n; i++)
        for (int k = 0; k < content.pools[pools[i]].loot_count; k++) total += content.pools[pools[i]].loot[k].weight;
    if (total <= 0) return ITEM_NONE;
    int roll = (int)(game_random(g) % (uint32_t)total);
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < content.pools[pools[i]].loot_count; k++) {
            if (roll < content.pools[pools[i]].loot[k].weight) return content.pools[pools[i]].loot[k].item;
            roll -= content.pools[pools[i]].loot[k].weight;
        }
    }
    return ITEM_NONE;
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
        if (l->area) {
            /* Whatever is found in this place: so many of them, each picked afresh. */
            for (int n = 0; n < count; n++) {
                ItemId found = game_area_find(g, t.x, t.y);
                if (found != ITEM_NONE) container_put(&g->inventory, c, found, 1);
            }
            continue;
        }
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

/* Whether a drop is within a character's reach to be picked up without asking. */
static bool drop_in_reach(const Game *g, const Drop *d, TilePos feet)
{
    int dx = world_dx(&g->world, feet.x, d->x), dy = (int)(d->y + 0.5f) - feet.y;
    return dx >= -1 && dx <= 1 && dy >= -2 && dy <= 1;
}

/* The live character picks up what is lying within their reach. */
static void collect_nearby(Game *g)
{
    if (!game_alive(g)) return;
    for (int i = 0; i < MAX_DROPS; i++) {
        Drop *d = &g->drops[i];
        if (!d->active || d->collect > 0.0f || d->attached || d->held_off || d->age < DROP_PICKUP_DELAY) continue;
        if (drop_in_reach(g, d, g->player.feet)) collect_drop(g, d);
    }
}

static void update_drops(Game *g, float dt)
{
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

        const World *w = &g->world;
        float lift = content.rules.drop_float_speed;
        int row = (int)d->y;
        if (lift > 0.0f && world_is_water(w, d->x, row)) {
            /* In water it floats: up to the top of the water it is in, and there it stays. */
            int top = row;
            while (top > 0 && world_is_water(w, d->x, top - 1)) top--;
            if (d->y > (float)top) {
                d->y -= lift * dt;
                if (d->y < (float)top) d->y = (float)top;
            }
        } else {
            /* Otherwise it falls, until something holds it up - ground, or water to float on. */
            int rest = row;
            while (!world_is_solid(w, d->x, rest + 1) && !(lift > 0.0f && world_is_water(w, d->x, rest + 1))) rest++;
            if (!world_is_solid(w, d->x, rest + 1)) rest++; /* into the top of the water */
            d->y += DROP_FALL_SPEED * dt;
            if (d->y > (float)rest) d->y = (float)rest;
        }

        /* Something put down on purpose stays down until everybody has been away from it. */
        bool in_reach = false;
        for (int c = 0; c < g->character_count; c++) in_reach = in_reach || drop_in_reach(g, d, character_feet(g, c));
        if (d->held_off && !in_reach) d->held_off = false;
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
    /* Flying is the same through the air, and quicker. */
    bool fly = !swim && g->world.aloft && buoyed(&g->world, p->feet) && buoyed(&g->world, to) && !is_supported(&g->world, to) && content.rules.fly_speed > 0.0f;
    p->step_swim = swim || fly;
    /* (A ride in a lift: quick, and so much a block.) */
    bool ride = s.dx == 0 && !s.climb && !p->climbing && (s.dy < -1 || s.dy > 1 || (s.dy != 0 && world_is_shaft(&g->world, to.x, to.y)));
    /* In a boat, across the top of the water: quicker than swimming, and the boat comes too. */
    bool boat = s.dy == 0 && (s.dx == 1 || s.dx == -1) && tile_def(world_get(&g->world, p->feet.x, p->feet.y))->afloat &&
                world_get(&g->world, to.x, to.y) == TILE_AIR && world_is_water(&g->world, to.x, to.y) && !world_is_water(&g->world, to.x, to.y - 1);
    p->step_boat = boat;
    if (boat) p->duration = content.rules.swim_time / (content.rules.boat_speed > 0.0f ? content.rules.boat_speed : 2.0f);
    else if (ride) p->duration = (content.rules.lift_time > 0.0f ? content.rules.lift_time : 0.04f) * (float)abs(s.dy);
    else if (hop) p->duration = HOP_TIME / game_walk_speed(g) / game_pace(g);
    /* (So many times the pace of a walk - the plain pace, not what boots make of it.) */
    else if (fly) p->duration = WALK_TIME / content.rules.fly_speed * (s.dx != 0 && s.dy != 0 ? 1.4142f : 1.0f) / game_pace(g);
    else if (swim) p->duration = content.rules.swim_time * (s.dx != 0 && s.dy != 0 ? 1.4142f : 1.0f) / game_pace(g);
    else if (s.climb || p->climbing) p->duration = CLIMB_TIME / game_pace(g);
    else if (s.dy > 0) p->duration = FALL_TIME;
    else p->duration = (crawl ? CRAWL_TIME : WALK_TIME) / game_walk_speed(g) / game_pace(g); /* (quicker in boots that make it so) */
    if (s.dx != 0) p->facing = s.dx > 0 ? 1 : -1;
}

/* Puts a block in the world, with a backwall behind it if it leaves one and
 * there is none there already. */
static void place_tile(World *w, TilePos t, TileType tile)
{
    /* A tall block goes up as a column of itself from where it is put. */
    for (int i = 0; i < tile_def(tile)->height; i++)
        world_set_at(w, tile_def(tile)->layer, t.x, t.y - i, tile);
    /* A door takes its look now, from what it is put between, and keeps it. */
    for (int i = 0; tile_def(tile)->mimics != MIMIC_NONE && i < tile_def(tile)->height; i++)
        if (world_meta(w, t.x, t.y - i) == 0) world_fix_look(w, t.x, t.y - i);
    if (tile_def(tile)->pair != TILE_AIR) world_set_at(w, tile_def(tile_def(tile)->pair)->layer, t.x + 1, t.y, tile_def(tile)->pair);
    for (int dx = -1; tile_def(tile)->lays != TILE_AIR && dx <= 1; dx++)
        world_set_at(w, tile_def(tile_def(tile)->lays)->layer, t.x + dx, t.y + 1, tile_def(tile)->lays);
    if (tile_def(tile)->wall && world_get_at(w, LAYER_WALL, t.x, t.y) == TILE_AIR)
        world_set_at(w, LAYER_WALL, t.x, t.y, tile);
    /* Put up not far above another block, it has backwall of itself run
     * down to that block through the open space between: a roof over a floor closes in the room. */
    for (int k = 1; tile_def(tile)->wall && tile_def(tile)->solid && k <= content.rules.wall_span; k++) {
        if (!world_in_bounds(w, t.x, t.y + k)) break;
        if (!world_is_solid(w, t.x, t.y + k)) continue;
        for (int y = t.y + 1; y < t.y + k; y++)
            if (world_get_at(w, LAYER_WALL, t.x, y) == TILE_AIR) world_set_at(w, LAYER_WALL, t.x, y, tile);
        break;
    }
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
    /* Wire goes on a block as well as anywhere else: in the same square, over the face of it. */
    if (tile_def(tile)->wire && tile_def(tile)->layer != LAYER_FOREGROUND)
        return world_in_bounds(w, t.x, t.y) && world_get_at(w, tile_def(tile)->layer, t.x, t.y) == TILE_AIR;
    for (int i = 0; i < tile_def(tile)->height; i++) {
        if (!world_in_bounds(w, t.x, t.y - i) || world_get(w, t.x, t.y - i) != TILE_AIR || world_is_solid(w, t.x, t.y - i) ||
            world_get_at(w, tile_def(tile)->layer, t.x, t.y - i) != TILE_AIR)
            return false;
    }
    /* Something that only goes on a block needs one under it. */
    if (tile_def(tile)->on_floor && !world_is_solid(w, t.x, t.y + 1)) return false;
    /* Something that lays track under itself needs room for three pieces of it, each on a block - or the track there already. */
    TileType lays = tile_def(tile)->lays;
    for (int dx = -1; lays != TILE_AIR && dx <= 1; dx++) {
        TileType there = world_get_at(w, tile_def(lays)->layer, t.x + dx, t.y + 1);
        if (!world_in_bounds(w, t.x + dx, t.y + 2) || world_is_solid(w, t.x + dx, t.y + 1) || !world_is_solid(w, t.x + dx, t.y + 2) ||
            (there != TILE_AIR && there != lays))
            return false;
    }
    /* Something two blocks wide needs the square to its right as well, and level ground under both. */
    TileType other = tile_def(tile)->pair;
    if (other != TILE_AIR) {
        if (world_get(w, t.x + 1, t.y) != TILE_AIR || world_get_at(w, tile_def(other)->layer, t.x + 1, t.y) != TILE_AIR) return false;
        if (!tile_def(tile)->pair_floats && (!world_is_solid(w, t.x, t.y + 1) || !world_is_solid(w, t.x + 1, t.y + 1))) return false;
    }
    return true;
}

/* Leaves what a removed block leaves: `count` of its main drop, the first
 * carrying `inside`, and whatever else it gives. */
static void drop_block_items_toned(Game *g, const TileDef *def, TilePos at, uint16_t inside, int count, int tones);

static void drop_block_items(Game *g, const TileDef *def, TilePos at, uint16_t inside, int count)
{
    drop_block_items_toned(g, def, at, inside, count, 0);
}

/* (`tones`: the colours of a two-tone block, which what it leaves keeps.) */
static void drop_block_items_toned(Game *g, const TileDef *def, TilePos at, uint16_t inside, int count, int tones)
{
    for (int i = 0; i < count; i++) {
        /* Something rotten does not always leave anything. (What a block
         * holds inside it is never lost that way.) */
        bool kept = def->drop_chance >= 1.0f || (i == 0 && inside != 0) ||
                    (float)(game_random(g) >> 8) / 16777216.0f < def->drop_chance;
        if (!kept) continue;
        /* (A block that is one size of a thing leaves that thing at that size.) */
        if (def->drop_meter > 0 && inside == 0) game_drop_item_sized(g, def->drop, at.x, at.y, def->drop_meter);
        else if (tones > 0 && (item_def(def->drop)->two_tone || item_def(def->drop)->written)) game_drop_item_sized(g, def->drop, at.x, at.y, tones);
        else spawn_drop_stack(g, (ItemStack){ (uint16_t)def->drop, 1, i == 0 ? inside : 0 }, at);
    }
    for (int k = 0; k < def->extra_drop_count; k++) {
        if (def->extra_chance[k] < 1.0f && (float)(game_random(g) >> 8) / 16777216.0f >= def->extra_chance[k]) continue;
        for (int i = 0; i < def->extra_drops[k].count; i++) {
            if (tones > 0 && item_def(def->extra_drops[k].id)->two_tone)
                game_drop_item_sized(g, def->extra_drops[k].id, at.x, at.y, tones);
            else spawn_drop(g, def->extra_drops[k].id, at);
        }
    }
}

static void begin_work(Game *g, Activity activity, float duration)
{
    g->player.activity = activity;
    g->player.timer = 0.0f;
    /* They turn to what they are working on. (Until now they went on facing whichever way they had last walked:
     * so, having come to a block from the far side of it, or climbed to it, they dug or built with their back to it.) */
    if (g->has_current) {
        int across = world_dx(&g->world, g->player.feet.x, world_wrap_x(&g->world, g->current.target.x));
        if (across != 0) g->player.facing = across > 0 ? 1 : -1;
    }
    /* (The weather having got to them, it takes longer.) */
    g->player.duration = duration / game_pace(g);
}

static void finish_activity(Game *g)
{
    Player *p = &g->player;
    if (p->activity == ACTIVITY_STEPPING) {
        /* A step across the seam lands on the far side of the world. */
        TilePos from = p->feet;
        p->feet = (TilePos){ world_wrap_x(&g->world, p->step_to.x), p->step_to.y };
        /* The boat they were in has come with them, if where they have got to is still open water. */
        if (p->step_boat && tile_def(world_get(&g->world, from.x, from.y))->afloat && world_get(&g->world, p->feet.x, p->feet.y) == TILE_AIR) {
            TileType boat = world_get(&g->world, from.x, from.y);
            world_set(&g->world, from.x, from.y, TILE_AIR);
            world_set(&g->world, p->feet.x, p->feet.y, boat);
        }
        p->step_boat = false;
        game_reveal(g, p->feet.x, p->feet.y - 1);
        p->step_to = p->feet;
        p->climbing = p->step.climb;
    } else if (p->activity == ACTIVITY_MINING) {
        TilePos t = g->current.target;
        int layer = action_layer(&g->world, g->current);
        /* The right half of something two blocks wide: it is the whole thing that comes up, from its left. */
        if (layer >= 0 && layer != LAYER_WALL && tile_def(world_get_at(&g->world, layer, t.x, t.y))->pair_of != TILE_AIR &&
            world_get_at(&g->world, layer, t.x - 1, t.y) == tile_def(world_get_at(&g->world, layer, t.x, t.y))->pair_of)
            t.x = world_wrap_x(&g->world, t.x - 1);
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
            for (int y = t.y + 1; def->breaks_below && world_get_at(&g->world, layer, t.x, y) == tile; y++) {
                drop_block_items(g, def, (TilePos){ t.x, y }, 0, 1);
                world_set_at(&g->world, layer, t.x, y, TILE_AIR);
            }
            /* A plant two blocks tall: its top goes with its foot... */
            if (def->topped_by != TILE_AIR && world_get_at(&g->world, layer, t.x, t.y - 1) == def->topped_by) {
                drop_block_items(g, tile_def(def->topped_by), (TilePos){ t.x, t.y - 1 }, 0,
                                 tile_def(def->topped_by)->drop_count > 0 ? tile_def(def->topped_by)->drop_count : 1);
                world_set_at(&g->world, layer, t.x, t.y - 1, TILE_AIR);
            }
            /* ...but the top taken by itself leaves the plant a stage back, to grow it again. */
            bool cut = def->cut_to != TILE_AIR && tile_def(world_get_at(&g->world, layer, t.x, t.y + 1))->topped_by == tile;
            /* A tree cut through dies from the cut upward. */
            decay_tree(&g->world, layer, t);
            int tones = def->two_tone ? tile_tones(&g->world, layer, t.x, t.y) : def->written ? world_meta(&g->world, t.x, t.y) : 0;
            /* (With luck on their side, now and then one more of it than there was.) */
            float luck = game_boons(g).luck;
            if (luck > 0.0f && inside == 0 && tones == 0 && def->storage_columns == 0 && !def->bench && item_def(def->drop)->container_slots == 0 &&
                (float)(game_random(g) >> 8) / 16777216.0f < luck)
                count++;
            drop_block_items_toned(g, def, (TilePos){ t.x, bottom }, inside, count, tones);
            /* (Some blocks are still there when they have been mined: there is always more in them.) */
            if (def->endless) {
                g->has_current = false;
                return;
            }
            replace_block(&g->world, layer, t, TILE_AIR);
            if (def->pair != TILE_AIR && world_get_at(&g->world, tile_def(def->pair)->layer, t.x + 1, t.y) == def->pair)
                world_set_at(&g->world, tile_def(def->pair)->layer, t.x + 1, t.y, TILE_AIR);
            /* What it laid under itself comes up with it, but for any piece something is standing on. */
            for (int dx = -1; def->lays != TILE_AIR && dx <= 1; dx++)
                if (world_get_at(&g->world, tile_def(def->lays)->layer, t.x + dx, t.y + 1) == def->lays && train_at(g, t.x + dx, t.y + 1) < 0)
                    world_set_at(&g->world, tile_def(def->lays)->layer, t.x + dx, t.y + 1, TILE_AIR);
            /* The top of a wall taken away - a block with no backwall over
             * it - takes the backwall under it down too, as far as the next block or as far as walls are run. */
            if (layer == LAYER_FOREGROUND && def->solid && content.rules.wall_span > 0 &&
                world_get_at(&g->world, LAYER_WALL, t.x, t.y - 1) == TILE_AIR) {
                /* (The wall behind the block itself goes with them.) */
                world_set_at(&g->world, LAYER_WALL, t.x, t.y, TILE_AIR);
                for (int k = 1; k <= content.rules.wall_span; k++) {
                    if (!world_in_bounds(&g->world, t.x, t.y + k) || world_is_solid(&g->world, t.x, t.y + k)) break;
                    world_set_at(&g->world, LAYER_WALL, t.x, t.y + k, TILE_AIR);
                }
            }
            if (cut) world_set_at(&g->world, layer, t.x, t.y + 1, def->cut_to);
        }
        g->has_current = false;
    } else if (p->activity == ACTIVITY_ATTACKING) {
        /* The blow lands if the creature is still there to be hit. */
        Mob *m = &g->mobs[g->current.drop];
        int range = game_attack_range(g);
        if (g->has_current && g->current.type == ACTION_ATTACK && m->active && m->id == g->current.drop_id &&
            in_attack_range(&g->world, p->feet, (TilePos){ m->x, m->y }, range)) {
            const ItemStack *held = inventory_selected(&g->inventory);
            const ItemDef *with = item_def(held->count > 0 ? held->id : ITEM_NONE);
            if (g->current.arg == HAND_SHEAR) {
                /* Shearing: it has to be in reach, and to have something to give. */
                if (!with->shears || !in_use_range(&g->world, p->feet, (TilePos){ m->x, m->y }) || !mob_shear(g, g->current.drop))
                    deny(g, (TilePos){ m->x, m->y });
            } else if (g->current.arg == HAND_CAGE) {
                /* Caging: the creature is gone from the world and is an item in hand, in place of the cage. */
                ItemId caged = content.mobs[m->kind].caged;
                if (!with->cages || caged == ITEM_NONE || !in_use_range(&g->world, p->feet, (TilePos){ m->x, m->y }) ||
                    inventory_remove(&g->inventory, held->id, 1) != 1) {
                    deny(g, (TilePos){ m->x, m->y });
                } else {
                    m->active = false;
                    game_give_items(g, caged, 1);
                }
            } else if (g->current.arg == HAND_MILK) {
                /* Milking: into something empty, from a creature that has milk to give. */
                bool empty = with->milks && (!with->holds_water || inventory_meter(&g->inventory, held) == 0);
                if (!empty || !in_use_range(&g->world, p->feet, (TilePos){ m->x, m->y }) || !mob_milk(g, g->current.drop))
                    deny(g, (TilePos){ m->x, m->y });
            } else if (range > 0 && !in_use_range(&g->world, p->feet, (TilePos){ m->x, m->y })) {
                /* A shot: something to draw. */
                g->shot.from_x = (float)p->feet.x + 0.5f;
                g->shot.from_y = (float)p->feet.y - 0.5f;
                g->shot.to_x = (float)p->feet.x + (float)world_dx(&g->world, p->feet.x, m->x) + 0.5f;
                g->shot.to_y = (float)m->y + 0.5f;
                g->shot.age = 0.0f;
                g->shot.live = true;
            }
            /* What it does at once, and what it leaves working in the creature. */
            if (g->current.arg != HAND_HIT) {}
            else if (with->casts) {
                /* A spell: on this one and whatever is within its splash; and one use of it is gone. */
                spell_on_mobs(g, with, m->x, m->y);
                wear_spell(g, held->id);
            } else if (!mob_hurt(g, g->current.drop, game_attack_damage(g)) && with->poison > 0)
                mob_poison_add(m, with->poison, with->poison_every);
        }
        /* One blow to a click: hitting it again is asked for again. */
        if (g->has_current && g->current.type == ACTION_ATTACK) g->has_current = false;
    } else if (p->activity == ACTIVITY_PLACING) {
        TilePos t = g->current.target;
        ItemId item = (ItemId)g->current.item;
        TileType tile = placed_tile(&g->world, item, t);
        ItemStack one;
        if (!inventory_take_one(&g->inventory, item, &one)) {
            deny(g, t);
        } else if (g->current.type == ACTION_PLANT) {
            int tones = item_def(item)->two_tone ? inventory_meter(&g->inventory, &one) : 0;
            container_free(&g->inventory, one.container);
            plant_item(g, &g->world, item, t, tones);
        } else if (!needs_storage(tile_def(tile))) {
            /* Something that comes in sizes goes down as the block of its size. */
            int size = item_def(item)->sized ? inventory_meter(&g->inventory, &one) : 0;
            int text = item_def(item)->written ? inventory_meter(&g->inventory, &one) : 0;
            if (size >= 1 && size <= MAX_ITEM_SIZES && item_def(item)->places_sized[size - 1] != TILE_AIR)
                tile = item_def(item)->places_sized[size - 1];
            container_free(&g->inventory, one.container);
            place_tile(&g->world, t, tile);
            /* (A sign goes up with what was written on it.) */
            if (text > 0 && tile_def(tile)->written) world_set_meta(&g->world, t.x, t.y, text);
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
/* Paint, or stripper, at a square: the block there, or failing a block the
 * backwall, takes the paint's colour or loses its own. Each use wears the
 * thing used; the last one finishes it. Painting something the colour it
 * already is, or stripping what is bare, uses nothing. */
static void use_paint(Game *g, ItemId item, TilePos at)
{
    const ItemDef *def = item_def(item);
    ItemStack *held = inventory_find(&g->inventory, item);
    int where = paintable(&g->world, at);
    if (!held || where < 0) return;
    int reads = inventory_meter(&g->inventory, held), now = world_paint(&g->world, where == 1, at.x, at.y);
    if (def->paints) {
        int tint = reads % PAINT_USED, used = reads / PAINT_USED;
        if (tint < 1 || tint > content.tint_count || now == tint) return;
        world_set_paint(&g->world, where == 1, at.x, at.y, tint);
        if (used + 1 >= def->uses) inventory_remove(&g->inventory, item, 1);
        else inventory_set_meter(&g->inventory, held, tint + (used + 1) * PAINT_USED);
    } else if (def->strips) {
        if (now == 0) return;
        world_set_paint(&g->world, where == 1, at.x, at.y, 0);
        if (reads <= 1) inventory_remove(&g->inventory, item, 1);
        else inventory_set_meter(&g->inventory, held, reads - 1);
    }
}

static bool working(const Game *g);

/* Uses up one use of the spell in an item; the item goes when it has none left. */
static void wear_spell(Game *g, ItemId item)
{
    ItemStack *held = inventory_find(&g->inventory, item);
    if (!held) return;
    int left = inventory_meter(&g->inventory, held);
    if (left <= 1) inventory_remove(&g->inventory, item, 1);
    else inventory_set_meter(&g->inventory, held, left - 1);
}

/* What a spell does to the creatures about a square: those it is cast at and
 * those within its splash. One weak to its element takes its bane; any other
 * that can be hurt at all takes its damage. */
static void spell_on_mobs(Game *g, const ItemDef *it, int x, int y)
{
    int reach = it->cast_splash;
    for (int i = 0; i < MAX_MOBS; i++) {
        const Mob *m = &g->mobs[i];
        if (!m->active) continue;
        int dx = world_dx(&g->world, x, m->x), dy = m->y - y;
        if (dx * dx + dy * dy > reach * reach + reach) continue;
        int harm = mob_weak_to(g, i, it->cast_element) ? it->cast_bane : it->cast_damage;
        mob_hurt_by(g, i, harm, it->cast_element, it->cast_cooks);
    }
}

/* Whether a spell is one for its caster and not for a place: there is nowhere to go to cast it. */
static bool spell_on_self(const ItemDef *it)
{
    return it->casts && it->cast_water == 0 && it->cast_dig == 0 && (it->cast_shield > 0.0f || it->cast_time >= 0);
}

/* Casts the spell in an item at a square. */
static void cast_spell(Game *g, ItemId item, TilePos at)
{
    const ItemDef *it = item_def(item);
    World *w = &g->world;
    if (!it->casts || inventory_count(&g->inventory, item) < 1) return;
    if (it->cast_water > 0 && !world_is_solid(w, at.x, at.y)) water_add(w, at.x, at.y, it->cast_water);
    if (it->cast_shield > 0.0f && g->shield < it->cast_shield) g->shield = it->cast_shield;
    if (it->cast_time >= 0) {
        /* On to the next sunrise, or the next moonrise: time only goes forward. */
        uint64_t cycle = (uint64_t)(DAY_TICKS + NIGHT_TICKS), now = g->tick % cycle, want = it->cast_time == 0 ? 0 : (uint64_t)DAY_TICKS;
        g->tick += (want + cycle - now) % cycle;
    }
    for (int dy = -it->cast_dig; it->cast_dig > 0 && dy <= it->cast_dig; dy++) {
        for (int dx = -it->cast_dig; dx <= it->cast_dig; dx++) {
            if (dx * dx + dy * dy > it->cast_dig * it->cast_dig) continue;
            TilePos t = { world_wrap_x(w, at.x + dx), at.y + dy };
            if (!world_in_bounds(w, t.x, t.y)) continue;
            /* Plain blocks, each leaving what it leaves: not a bench, a chest, or anything of more than one square. */
            TileType block = world_get(w, t.x, t.y);
            const TileDef *def = tile_def(block);
            if (block == TILE_AIR || !def->removable || def->bench || def->trade || def->storage_columns > 0 || def->height > 1 ||
                def->pair != TILE_AIR || def->pair_of != TILE_AIR || def->written)
                continue;
            drop_block_items(g, def, t, 0, def->drop_count > 0 ? def->drop_count : 1);
            world_set(w, t.x, t.y, TILE_AIR);
        }
    }
    spell_on_mobs(g, it, at.x, at.y);
    wear_spell(g, item);
}

static bool start_next_activity(Game *g)
{
    Player *p = &g->player;
    const World *w = &g->world;

    /* Riding, they stay where they are put. */
    if (g->ride) return false;

    if (is_supported(w, p->feet)) {
        p->climbing = false;
    } else if (!p->climbing || !can_cling(w, p->feet)) {
        p->climbing = false;
        begin_step(g, (Step){ 0, 1, false });
        return true;
    }
    /* The dead do nothing more than lie where they fell. */
    if (g->vitals.respawn > 0) return false;
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
                if (in_attack_range(w, p->feet, at, g->current.arg != HAND_HIT ? 0 : g->current.amount)) {
                    /* Within reach: one blow. */
                    g->current.target = at;
                    begin_work(g, ACTIVITY_ATTACKING, content.rules.attack_time > 0.0f ? content.rules.attack_time : 0.3f);
                    return true;
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
                    /* (Set fire to, it is down in a moment, whatever it is.) */
                    if (g->current.item != ITEM_NONE && item_def((ItemId)g->current.item)->ignites &&
                        inventory_count(&g->inventory, (ItemId)g->current.item) > 0 && !g->current.wall &&
                        tile_def(world_get_at(w, action_layer(w, g->current), g->current.target.x, g->current.target.y))->burns) {
                        inventory_remove(&g->inventory, (ItemId)g->current.item, 1);
                        begin_work(g, ACTIVITY_MINING, content.rules.burn_time > 0.0f ? content.rules.burn_time : 0.3f);
                        return true;
                    }
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
                case ACTION_PAINT:
                    use_paint(g, (ItemId)g->current.item, g->current.target);
                    g->has_current = false;
                    continue;
                case ACTION_RELEASE: {
                    /* Out it comes, the player's own from now on; and the cage is empty again. */
                    ItemId item = (ItemId)g->current.item, cage = ITEM_NONE;
                    ItemStack one;
                    if (item_def(item)->releases < 0 || !inventory_take_one(&g->inventory, item, &one)) break;
                    container_free(&g->inventory, one.container);
                    if (mob_spawn(g, item_def(item)->releases, g->current.target.x, g->current.target.y, true, false) < 0) {
                        inventory_add(&g->inventory, item, 1);
                        break;
                    }
                    for (ItemId i = 1; i < content.item_count && cage == ITEM_NONE; i++)
                        if (content.items[i].cages) cage = i;
                    if (cage != ITEM_NONE) game_give_items(g, cage, 1);
                    g->has_current = false;
                    continue;
                }
                case ACTION_COLLECT: {
                    Drop *d = &g->drops[g->current.drop];
                    bool there = d->active && d->id == g->current.drop_id && d->collect == 0.0f;
                    /* Someone else's hands, or a despawn, is not worth a complaint. */
                    if (there && (!in_use_range(w, p->feet, drop_tile(d)) || !collect_drop(g, d)))
                        break;
                    /* The rest of the same thing lying in the same square comes up with it: as many as there is room for. */
                    for (int i = 0; there && i < MAX_DROPS; i++) {
                        Drop *more = &g->drops[i];
                        if (more == d || !more->active || more->attached || more->collect != 0.0f || more->item != d->item) continue;
                        if (more->x != d->x || (int)(more->y + 0.5f) != (int)(d->y + 0.5f)) continue;
                        if (!collect_drop(g, more)) break;
                    }
                    g->has_current = false;
                    continue;
                }
                case ACTION_CAST:
                    cast_spell(g, (ItemId)g->current.item, g->current.target);
                    g->has_current = false;
                    continue;
                case ACTION_SLEEP:
                    /* In the bed: they lie down, if they are tired enough to. */
                    g->has_current = false;
                    game_rest(g, REST_SLEEP);
                    continue;
                case ACTION_USE:
                    game_close_bench(g);
                    g->bench_open = true;
                    g->bench_pos = g->current.target;
                    /* Come only to open it, it is the player's to close: what is queued after waits for that. */
                    g->menu_hold = g->current.work == WORK_NONE;
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
        /* Work at a bench is seen through, and a menu that was come to is closed, before anything else is started. */
        if (game_queue_held(g)) return false;
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

    /* (The keys do not take someone away from work they have in hand.) */
    if (working(g)) return false;
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
    /* Nobody makes anything with no energy left to do it on. */
    if (g->vitals.energy <= 0.0f) return false;
    /* Only what this bench makes, at the level it has reached. */
    TileType bench = game_bench_at(g, g->bench_pos.x, g->bench_pos.y);
    if (strcmp(r->bench, tile_def(bench)->bench_kind) != 0 || recipe_locked(bench, r)) return false;
    if (game_bench_fuel(g, NULL) == 0) return false; /* out of fuel */
    int max = recipe_max_crafts(&g->inventory, r, g->crystals);
    if (max < 1) return false;
    /* A character is made one at a time, and only while the world has room for one. */
    if (item_def(r->output.id)->becomes_player) {
        if (g->character_count >= content.rules.max_players || g->character_count >= MAX_CHARACTERS) return false;
        count = 1;
    }
    /* Something made only at a certain number of characters: the way the next one is made, say. */
    if (r->characters > 0 && g->character_count != r->characters) return false;
    g->craft_recipe = r;
    g->craft_total = g->craft_remaining = count < max ? count : max;
    g->craft_timer = 0.0f;
    /* An electric bench has to be sent what the work will take before it begins. */
    g->craft_power = game_power_for(g, bench, r, g->craft_remaining);
    storage_ensure(g, g->bench_pos, tile_def(bench));
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

int game_fuel_value(const Game *g, ItemId item)
{
    if (!g->bench_open) return 0;
    const TileDef *def = tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y));
    /* (Something that burns only certain things burns each for as long as it says.) */
    if (def->fuel_count == 0) return item_def(item)->fuel;
    for (int i = 0; i < def->fuel_count; i++)
        if (def->fuels[i].item == item) return def->fuels[i].ticks;
    return 0;
}

static bool add_fuel_now(Game *g, ItemId item)
{
    int capacity = 0, fuel = game_bench_fuel(g, &capacity);
    if (fuel < 0 || fuel >= capacity || game_fuel_value(g, item) <= 0) return false;
    if (inventory_remove(&g->inventory, item, 1) != 1) return false;
    fuel += game_fuel_value(g, item);
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
    if (def->upgrade_to == TILE_AIR || g->crystals < def->upgrade_crystals ||
        !inventory_has_all(&g->inventory, def->upgrade_cost, def->upgrade_cost_count))
        return false;
    if (def->upgrade_time > 0.0f) {
        /* It takes a while: begun now, and done (and paid for) when the time is up. */
        if (g->upgrading) return false;
        g->upgrading = true;
        g->upgrade_timer = 0.0f;
        return true;
    }
    inventory_remove_all(&g->inventory, def->upgrade_cost, def->upgrade_cost_count);
    g->crystals -= def->upgrade_crystals;
    replace_block(&g->world, layer, g->bench_pos, def->upgrade_to);
    return true;
}

float game_craft_time(const Recipe *r)
{
    return r && r->time > 0.0f ? r->time : CRAFT_TIME;
}

float game_craft_boost(const Game *g, int x, int y)
{
    float best = 1.0f;
    for (int dy = -12; dy <= 12; dy++) {
        for (int dx = -12; dx <= 12; dx++) {
            for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
                const TileDef *def = tile_def(world_get_at(&g->world, layer, x + dx, y + dy));
                if (def->craft_speed <= best || dx * dx + dy * dy > def->craft_radius * def->craft_radius) continue;
                best = def->craft_speed;
            }
        }
    }
    return best;
}

float game_shield(const Game *g)
{
    return g->shield;
}

float game_character_shield(const Game *g, int c)
{
    if (c < 0 || c >= g->character_count) return 0.0f;
    return c == g->live ? g->shield : g->characters[c].shield;
}

float game_craft_seconds(const Game *g, const Recipe *r)
{
    if (g->quick_craft) return 1.0f / (float)(g->tick_rate > 0 ? g->tick_rate : DEFAULT_TICK_RATE > 0 ? DEFAULT_TICK_RATE : 10);
    /* (Quicker at a bench with something near it that quickens work.) */
    return game_craft_time(r) / (g->bench_open ? game_craft_boost(g, g->bench_pos.x, g->bench_pos.y) : 1.0f);
}

int game_craft_rush_cost(const Game *g)
{
    float per = content.rules.rush_seconds;
    if (per <= 0.0f || !g->bench_open || g->craft_remaining <= 0 || !g->craft_recipe || g->craft_power > 0) return -1;
    float left = game_craft_seconds(g, g->craft_recipe) * (float)g->craft_remaining - g->craft_timer;
    if (left <= 0.0f) return -1;
    int cost = (int)ceilf(left / per);
    return cost < 1 ? 1 : cost;
}

bool game_craft_rush(Game *g)
{
    int cost = game_craft_rush_cost(g);
    if (cost < 0 || g->crystals < cost) return false;
    g->crystals -= cost;
    /* All the time it still had to take is counted as passed: the next moment it is done. */
    g->craft_timer = game_craft_seconds(g, g->craft_recipe) * (float)g->craft_remaining;
    return true;
}

void game_note_found(Game *g)
{
    const Inventory *inv = &g->inventory;
    for (int i = 0; i < INVENTORY_SLOTS + ARMOR_SLOTS; i++) {
        const ItemStack *st = i < INVENTORY_SLOTS ? &inv->slots[i] : &inv->armor[i - INVENTORY_SLOTS];
        if (st->count == 0 || st->id >= MAX_ITEMS) continue;
        g->discovered[st->id] = 1;
        /* (Something that knows everything: with it in hand, so do they.) */
        if (item_def((ItemId)st->id)->reveals_all) memset(g->discovered, 1, sizeof g->discovered);
        /* And what is in a basket in the bar. */
        if (st->container == 0 || st->container >= MAX_CONTAINERS || inv->containers[st->container].used != CONTAINER_ITEMS) continue;
        for (int k = 0; k < CONTAINER_MAX_SLOTS; k++) {
            const ItemStack *in = &inv->containers[st->container].slots[k];
            if (in->count > 0 && in->id < MAX_ITEMS) g->discovered[in->id] = 1;
            if (in->count > 0 && in->id < MAX_ITEMS && item_def((ItemId)in->id)->reveals_all) memset(g->discovered, 1, sizeof g->discovered);
        }
    }
}

float game_craft_progress(const Game *g)
{
    if (g->craft_remaining <= 0 || !g->craft_recipe) return -1.0f;
    float t = g->craft_timer / game_craft_seconds(g, g->craft_recipe);
    return t > 1.0f ? 1.0f : t;
}

float game_upgrade_progress(const Game *g)
{
    if (!g->upgrading || !g->bench_open) return -1.0f;
    float time = tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y))->upgrade_time;
    return time > 0.0f ? (g->upgrade_timer / time > 1.0f ? 1.0f : g->upgrade_timer / time) : 1.0f;
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
    if (!game_alive(g) || (!def->bench && !def->trade)) return false;
    if (g->bench_open && g->bench_pos.x == x && g->bench_pos.y == y) return true;
    /* (Not while there is work in hand at another: that one stays open.) */
    if (game_bench_busy(g)) return false;
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
    if (!game_alive(g) || !g->bench_open || work == WORK_NONE) return false;
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
    if (!game_alive(g) || cost < 0 || g->crystals < cost || in_use_range(w, p->feet, g->bench_pos)) return false;
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
    g->upgrading = false;
    g->menu_hold = false;
}

/* Work going on at the open bench this moment. */
static bool working(const Game *g)
{
    return g->bench_open && (g->craft_remaining > 0 || g->upgrading);
}

bool game_queue_held(const Game *g)
{
    return working(g) || (g->bench_open && g->menu_hold);
}

bool game_bench_busy(const Game *g)
{
    if (!g->bench_open) return false;
    if (working(g)) return true;
    /* On the way to it with something to do there. */
    if (g->has_current && g->current.type == ACTION_USE && g->current.work != WORK_NONE &&
        g->current.target.x == g->bench_pos.x && g->current.target.y == g->bench_pos.y)
        return true;
    for (int i = 0; i < g->queue_count; i++)
        if (g->queue[i].type == ACTION_USE && g->queue[i].work != WORK_NONE &&
            g->queue[i].target.x == g->bench_pos.x && g->queue[i].target.y == g->bench_pos.y)
            return true;
    return false;
}

void game_craft_stop(Game *g)
{
    g->craft_remaining = 0;
    g->upgrading = false;
}

static void update_bench(Game *g, float dt)
{
    if (!g->bench_open || !game_alive(g)) return;
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
        g->upgrading = false;
        return;
    }
    /* An upgrade that takes time: when it is up, it is paid for and done. */
    if (g->upgrading) {
        int layer = LAYER_FOREGROUND;
        const TileDef *def = tile_def(usable_at(&g->world, g->bench_pos.x, g->bench_pos.y, &layer));
        g->upgrade_timer += dt;
        if (def->upgrade_to == TILE_AIR) {
            g->upgrading = false;
        } else if (g->upgrade_timer >= def->upgrade_time) {
            g->upgrading = false;
            if (g->crystals >= def->upgrade_crystals && inventory_has_all(&g->inventory, def->upgrade_cost, def->upgrade_cost_count)) {
                inventory_remove_all(&g->inventory, def->upgrade_cost, def->upgrade_cost_count);
                g->crystals -= def->upgrade_crystals;
                replace_block(&g->world, layer, g->bench_pos, def->upgrade_to);
            }
        }
    }

    /* The fire going out stops the work. */
    if (game_bench_fuel(g, NULL) == 0) g->craft_remaining = 0;
    if (g->craft_remaining <= 0) return;
    if (g->craft_power > 0) {
        /* Waiting for the electricity: when all of it has come down the wire, it is used and the work starts. */
        Storage *st = storage_at(g, g->bench_pos);
        if (!st || st->power < g->craft_power) return;
        st->power -= g->craft_power;
        g->craft_power = 0;
    }
    /* Worn out, the work waits for them; as happy as can be, it goes that much the quicker. */
    if (g->vitals.energy <= 0.0f) return;
    g->craft_timer += dt * (game_pace(g) > 1.0f ? game_pace(g) : 1.0f) * game_boons(g).craft;
    float each = game_craft_seconds(g, g->craft_recipe);
    while (g->craft_remaining > 0 && g->craft_timer >= each) {
        g->craft_timer -= each;
        if (item_def(g->craft_recipe->output.id)->becomes_player) {
            /* Not a thing to carry: somebody new, standing at the bench. */
            const Recipe *r = g->craft_recipe;
            g->craft_remaining = 0;
            if (recipe_max_crafts(&g->inventory, r, g->crystals) < 1) break;
            TilePos at = g->player.feet;
            if (game_add_character(g, at) < 0) break;
            inventory_remove_all(&g->inventory, r->inputs, r->input_count);
            g->crystals -= r->crystals;
            break;
        }
        /* (Something that comes out as one of several things by chance: which, this time.) */
        Recipe rolled = *g->craft_recipe;
        if (rolled.one_of_count > 0) {
            int total = 0, pick;
            for (int k = 0; k < rolled.one_of_count; k++) total += rolled.one_of[k].weight > 0 ? rolled.one_of[k].weight : 0;
            pick = total > 0 ? (int)(game_random(g) % (uint32_t)total) : 0;
            for (int k = 0; k < rolled.one_of_count; k++) {
                pick -= rolled.one_of[k].weight > 0 ? rolled.one_of[k].weight : 0;
                if (pick < 0) {
                    rolled.output.id = rolled.one_of[k].item;
                    break;
                }
            }
        }
        int left = recipe_craft(&g->inventory, &rolled, &g->crystals);
        if (left < 0) {
            g->craft_remaining = 0;
            break;
        }
        for (int i = 0; i < left; i++) spawn_drop(g, rolled.output.id, g->player.feet);
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
    /* (What turns up near a character turns up near whichever is nearest.) */
    game_use_character(g, game_nearest_character(g, x, y));

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
        world_grow_plant(w, layer, x, y);
        return;
    }

    int kind = tree_leaves_at(w, x, y);
    if (kind < 0 || content.trees[kind].fruit == ITEM_NONE || attached_drop_at(g, x, y)) return;
    Drop *fruit = spawn_drop(g, content.trees[kind].fruit, (TilePos){ x, y });
    if (!fruit) return;
    fruit->attached = true;
    fruit->ttl = content.trees[kind].fruit_ticks;
}

/* ---- characters --------------------------------------------------------- */

static void character_stash(Game *g, Character *c)
{
    memcpy(c->name, g->name, sizeof c->name);
    c->player = g->player;
    memcpy(c->slots, g->inventory.slots, sizeof c->slots);
    memcpy(c->armor, g->inventory.armor, sizeof c->armor);
    c->sel_slot = g->inventory.sel_slot;
    c->sel_inner = g->inventory.sel_inner;
    memcpy(c->choice, g->inventory.choice, sizeof c->choice);
    c->has_current = g->has_current;
    c->current = g->current;
    c->steps = g->steps;
    c->step_count = g->step_count;
    c->step_index = g->step_index;
    memcpy(c->queue, g->queue, sizeof c->queue);
    c->queue_count = g->queue_count;
    c->move_dx = g->move_dx;
    c->move_dy = g->move_dy;
    c->vitals = g->vitals;
    c->bench_open = g->bench_open;
    c->bench_pos = g->bench_pos;
    c->craft_recipe = g->craft_recipe;
    c->craft_total = g->craft_total;
    c->craft_remaining = g->craft_remaining;
    c->craft_timer = g->craft_timer;
    c->craft_power = g->craft_power;
    c->menu_hold = g->menu_hold;
    c->shield = g->shield;
    c->chill = g->chill;
    c->ride = g->ride;
    c->relic_box = g->relic_box;
    c->ride_seat = g->ride_seat;
    c->upgrading = g->upgrading;
    c->upgrade_timer = g->upgrade_timer;
}

static void character_fetch(Game *g, const Character *c)
{
    memcpy(g->name, c->name, sizeof g->name);
    g->player = c->player;
    memcpy(g->inventory.slots, c->slots, sizeof c->slots);
    memcpy(g->inventory.armor, c->armor, sizeof c->armor);
    g->inventory.sel_slot = c->sel_slot;
    g->inventory.sel_inner = c->sel_inner;
    memcpy(g->inventory.choice, c->choice, sizeof c->choice);
    g->has_current = c->has_current;
    g->current = c->current;
    g->steps = c->steps;
    g->step_count = c->step_count;
    g->step_index = c->step_index;
    memcpy(g->queue, c->queue, sizeof c->queue);
    g->queue_count = c->queue_count;
    g->move_dx = c->move_dx;
    g->move_dy = c->move_dy;
    g->vitals = c->vitals;
    g->bench_open = c->bench_open;
    g->bench_pos = c->bench_pos;
    g->craft_recipe = c->craft_recipe;
    g->craft_total = c->craft_total;
    g->craft_remaining = c->craft_remaining;
    g->craft_timer = c->craft_timer;
    g->craft_power = c->craft_power;
    g->menu_hold = c->menu_hold;
    g->shield = c->shield;
    g->chill = c->chill;
    g->ride = c->ride;
    g->relic_box = c->relic_box;
    g->ride_seat = c->ride_seat;
    g->upgrading = c->upgrading;
    g->upgrade_timer = c->upgrade_timer;
}

void game_use_character(Game *g, int c)
{
    if (c == g->live || c < 0 || c >= g->character_count) return;
    character_stash(g, &g->characters[g->live]);
    character_fetch(g, &g->characters[c]);
    g->live = c;
}

void game_control(Game *g, int c)
{
    if (c < 0 || c >= g->character_count) return;
    /* Whoever is let go of stops being steered, but keeps on with what they were told. */
    game_use_character(g, g->controlled);
    g->move_dx = g->move_dy = 0;
    g->controlled = c;
    game_use_character(g, c);
}

/* Where a character is, live or not. */
static TilePos character_feet(const Game *g, int c)
{
    return c == g->live ? g->player.feet : g->characters[c].player.feet;
}

static const Vitals *character_vitals(const Game *g, int c)
{
    return c == g->live ? &g->vitals : &g->characters[c].vitals;
}

const Vitals *game_character_vitals(const Game *g, int c)
{
    return character_vitals(g, c < 0 || c >= g->character_count ? g->live : c);
}

int game_nearest_character(const Game *g, int x, int y)
{
    int best = g->live;
    long nearest = -1;
    for (int c = 0; c < g->character_count; c++) {
        /* The living, if there are any: the dead are nobody to a creature. */
        if (character_vitals(g, c)->respawn > 0) continue;
        TilePos at = character_feet(g, c);
        long dx = world_dx(&g->world, at.x, x), dy = y - at.y, d = dx * dx + dy * dy;
        if (nearest < 0 || d < nearest) nearest = d, best = c;
    }
    return best;
}

int game_add_character(Game *g, TilePos at)
{
    int most = content.rules.max_players > 0 ? content.rules.max_players : 1;
    if (g->character_count >= MAX_CHARACTERS || g->character_count >= most) return -1;
    Step *steps = malloc(sizeof(Step) * (size_t)g->world.width * g->world.height);
    if (!steps) return -1;
    int c = g->character_count++;
    Character *made = &g->characters[c];
    memset(made, 0, sizeof *made);
    made->steps = steps;
    made->player.feet = made->player.step_to = (TilePos){ world_wrap_x(&g->world, at.x), at.y };
    made->player.facing = 1;
    made->sel_inner = -1;
    memset(made->choice, -1, sizeof made->choice);
    made->vitals = (Vitals){ .health = 1.0f, .hunger = 1.0f, .energy = 1.0f, .oxygen = 1.0f, .death = 1.0f };
    snprintf(made->name, sizeof made->name, "Player %d", c + 1);
    game_reveal(g, at.x, at.y - 1);
    return c;
}

const char *game_character_name(const Game *g, int c)
{
    return c == g->live ? g->name : c >= 0 && c < g->character_count ? g->characters[c].name : "";
}

TilePos game_character_pos(const Game *g, int c)
{
    return character_feet(g, c < 0 || c >= g->character_count ? g->live : c);
}

bool game_alive(const Game *g)
{
    return g->character_count > 0 && g->vitals.respawn == 0;
}

bool game_character_dead(const Game *g, int c)
{
    return c >= 0 && c < g->character_count && character_vitals(g, c)->respawn > 0;
}

bool game_is_portal(const Game *g, int x, int y)
{
    const TileDef *def = tile_def(game_bench_at(g, x, y));
    return def->bench && content.rules.teleport_bench[0] && strcmp(def->bench_kind, content.rules.teleport_bench) == 0;
}

/* Somewhere to stand at a portal: its foot, or failing that beside it. */
static bool portal_standing(const Game *g, TilePos portal, TilePos *out)
{
    const World *w = &g->world;
    int layer = LAYER_FOREGROUND, bottom = portal.y;
    usable_at(w, portal.x, portal.y, &layer);
    world_structure(w, layer, portal.x, portal.y, &bottom);
    static const int beside[5] = { 0, -1, 1, -2, 2 };
    for (int i = 0; i < 5; i++) {
        for (int dy = 0; dy <= 2; dy++) {
            TilePos at = { world_wrap_x(w, portal.x + beside[i]), bottom - dy };
            if (!body_fits(w, at.x, at.y) || !is_supported(w, at)) continue;
            *out = at;
            return true;
        }
    }
    return false;
}

void game_clear_characters(Game *g)
{
    while (g->character_count > 1) {
        int last = g->character_count - 1;
        if (g->live == last) game_use_character(g, 0);
        for (int i = 0; i < INVENTORY_SLOTS; i++) container_free(&g->inventory, g->characters[last].slots[i].container);
        free(g->characters[last].steps);
        g->character_count--;
    }
    for (int i = 0; i < INVENTORY_SLOTS; i++) container_free(&g->inventory, g->inventory.slots[i].container);
    for (int i = 0; i < ARMOR_SLOTS; i++) container_free(&g->inventory, g->inventory.armor[i].container);
    memset(g->inventory.slots, 0, sizeof g->inventory.slots);
    memset(g->inventory.armor, 0, sizeof g->inventory.armor);
    g->queue_count = 0;
    g->has_current = false;
    g->bench_open = false;
    g->player.activity = ACTIVITY_IDLE;
    g->character_count = 0;
    g->live = g->controlled = 0;
    g->started = false;
}

int game_create_character(Game *g, TilePos portal, const char *name)
{
    TilePos at;
    portal.x = world_wrap_x(&g->world, portal.x);
    if (!game_is_portal(g, portal.x, portal.y) || !portal_standing(g, portal, &at)) return -1;
    int c;
    if (g->character_count == 0) {
        /* Into the place the Game itself keeps for whoever is live. */
        c = 0;
        g->character_count = 1;
        g->live = g->controlled = 0;
        memset(&g->player, 0, sizeof g->player);
        g->player.feet = g->player.step_to = at;
        g->player.facing = 1;
        memset(g->inventory.slots, 0, sizeof g->inventory.slots);
        memset(g->inventory.armor, 0, sizeof g->inventory.armor);
        g->inventory.sel_slot = 0;
        g->inventory.sel_inner = -1;
        memset(g->inventory.choice, -1, sizeof g->inventory.choice);
        g->queue_count = 0;
        g->has_current = false;
        g->move_dx = g->move_dy = 0;
        g->bench_open = false;
        g->craft_remaining = 0;
        g->vitals = (Vitals){ .health = 1.0f, .hunger = 1.0f, .energy = 1.0f, .oxygen = 1.0f, .death = 1.0f };
        game_reveal(g, at.x, at.y - 1);
    } else {
        c = game_add_character(g, at);
        if (c < 0) return -1;
    }
    char *named = c == g->live ? g->name : g->characters[c].name;
    if (name && name[0]) snprintf(named, CHARACTER_NAME_MAX, "%s", name);
    else snprintf(named, CHARACTER_NAME_MAX, "Player %d", c + 1);
    /* A world's first character is given what a new player starts with. */
    if (!g->started) {
        int was = g->live;
        game_use_character(g, c);
        for (int i = 0; i < content.rules.start_item_count; i++)
            inventory_add(&g->inventory, content.rules.start_items[i].id, content.rules.start_items[i].count);
        game_use_character(g, was);
        g->started = true;
    }
    return c;
}

/* The live character dies. */
static void die(Game *g)
{
    Vitals *v = &g->vitals;
    train_unseat(g, -1);
    v->health = v->death = 0.0f;
    v->respawn = content.rules.respawn_ticks > 0 ? content.rules.respawn_ticks : 1;
    v->rest = REST_NONE;
    v->meditation = 0.0f;
    v->poison = 0;
    game_cancel_actions(g);
    game_close_bench(g);
    g->move_dx = g->move_dy = 0;
    /* If they were the one being played and somebody else is alive, that one is played instead. */
    if (g->controlled != g->live) return;
    for (int c = 0; c < g->character_count; c++) {
        if (c == g->live || character_vitals(g, c)->respawn > 0) continue;
        g->controlled = c;
        return;
    }
}

void game_hurt(Game *g, float amount)
{
    Vitals *v = &g->vitals;
    if (!game_alive(g) || amount <= 0.0f) return;
    /* A shield takes it first, for as long as it lasts. */
    if (g->shield > 0.0f) {
        float taken = amount < g->shield ? amount : g->shield;
        g->shield -= taken;
        amount -= taken;
        if (g->shield < 0.0001f) g->shield = 0.0f;
        if (amount <= 0.0f) return;
    }
    v->health -= amount;
    if (v->health >= 0.0f) return;
    /* Past the end of health it comes off the bar underneath. */
    float bar = content.rules.death_bar > 0.0f ? content.rules.death_bar : 1.0f;
    v->death += v->health / bar;
    v->health = 0.0f;
    game_wake(g);
    if (v->death <= 0.0f) die(g);
}

/* Mends the live character by so much of a health bar: the death bar first, then health. */
static void heal(Game *g, float amount)
{
    Vitals *v = &g->vitals;
    float bar = content.rules.death_bar > 0.0f ? content.rules.death_bar : 1.0f;
    if (v->death < 1.0f) {
        float need = (1.0f - v->death) * bar, used = amount < need ? amount : need;
        v->death += used / bar;
        if (v->death > 0.9999f) v->death = 1.0f;
        amount -= used;
    }
    v->health = v->health + amount > 1.0f ? 1.0f : v->health + amount;
}

/* Brings the live character back: at the spawn portal, whole. */
static void revive(Game *g)
{
    Player *p = &g->player;
    TilePos at = { g->world.spawn_x, g->world.spawn_y };
    portal_standing(g, at, &at);
    p->feet = p->step_to = at;
    p->activity = ACTIVITY_IDLE;
    p->climbing = false;
    float hunger = g->vitals.hunger > 0.5f ? g->vitals.hunger : 0.5f, energy = g->vitals.energy > 0.5f ? g->vitals.energy : 0.5f;
    g->vitals = (Vitals){ .health = 1.0f, .hunger = hunger, .energy = energy, .oxygen = 1.0f, .death = 1.0f };
    g->chill = 0.0f;
    game_reveal(g, at.x, at.y - 1);
}

int game_respawn_cost(const Game *g, int c)
{
    if (!game_character_dead(g, c)) return -1;
    int total = content.rules.respawn_ticks > 0 ? content.rules.respawn_ticks : 1;
    long left = character_vitals(g, c)->respawn;
    return (int)((left * content.rules.respawn_cost + total - 1) / total);
}

bool game_respawn(Game *g, int c)
{
    int cost = game_respawn_cost(g, c);
    if (cost < 0 || g->crystals < cost) return false;
    g->crystals -= cost;
    int was = g->live;
    game_use_character(g, c);
    revive(g);
    game_use_character(g, was);
    return true;
}

bool game_remove_character(Game *g, int c)
{
    if (!game_character_dead(g, c)) return false;
    int played = g->controlled;
    game_use_character(g, c);
    /* All they carried is left lying where they died. */
    TilePos at = g->player.feet;
    for (int i = 0; i < INVENTORY_SLOTS + ARMOR_SLOTS; i++) {
        ItemStack *s = i < INVENTORY_SLOTS ? &g->inventory.slots[i] : &g->inventory.armor[i - INVENTORY_SLOTS];
        ItemStack one;
        while (s->count > 0 && inventory_split(&g->inventory, s, 1, &one)) {
            if (!spawn_drop_stack(g, one, at)) break;
        }
        *s = (ItemStack){ 0 };
    }
    game_cancel_actions(g);
    game_close_bench(g);
    if (g->character_count == 1) {
        /* Nobody left: the world waits for a new first character. */
        g->character_count = 0;
        g->live = g->controlled = 0;
        g->vitals.respawn = 0;
        return true;
    }
    game_use_character(g, c == 0 ? 1 : 0);
    free(g->characters[c].steps);
    for (int i = c; i + 1 < g->character_count; i++) g->characters[i] = g->characters[i + 1];
    g->character_count--;
    if (g->live > c) g->live--;
    if (played == c) played = g->live;
    else if (played > c) played--;
    /* (Somebody alive, if the one who was being played is the one who went.) */
    for (int i = 0; i < g->character_count && character_vitals(g, played)->respawn > 0; i++)
        if (character_vitals(g, i)->respawn == 0) played = i;
    g->controlled = played;
    game_use_character(g, played);
    return true;
}

void game_roll_recipes(const Game *g)
{
    for (int i = 0; i < content.recipe_count; i++) {
        Recipe *r = &content.recipes[i];
        if (r->random_item_count == 0) continue;
        /* Each of the total is one of the kinds, picked by the world's seed. */
        int counts[RECIPE_MAX_INPUTS] = { 0 };
        uint32_t x = g->world.seed ^ (0x9E3779B9u * (uint32_t)(i + 1));
        for (int n = 0; n < r->random_total; n++) {
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            counts[(x >> 8) % (uint32_t)r->random_item_count]++;
        }
        r->input_count = 0;
        for (int k = 0; k < r->random_item_count; k++)
            if (counts[k] > 0) r->inputs[r->input_count++] = (RecipeItem){ r->random_items[k], counts[k], false };
    }
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

bool game_lifted(const Game *g)
{
    for (int i = 0; i < ARMOR_SLOTS; i++)
        if (g->inventory.armor[i].count > 0 && item_def((ItemId)g->inventory.armor[i].id)->lifts) return true;
    return false;
}

/* Says, for everything that moves the character in hand about, whether the air holds them up. */
static void set_aloft(Game *g)
{
    g->world.aloft = g->sim.aloft = game_lifted(g) && game_alive(g);
}

bool game_player_flying(const Game *g)
{
    const Player *p = &g->player;
    return game_lifted(g) && game_alive(g) && !in_water(&g->world, p->feet) && !is_supported(&g->world, p->feet) &&
           !can_cling_side(&g->world, p->feet, -1) && !can_cling_side(&g->world, p->feet, 1) && !g->ride;
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
    /* And something that takes energy past full, while there is further to take it. */
    if (item_def(id)->energy_over && g->vitals.energy < 1.0f + content.rules.energy_over - 0.001f) return true;
    /* On a full stomach only cooked food does anything: it keeps the bar full a while. */
    if (game_bar_full(g->vitals.hunger) && !item_def(id)->cooked) return false;
    return !game_saturated(g);
}

bool game_eat(Game *g)
{
    Vitals *v = &g->vitals;
    const ItemStack *held = inventory_selected(&g->inventory);
    if (!game_alive(g) || held->count < 1 || !game_can_eat(g, held->id)) return false;
    const ItemDef *def = item_def(held->id);
    ItemStack eaten;
    if (!inventory_take_one(&g->inventory, held->id, &eaten)) return false;
    container_free(&g->inventory, eaten.container);

    v->oxygen = clamp01(v->oxygen + def->food_oxygen);
    /* On a full stomach it does not fill the bar; it keeps it full. */
    if (game_bar_full(v->hunger) && def->cooked) v->saturation += def->food * (float)content.rules.saturation_ticks;
    v->hunger = clamp01(v->hunger + def->food);
    float mend = def->food_health >= 0.0f ? def->food_health : def->food * content.rules.food_health;
    heal(g, mend);
    /* (Some things - coffee - can take it past full, where it stays topped up for a while.) */
    float most = def->energy_over ? 1.0f + content.rules.energy_over : 1.0f;
    if (v->energy < most) {
        v->energy += def->food_energy;
        if (v->energy > most) v->energy = most;
    }
    return true;
}

void game_wake(Game *g)
{
    g->vitals.rest = REST_NONE;
    g->vitals.meditation = 0.0f;
}

const SignText *game_sign_text(const Game *g, int id)
{
    if (!g->signs || id <= 0 || id >= MAX_SIGN_TEXTS || !g->signs[id].used) return NULL;
    return &g->signs[id];
}

static bool sign_in(const World *w, int x, int y)
{
    return tile_def(world_get(w, x, y))->written || tile_def(world_get_at(w, LAYER_BACKGROUND, x, y))->written;
}

/* The square a sign's text is kept with: its left half. (Given either half.) */
static bool sign_head(const World *w, int *x, int y)
{
    if (sign_in(w, *x, y)) return true;
    TileType of = tile_def(world_get(w, *x, y))->pair_of;
    if (of == TILE_AIR || !tile_def(of)->written || world_get(w, *x - 1, y) != of) return false;
    *x = world_wrap_x(w, *x - 1);
    return true;
}

const SignText *game_sign_at(const Game *g, int x, int y)
{
    return sign_head(&g->world, &x, y) ? game_sign_text(g, world_meta(&g->world, x, y)) : NULL;
}

/* A number for a new text. When they have all been given out, the ones no
 * sign has any more - none standing, none carried - are taken back first. */
static int sign_alloc(Game *g)
{
    for (int pass = 0; pass < 2; pass++) {
        for (int id = 1; id < MAX_SIGN_TEXTS; id++)
            if (!g->signs[id].used) return id;
        if (pass == 1) break;
        static bool kept[MAX_SIGN_TEXTS];
        memset(kept, 0, sizeof kept);
        const World *w = &g->world;
        for (int y = 0; y < w->height; y++)
            for (int x = 0; x < w->width; x++)
                if (w->meta[(size_t)y * w->width + x] != 0 && w->meta[(size_t)y * w->width + x] < MAX_SIGN_TEXTS && sign_in(w, x, y))
                    kept[w->meta[(size_t)y * w->width + x]] = true;
        /* (What a carried thing's meter reads is kept in a container of readings: any of those might be a sign's.) */
        for (int c = 1; c < MAX_CONTAINERS; c++) {
            const Container *box = &g->inventory.containers[c];
            if (box->used != CONTAINER_METERS) continue;
            for (int k = 0; k < CONTAINER_MAX_SLOTS; k++)
                if (box->slots[k].count > 0 && box->slots[k].id >= 1 && box->slots[k].id - 1 < MAX_SIGN_TEXTS) kept[box->slots[k].id - 1] = true;
        }
        for (int id = 1; id < MAX_SIGN_TEXTS; id++)
            if (!kept[id]) g->signs[id].used = false;
    }
    return 0;
}

bool game_sign_write(Game *g, int x, int y, const char *text)
{
    if (!g->signs || !sign_head(&g->world, &x, y)) return false;
    if (!text || !text[0]) {
        world_set_meta(&g->world, x, y, 0);
        return true;
    }
    /* A text of its own, always: other signs that were copies of this one keep what they said. */
    int id = sign_alloc(g);
    if (id == 0) return false;
    memset(&g->signs[id], 0, sizeof g->signs[id]);
    g->signs[id].used = true;
    snprintf(g->signs[id].text, sizeof g->signs[id].text, "%s", text);
    world_set_meta(&g->world, x, y, id);
    return true;
}

float game_sleep_speed(const Game *g, bool *over)
{
    /* A bed is where the character is standing: either of their two squares, either layer. */
    float best = 1.0f;
    if (over) *over = false;
    for (int dy = 0; dy <= 1; dy++) {
        for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
            const TileDef *def = tile_def(world_get_at(&g->world, layer, g->player.feet.x, g->player.feet.y - dy));
            if (def->pair_of != TILE_AIR) def = tile_def(def->pair_of);
            if (def->sleep_speed <= 0.0f) continue;
            if (def->sleep_speed > best) best = def->sleep_speed;
            if (over && def->sleep_over) *over = true;
        }
    }
    return best;
}

TileType game_bed_at(const Game *g, int x, int y)
{
    for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
        TileType t = world_get_at(&g->world, layer, x, y);
        if (tile_def(t)->pair_of != TILE_AIR) t = tile_def(t)->pair_of;
        if (tile_def(t)->sleep_speed > 0.0f) return t;
    }
    return TILE_AIR;
}

bool game_sleep_in(Game *g, int x, int y)
{
    if (game_bed_at(g, x, y) == TILE_AIR) return false;
    return game_queue_action(g, ACTION_SLEEP, world_wrap_x(&g->world, x), y);
}

bool game_rest(Game *g, Rest kind)
{
    Player *p = &g->player;
    if (kind == REST_NONE) {
        game_wake(g);
        return true;
    }
    if (!game_alive(g)) return false;
    bool over = false;
    game_sleep_speed(g, &over);
    if (kind == REST_SLEEP && game_bar_full(g->vitals.energy) && !(over && g->vitals.energy < 1.0f + content.rules.energy_over)) return false;
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
    /* Time only runs on when every character is at rest. */
    bool all = true;
    int living = 0;
    for (int c = 0; c < g->character_count; c++) {
        if (character_vitals(g, c)->respawn > 0) continue; /* (the dead are not waited for) */
        living++;
        all = all && character_vitals(g, c)->rest != REST_NONE;
    }
    return g->tick_rate * (living > 0 && all && content.rules.rest_speed > 1 ? content.rules.rest_speed : 1);
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
    /* The dead only wait: when the wait is over they are back. */
    if (v->respawn > 0) {
        if (--v->respawn == 0) revive(g);
        return;
    }

    /* Poison works for as long as it lasts. */
    if (v->poison > 0) {
        v->poison--;
        game_hurt(g, u->poison_damage);
        if (v->respawn > 0) return;
    }

    /* Meditating, the body wants nothing: neither hunger nor energy runs down. */
    bool still = v->rest == REST_MEDITATE;

    /* Hunger waits for a full stomach to wear off, then runs down. */
    if (still || game_sated(g)) {} /* (nor with something on that keeps hunger off) */
    else if (v->saturation > 0.0f) v->saturation = v->saturation > 1.0f ? v->saturation - 1.0f : 0.0f;
    else if (u->hunger_ticks > 0) v->hunger = clamp01(v->hunger - game_boons(g).appetite / (float)u->hunger_ticks);

    /* Health mends by itself, unless there is nothing left to eat on. */
    if (v->hunger <= 0.0f && u->starve_ticks > 0) {
        if (v->health > u->starve_floor) {
            v->health -= 1.0f / (float)u->starve_ticks;
            if (v->health < u->starve_floor) v->health = u->starve_floor;
        }
    } else if (u->health_regen_ticks > 0) {
        heal(g, game_boons(g).regen / (float)u->health_regen_ticks);
    }

    /* Breath: it runs down with the head under water and comes back in the
     * air. With none left, health goes instead. */
    if (u->oxygen_ticks > 0 && game_player_underwater(g)) {
        v->oxygen = clamp01(v->oxygen - 1.0f / (float)u->oxygen_ticks / game_boons(g).lung);
        if (v->oxygen <= 0.0f && u->drown_ticks > 0) {
            game_wake(g);
            game_hurt(g, 1.0f / (float)u->drown_ticks);
            if (v->respawn > 0) return;
        }
    } else {
        v->oxygen = u->oxygen_refill_ticks > 0 ? clamp01(v->oxygen + 1.0f / (float)u->oxygen_refill_ticks) : 1.0f;
    }

    /* The weather: out in the rain or the cold it gets to them, as far as what they have on lets it; under cover it
     * wears off. With nothing on, and nothing left, it is health that goes. */
    if (u->env_fall_ticks > 0) {
        float most = 1.0f - game_environment_floor(g);
        if (game_exposed(g)) g->chill += 1.0f / (float)u->env_fall_ticks;
        else g->chill -= u->env_rise_ticks > 0 ? 1.0f / (float)u->env_rise_ticks : 1.0f;
        if (g->chill > most) g->chill = most;
        if (g->chill < 0.0f) g->chill = 0.0f;
        if (g->chill >= 0.9999f && u->env_harm_ticks > 0) {
            game_wake(g);
            game_hurt(g, 1.0f / (float)u->env_harm_ticks);
            if (v->respawn > 0) return;
        }
    }

    if (v->rest == REST_SLEEP) {
        /* In a bed it comes quicker; in a soft one it goes on past full. */
        bool over = false;
        float speed = game_sleep_speed(g, &over), most = over ? 1.0f + u->energy_over : 1.0f;
        if (u->sleep_ticks > 0 && v->energy < most) {
            v->energy += speed / (float)u->sleep_ticks;
            if (v->energy > most) v->energy = most;
        }
        if (v->energy >= most || u->sleep_ticks <= 0) game_wake(g);
    } else if (u->energy_ticks > 0 && !still && !game_tireless(g)) {
        v->energy -= game_boons(g).stamina / (float)u->energy_ticks;
        if (v->energy < 0.0f) v->energy = 0.0f;
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

    /* Random events: so many blocks in every chunk. The weather goes by all
     * of them. What grows and what turns up go by only as many as they
     * always did, so that setting the number higher hurries the one and not the other. */
    int base = content.rules.random_event_base;
    for (int chunk = 0; chunk * CHUNK_WIDTH < w->width; chunk++) {
        for (int n = 0; n < g->random_events; n++) {
            uint32_t r = game_random(g);
            int x = chunk * CHUNK_WIDTH + (int)(r % (uint32_t)CHUNK_WIDTH), y = (int)((r >> 8) % (uint32_t)w->height);
            if (x >= w->width) continue;
            game_weather_event(g, x, y);
            /* A plant on rich ground gets more than its share of them: that many times over. */
            int share = base;
            float rich = tile_def(world_get(w, x, y + 1))->fertile;
            if (rich > 1.0f && (tile_def(world_get(w, x, y))->grows_into != TILE_AIR || tile_def(world_get_at(w, LAYER_BACKGROUND, x, y))->grows_into != TILE_AIR))
                share = (int)((float)base * rich + 0.5f);
            if (g->random_events <= share || (int)(game_random(g) % (uint32_t)g->random_events) < share)
                game_random_event(g, x, y);
            else if (rich > 1.0f && g->random_events <= base && share > base)
                /* (With no more events than the usual share there is nothing extra to give: so it is given again.) */
                for (int extra = base; extra < share; extra++) game_random_event(g, x, y);
        }
    }

    mobs_tick(g);
    water_step(w);
    power_tick(g);
    industry_tick(g);
    digital_tick(g);
    train_tick(g);
    /* Everybody gets hungry, whoever is being played. */
    for (int c = 0; c < g->character_count; c++) {
        game_use_character(g, c);
        vitals_tick(g);
        game_note_found(g);
    }
    game_use_character(g, g->controlled);

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

int game_season(const Game *g)
{
    const RulesDef *u = &content.rules;
    if (u->season_count < 1 || u->season_days < 1) return 0;
    uint64_t day = g->tick / (uint64_t)(DAY_TICKS + NIGHT_TICKS);
    return (int)(day / (uint64_t)u->season_days % (uint64_t)u->season_count);
}

void game_date(const Game *g, int *year, int *day, int *tick)
{
    const RulesDef *u = &content.rules;
    uint64_t length = (uint64_t)(DAY_TICKS + NIGHT_TICKS), days = g->tick / length;
    uint64_t in_year = (uint64_t)(u->season_days > 0 ? u->season_days : 1) * (uint64_t)(u->season_count > 0 ? u->season_count : 1);
    if (year) *year = (int)(days / in_year) + 1;
    if (day) *day = (int)(days % in_year) + 1;
    if (tick) *tick = (int)(g->tick % length);
}

/* Whether a stretch of the weather is a wet one: the same answer every time for the same world and stretch. */
static bool wet_spell(const Game *g, uint64_t spell)
{
    uint32_t h = (uint32_t)spell * 2654435761u ^ g->world.seed * 40503u ^ 0x7A1Eu;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return (float)(h >> 8) / 16777216.0f < content.rules.rain_chance;
}

float game_rain(const Game *g)
{
    const RulesDef *u = &content.rules;
    if (u->weather_ticks < 1 || u->rain_chance <= 0.0f) return 0.0f;
    uint64_t spell = g->tick / (uint64_t)u->weather_ticks;
    if (!wet_spell(g, spell)) return 0.0f;
    /* It comes on over the first tenth of a spell and eases off over the
     * last, unless the spell beside it is wet too. */
    float into = (float)(g->tick % (uint64_t)u->weather_ticks) / (float)u->weather_ticks, edge = 0.1f, amount = 1.0f;
    if (into < edge && !(spell > 0 && wet_spell(g, spell - 1))) amount = into / edge;
    if (into > 1.0f - edge && !wet_spell(g, spell + 1)) amount = (1.0f - into) / edge;
    return amount;
}

/* The row of the block a column's snow would lie on: the highest one open to
 * the sky. -1 where there is none, or it is under water. */
static int snow_row(const World *w, int x)
{
    for (int y = 0; y < w->height; y++) {
        if (world_is_water(w, x, y)) return -1;
        if (world_is_solid(w, x, y)) return y;
    }
    return -1;
}

int game_sky_floor(const Game *g, int x, bool water)
{
    const World *w = &g->world;
    for (int y = 0; y < w->height; y++) {
        if (world_is_solid(w, x, y)) return y;
        if (world_is_water(w, x, y)) return water ? y : -1;
    }
    return -1;
}

int game_snow(const Game *g, int x)
{
    const RulesDef *u = &content.rules;
    const World *w = &g->world;
    if (!g->snow || u->snow_max < 1) return 0;
    x = world_wrap_x(w, x);
    int top = snow_row(w, x);
    if (top < 0) return 0;
    /* High ground keeps its snow the year round. */
    if (u->snow_line > 0 && world_row_to_y(w, top) + 1 > u->snow_line) return u->snow_max;
    return g->snow[x] > u->snow_max ? u->snow_max : g->snow[x];
}

void game_weather_event(Game *g, int x, int y)
{
    const RulesDef *u = &content.rules;
    World *w = &g->world;
    if (u->season_count < 1 || !world_in_bounds(w, x, y)) return;
    int season = game_season(g);
    x = world_wrap_x(w, x);

    /* Snow: while it is coming down a layer is added now and then, up to as
     * deep as it lies; out of season it goes the same way, a layer at a time. */
    if (g->snow && u->snow_max > 0) {
        if (u->seasons[season].snows) {
            if (g->snow[x] < u->snow_max && game_rain(g) > 0.0f && snow_row(w, x) >= 0 &&
                (float)(game_random(g) >> 8) / 16777216.0f < u->snow_chance)
                g->snow[x]++;
        } else if (g->snow[x] > 0 && (float)(game_random(g) >> 8) / 16777216.0f < u->melt_chance) {
            g->snow[x]--;
        }
    }
    if (u->ice == TILE_AIR) return;
    if (u->seasons[season].freezes) {
        /* Open water: a square of it with the air over it. It turns to ice, and the ice remembers how much it was. */
        int level = world_water(w, x, y);
        if (world_is_water(w, x, y) && world_get(w, x, y) == TILE_AIR && world_get(w, x, y - 1) == TILE_AIR &&
            world_water(w, x, y - 1) == 0 && (float)(game_random(g) >> 8) / 16777216.0f < u->freeze_chance) {
            world_set_water(w, x, y, 0);
            world_set(w, x, y, u->ice);
            world_set_meta(w, x, y, level);
        }
    } else if (u->seasons[season].thaws && world_get(w, x, y) == u->ice &&
               (float)(game_random(g) >> 8) / 16777216.0f < u->thaw_chance) {
        /* Ice goes back to as much water as it was - all of a square, if it was never water. */
        int level = world_meta(w, x, y);
        world_set_meta(w, x, y, 0);
        world_set(w, x, y, TILE_AIR);
        world_set_water(w, x, y, level > 0 && level <= WATER_FULL ? level : WATER_FULL);
        water_wake(w, x);
    }
}

float game_flail(const Game *g)
{
    return g->live >= 0 && g->live < MAX_CHARACTERS ? g->flail[g->live] : 0.0f;
}

float game_environment(const Game *g)
{
    return 1.0f - g->chill;
}

float game_character_environment(const Game *g, int c)
{
    if (c < 0 || c >= g->character_count) return 1.0f;
    return 1.0f - (c == g->live ? g->chill : g->characters[c].chill);
}

/* How someone is doing, from how they are fed, how the weather has left them, and how much health they have. */
static bool sheltered(const ItemStack *armor)
{
    for (int i = 0; i < ARMOR_SLOTS; i++)
        if (armor[i].count > 0 && item_def((ItemId)armor[i].id)->shelters) return true;
    return false;
}

/* How someone is doing: the average of how rested and how well fed they are and how the weather has left them,
 * and never more than their health. Past 1 - "saturated" - only when they are whole and all three of those are
 * themselves kept past full: energy from a soft bed, hunger by cooked food, the weather by what they have on. */
static float happiness_of(const Vitals *v, float chill, const ItemStack *armor)
{
    if (v->respawn > 0) return 0.0f;
    float hunger = clamp01(v->hunger), health = clamp01(v->health), energy = clamp01(v->energy);
    if (v->health >= 1.0f - content.rules.full_margin && v->energy > 1.0f && v->saturation > 0.0f && chill <= 0.0f && sheltered(armor))
        return 1.0f + content.rules.happy_boost;
    float made = (energy + hunger + (1.0f - chill)) / 3.0f;
    return made < health ? made : health;
}

float game_happiness(const Game *g)
{
    return happiness_of(&g->vitals, g->chill, g->inventory.armor);
}

float game_character_happiness(const Game *g, int c)
{
    if (c < 0 || c >= g->character_count) return 1.0f;
    return c == g->live ? game_happiness(g) : happiness_of(&g->characters[c].vitals, g->characters[c].chill, g->characters[c].armor);
}

bool game_environment_saturated(const Game *g)
{
    return g->chill <= 0.0f && sheltered(g->inventory.armor);
}

bool game_character_environment_saturated(const Game *g, int c)
{
    if (c < 0 || c >= g->character_count) return false;
    return c == g->live ? game_environment_saturated(g) : g->characters[c].chill <= 0.0f && sheltered(g->characters[c].armor);
}

float game_environment_floor(const Game *g)
{
    float kept = 0.0f;
    bool anything = false;
    for (int i = 0; i < ARMOR_SLOTS; i++) {
        if (g->inventory.armor[i].count <= 0) continue;
        anything = true;
        kept += item_def((ItemId)g->inventory.armor[i].id)->warmth;
    }
    kept += game_boons(g).warmth;
    /* (Anything on at all is enough to live through it.) */
    if (anything && kept < content.rules.env_clothed) kept = content.rules.env_clothed;
    return kept > 1.0f ? 1.0f : kept;
}

bool game_exposed(const Game *g)
{
    const RulesDef *u = &content.rules;
    const World *w = &g->world;
    TilePos at = g->player.feet;
    /* Indoors is anywhere with a block overhead; and nothing falls on what is under water. */
    for (int y = at.y - 1; y >= 0; y--)
        if (world_is_solid(w, at.x, y)) return false;
    if (world_is_water(w, at.x, at.y - 1)) return false;
    if (game_rain(g) > 0.0f) return true;
    if (u->season_count > 0 && u->seasons[game_season(g)].cold) return true;
    if (u->snow_line > 0 && world_row_to_y(w, at.y) > u->snow_line) return true;
    int biome = world_biome(w, at.x);
    return biome >= 0 && biome < content.biome_count && content.biomes[biome].cold;
}

float game_pace(const Game *g)
{
    /* (Past full, the little over is all speed; short of it, each part short costs its share.) */
    float happy = game_happiness(g);
    return happy > 1.0f ? happy : 1.0f - content.rules.happy_slow * (1.0f - happy);
}

float game_walk_speed(const Game *g)
{
    float best = 1.0f;
    for (int i = 0; i < ARMOR_SLOTS; i++)
        if (g->inventory.armor[i].count > 0 && item_def((ItemId)g->inventory.armor[i].id)->walk_speed > best)
            best = item_def((ItemId)g->inventory.armor[i].id)->walk_speed;
    return best * game_boons(g).walk;
}

bool game_tireless(const Game *g)
{
    for (int i = 0; i < ARMOR_SLOTS; i++)
        if (g->inventory.armor[i].count > 0 && item_def((ItemId)g->inventory.armor[i].id)->tireless) return true;
    return false;
}

bool game_sated(const Game *g)
{
    for (int i = 0; i < ARMOR_SLOTS; i++)
        if (g->inventory.armor[i].count > 0 && item_def((ItemId)g->inventory.armor[i].id)->sated) return true;
    return false;
}

float game_protection(const Game *g)
{
    float total = 0.0f;
    for (int i = 0; i < ARMOR_SLOTS; i++)
        if (g->inventory.armor[i].count > 0) total += item_def((ItemId)g->inventory.armor[i].id)->armor;
    total += game_boons(g).armor;
    return total > 0.9f ? 0.9f : total;
}

void game_attacked(Game *g, float amount)
{
    /* (They throw their arms about for a moment.) */
    if (g->live >= 0 && g->live < MAX_CHARACTERS) g->flail[g->live] = content.rules.flail_time;
    game_hurt(g, amount * (1.0f - game_protection(g)));
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

    update_drops(g, frame);
    for (int c = 0; c < MAX_CHARACTERS; c++)
        if (g->flail[c] > 0.0f) g->flail[c] = g->flail[c] > frame ? g->flail[c] - frame : 0.0f;
    if (g->shot.live && (g->shot.age += frame) > 0.2f) g->shot.live = false;
    /* Every character gets on with what they are doing, each in turn. */
    for (int c = 0; c < g->character_count; c++) {
        game_use_character(g, c);
        set_aloft(g);
        Player *p = &g->player;
        float left = frame;
        for (;;) {
            if (p->activity != ACTIVITY_IDLE) {
                float remaining = p->duration - p->timer;
                if (left < remaining) {
                    p->timer += left;
                    break;
                }
                left -= remaining;
                finish_activity(g);
            }
            if (!start_next_activity(g)) break;
        }
        collect_nearby(g);
        update_bench(g, frame);
    }
    game_use_character(g, g->controlled);
    set_aloft(g);
    train_update(g, frame);
}

void game_set_move_input(Game *g, int dx, int dy)
{
    if (!game_alive(g)) dx = dy = 0;
    g->move_dx = dx;
    g->move_dy = dy;
    if (dx != 0 || dy != 0) game_wake(g);
}

void game_cancel_actions(Game *g)
{
    g->queue_count = 0;
    g->has_current = false;
    if (g->player.activity == ACTIVITY_MINING || g->player.activity == ACTIVITY_PLACING ||
        g->player.activity == ACTIVITY_ATTACKING)
        g->player.activity = ACTIVITY_IDLE;
}

void game_player_visual_pos(const Game *g, float *x, float *y)
{
    /* Riding, they are wherever what they are riding is. */
    if (train_rider_visual(g, x, y)) return;
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
    return game_init_with(g, width, height, seed, kind, NULL);
}

bool game_init_with(Game *g, int width, int height, uint32_t seed, WorldKind kind, const WorldOptions *options)
{
    memset(g, 0, sizeof *g);
    size_t cells = (size_t)width * height;
    bool ok = world_init_with(&g->world, width, height, seed, kind, options) &&
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
    g->snow = calloc((size_t)width, 1);
    g->flows = calloc(MAX_POWER_FLOWS, sizeof(PowerFlow));
    g->signs = calloc(MAX_SIGN_TEXTS, sizeof(SignText));
    digital_init(g);
    g->random_events = content.rules.random_events_per_chunk;
    /* (Nothing grows, rots, freezes or falls in a world that is there to be looked at.) */
    if (kind == WORLD_DEBUG) g->random_events = 0;
    if (!g->explored || !g->snow || !g->flows || !g->signs || !g->cells || !g->jobs) {
        game_free(g);
        return false;
    }
    g->vitals = (Vitals){ .health = 1.0f, .hunger = 1.0f, .energy = 1.0f, .oxygen = 1.0f, .death = 1.0f };
    snprintf(g->name, sizeof g->name, "Player 1");
    g->started = true;
    g->character_count = 1;
    g->live = g->controlled = 0;
    game_roll_recipes(g);
    g->fog = content.rules.fog_enabled;
    game_reveal(g, g->player.feet.x, g->player.feet.y - 1);
    g->player.step_to = g->player.feet;
    return true;
}

void game_free(Game *g)
{
    world_free(&g->world);
    world_free(&g->sim);
    /* (Each character has a route of their own; the live one's is in the Game.) */
    for (int c = 0; c < g->character_count && c < MAX_CHARACTERS; c++)
        if (c != g->live) free(g->characters[c].steps);
    free(g->steps);
    free(g->bfs_step);
    free(g->bfs_parent);
    free(g->bfs_queue);
    free(g->bfs_seen);
    free(g->explored);
    free(g->snow);
    free(g->flows);
    free(g->signs);
    digital_free(g);
    memset(g, 0, sizeof *g);
}
