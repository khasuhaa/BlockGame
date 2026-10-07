#include "power.h"

#include <stdlib.h>
#include <string.h>

#define MAX_WIRE_SEARCH 8192 /* squares of wire looked along for something to draw on */

/* What a block does with electricity, if anything. */
static const TileDef *device_def(const World *w, int x, int y)
{
    for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
        const TileDef *def = tile_def(world_get_at(w, layer, x, y));
        if (def->gen_power > 0 || def->battery > 0 || def->electric > 0 || def->motor_power > 0) return def;
    }
    return NULL;
}

static Storage *device_at(Game *g, int x, int y)
{
    x = world_wrap_x(&g->world, x);
    for (int i = 0; i < MAX_STORAGES; i++)
        if (g->storages[i].used && g->storages[i].x == x && g->storages[i].y == y)
            return device_def(&g->world, x, y) ? &g->storages[i] : NULL;
    return NULL;
}

int game_power_at(const Game *g, int x, int y, int *capacity)
{
    const TileDef *def = device_def(&g->world, x, y);
    Storage *st = device_at((Game *)g, x, y);
    if (capacity) *capacity = def ? def->battery : 0;
    return st ? st->power : 0;
}

int game_power_made(const Game *g, int x, int y)
{
    const TileDef *def = device_def(&g->world, x, y);
    Storage *st = device_at((Game *)g, x, y);
    return st && def && def->gen_power > 0 ? st->made : 0;
}

int game_power_for(const Game *g, TileType bench, const Recipe *r, int count)
{
    const TileDef *def = tile_def(bench);
    if (def->electric <= 0 || !r || count < 1) return 0;
    float each = game_craft_seconds(g, r);
    int need = (int)((float)def->electric * each * (float)count + 0.5f);
    return need < 1 ? 1 : need;
}

int game_craft_power(const Game *g, int *arrived)
{
    if (arrived) *arrived = 0;
    if (!g->bench_open || g->craft_remaining <= 0 || g->craft_power <= 0) return 0;
    if (arrived) {
        *arrived = game_power_at(g, g->bench_pos.x, g->bench_pos.y, NULL);
        if (*arrived > g->craft_power) *arrived = g->craft_power;
    }
    return g->craft_power;
}

void power_reset(Game *g)
{
    if (g->flows) memset(g->flows, 0, sizeof(PowerFlow) * MAX_POWER_FLOWS);
}

/* What a generator can give this tick. */
static int generate(const Game *g, const TileDef *def, const Storage *st)
{
    const World *w = &g->world;
    if (def->gen_needs == POWER_NEEDS_FUEL) return st->fuel > 0 ? def->gen_power : 0;
    if (def->gen_needs == POWER_NEEDS_SUN) {
        /* The sun has to be up, and to fall straight on it: nothing solid anywhere over it. */
        bool day;
        game_day_phase(g, &day);
        if (!day) return 0;
        for (int y = st->y - 1; y >= 0; y--)
            if (world_is_solid(w, st->x, y)) return 0;
        return (int)((float)def->gen_power * game_daylight(g) + 0.5f);
    }
    return def->gen_power;
}

/* Units on their way to a thing: in the wire now, or still to be sent. */
static int incoming(const Game *g, const Storage *to, bool *any)
{
    int total = 0;
    if (any) *any = false;
    for (int i = 0; i < MAX_POWER_FLOWS; i++) {
        const PowerFlow *f = &g->flows[i];
        if (!f->active || f->to_x != to->x || f->to_y != to->y) continue;
        if (any) *any = true;
        if (f->remaining > 0) total += f->remaining;
        for (int k = 0; k < f->length; k++) total += f->moving[k];
    }
    return total;
}

/* Whether a thing can be drawn on now: a battery with something in it (if
 * batteries are wanted), or a generator with something left of what it made this tick. */
static bool can_give(Game *g, int x, int y, bool batteries)
{
    const TileDef *def = device_def(&g->world, x, y);
    Storage *st = device_at(g, x, y);
    if (!def || !st) return false;
    if (def->battery > 0) return batteries && st->power > 0;
    return def->gen_power > 0 && st->made - st->given > 0;
}

/* Whether a thing is a battery that could take more than it has and has coming. */
static bool has_room(Game *g, int x, int y)
{
    const TileDef *def = device_def(&g->world, x, y);
    Storage *st = device_at(g, x, y);
    return def && st && def->battery > 0 && st->power + incoming(g, st, NULL) < def->battery;
}

/* Looks along the wires from a thing for the nearest thing to draw on, and
 * fills in the way there, from that thing's end. False if there is none in reach.
 * With `store`, it is the other way about: `to` is something with electricity
 * to spare, what is looked for is the nearest battery with room for it, and
 * the way filled in runs from the one to the other. */
static bool find_source(Game *g, const Storage *to, bool batteries, bool store, PowerFlow *out)
{
    static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    const World *w = &g->world;
    uint32_t gen = ++g->bfs_gen;
    int head = 0, tail = 0, start = to->y * w->width + to->x;
    g->bfs_seen[(size_t)start * 2] = gen;
    g->bfs_parent[(size_t)start * 2] = -1;
    g->bfs_queue[tail++] = start;
    while (head < tail && head < MAX_WIRE_SEARCH) {
        int at = g->bfs_queue[head++], ax = at % w->width, ay = at / w->width;
        for (int d = 0; d < 4; d++) {
            int nx = world_wrap_x(w, ax + dirs[d][0]), ny = ay + dirs[d][1];
            if (!world_in_bounds(w, nx, ny)) continue;
            int next = ny * w->width + nx;
            if (g->bfs_seen[(size_t)next * 2] == gen) continue;
            bool source = next != start && (store ? has_room(g, nx, ny) : can_give(g, nx, ny, batteries));
            if (!source && !world_is_wired(w, nx, ny)) continue;
            g->bfs_seen[(size_t)next * 2] = gen;
            g->bfs_parent[(size_t)next * 2] = at;
            if (!source) {
                g->bfs_queue[tail++] = next;
                continue;
            }
            /* Found. The way back from it is the way the electricity will come. */
            int length = 0;
            for (int c = at; c != start; c = g->bfs_parent[(size_t)c * 2]) length++;
            if (length > MAX_WIRE_PATH) return false;
            memset(out, 0, sizeof *out);
            out->active = true;
            out->from_x = nx;
            out->from_y = ny;
            out->to_x = to->x;
            out->to_y = to->y;
            out->length = length;
            int k = 0;
            for (int c = at; c != start; c = g->bfs_parent[(size_t)c * 2]) out->cells[k++] = c;
            if (store) {
                /* (The way was found from the giving end: turn it about.) */
                out->from_x = to->x;
                out->from_y = to->y;
                out->to_x = nx;
                out->to_y = ny;
                for (int i = 0; i < length / 2; i++) {
                    int32_t c = out->cells[i];
                    out->cells[i] = out->cells[length - 1 - i];
                    out->cells[length - 1 - i] = c;
                }
            }
            return true;
        }
    }
    return false;
}

static PowerFlow *free_flow(Game *g)
{
    for (int i = 0; i < MAX_POWER_FLOWS; i++)
        if (!g->flows[i].active) return &g->flows[i];
    return NULL;
}

/* Moves what is in a wire a block along, hands over what has reached the
 * end, and puts in what its source has to give this tick. */
static void run_flow(Game *g, PowerFlow *f)
{
    const World *w = &g->world;
    Storage *from = device_at(g, f->from_x, f->from_y), *to = device_at(g, f->to_x, f->to_y);
    bool whole = from && to;
    for (int k = 0; whole && k < f->length; k++)
        whole = world_is_wired(w, f->cells[k] % w->width, f->cells[k] / w->width);
    if (!whole) {
        /* A wire cut, or an end gone: what was in it is lost. */
        f->active = false;
        return;
    }
    const TileDef *source = device_def(w, from->x, from->y), *sink = device_def(w, to->x, to->y);
    int arrived = 0;
    if (f->length > 0) {
        arrived = f->moving[f->length - 1];
        memmove(f->moving + 1, f->moving, sizeof(int32_t) * (size_t)(f->length - 1));
        f->moving[0] = 0;
    }
    /* What to put in: all of it at once from a battery, a tick's worth at a
     * time from a generator or for a thing that takes it steadily. */
    int want = f->remaining < 0 ? f->rate : f->rate > 0 && f->rate < f->remaining ? f->rate : f->remaining;
    if (sink->battery > 0) {
        int travelling = 0;
        for (int k = 0; k < f->length; k++) travelling += f->moving[k];
        int room = sink->battery - to->power - arrived - travelling;
        if (want > room) want = room < 0 ? 0 : room;
    }
    int have = source->battery > 0 ? from->power : from->made - from->given;
    int give = want < have ? want : have;
    if (give < 0) give = 0;
    if (source->battery > 0) from->power -= give;
    else from->given += give;
    if (f->remaining > 0) f->remaining -= give;
    if (f->length > 0) f->moving[0] += give;
    else arrived += give;
    to->power += arrived;
    if (sink->battery > 0 && to->power > sink->battery) to->power = sink->battery;

    /* Done when there is no more to send and none on the way. One that runs
     * on and on stops when its source has nothing for it, or what it fills is full. */
    bool empty = true;
    for (int k = 0; k < f->length; k++) empty = empty && f->moving[k] == 0;
    if (empty && (f->remaining == 0 || (f->remaining < 0 && give == 0))) f->active = false;
}

/* Asks for electricity for a thing: finds the nearest thing that has some and starts it on its way. */
static bool ask(Game *g, Storage *to, int amount, int rate, bool batteries)
{
    PowerFlow found, *slot = free_flow(g);
    if (!slot || !find_source(g, to, batteries, false, &found)) return false;
    const TileDef *source = device_def(&g->world, found.from_x, found.from_y);
    Storage *from = device_at(g, found.from_x, found.from_y);
    if (amount >= 0 && source->battery > 0) {
        /* A battery sends what is asked, or what it has, in one go. */
        found.remaining = amount < from->power ? amount : from->power;
        found.rate = 0;
    } else {
        found.remaining = amount;
        found.rate = rate > 0 ? rate : source->gen_power;
    }
    *slot = found;
    return true;
}

/* A generator with something left over sends it to the nearest battery with
 * room for it, for as long as it has any and the battery has room. */
static void store(Game *g, Storage *from, const TileDef *def)
{
    for (int i = 0; i < MAX_POWER_FLOWS; i++) {
        const PowerFlow *f = &g->flows[i];
        if (!f->active || f->from_x != from->x || f->from_y != from->y) continue;
        const TileDef *sink = device_def(&g->world, f->to_x, f->to_y);
        if (sink && sink->battery > 0) return; /* already sending to one */
    }
    PowerFlow found, *slot = free_flow(g);
    if (!slot || !find_source(g, from, false, true, &found)) return;
    found.remaining = -1;
    found.rate = def->gen_power;
    *slot = found;
}

void power_tick(Game *g)
{
    World *w = &g->world;
    if (!g->flows) return;

    /* What each generator has to give this tick. */
    bool any = false;
    for (int i = 0; i < MAX_STORAGES; i++) {
        Storage *st = &g->storages[i];
        const TileDef *def = st->used ? device_def(w, st->x, st->y) : NULL;
        if (!def) continue;
        any = true;
        st->made = def->gen_power > 0 ? generate(g, def, st) : 0;
        st->given = 0;
    }
    if (!any) return;

    /* What is on its way moves on: to the things that use it first, then to the batteries. */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < MAX_POWER_FLOWS; i++) {
            PowerFlow *f = &g->flows[i];
            if (!f->active) continue;
            const TileDef *sink = device_def(w, f->to_x, f->to_y);
            if ((sink && sink->battery > 0) == (pass == 1) || (!sink && pass == 0)) run_flow(g, f);
        }
    }

    /* Benches waiting to start: one for each character who has set one going. */
    int was = g->live;
    for (int c = 0; c < g->character_count; c++) {
        game_use_character(g, c);
        if (!g->bench_open || g->craft_remaining <= 0 || g->craft_power <= 0) continue;
        Storage *st = device_at(g, g->bench_pos.x, g->bench_pos.y);
        if (!st) continue;
        int short_of = g->craft_power - st->power - incoming(g, st, NULL);
        if (short_of > 0) ask(g, st, short_of, 0, true);
    }
    game_use_character(g, was);

    for (int i = 0; i < MAX_STORAGES; i++) {
        Storage *st = &g->storages[i];
        const TileDef *def = st->used ? device_def(w, st->x, st->y) : NULL;
        if (!def) continue;
        bool fed;
        incoming(g, st, &fed);
        if (def->motor_power > 0) {
            /* A motor takes its share every tick, and keeps a little by; it
             * runs while it gets it. */
            if (!fed && st->power < def->motor_power * 2) ask(g, st, -1, def->motor_power, true);
            bool runs = st->power >= def->motor_power;
            if (runs) st->power -= def->motor_power;
            world_set_meta(w, st->x, st->y, runs ? 1 : 0);
        } else if (def->gen_power > 0 && st->made - st->given > 0) {
            /* What a generator makes and nothing has asked for, it looks for a battery to put in. */
            store(g, st, def);
        }
    }
}

int game_sparks(const Game *g, Spark *out, int max)
{
    const World *w = &g->world;
    int n = 0;
    for (int i = 0; g->flows && i < MAX_POWER_FLOWS; i++) {
        const PowerFlow *f = &g->flows[i];
        if (!f->active) continue;
        for (int k = 0; k < f->length && n < max; k++) {
            if (f->moving[k] <= 0) continue;
            int next = k + 1 < f->length ? f->cells[k + 1] : f->to_y * w->width + f->to_x;
            out[n++] = (Spark){ f->cells[k] % w->width, f->cells[k] / w->width, next % w->width, next / w->width };
        }
    }
    return n;
}
