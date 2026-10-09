#include "disaster.h"

#include <stdlib.h>

#include "water.h"

#define WARD_SCAN 24

const DisasterDef *game_disaster(const Game *g)
{
    return g->disaster > 0 && g->disaster <= content.rules.disaster_count ? &content.rules.disasters[g->disaster - 1] : NULL;
}

bool disaster_start(Game *g, int which)
{
    if (which < 0 || which >= content.rules.disaster_count || g->disaster > 0) return false;
    g->disaster = which + 1;
    g->disaster_until = g->tick + (uint64_t)(content.rules.disasters[which].ticks > 0 ? content.rules.disasters[which].ticks : 1);
    return true;
}

static int roll(Game *g, int span)
{
    return span > 0 ? (int)(game_random(g) % (uint32_t)span) : 0;
}

/* Whether a bolt bound for a column is drawn off by something that wards: the column it goes to instead, or -1. */
static int warded(const Game *g, int x)
{
    const World *w = &g->world;
    for (int d = 0; d <= WARD_SCAN; d++)
        for (int side = -1; side <= 1; side += 2) {
            int cx = world_wrap_x(w, x + side * d), floor = game_sky_floor(g, cx, false);
            for (int y = floor - 8; floor > 0 && y < floor; y++)
                for (int layer = LAYER_FOREGROUND; layer <= LAYER_BACKGROUND; layer++)
                    if (world_in_bounds(w, cx, y) && tile_def(world_get_at(w, layer, cx, y))->wards >= d && tile_def(world_get_at(w, layer, cx, y))->wards > 0) return cx;
        }
    return -1;
}

static void hurt_near(Game *g, const DisasterDef *def, int x, int y)
{
    for (int c = 0; c < g->character_count; c++) {
        game_use_character(g, c);
        TilePos at = g->player.feet;
        bool near_ = abs(world_dx(&g->world, x, at.x)) <= def->hurt_radius && abs(at.y - y) <= def->hurt_radius + 2;
        if (game_alive(g) && (def->exposed ? game_exposed(g) : near_)) game_hurt(g, def->hurt);
    }
    game_use_character(g, g->controlled);
}

static void strike(Game *g, const DisasterDef *def)
{
    World *w = &g->world;
    if (g->character_count < 1) return;
    TilePos by = game_character_pos(g, roll(g, g->character_count));
    int x = world_wrap_x(w, by.x + roll(g, 2 * def->reach + 1) - def->reach), y = game_sky_floor(g, x, false);
    /* (What falls on everybody out of doors falls wherever they are.) */
    if (def->exposed && def->hurt > 0.0f) hurt_near(g, def, x, y);
    if (y < 1) return;
    if (def->bolt) {
        int rod = warded(g, x);
        g->lightning = 6;
        g->lightning_x = rod >= 0 ? rod : x;
        if (rod >= 0) return; /* (drawn off: it does nothing) */
    }
    for (int dy = -def->burn; def->burn > 0 && dy <= def->burn; dy++)
        for (int dx = -def->burn; dx <= def->burn; dx++)
            for (int layer = LAYER_FOREGROUND; layer <= LAYER_BACKGROUND; layer++) {
                int bx = world_wrap_x(w, x + dx), by_ = y - 1 + dy;
                if (world_in_bounds(w, bx, by_) && tile_def(world_get_at(w, layer, bx, by_))->burns) world_set_at(w, layer, bx, by_, TILE_AIR);
            }
    if (def->blast > 0) {
        game_blast(g, x, y, def->blast);
        if (def->leaves != TILE_AIR && world_in_bounds(w, x, y + def->blast) && y + def->blast < w->height - 1) world_set(w, x, y + def->blast, def->leaves);
    }
    for (int n = 0; n < def->breaks; n++) {
        int bx = world_wrap_x(w, x + roll(g, 7) - 3), by_ = y + def->depth_min + roll(g, def->depth_max - def->depth_min + 1);
        if (world_in_bounds(w, bx, by_) && by_ < w->height - 1 && world_is_solid(w, bx, by_)) game_break_block(g, bx, by_);
    }
    if (def->water > 0) water_add(w, x, y - 1, def->water);
    if (!def->exposed && def->hurt > 0.0f) hurt_near(g, def, x, y);
}

void disaster_tick(Game *g)
{
    const RulesDef *u = &content.rules;
    if (u->disaster_count < 1) return;
    const DisasterDef *now = game_disaster(g);
    if (now) {
        if (g->tick >= g->disaster_until) g->disaster = 0;
        else if (now->every > 0 && g->tick % (uint64_t)now->every == 0) strike(g, now);
        return;
    }
    if (u->disaster_period < 1 || g->tick % (uint64_t)u->disaster_period != 0 || !game_alive(g)) return;
    bool day;
    game_day_phase(g, &day);
    for (int i = 0; i < u->disaster_count; i++) {
        const DisasterDef *def = &u->disasters[i];
        if (def->when != 0 && (def->when == 1) != day) continue;
        if ((float)(game_random(g) % 100000u) < def->chance * 100000.0f) {
            disaster_start(g, i);
            return;
        }
    }
}
