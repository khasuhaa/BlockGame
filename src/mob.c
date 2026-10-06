#include "mob.h"

#include <math.h>

static float chance(Game *g)
{
    return (float)(game_random(g) >> 8) / 16777216.0f;
}

/* Somewhere a creature can be: open, on solid ground, and dry. */
static bool can_stand(const World *w, int x, int y)
{
    return !world_is_solid(w, x, y) && world_is_solid(w, x, y + 1) && !world_is_water(w, x, y);
}

static bool in_range(const Game *g, int x, int y)
{
    float dx = (float)world_dx(&g->world, g->player.feet.x, x), dy = (float)(y - g->player.feet.y);
    float r = (float)content.mob_spawn_radius;
    return dx * dx + dy * dy <= r * r;
}

int mobs_near_player(const Game *g)
{
    int n = 0;
    for (int i = 0; i < MAX_MOBS; i++)
        if (g->mobs[i].active && in_range(g, g->mobs[i].x, g->mobs[i].y)) n++;
    return n;
}

int mob_spawn(Game *g, int kind, int x, int y, bool owned, bool baby)
{
    if (kind < 0 || kind >= content.mob_count) return -1;
    for (int i = 0; i < MAX_MOBS; i++) {
        if (g->mobs[i].active) continue;
        x = world_wrap_x(&g->world, x);
        g->mobs[i] = (Mob){ .active = true, .kind = (uint8_t)kind, .owned = owned, .baby = baby,
                            .x = x, .y = y, .to_x = x, .to_y = y,
                            .dir = (game_random(g) & 1) ? 1 : -1, .id = ++g->next_mob_id };
        return i;
    }
    return -1;
}

bool mobs_try_spawn(Game *g, int x, int y)
{
    const World *w = &g->world;
    if (!world_in_bounds(w, x, y) || world_get(w, x, y) != TILE_AIR) return false;
    if (world_get_at(w, LAYER_BACKGROUND, x, y) != TILE_AIR || world_is_water(w, x, y)) return false;
    TileType ground = world_get(w, x, y + 1);

    for (int kind = 0; kind < content.mob_count; kind++) {
        const MobDef *def = &content.mobs[kind];
        bool here = false;
        for (int i = 0; i < def->spawn_above_count; i++) here = here || def->spawn_above[i] == ground;
        if (!here) continue;
        /* Only near the player, and only while there is room for more. */
        if (!in_range(g, x, y) || mobs_near_player(g) >= content.mob_cap) return false;
        if (chance(g) >= def->spawn_chance) return false;
        return mob_spawn(g, kind, x, y, false, false) >= 0;
    }
    return false;
}

bool mobs_try_hatch(Game *g, int x, int y)
{
    World *w = &g->world;
    for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
        const TileDef *def = tile_def(world_get_at(w, layer, x, y));
        if (def->hatches < 0) continue;
        /* A wild egg waits while there are already as many creatures about
         * as there may be; one the player put down hatches regardless. */
        if (!def->hatch_owned && (!in_range(g, x, y) || mobs_near_player(g) >= content.mob_cap)) return false;
        /* What comes out is the player's only if the egg was put there by the player. */
        if (mob_spawn(g, def->hatches, x, y, def->hatch_owned, true) < 0) return false;
        world_set_at(w, layer, x, y, TILE_AIR);
        return true;
    }
    return false;
}

void mob_kill(Game *g, int mob)
{
    if (mob < 0 || mob >= MAX_MOBS || !g->mobs[mob].active) return;
    Mob *m = &g->mobs[mob];
    const MobDef *def = &content.mobs[m->kind];
    for (int i = 0; !m->baby && i < def->drop_count; i++) {
        int span = def->drops[i].max - def->drops[i].min + 1;
        int n = def->drops[i].min + (span > 1 ? (int)(game_random(g) % (uint32_t)span) : 0);
        for (int k = 0; k < n; k++) game_drop_item(g, def->drops[i].item, m->x, m->y);
    }
    m->active = false;
}

/* Sets a creature walking one block the way it faces: along the level, up a
 * step of one block, or down one. It will not cross a gap or drop further;
 * faced with either it turns round instead. */
static void start_step(Game *g, Mob *m)
{
    const World *w = &g->world;
    int nx = m->x + m->dir, ny = m->y;
    if (can_stand(w, nx, m->y)) ny = m->y;
    else if (world_is_solid(w, nx, m->y) && !world_is_solid(w, m->x, m->y - 1) && can_stand(w, nx, m->y - 1))
        ny = m->y - 1;
    else if (!world_is_solid(w, nx, m->y) && can_stand(w, nx, m->y + 1)) ny = m->y + 1;
    else {
        m->dir = -m->dir;
        return;
    }
    m->to_x = world_wrap_x(w, nx);
    m->to_y = ny;
    m->step_total = m->step_left = content.mobs[m->kind].step_ticks;
}

void mobs_tick(Game *g)
{
    World *w = &g->world;
    for (int i = 0; i < MAX_MOBS; i++) {
        Mob *m = &g->mobs[i];
        if (!m->active) continue;
        const MobDef *def = &content.mobs[m->kind];

        m->age++;
        if (m->baby && m->age >= def->baby_ticks) m->baby = false;
        /* A wild one that has got too far from the player is gone. */
        if (!m->owned && !in_range(g, m->x, m->y)) {
            m->active = false;
            continue;
        }
        if (m->step_left > 0) {
            if (--m->step_left == 0) {
                m->x = m->to_x;
                m->y = m->to_y;
            }
            continue;
        }
        /* With the ground gone from under it, it falls. */
        if (!world_is_solid(w, m->x, m->y + 1)) {
            m->to_x = m->x;
            m->to_y = m->y + 1;
            m->step_total = m->step_left = 1;
            continue;
        }
        if (chance(g) < def->move_chance) {
            if (chance(g) < def->turn_chance) m->dir = -m->dir;
            start_step(g, m);
            continue;
        }
        /* Now and then a grown one leaves an egg where it stands, if nothing is there. */
        if (!m->baby && def->lays != TILE_AIR && chance(g) < def->lay_chance &&
            world_get(w, m->x, m->y) == TILE_AIR &&
            world_get_at(w, tile_def(def->lays)->layer, m->x, m->y) == TILE_AIR)
            world_set_at(w, tile_def(def->lays)->layer, m->x, m->y, def->lays);
    }
}

void game_mob_pos(const Game *g, int mob, float *x, float *y)
{
    const Mob *m = &g->mobs[mob];
    float t = 0.0f;
    if (m->step_left > 0 && m->step_total > 0) {
        float into_tick = g->tick_timer < 1.0f ? g->tick_timer : 1.0f;
        t = ((float)(m->step_total - m->step_left) + into_tick) / (float)m->step_total;
    }
    *x = (float)m->x + (float)world_dx(&g->world, m->x, m->to_x) * t;
    *y = (float)m->y + (float)(m->to_y - m->y) * t;
}

int game_mob_at(const Game *g, float x, float y)
{
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!g->mobs[i].active) continue;
        float mx, my;
        game_mob_pos(g, i, &mx, &my);
        float dx = (float)world_dx(&g->world, (int)floorf(mx), (int)floorf(x)) + (x - floorf(x)) - (mx - floorf(mx));
        if (dx >= 0.0f && dx < 1.0f && y >= my && y < my + 1.0f) return i;
    }
    return -1;
}
