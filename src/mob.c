#include "mob.h"

#include <math.h>
#include <stdlib.h>

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

static bool spawns_in(const World *w, const MobDef *def, int x, int y);

/* Creatures of the land and of the water are counted apart: each has its own cap. */
static int count_near(const Game *g, bool swimmers)
{
    int n = 0;
    for (int i = 0; i < MAX_MOBS; i++)
        if (g->mobs[i].active && content.mobs[g->mobs[i].kind].swims == swimmers && in_range(g, g->mobs[i].x, g->mobs[i].y))
            n++;
    return n;
}

int mobs_near_player(const Game *g)
{
    return count_near(g, false);
}

int mobs_near_player_in_water(const Game *g)
{
    return count_near(g, true);
}

int mob_spawn(Game *g, int kind, int x, int y, bool owned, bool baby)
{
    if (kind < 0 || kind >= content.mob_count) return -1;
    for (int i = 0; i < MAX_MOBS; i++) {
        if (g->mobs[i].active) continue;
        x = world_wrap_x(&g->world, x);
        const MobDef *def = &content.mobs[kind];
        int span = def->size_max - def->size_min + 1;
        g->mobs[i] = (Mob){ .active = true, .kind = (uint8_t)kind, .owned = owned, .baby = baby,
                            .x = x, .y = y, .to_x = x, .to_y = y,
                            .dir = (game_random(g) & 1) ? 1 : -1, .id = ++g->next_mob_id,
                            .hp = def->health > 0 ? def->health : 1,
                            .size = def->size_min + (span > 1 ? (int)(game_random(g) % (uint32_t)span) : 0) };
        return i;
    }
    return -1;
}

/* Whether a kind of creature will live in a kind of tree. */
static bool perch_tree(const MobDef *def, int tree)
{
    if (tree < 0) return false;
    if (def->perch_tree_count == 0) return true;
    for (int i = 0; i < def->perch_tree_count; i++)
        if (def->perch_trees[i] == tree) return true;
    return false;
}

static bool trunk_for(const World *w, const MobDef *def, int x, int y)
{
    return perch_tree(def, tree_trunk_at(w, x, y));
}

static int kind_near_player(const Game *g, int kind)
{
    int n = 0;
    for (int i = 0; i < MAX_MOBS; i++)
        if (g->mobs[i].active && g->mobs[i].kind == kind && in_range(g, g->mobs[i].x, g->mobs[i].y)) n++;
    return n;
}

/* A random event on the trunk of a tree can bring something that lives in
 * trees to the top of it. */
static bool try_spawn_in_tree(Game *g, int x, int y)
{
    const World *w = &g->world;
    for (int kind = 0; kind < content.mob_count; kind++) {
        const MobDef *def = &content.mobs[kind];
        if (!def->perches || !trunk_for(w, def, x, y)) continue;
        int top = y;
        while (trunk_for(w, def, x, top - 1)) top--;
        if (!in_range(g, x, top) || mobs_near_player(g) >= content.mob_cap) return false;
        if (def->cap > 0 && kind_near_player(g, kind) >= def->cap) return false;
        for (int i = 0; i < MAX_MOBS; i++)
            if (g->mobs[i].active && g->mobs[i].kind == kind && g->mobs[i].x == world_wrap_x(w, x)) return false;
        if (chance(g) >= def->spawn_chance) return false;
        int slot = mob_spawn(g, kind, x, top, false, false);
        if (slot < 0) return false;
        g->mobs[slot].state = MOB_PERCH;
        g->mobs[slot].home_x = g->mobs[slot].x;
        return true;
    }
    return false;
}

static bool flyable(const World *w, int x, int y);

/* Whether it is the time of day a creature turns up at. */
static bool spawns_now(const Game *g, const MobDef *def)
{
    bool day;
    game_day_phase(g, &day);
    return def->spawn_time == 0 || (def->spawn_time == 1) == day;
}

/* A random event on a square of open sky, well clear of the ground, can bring something that lives up there. */
static bool try_spawn_in_sky(Game *g, int x, int y)
{
    const World *w = &g->world;
    for (int k = 0; k <= 4; k++)
        if (!flyable(w, x, y + k)) return false;
    for (int kind = 0; kind < content.mob_count; kind++) {
        const MobDef *def = &content.mobs[kind];
        if (!def->flies || !def->spawn_sky || def->spawn_chance <= 0.0f) continue;
        if (!spawns_in(w, def, x, y) || !spawns_now(g, def)) continue;
        if (!in_range(g, x, y) || mobs_near_player(g) >= content.mob_cap) return false;
        if (def->cap > 0 && kind_near_player(g, kind) >= def->cap) continue;
        if (chance(g) >= def->spawn_chance) continue;
        return mob_spawn(g, kind, x, y, false, false) >= 0;
    }
    return false;
}

/* A random event on a square of water can bring something that lives in it. */
static bool try_spawn_in_water(Game *g, int x, int y)
{
    const World *w = &g->world;
    if (!world_is_water(w, x, y)) return false;
    for (int kind = 0; kind < content.mob_count; kind++) {
        const MobDef *def = &content.mobs[kind];
        if (!def->swims || !def->spawn_water) continue;
        if (!spawns_in(w, def, x, y) || !spawns_now(g, def)) continue;
        if (!in_range(g, x, y) || mobs_near_player_in_water(g) >= content.mob_water_cap) return false;
        if (def->cap > 0 && kind_near_player(g, kind) >= def->cap) continue;
        if (chance(g) >= def->spawn_chance) continue;
        return mob_spawn(g, kind, x, y, false, false) >= 0;
    }
    return false;
}

bool mobs_try_spawn(Game *g, int x, int y)
{
    const World *w = &g->world;
    if (!world_in_bounds(w, x, y) || !game_alive(g)) return false; /* nothing turns up with nobody there */
    if (try_spawn_in_tree(g, x, y)) return true;
    if (try_spawn_in_water(g, x, y)) return true;
    if (try_spawn_in_sky(g, x, y)) return true;
    if (world_get(w, x, y) != TILE_AIR) return false;
    if (world_get_at(w, LAYER_BACKGROUND, x, y) != TILE_AIR || world_is_water(w, x, y)) return false;
    TileType ground = world_get(w, x, y + 1);

    for (int kind = 0; kind < content.mob_count; kind++) {
        const MobDef *def = &content.mobs[kind];
        bool here = false;
        for (int i = 0; i < def->spawn_above_count; i++) here = here || def->spawn_above[i] == ground;
        if (!here || def->perches || def->swims || def->flies || def->spawn_chance <= 0.0f || !spawns_in(w, def, x, y)) continue;
        if (!spawns_now(g, def) || (def->spawn_cave && world_get_at(w, LAYER_WALL, x, y) == TILE_AIR)) continue;
        /* Only near the player, and only while there is room for more. */
        if (!in_range(g, x, y) || mobs_near_player(g) >= content.mob_cap) return false;
        /* (Where several kinds live, each has its own chance in turn.) */
        if (def->cap > 0 && kind_near_player(g, kind) >= def->cap) continue;
        if (chance(g) >= def->spawn_chance) continue;
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

int mob_poison(const Mob *m)
{
    return m->poison & 0xFF;
}

void mob_poison_add(Mob *m, int points, int every)
{
    int left = mob_poison(m) + points;
    if (left > 255) left = 255;
    if (every < 1) every = 1;
    if (every > 255) every = 255;
    m->poison = left | (every << 8) | (m->poison & ~0xFFFF);
}

bool mob_shorn(const Mob *m)
{
    return m->active && content.mobs[m->kind].shear_item != ITEM_NONE && m->timer > 0;
}

bool mob_shear(Game *g, int mob)
{
    if (mob < 0 || mob >= MAX_MOBS || !g->mobs[mob].active) return false;
    Mob *m = &g->mobs[mob];
    const MobDef *def = &content.mobs[m->kind];
    if (def->shear_item == ITEM_NONE || m->baby || m->timer > 0) return false;
    int span = def->shear_max - def->shear_min + 1;
    int n = def->shear_min + (span > 1 ? (int)(game_random(g) % (uint32_t)span) : 0);
    for (int k = 0; k < n; k++) game_drop_item(g, def->shear_item, m->x, m->y);
    m->timer = def->shear_ticks > 0 ? def->shear_ticks : 1;
    return true;
}

bool mob_milk(Game *g, int mob)
{
    if (mob < 0 || mob >= MAX_MOBS || !g->mobs[mob].active) return false;
    Mob *m = &g->mobs[mob];
    const MobDef *def = &content.mobs[m->kind];
    if (def->milk_item == ITEM_NONE || def->perches || m->baby || m->state > 0) return false;
    game_drop_item(g, def->milk_item, m->x, m->y);
    m->state = def->milk_ticks > 0 ? def->milk_ticks : 1;
    return true;
}

/* Whether a kind of creature turns up in a column's biome. */
static bool spawns_in(const World *w, const MobDef *def, int x, int y)
{
    if (def->spawn_biome_count == 0) return true;
    for (int i = 0; i < def->spawn_biome_count; i++)
        if (def->spawn_biomes[i] == world_biome_at(w, x, y)) return true;
    return false;
}

static void kill_mob(Game *g, int mob, bool cooked)
{
    if (mob < 0 || mob >= MAX_MOBS || !g->mobs[mob].active) return;
    Mob *m = &g->mobs[mob];
    const MobDef *def = &content.mobs[m->kind];
    for (int i = 0; !m->baby && i < def->drop_count; i++) {
        if (def->drops[i].chance < 1.0f && chance(g) >= def->drops[i].chance) continue;
        int span = def->drops[i].max - def->drops[i].min + 1;
        int n = def->drops[i].min + (span > 1 ? (int)(game_random(g) % (uint32_t)span) : 0);
        /* (Killed by a spell that cooks, what it leaves that can be cooked is.) */
        ItemId item = def->drops[i].item;
        if (cooked && item_def(item)->cooks_into != ITEM_NONE) item = item_def(item)->cooks_into;
        for (int k = 0; k < n; k++) {
            if (def->drops[i].sized) game_drop_item_sized(g, item, m->x, m->y, m->size > 0 ? m->size : 1);
            else game_drop_item(g, item, m->x, m->y);
        }
    }
    m->active = false;
}

bool mob_weak_to(const Game *g, int mob, int element)
{
    if (mob < 0 || mob >= MAX_MOBS || !g->mobs[mob].active || element < 0) return false;
    return content.mobs[g->mobs[mob].kind].weak_to == element;
}

static void drop_on(Game *g, Mob *m, int column);
static bool trunk_for(const World *w, const MobDef *def, int x, int y);

/* What bites or stings someone with thorns on is hurt for it - without that making it any angrier than it was. */
static void thorned(Game *g, Mob *m)
{
    int thorns = game_boons(g).thorns;
    if (thorns <= 0 || !m->active) return;
    if (m->hp <= 0) m->hp = content.mobs[m->kind].health;
    m->hp -= thorns;
    if (m->hp <= 0) kill_mob(g, (int)(m - g->mobs), false);
}

/* Something that attacks, hit and still alive, goes for whoever hit it there and then: one up a tree comes down
 * on them, and one that was resting from its last attack has done resting. */
static void provoke(Game *g, Mob *m)
{
    const MobDef *def = &content.mobs[m->kind];
    if (!game_alive(g)) return;
    if (def->perches && def->bite > 0.0f) {
        bool up = m->state == MOB_PERCH && trunk_for(&g->world, def, m->x, m->y);
        if (m->state == MOB_PERCH || m->state == MOB_SEEK) m->home_x = m->x;
        m->state = MOB_HUNT;
        m->timer = def->chase_ticks;
        if (up && !world_is_solid(&g->world, m->x, m->y + 1)) drop_on(g, m, g->player.feet.x);
    } else if (def->sting_ticks > 0 || def->hunts) {
        m->timer = 0;
    }
}

bool mob_hurt_by(Game *g, int mob, int damage, int element, bool cooks)
{
    if (mob < 0 || mob >= MAX_MOBS || !g->mobs[mob].active || damage <= 0) return false;
    Mob *m = &g->mobs[mob];
    if (content.mobs[m->kind].immune && !mob_weak_to(g, mob, element)) return false;
    if (m->hp <= 0) m->hp = content.mobs[m->kind].health; /* (from before creatures had any) */
    m->hp -= damage;
    if (m->hp > 0) {
        provoke(g, m);
        return false;
    }
    kill_mob(g, mob, cooks);
    return true;
}

bool mob_hurt(Game *g, int mob, int damage)
{
    return mob_hurt_by(g, mob, damage, -1, false);
}

void mob_kill(Game *g, int mob)
{
    kill_mob(g, mob, false);
}

/* Sets a creature walking one block the way it faces: along the level, up a
 * step of one block, or down one. It will not cross a gap or drop further;
 * faced with either it turns round instead. */
static bool start_step(Game *g, Mob *m)
{
    const World *w = &g->world;
    int nx = m->x + m->dir, ny = m->y;
    if (can_stand(w, nx, m->y)) ny = m->y;
    else if (world_is_solid(w, nx, m->y) && !world_is_solid(w, m->x, m->y - 1) && can_stand(w, nx, m->y - 1))
        ny = m->y - 1;
    else if (!world_is_solid(w, nx, m->y) && can_stand(w, nx, m->y + 1)) ny = m->y + 1;
    else {
        m->dir = -m->dir;
        return false;
    }
    m->to_x = world_wrap_x(w, nx);
    m->to_y = ny;
    m->step_total = m->step_left = content.mobs[m->kind].step_ticks;
    return true;
}

/* ---- creatures that live in trees --------------------------------------- */

/* One block up or down the trunk it is on. */
static void climb(Mob *m, int dy)
{
    m->to_x = m->x;
    m->to_y = m->y + dy;
    m->step_total = m->step_left = content.mobs[m->kind].step_ticks;
}

/* Whether a column has a trunk it could live on, somewhere about its own height. */
static bool tree_in_column(const World *w, const MobDef *def, int x, int y)
{
    for (int dy = -8; dy <= 8; dy++)
        if (trunk_for(w, def, x, y + dy)) return true;
    return false;
}

/* Goes a step nearer a column on foot, coming down off a trunk first.
 * Returns false if there is no getting any nearer that way. */
static bool head_for(Game *g, Mob *m, int column)
{
    const World *w = &g->world;
    int dx = world_dx(w, m->x, column);
    if (!world_is_solid(w, m->x, m->y + 1)) {
        climb(m, 1);
        return true;
    }
    if (dx == 0) return true;
    m->dir = dx > 0 ? 1 : -1;
    return start_step(g, m);
}

/* Up the trunk it has reached, to sit at the top. */
static void go_up(Mob *m, const World *w, const MobDef *def)
{
    if (trunk_for(w, def, m->x, m->y - 1) && !world_is_solid(w, m->x, m->y - 1)) {
        climb(m, -1);
        return;
    }
    m->state = MOB_PERCH;
    m->home_x = m->x;
}

/* Lets go of the tree and comes down in one fall: onto the column asked for
 * if the way down to it is clear, and otherwise straight down its own. */
static void drop_on(Game *g, Mob *m, int column)
{
    const World *w = &g->world;
    int dx = world_dx(w, m->x, column);
    for (int attempt = 0; attempt < 2; attempt++, dx = 0) {
        int x = m->x + dx, y = m->y;
        bool clear = !world_is_solid(w, x, y);
        while (clear && y < w->height - 1 && !world_is_solid(w, x, y + 1)) y++;
        if (!clear || y <= m->y) continue;
        m->to_x = world_wrap_x(w, x);
        m->to_y = y;
        /* The further the fall the longer it takes, but not in proportion: it gathers speed. */
        int ticks = 2 + (int)sqrtf((float)(y - m->y));
        m->step_total = m->step_left = ticks;
        if (dx != 0) m->dir = dx > 0 ? 1 : -1;
        return;
    }
}

static void perch_tick(Game *g, Mob *m, const MobDef *def)
{
    const World *w = &g->world;
    const Player *p = &g->player;
    bool on_trunk = trunk_for(w, def, m->x, m->y);
    int pdx = world_dx(w, m->x, p->feet.x), pdy = p->feet.y - m->y;

    switch ((MobState)m->state) {
    case MOB_PERCH:
        if (!on_trunk) {
            /* Its tree is gone from under it. */
            m->state = MOB_SEEK;
        } else if (trunk_for(w, def, m->x, m->y - 1)) {
            go_up(m, w, def); /* the tree has grown, or it is still on its way back up it */
        } else if (m->timer > 0) {
            /* At the top again after an attack: it waits there a moment before it will come down for another. */
            m->timer--;
        } else if (game_alive(g) && abs(pdx) <= def->ambush_range && pdy > 0 &&
                   pdy <= def->ambush_depth) {
            /* Somebody is walking by underneath: it lets go and drops on them,
             * all the way down in one go. */
            m->state = MOB_HUNT;
            m->home_x = m->x;
            m->timer = def->chase_ticks;
            drop_on(g, m, p->feet.x);
        }
        break;
    case MOB_HUNT:
        if (m->timer > 0) m->timer--;
        if (game_alive(g) && abs(pdx) <= 1 && pdy >= -1 && pdy <= 1) {
            /* Got them. A bite wakes anybody. */
            game_wake(g);
            game_attacked(g, def->bite);
            m->state = MOB_RETURN;
            m->timer = def->bite_rest;
            thorned(g, m);
        } else if (!game_alive(g) || m->timer == 0 || abs(pdx) > def->seek_radius || !head_for(g, m, p->feet.x)) {
            /* Too far, too long, or no way to them: back up the tree. */
            m->state = MOB_RETURN;
            m->timer = def->bite_rest;
        }
        break;
    case MOB_RETURN:
        if (!tree_in_column(w, def, m->home_x, m->y)) {
            m->state = MOB_SEEK;
        } else if (m->x == m->home_x && on_trunk) {
            go_up(m, w, def);
        } else if (m->x == m->home_x || !head_for(g, m, m->home_x)) {
            m->state = MOB_SEEK; /* cannot get home: any tree will do */
        }
        break;
    default: {
        /* No tree: make for the nearest one in sight. */
        if (on_trunk) {
            go_up(m, w, def);
            break;
        }
        int found = 0;
        for (int d = 1; d <= def->seek_radius && found == 0; d++) {
            if (tree_in_column(w, def, m->x + d * m->dir, m->y)) found = d * m->dir;
            else if (tree_in_column(w, def, m->x - d * m->dir, m->y)) found = -d * m->dir;
        }
        /* With none in sight, or no way to it, it runs on the way it was going. */
        if (found != 0 && m->timer == 0) {
            if (!head_for(g, m, m->x + found)) m->timer = 4 * def->step_ticks + (int)(game_random(g) % 20u);
        } else {
            if (m->timer > 0) m->timer--;
            if (world_is_solid(w, m->x, m->y + 1)) start_step(g, m);
            else climb(m, 1);
        }
        m->state = MOB_SEEK;
        break;
    }
    }
}

/* ---- creatures of the water --------------------------------------------- */

static bool swimmable(const World *w, int x, int y)
{
    return world_in_bounds(w, x, y) && !world_is_solid(w, x, y) && world_is_water(w, x, y);
}

/* Open sky: nothing in it and nothing behind it, and not right down on the ground. */
static bool flyable(const World *w, int x, int y)
{
    return world_in_bounds(w, x, y) && y >= 1 && !world_is_solid(w, x, y) && !world_is_water(w, x, y) &&
           world_get_at(w, LAYER_WALL, x, y) == TILE_AIR && !world_is_solid(w, x, y + 1) && !world_is_water(w, x, y + 1);
}

/* The element a creature of the water or of the air lives in. */
static bool in_element(const World *w, const MobDef *def, int x, int y)
{
    return def->flies ? flyable(w, x, y) : swimmable(w, x, y);
}

static bool swim_to(Game *g, Mob *m, int dx, int dy)
{
    const World *w = &g->world;
    if ((dx == 0 && dy == 0) || !in_element(w, &content.mobs[m->kind], m->x + dx, m->y + dy)) return false;
    /* Not through the corner of a block on the slant. */
    if (dx != 0 && dy != 0 && (world_is_solid(w, m->x + dx, m->y) || world_is_solid(w, m->x, m->y + dy))) return false;
    m->to_x = world_wrap_x(w, m->x + dx);
    m->to_y = m->y + dy;
    m->step_total = m->step_left = content.mobs[m->kind].step_ticks;
    if (dx != 0) m->dir = dx;
    return true;
}

/* A step toward (or, with `away`, from) a place, the straightest the water allows. */
static bool swim_toward(Game *g, Mob *m, int dx, int dy, bool away)
{
    int sx = dx > 0 ? 1 : dx < 0 ? -1 : 0, sy = dy > 0 ? 1 : dy < 0 ? -1 : 0;
    if (away) sx = sx != 0 ? -sx : m->dir, sy = -sy;
    return swim_to(g, m, sx, sy) || (abs(dx) >= abs(dy) ? swim_to(g, m, sx, 0) || swim_to(g, m, 0, sy)
                                                         : swim_to(g, m, 0, sy) || swim_to(g, m, sx, 0));
}

static void swim_tick(Game *g, Mob *m, const MobDef *def)
{
    const World *w = &g->world;
    const Player *p = &g->player;
    if (!in_element(w, def, m->x, m->y)) {
        /* (A creature of the air that has come down too low goes up again, if it can.) */
        if (def->flies && flyable(w, m->x, m->y - 1) && swim_to(g, m, 0, -1)) return;
        if (def->flies && !world_is_solid(w, m->x, m->y - 1) && !world_is_water(w, m->x, m->y)) {
            m->to_x = m->x;
            m->to_y = m->y - 1;
            m->step_total = m->step_left = def->step_ticks;
            return;
        }
        /* Out of its element: it drops to whatever is under it, and lies there. */
        if (!world_is_solid(w, m->x, m->y + 1)) {
            m->to_x = m->x;
            m->to_y = m->y + 1;
            m->step_total = m->step_left = 1;
        }
        return;
    }
    if (def->hunts) {
        /* The nearer of the swimmer's two squares. */
        int pdx = world_dx(w, m->x, p->feet.x), pdy = p->feet.y - m->y;
        if (abs(pdy + (-1)) < abs(pdy)) pdy -= 1;
        if (m->timer > 0) {
            /* Just bitten: off it goes, before it comes round again. */
            m->timer--;
            if (swim_toward(g, m, pdx, pdy, true)) return;
        } else if (game_alive(g) && game_player_in_water(g) && pdx * pdx + pdy * pdy <= def->sight * def->sight) {
            if (abs(pdx) <= 1 && abs(pdy) <= 1) {
                game_wake(g);
                game_attacked(g, def->bite);
                m->timer = def->bite_rest;
                thorned(g, m);
                return;
            }
            if (swim_toward(g, m, pdx, pdy, false)) return;
        }
    }
    /* Otherwise it idles about: mostly on the way it is going, sometimes up or down. */
    if (chance(g) >= def->move_chance) return;
    if (chance(g) < def->turn_chance) m->dir = -m->dir;
    uint32_t roll = game_random(g) % 4u;
    int dy = roll == 0 ? -1 : roll == 1 ? 1 : 0;
    if (!swim_to(g, m, m->dir, dy) && !swim_to(g, m, m->dir, 0) && !swim_to(g, m, 0, dy)) m->dir = -m->dir;
}

void mobs_tick(Game *g)
{
    World *w = &g->world;
    for (int i = 0; i < MAX_MOBS; i++) {
        Mob *m = &g->mobs[i];
        if (!m->active) continue;
        const MobDef *def = &content.mobs[m->kind];
        /* What a creature makes of "the player" is whichever character is nearest it. */
        game_use_character(g, game_nearest_character(g, m->x, m->y));

        m->age++;
        if (m->baby && m->age >= def->baby_ticks) m->baby = false;
        /* A wild one that has got too far from the player is gone. (With nobody
         * alive to be far from, things stay as they are.) */
        if (!m->owned && game_alive(g) && !in_range(g, m->x, m->y)) {
            m->active = false;
            continue;
        }
        if (m->hp <= 0) m->hp = def->health > 0 ? def->health : 1; /* (from before creatures had any) */
        /* Poison in it does its work a point at a time. */
        if (mob_poison(m) > 0) {
            int left = m->poison & 0xFF, every = (m->poison >> 8) & 0xFF, waited = (m->poison >> 16) + 1;
            if (waited >= every) {
                waited = 0;
                left--;
            }
            m->poison = left > 0 ? left | (every << 8) | (waited << 16) : 0;
            if (waited == 0 && mob_hurt(g, i, 1)) continue;
        }
        /* What was shorn off it grows back; one that has stung is a while about stinging again. */
        if ((def->shear_item != ITEM_NONE || def->sting_ticks > 0) && m->timer > 0) m->timer--;
        if (def->milk_item != ITEM_NONE && !def->perches && m->state > 0) m->state--;
        if (m->step_left > 0) {
            if (--m->step_left > 0) continue;
            /* There. It goes straight on into whatever it does next, with
             * no pause between one step and the following one. */
            m->x = m->to_x;
            m->y = m->to_y;
        }
        if (def->swims || def->flies) {
            swim_tick(g, m, def);
            continue;
        }
        /* With the ground gone from under it, it falls - unless it has hold of a tree. */
        if (!world_is_solid(w, m->x, m->y + 1) && !(def->perches && trunk_for(w, def, m->x, m->y))) {
            m->to_x = m->x;
            m->to_y = m->y + 1;
            m->step_total = m->step_left = 1;
            continue;
        }
        if (def->perches) {
            perch_tick(g, m, def);
            continue;
        }
        if (def->sting_ticks > 0 && game_alive(g) && g->vitals.respawn == 0) {
            /* One that stings: a player beside it is stung; one it can see, it makes for. */
            const Player *p = &g->player;
            int pdx = world_dx(w, m->x, p->feet.x), pdy = p->feet.y - m->y;
            if (abs(pdx) <= 1 && pdy >= -1 && pdy <= 2) {
                if (m->timer == 0) {
                    /* (What the player wears keeps some of it out.) */
                    game_poison(g, (int)((float)def->sting_ticks * (1.0f - game_protection(g)) + 0.5f));
                    game_attacked(g, 0.0f); /* (no harm of itself: only that they know it) */
                    m->timer = def->sting_rest > 0 ? def->sting_rest : 1;
                    thorned(g, m);
                }
                if (pdx != 0) m->dir = pdx > 0 ? 1 : -1;
                continue;
            }
            if (m->timer == 0 && abs(pdx) <= def->sting_sight && abs(pdy) <= def->sting_sight) {
                m->dir = pdx > 0 ? 1 : -1;
                if (start_step(g, m)) continue;
            }
        }
        if (chance(g) < def->move_chance) {
            if (chance(g) < def->turn_chance) m->dir = -m->dir;
            start_step(g, m);
            continue;
        }
        /* Now and then a grown one leaves an egg where it stands, if nothing is there. */
        if (!m->baby && def->lays != TILE_AIR && chance(g) < def->lay_chance &&
            world_get(w, m->x, m->y) == TILE_AIR) {
            /* Mostly its own egg; sometimes that of a kind it breeds on into. */
            TileType egg = def->lays;
            if (def->lay_next_count > 0 && chance(g) < def->lay_next_chance) {
                TileType other = content.mobs[def->lay_next[game_random(g) % (uint32_t)def->lay_next_count]].lays;
                if (other != TILE_AIR) egg = other;
            }
            if (world_get_at(w, tile_def(egg)->layer, m->x, m->y) == TILE_AIR)
                world_set_at(w, tile_def(egg)->layer, m->x, m->y, egg);
        }
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
    /* Up a step it rises and then goes forward; down one it goes forward and
     * then drops: round the corner of the block, never through it. */
    int dx = world_dx(&g->world, m->x, m->to_x), dy = m->to_y - m->y;
    float ox, oy;
    /* It sets off and comes to rest gently rather than all at once. */
    if (dy == 0) t = t * t * (3.0f - 2.0f * t);
    game_step_offset(dx, dy, dy < 0, t, &ox, &oy);
    if (content.mobs[m->kind].swims) {
        /* Through the water it goes straight, slant or no. */
        ox = (float)dx * t;
        oy = (float)dy * t;
    }
    if (dy > 1) {
        /* A long way down is a fall: it gathers speed, and carries across as it goes. */
        ox = (float)dx * t;
        oy = (float)dy * t * t;
    }
    *x = (float)m->x + ox;
    *y = (float)m->y + oy;
}

bool game_mob_walking(const Game *g, int mob, float *stride)
{
    const Mob *m = &g->mobs[mob];
    bool walking = m->step_left > 0 && m->step_total > 0 && (m->to_x != m->x || m->to_y != m->y);
    if (stride) {
        float into_tick = g->tick_timer < 1.0f ? g->tick_timer : 1.0f;
        *stride = walking ? ((float)(m->step_total - m->step_left) + into_tick) / (float)m->step_total : 0.0f;
    }
    return walking;
}

int game_mob_at(const Game *g, float x, float y)
{
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!g->mobs[i].active) continue;
        float mx, my;
        game_mob_pos(g, i, &mx, &my);
        float dx = (float)world_dx(&g->world, (int)floorf(mx), (int)floorf(x)) + (x - floorf(x)) - (mx - floorf(mx));
        /* (A big one reaches out round its own square: half its length each way.) */
        float half = g->mobs[i].size > 1 ? ((float)g->mobs[i].size - 1.0f) / 2.0f : 0.0f;
        if (dx >= -half && dx < 1.0f + half && y >= my - half / 2.0f && y < my + 1.0f + half / 2.0f) return i;
    }
    return -1;
}
