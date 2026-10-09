#include "ship.h"

#include <stdlib.h>
#include <string.h>

#include "mob.h"
#include "water.h"

#define MEASURE_EVERY 20
#define GUN_RANGE 14

bool game_adrift(const Game *g)
{
    return content.worldgen.adrift && content.worldgen.sea_count > 0 && g->world.sea_row >= 0;
}

const SeaDef *game_sea(const Game *g)
{
    if (!game_adrift(g)) return NULL;
    return &content.worldgen.seas[g->sea >= 0 && g->sea < content.worldgen.sea_count ? g->sea : 0];
}

bool game_helm_open(const Game *g)
{
    return g->bench_open && tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y))->helm;
}

/* Whether a square of a layer is where its block is counted: the foot of a tall one, the first half of a wide one. */
static bool counted(const World *w, int layer, int x, int y, TileType t)
{
    const TileDef *def = tile_def(t);
    if (def->pair_of != TILE_AIR) return false;
    return def->height <= 1 || world_get_at(w, layer, x, y + 1) != t;
}

void ship_measure(Game *g)
{
    World *w = &g->world;
    const WorldGenDef *gen = &content.worldgen;
    if (!game_adrift(g)) return;
    ShipReport r;
    memset(&r, 0, sizeof r);
    int water = w->sea_row + 1, lng = gen->keel_long, above = gen->keel_above, below = gen->keel_below;

    /* The keel: the best aboard, looked for in the most room any keel gives. (Nothing that comes alongside has one.) */
    int most_long = lng, most_above = above, most_below = below;
    for (TileType t = 1; t < content.tile_count; t++) {
        const TileDef *def = tile_def(t);
        if (def->keel_level <= 0) continue;
        if (def->keel_long > most_long) most_long = def->keel_long;
        if (def->keel_above > most_above) most_above = def->keel_above;
        if (def->keel_below > most_below) most_below = def->keel_below;
    }
    for (int dx = -(most_long / 2); dx < most_long - most_long / 2; dx++)
        for (int y = water - most_above; y < water + most_below; y++)
            for (int layer = LAYER_FOREGROUND; layer <= LAYER_BACKGROUND; layer++) {
                if (!world_in_bounds(w, w->spawn_x + dx, y)) continue;
                const TileDef *def = tile_def(world_get_at(w, layer, w->spawn_x + dx, y));
                if (def->keel_level <= r.keel) continue;
                r.keel = def->keel_level;
                lng = def->keel_long, above = def->keel_above, below = def->keel_below;
            }
    /* (More room would take in what is alongside: it is let go first.) */
    if (r.keel != g->ship.keel && g->ship.measured && g->alongside > 0) ship_cast_off(g);
    r.x0 = -(lng / 2), r.x1 = r.x0 + lng - 1;
    r.y0 = water - above, r.y1 = water + below - 1;

    int weakest = 0, first = 1, last = 0, lowest = -1;
    for (int dx = r.x0; dx <= r.x1; dx++) {
        int top = -1;
        for (int y = r.y0; y <= r.y1; y++) {
            if (!world_in_bounds(w, w->spawn_x + dx, y)) continue;
            for (int layer = LAYER_FOREGROUND; layer <= LAYER_BACKGROUND; layer++) {
                TileType t = world_get_at(w, layer, w->spawn_x + dx, y);
                const TileDef *def = tile_def(t);
                if (t == TILE_AIR || !counted(w, layer, w->spawn_x + dx, y, t)) continue;
                r.stats[SHIP_BURDEN] += def->ship[SHIP_BURDEN];
                r.stats[SHIP_WEIGHT] += def->ship[SHIP_WEIGHT];
                r.stats[SHIP_SAIL] += def->ship[SHIP_SAIL];
                /* (An engine that burns something gives nothing cold.) */
                if (def->fuel_capacity <= 0 || game_fuel_at(g, w->spawn_x + dx, y) > 0) r.stats[SHIP_THRUST] += def->ship[SHIP_THRUST];
                r.stats[SHIP_GUNS] += def->ship[SHIP_GUNS];
                for (int k = SHIP_REACH; k <= SHIP_PUMP; k++)
                    if (def->ship[k] > r.stats[k]) r.stats[k] = def->ship[k];
                if (def->ship[SHIP_BURDEN] > 0 && layer == LAYER_FOREGROUND) {
                    /* (The hull is as strong as the weakest of it.) */
                    if (def->ship[SHIP_STRENGTH] > 0 && (weakest == 0 || def->ship[SHIP_STRENGTH] < weakest)) weakest = def->ship[SHIP_STRENGTH];
                    if (top < 0) top = y;
                }
            }
        }
        if (top < 0) continue;
        if (first > last) first = dx;
        last = dx;
    }
    r.stats[SHIP_STRENGTH] = weakest;
    /* Freeboard: how far out of the water her side stands where it is lowest, from one end of her to the other. */
    for (int dx = first; dx <= last; dx++) {
        int top = -1;
        for (int y = r.y0; y <= r.y1 && top < 0; y++)
            if (world_in_bounds(w, w->spawn_x + dx, y) && world_is_solid(w, w->spawn_x + dx, y)) top = y; /* (her deck, or her side) */
        int out = top < 0 ? 0 : water - top;
        if (out < 0) out = 0;
        if (lowest < 0 || out < lowest) lowest = out;
    }
    r.freeboard = lowest < 0 ? 0 : lowest;
    r.length = first > last ? 0 : last - first + 1;
    r.bow = first > last ? 0 : last;
    r.way = (r.stats[SHIP_SAIL] + r.stats[SHIP_THRUST]) * 4 / (20 + r.stats[SHIP_WEIGHT]);
    r.measured = true;
    g->ship = r;

    w->build_limit = true;
    w->build_x0 = r.x0, w->build_x1 = r.x1, w->build_y0 = r.y0, w->build_y1 = r.y1;
    w->build_room = r.stats[SHIP_BURDEN] - r.stats[SHIP_WEIGHT];
}

int ship_has(const Game *g, int need)
{
    const ShipReport *r = &g->ship;
    switch (need) {
    case NEED_KEEL: return r->keel;
    case NEED_WAY: return r->way;
    case NEED_FREEBOARD: return r->freeboard;
    case NEED_STRENGTH: return r->stats[SHIP_STRENGTH];
    case NEED_CABLE: return r->stats[SHIP_CABLE];
    case NEED_DEPTH: return r->stats[SHIP_DEPTH];
    case NEED_LIGHT: return r->stats[SHIP_LIGHT];
    case NEED_GUNS: return r->stats[SHIP_GUNS];
    case NEED_REACH: return r->stats[SHIP_REACH];
    default: return 0;
    }
}

bool ship_can_sail(const Game *g, int sea, int *lacking)
{
    if (lacking) *lacking = -1;
    if (!game_adrift(g) || sea < 0 || sea >= content.worldgen.sea_count) return false;
    for (int k = 0; k < SEA_NEEDS; k++)
        if (ship_has(g, k) < content.worldgen.seas[sea].needs[k]) {
            if (lacking) *lacking = k;
            return false;
        }
    return true;
}

bool ship_set_course(Game *g, int sea)
{
    if (!ship_can_sail(g, sea, NULL) || sea == g->sea || g->passage > 0) return false;
    ship_cast_off(g);
    g->sea_to = sea;
    g->passage = content.worldgen.seas[sea].passage_ticks > 0 ? content.worldgen.seas[sea].passage_ticks : 1;
    return true;
}

const char *ship_alongside(const Game *g)
{
    const SeaDef *sea = game_sea(g);
    if (!sea || g->alongside <= 0 || g->alongside > sea->alongside_count) return NULL;
    return sea->alongside[g->alongside - 1].label;
}

static const StructureDef *structure_named(const char *name)
{
    for (int i = 0; i < content.structure_count; i++)
        if (strcmp(content.structures[i].name, name) == 0) return &content.structures[i];
    return NULL;
}

bool ship_cast_off(Game *g)
{
    World *w = &g->world;
    const SeaDef *sea = game_sea(g);
    if (!sea || g->alongside <= 0) return false;
    const StructureDef *s = g->alongside <= sea->alongside_count ? structure_named(sea->alongside[g->alongside - 1].structure) : NULL;
    /* It goes: but only what it brought. A square somebody has dug is empty already, and one somebody has built on is theirs. */
    for (int sy = 0; s && sy < s->height; sy++)
        for (int sx = 0; sx < s->width; sx++) {
            int x = world_wrap_x(w, g->alongside_x + sx), y = g->alongside_base - (s->height - 1) + sy;
            if (s->cells[sy][sx].keep || y < 1 || y >= w->height - 1) continue;
            TileType front = s->cells[sy][sx].front, back = s->cells[sy][sx].back, wall = s->cells[sy][sx].wall;
            if (front != TILE_AIR && world_get_at(w, LAYER_FOREGROUND, x, y) == front) {
                game_forget_storage(g, x, y);
                world_set_at(w, LAYER_FOREGROUND, x, y, TILE_AIR);
            }
            if (back != TILE_AIR && world_get_at(w, LAYER_BACKGROUND, x, y) == back) {
                game_forget_storage(g, x, y);
                world_set_at(w, LAYER_BACKGROUND, x, y, TILE_AIR);
            }
            if (wall != TILE_AIR && world_get_at(w, LAYER_WALL, x, y) == wall) world_set_at(w, LAYER_WALL, x, y, TILE_AIR);
            if (y > w->sea_row && !world_is_solid(w, x, y) && world_get_at(w, LAYER_WALL, x, y) == TILE_AIR) world_set_water(w, x, y, WATER_FULL);
        }
    /* Whatever lived on it goes with it. */
    for (int i = 0; s && i < MAX_MOBS; i++) {
        Mob *m = &g->mobs[i];
        int dx = world_dx(w, g->alongside_x, m->x);
        if (m->active && !m->owned && dx >= 0 && dx < s->width && m->y <= g->alongside_base && m->y > g->alongside_base - s->height - 2) m->active = false;
    }
    g->alongside = 0;
    g->alongside_next = g->tick + (uint64_t)(sea->alongside_every > 0 ? sea->alongside_every : 1) / (uint64_t)(1 + g->ship.way / 2);
    return true;
}

static void come_alongside(Game *g)
{
    World *w = &g->world;
    const SeaDef *sea = game_sea(g);
    int total = 0;
    for (int i = 0; i < sea->alongside_count; i++)
        if (sea->alongside[i].cable <= g->ship.stats[SHIP_CABLE] && sea->alongside[i].reach <= g->ship.stats[SHIP_REACH]) total += sea->alongside[i].weight;
    if (total <= 0) return;
    int roll = (int)(game_random(g) % (uint32_t)total), pick = -1;
    for (int i = 0; i < sea->alongside_count && pick < 0; i++) {
        if (sea->alongside[i].cable > g->ship.stats[SHIP_CABLE] || sea->alongside[i].reach > g->ship.stats[SHIP_REACH]) continue;
        if (roll < sea->alongside[i].weight) pick = i;
        roll -= sea->alongside[i].weight;
    }
    if (pick < 0) return;
    /* Off her bow, touching: clear of everything she is, and of her room if she has grown to the end of it. */
    int x = w->spawn_x + g->ship.bow + 1, base = w->sea_row + sea->alongside[pick].sunk, wide = 0, high = 0;
    const StructureDef *s = structure_named(sea->alongside[pick].structure);
    if (!s) return;
    for (int sy = 0; sy < s->height; sy++)
        for (int sx = 0; sx < s->width; sx++) {
            int y = base - (s->height - 1) + sy;
            if (!world_in_bounds(w, x + sx, y)) continue;
            if (world_get(w, x + sx, y) != TILE_AIR || world_get_at(w, LAYER_BACKGROUND, x + sx, y) != TILE_AIR) return; /* (something is in the way: another time) */
        }
    if (!world_stamp(w, s->name, x, base, &wide, &high)) return;
    for (int sx = -1; sx <= wide; sx++) water_wake(w, world_wrap_x(w, x + sx));
    g->alongside = pick + 1;
    g->alongside_x = world_wrap_x(w, x);
    g->alongside_base = base;
    g->alongside_until = g->tick + (uint64_t)sea->alongside[pick].stay * (uint64_t)(1 + g->ship.stats[SHIP_CABLE]);
}

static bool open_sea(const World *w, int x, int y)
{
    return y > w->sea_row && world_is_water(w, x, y) && world_get_at(w, LAYER_WALL, x, y) == TILE_AIR;
}

bool ship_drop_adrift(const Game *g, int drop)
{
    if (!game_adrift(g) || drop < 0 || drop >= MAX_DROPS) return false;
    const Drop *d = &g->drops[drop];
    return d->active && !d->attached && d->collect == 0.0f && open_sea(&g->world, d->x, (int)(d->y + 0.001f));
}

static int drift_interval(const Game *g)
{
    int every = content.worldgen.drift_ticks * 4 / (4 + g->ship.way * 2);
    return every < 2 ? 2 : every;
}

float ship_drift_phase(const Game *g)
{
    int every = drift_interval(g);
    float done = ((float)(g->tick % (uint64_t)every) + g->tick_timer * (float)g->tick_rate) / (float)every;
    return done < 0.0f ? 0.0f : done > 1.0f ? 1.0f : done;
}

float ship_drop_slide(const Game *g, int drop)
{
    if (!ship_drop_adrift(g, drop)) return 0.0f;
    const World *w = &g->world;
    const Drop *d = &g->drops[drop];
    int row = (int)(d->y + 0.001f), nx = world_wrap_x(w, d->x - 1);
    /* (One that will go under her, or is held, waits where it is.) */
    return world_is_water(w, nx, row) ? ship_drift_phase(g) : 0.0f;
}

float ship_speed(const Game *g)
{
    return game_adrift(g) ? (float)(g->tick_rate > 0 ? g->tick_rate : 10) / (float)drift_interval(g) : 0.0f;
}

bool ship_hook(Game *g, int drop)
{
    if (!game_adrift(g) || drop < 0 || drop >= MAX_DROPS || !g->drops[drop].active || !game_alive(g)) return false;
    const ItemStack *held = inventory_selected(&g->inventory);
    int reach = held && held->count > 0 ? item_def((ItemId)held->id)->hook : 0;
    Drop *d = &g->drops[drop];
    if (reach <= 0 || d->attached || d->collect > 0.0f) return false;
    int dx = world_dx(&g->world, g->player.feet.x, d->x), dy = (int)d->y - g->player.feet.y;
    if (dx * dx + dy * dy > reach * reach) return false;
    /* Hauled in: it lands at their feet and is picked up like anything else. */
    d->x = g->player.feet.x;
    d->y = (float)g->player.feet.y;
    d->held_off = false;
    d->age = 1.0f;
    return true;
}

/* One step of the sea: everything adrift goes a square astern - under her if she is in the way - and what
 * meets a net is kept by it. */
static void drift_step(Game *g)
{
    World *w = &g->world;
    for (int i = 0; i < MAX_DROPS; i++) {
        Drop *d = &g->drops[i];
        if (!ship_drop_adrift(g, i)) continue;
        int row = (int)(d->y + 0.001f), nx = world_wrap_x(w, d->x - 1);
        if (world_dx(w, w->spawn_x, nx) < g->ship.x0 - content.worldgen.drift_ahead) {
            container_free(&g->inventory, d->container);
            d->active = false;
            continue;
        }
        int to = row;
        while (to < w->height - 2 && world_is_solid(w, nx, to)) to++;
        if (world_is_solid(w, nx, to) || !world_is_water(w, nx, to)) continue; /* (held against something) */
        d->x = nx;
        if (to != row) d->y = (float)to;
        for (int layer = LAYER_FOREGROUND; layer <= LAYER_BACKGROUND && d->active; layer++)
            if (tile_def(world_get_at(w, layer, nx, to))->catches && d->container == 0 && game_store_at(g, nx, to, (ItemId)d->item)) d->active = false;
    }
}

static void drift_spawn(Game *g, int ahead)
{
    const SeaDef *sea = game_sea(g);
    bool day;
    game_day_phase(g, &day);
    int total = 0;
    for (int i = 0; i < sea->drift_count; i++)
        if (sea->drift[i].when == 0 || (sea->drift[i].when == 1) == day) total += sea->drift[i].weight;
    if (total <= 0) return;
    int roll = (int)(game_random(g) % (uint32_t)total);
    for (int i = 0; i < sea->drift_count; i++) {
        if (!(sea->drift[i].when == 0 || (sea->drift[i].when == 1) == day)) continue;
        if (roll < sea->drift[i].weight) {
            game_drop_item(g, sea->drift[i].item, world_wrap_x(&g->world, g->world.spawn_x + g->ship.x1 + ahead), g->world.sea_row + 1);
            return;
        }
        roll -= sea->drift[i].weight;
    }
}

/* The open sea has no end: outside the ship's own columns it is full to the waterline and empty above it,
 * whatever has been displaced into it or drawn out of it. */
static void level_sea(Game *g)
{
    World *w = &g->world;
    for (int x = 0; x < w->width; x++) {
        int dx = world_dx(w, w->spawn_x, x);
        if (dx >= g->ship.x0 - 1 && dx <= g->ship.x1 + 1) continue;
        bool changed = false;
        for (int y = 1; y < w->height - 1; y++) {
            size_t cell = (size_t)y * w->width + x;
            if (w->layers[LAYER_WALL][cell] != TILE_AIR || tile_def((TileType)w->layers[LAYER_FOREGROUND][cell])->solid) continue;
            uint8_t want = y > w->sea_row ? WATER_FULL : 0;
            if (w->water[cell] == want) continue;
            w->water[cell] = want;
            changed = true;
        }
        if (changed) water_wake(w, x);
    }
}

static void trawl(Game *g)
{
    const SeaDef *sea = game_sea(g);
    for (int i = 0; i < MAX_STORAGES; i++) {
        Storage *st = &g->storages[i];
        if (!st->used) continue;
        const TileDef *def = tile_def(game_bench_at(g, st->x, st->y));
        if (def->trawl <= 0) continue;
        /* (Lying still it fishes too, slowly: the more way on her, the more ground it covers.) */
        int work = st->work + 10 * (1 + g->ship.way);
        if (work < def->trawl) {
            st->work = (int16_t)work;
            continue;
        }
        st->work = 0;
        int total = 0;
        for (int k = 0; k < sea->bed_count; k++)
            if (sea->bed[k].depth <= def->ship[SHIP_DEPTH]) total += sea->bed[k].weight;
        if (total <= 0) continue;
        int roll = (int)(game_random(g) % (uint32_t)total);
        for (int k = 0; k < sea->bed_count; k++) {
            if (sea->bed[k].depth > def->ship[SHIP_DEPTH]) continue;
            if (roll < sea->bed[k].weight) {
                game_store_at(g, st->x, st->y, sea->bed[k].item);
                break;
            }
            roll -= sea->bed[k].weight;
        }
    }
}

/* Her pumps: water that stands inside her, where the sea cannot get at it, is thrown over the side - so many
 * squares of it each time, from the top down. What the sea can reach under water is the sea, and stays. */
static void pump_bilge(Game *g)
{
    World *w = &g->world;
    int power = g->ship.stats[SHIP_PUMP];
    if (power <= 0) return;
    int x0 = g->ship.x0 - 1, x1 = g->ship.x1 + 1, y0 = w->sea_row + 1, y1 = g->ship.y1 + 1;
    int wide = x1 - x0 + 1, high = y1 - y0 + 1;
    if (wide <= 0 || high <= 0 || wide * high > 1 << 16) return;
    static uint8_t reached[1 << 16];
    static int32_t queue[1 << 16];
    int head = 0, tail = 0;
    memset(reached, 0, (size_t)(wide * high));
    for (int y = 0; y < high; y++)
        for (int x = 0; x < wide; x++) {
            if (x != 0 && x != wide - 1 && y != high - 1) continue;
            if (world_is_solid(w, w->spawn_x + x0 + x, y0 + y)) continue;
            reached[y * wide + x] = 1;
            queue[tail++] = y * wide + x;
        }
    while (head < tail) {
        int at = queue[head++], x = at % wide, y = at / wide;
        static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        for (int k = 0; k < 4; k++) {
            int nx = x + dirs[k][0], ny = y + dirs[k][1];
            if (nx < 0 || ny < 0 || nx >= wide || ny >= high || reached[ny * wide + nx]) continue;
            if (world_is_solid(w, w->spawn_x + x0 + nx, y0 + ny)) continue;
            reached[ny * wide + nx] = 1;
            queue[tail++] = ny * wide + nx;
        }
    }
    int left = power * WATER_FULL;
    for (int y = g->ship.y0; y <= g->ship.y1 && left > 0; y++)
        for (int dx = g->ship.x0; dx <= g->ship.x1 && left > 0; dx++) {
            int x = w->spawn_x + dx, have = world_water(w, x, y);
            if (have <= 0) continue;
            if (y >= y0 && reached[(y - y0) * wide + (dx - x0)]) continue;
            /* (Above the waterline it is what came aboard over the side.) */
            int take = have < left ? have : left;
            world_set_water(w, x, y, have - take);
            left -= take;
        }
}

/* What hunts in the sea bites at her: a piece of hull under water that is no stronger than the sea she is in asks
 * may be holed by one that is against it, and the sea comes in there until it is cut out and made good. */
static void bite_hull(Game *g)
{
    World *w = &g->world;
    static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (int i = 0; i < MAX_MOBS; i++) {
        const Mob *m = &g->mobs[i];
        if (!m->active || m->owned || !content.mobs[m->kind].hunts) continue;
        for (int k = 0; k < 4; k++) {
            int x = world_wrap_x(w, m->x + dirs[k][0]), y = m->y + dirs[k][1];
            const TileDef *def = tile_def(world_get(w, x, y));
            if (y <= w->sea_row || def->holed == TILE_AIR || def->ship[SHIP_STRENGTH] > game_sea(g)->needs[NEED_STRENGTH] + 1) continue;
            if (game_random(g) % 8u != 0) continue;
            world_set_at(w, LAYER_FOREGROUND, x, y, def->holed);
            water_wake(w, x);
            return;
        }
    }
}

/* Weather: while it rains the sea stands up as high as the sea she is in runs, and comes aboard wherever her
 * deck or her side is lower than that - a little at a time, onto the top of her, to run down whatever is open. */
static void ship_seas(Game *g)
{
    World *w = &g->world;
    int high = (int)(game_sea(g)->swell + 0.5f);
    if (high < 1 || game_rain(g) <= 0.0f) return;
    for (int dx = g->ship.bow - g->ship.length + 1; g->ship.length > 0 && dx <= g->ship.bow; dx++) {
        int x = w->spawn_x + dx, top = -1;
        for (int y = g->ship.y0; y <= g->ship.y1 && top < 0; y++)
            if (world_is_solid(w, x, y)) top = y;
        if (top < 1 || w->sea_row + 1 - top >= high || world_is_solid(w, x, top - 1)) continue;
        if (game_random(g) % 4u == 0) water_add(w, x, top - 1, 2);
    }
}

/* Her guns fire by themselves at whatever comes for her. */
static void fire_guns(Game *g)
{
    World *w = &g->world;
    if (g->ship.stats[SHIP_GUNS] <= 0) return;
    for (int i = 0; i < MAX_MOBS; i++) {
        const Mob *m = &g->mobs[i];
        if (!m->active || m->owned) continue;
        const MobDef *def = &content.mobs[m->kind];
        if (!def->hunts && def->sting_ticks <= 0 && def->strike_power <= 0 && def->sting_power <= 0) continue;
        int dx = world_dx(w, w->spawn_x, m->x);
        if (dx < g->ship.x0 - GUN_RANGE || dx > g->ship.x1 + GUN_RANGE || m->y < g->ship.y0 - GUN_RANGE || m->y > g->ship.y1 + GUN_RANGE) continue;
        g->shot.from_x = (float)(w->spawn_x + (dx < 0 ? g->ship.x0 : g->ship.x1)) + 0.5f, g->shot.from_y = (float)w->sea_row - 0.5f;
        g->shot.to_x = (float)m->x + 0.5f, g->shot.to_y = (float)m->y + 0.5f;
        g->shot.age = 0.0f, g->shot.live = true;
        mob_hurt(g, i, g->ship.stats[SHIP_GUNS]);
        return;
    }
}

void ship_tick(Game *g)
{
    if (!game_adrift(g)) return;
    const SeaDef *sea = game_sea(g);
    if (!g->ship.measured || g->tick % MEASURE_EVERY == 0) ship_measure(g);

    /* A world just begun has something in sight already. */
    if (g->tick <= 1)
        for (int k = 0; k < 5; k++) drift_spawn(g, 6 + (int)(game_random(g) % (uint32_t)content.worldgen.drift_ahead));
    float rate = sea->drift_rate / 1000.0f * (1.0f + 0.5f * (float)g->ship.way);
    if ((float)(game_random(g) % 100000u) < rate * 100000.0f) drift_spawn(g, content.worldgen.drift_ahead);
    if (g->tick % (uint64_t)drift_interval(g) == 0) drift_step(g);

    if (g->tick % 10 == 0) {
        level_sea(g);
        trawl(g);
        pump_bilge(g);
        if (g->tick % 20 == 0) bite_hull(g);
        if (g->tick % 20 == 10) ship_seas(g);
        fire_guns(g);
    }

    if (g->passage > 0) {
        g->passage -= 1 + g->ship.way / 2;
        if (g->passage <= 0) {
            g->passage = 0;
            g->sea = g->sea_to;
            g->alongside_next = g->tick + (uint64_t)content.worldgen.seas[g->sea].alongside_every / 2;
        }
        return;
    }
    if (g->alongside > 0) {
        if (g->tick >= g->alongside_until) ship_cast_off(g);
    } else if (sea->alongside_count > 0) {
        if (g->alongside_next == 0) g->alongside_next = g->tick + (uint64_t)sea->alongside_every / 2;
        if (g->tick >= g->alongside_next) {
            come_alongside(g);
            if (g->alongside == 0) g->alongside_next = g->tick + 200;
        }
    }
}
