#include "train.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define REACH_X 4 /* how near a character has to be to a vehicle to do anything with it, in squares */
#define REACH_Y 3
#define STATION_SEARCH 4096 /* how far along the track a station is looked for */

bool train_rail(const World *w, int x, int y)
{
    if (!world_in_bounds(w, 0, y)) return false;
    for (int layer = 0; layer < BLOCK_LAYERS; layer++)
        if (tile_def(world_get_at(w, layer, x, y))->rail) return true;
    return false;
}

bool train_next(const World *w, int x, int y, int dir, int *row)
{
    /* On the level first; failing that up, then down. */
    static const int rise[3] = { 0, -1, 1 };
    for (int i = 0; i < 3; i++) {
        if (!train_rail(w, x + dir, y + rise[i])) continue;
        *row = y + rise[i];
        return true;
    }
    return false;
}

static const ItemDef *kind(const Vehicle *v)
{
    return item_def((ItemId)v->item);
}

static int half(const Vehicle *v)
{
    return (kind(v)->vehicle.width - 1) / 2;
}

/* The square of track `off` along from a vehicle's middle. */
static bool cell(const Game *g, const Vehicle *v, int off, int *x, int *y)
{
    int cx = v->x, cy = v->y, dir = off < 0 ? -1 : 1;
    for (int i = 0; i != off; i += dir) {
        if (!train_next(&g->world, cx, cy, dir, &cy)) return false;
        cx = world_wrap_x(&g->world, cx + dir);
    }
    *x = cx;
    *y = cy;
    return true;
}

int train_at(const Game *g, int x, int y)
{
    x = world_wrap_x(&g->world, x);
    for (int i = 0; i < MAX_VEHICLES; i++) {
        const Vehicle *v = &g->vehicles[i];
        if (!v->used) continue;
        for (int off = -half(v); off <= half(v); off++) {
            int cx, cy;
            if (cell(g, v, off, &cx, &cy) && cx == x && cy == y) return i;
        }
    }
    return -1;
}

int train_under(const Game *g, int x, int y)
{
    int v = train_at(g, x, y);
    return v >= 0 ? v : train_at(g, x, y + 1);
}

/* How far along from a vehicle's middle each of its seats is. */
static int seat_off(const Vehicle *v, int seat)
{
    /* (The one seat of something long is at the back of it, where the cab is: the left end.) */
    if (half(v) == 0) return 0;
    return kind(v)->vehicle.seats < 2 || seat == 0 ? -1 : 1;
}

int train_seat_at(const Game *g, int v, int x)
{
    const Vehicle *it = &g->vehicles[v];
    if (kind(it)->vehicle.seats < 2) return 0;
    return world_dx(&g->world, it->x, world_wrap_x(&g->world, x)) < 0 ? 0 : 1;
}

/* Whether a vehicle of an item would fit with its middle on a square of track. */
static bool fits(const Game *g, ItemId item, int x, int y)
{
    Vehicle probe = { .used = 1, .item = (uint16_t)item, .x = world_wrap_x(&g->world, x), .y = y };
    if (!train_rail(&g->world, x, y)) return false;
    for (int off = -half(&probe); off <= half(&probe); off++) {
        int cx, cy;
        if (!cell(g, &probe, off, &cx, &cy) || train_at(g, cx, cy) >= 0) return false;
        /* (And room for the height of it.) */
        if (world_is_solid(&g->world, cx, cy) || world_is_solid(&g->world, cx, cy - 1)) return false;
    }
    return true;
}

/* Whether, put there, it would be touching another. */
static bool couples(const Game *g, ItemId item, int x, int y)
{
    Vehicle probe = { .used = 1, .item = (uint16_t)item, .x = world_wrap_x(&g->world, x), .y = y };
    int cx, cy;
    return (cell(g, &probe, half(&probe) + 1, &cx, &cy) && train_at(g, cx, cy) >= 0) ||
           (cell(g, &probe, -half(&probe) - 1, &cx, &cy) && train_at(g, cx, cy) >= 0);
}

static bool near(const Game *g, int x, int y, int reach)
{
    int dx = world_dx(&g->world, g->player.feet.x, world_wrap_x(&g->world, x)), dy = y - g->player.feet.y;
    return dx >= -reach && dx <= reach && dy >= -REACH_Y && dy <= REACH_Y;
}

int train_place(Game *g, ItemId item, int x, int y)
{
    const World *w = &g->world;
    if (item <= ITEM_NONE || item >= content.item_count || !item_def(item)->vehicle.is || g->ride) return -1;
    if (!train_rail(w, x, y)) y++;
    if (!train_rail(w, x, y) || !near(g, x, y, REACH_X) || inventory_count(&g->inventory, item) < 1) return -1;
    /* Where it was asked for; or a square or two along, if that puts it up against another. */
    static const int slide[5] = { 0, 1, -1, 2, -2 };
    int at = -1, row = y, first = -1, first_row = y;
    for (int i = 0; i < 5 && at < 0; i++) {
        Vehicle from = { .used = 1, .item = (uint16_t)item, .x = world_wrap_x(w, x), .y = y };
        int cx, cy;
        /* (Along the track, not through the air.) */
        Vehicle one = from;
        one.item = 0;
        if (slide[i] != 0) {
            cx = from.x, cy = from.y;
            bool ok = true;
            for (int k = 0; ok && k != slide[i]; k += slide[i] < 0 ? -1 : 1) {
                ok = train_next(w, cx, cy, slide[i] < 0 ? -1 : 1, &cy);
                cx = world_wrap_x(w, cx + (slide[i] < 0 ? -1 : 1));
            }
            if (!ok) continue;
        } else {
            cx = from.x, cy = from.y;
        }
        if (!fits(g, item, cx, cy)) continue;
        if (first < 0) first = cx, first_row = cy;
        if (couples(g, item, cx, cy)) at = cx, row = cy;
    }
    if (at < 0) at = first, row = first_row;
    if (at < 0) return -1;
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &g->vehicles[i];
        if (v->used) continue;
        if (inventory_remove(&g->inventory, item, 1) != 1) return -1;
        *v = (Vehicle){ .used = 1, .item = (uint16_t)item, .x = at, .y = row };
        if (item_def(item)->vehicle.engine) v->fuel = container_alloc(&g->inventory);
        if (item_def(item)->vehicle.cargo_slots > 0) v->cargo = container_alloc(&g->inventory);
        return i;
    }
    return -1;
}

/* Where a character's feet are while they are in a seat. */
static void seat_rider(Game *g, int v, int seat)
{
    const Vehicle *it = &g->vehicles[v];
    int who = it->seats[seat] - 1, x, y;
    if (who < 0 || who >= g->character_count || !cell(g, it, seat_off(it, seat), &x, &y)) return;
    Player *p = who == g->live ? &g->player : &g->characters[who].player;
    p->feet = p->step_to = (TilePos){ x, y };
    p->activity = ACTIVITY_IDLE;
    p->climbing = false;
}

void train_unseat(Game *g, int character)
{
    if (character < 0) character = g->live;
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &g->vehicles[i];
        for (int s = 0; v->used && s < VEHICLE_SEATS; s++) {
            if (v->seats[s] != character + 1) continue;
            v->seats[s] = 0;
            /* (Nobody at the handle: it stops.) */
            if (kind(v)->vehicle.hand) v->dir = 0;
        }
    }
    if (character == g->live) g->ride = g->ride_seat = 0;
    else if (character < g->character_count) g->characters[character].ride = g->characters[character].ride_seat = 0;
}

void train_close(Game *g)
{
    g->open_vehicle = 0;
}

uint16_t train_open_container(const Game *g, int *slots)
{
    if (g->open_vehicle < 1 || g->open_vehicle > MAX_VEHICLES || !g->vehicles[g->open_vehicle - 1].used) return 0;
    const Vehicle *v = &g->vehicles[g->open_vehicle - 1];
    if (slots) *slots = g->open_vehicle_cargo ? kind(v)->vehicle.cargo_slots : 1;
    return g->open_vehicle_cargo ? v->cargo : v->fuel;
}

void train_remove(Game *g, int v)
{
    Vehicle *it = &g->vehicles[v];
    if (v < 0 || v >= MAX_VEHICLES || !it->used) return;
    for (int s = 0; s < VEHICLE_SEATS; s++)
        if (it->seats[s] > 0) train_unseat(g, it->seats[s] - 1);
    uint16_t boxes[2] = { it->fuel, it->cargo };
    for (int b = 0; b < 2; b++) {
        if (boxes[b] == 0 || boxes[b] >= MAX_CONTAINERS) continue;
        Container *box = &g->inventory.containers[boxes[b]];
        for (int k = 0; k < CONTAINER_MAX_SLOTS; k++) {
            if (box->slots[k].count > 0) game_drop_stack(g, box->slots[k], it->x, it->y);
            box->slots[k] = (ItemStack){ 0 };
        }
        container_free(&g->inventory, boxes[b]);
    }
    if (g->open_vehicle == v + 1) g->open_vehicle = 0;
    game_drop_item(g, (ItemId)it->item, it->x, it->y);
    /* (What it was coupled to stops where it is.) */
    it->used = 0;
    memset(it, 0, sizeof *it);
}

int train_group(const Game *g, int v, int *out)
{
    int n = 0, left[MAX_VEHICLES], lefts = 0;
    /* Out to the left end, then back and on to the right. */
    for (int dir = -1; dir <= 1; dir += 2) {
        int at = v;
        while (n + lefts < MAX_VEHICLES - 1) {
            const Vehicle *it = &g->vehicles[at];
            int x, y, next;
            if (!cell(g, it, dir * (half(it) + 1), &x, &y) || (next = train_at(g, x, y)) < 0 || next == v) break;
            if (dir < 0) left[lefts++] = next;
            else out[lefts + 1 + n++] = next;
            at = next;
        }
        if (dir < 0) {
            for (int i = 0; i < lefts; i++) out[i] = left[lefts - 1 - i];
            out[lefts] = v;
        }
    }
    return lefts + 1 + n;
}

bool train_station(const Game *g, int v, int dir, int *sx, int *sy, const char **name)
{
    const World *w = &g->world;
    int x = g->vehicles[v].x, y = g->vehicles[v].y;
    for (int i = 0; i < STATION_SEARCH && i < w->width; i++) {
        if (!train_next(w, x, y, dir, &y)) return false;
        x = world_wrap_x(w, x + dir);
        bool here = false;
        for (int layer = 0; layer < BLOCK_LAYERS; layer++) here = here || tile_def(world_get_at(w, layer, x, y - 1))->station;
        if (!here) continue;
        const SignText *text = game_sign_at(g, x, y - 1);
        if (sx) *sx = x;
        if (sy) *sy = y;
        if (name) *name = text ? text->text : "";
        return true;
    }
    return false;
}

static bool riding(const Game *g, int v)
{
    return g->ride == v + 1;
}

int train_options(const Game *g, int v, int x, TrainOption *out)
{
    if (v < 0 || v >= MAX_VEHICLES || !g->vehicles[v].used || !game_alive(g)) return 0;
    const Vehicle *it = &g->vehicles[v];
    const ItemDef *def = kind(it);
    /* From beside it; or from a seat in it, or in anything coupled to it. */
    bool aboard = riding(g, v);
    if (!aboard && g->ride) {
        int members[MAX_VEHICLES], count = train_group(g, v, members);
        for (int i = 0; i < count; i++) aboard = aboard || riding(g, members[i]);
        if (!aboard) return 0;
    } else if (!aboard && !near(g, it->x, it->y, REACH_X + half(it))) {
        return 0;
    }
    int n = 0;
    if (def->vehicle.engine) {
        if (it->dir != 0 || it->pull != 0) {
            out[n++] = (TrainOption){ TRAIN_STOP, 0, "Stop" };
        } else {
            for (int dir = -1; dir <= 1; dir += 2) {
                const char *name = "";
                TrainOption go = { TRAIN_GO, dir, "" };
                if (!train_station(g, v, dir, NULL, NULL, &name)) snprintf(go.label, sizeof go.label, dir < 0 ? "Move left" : "Move right");
                else if (name[0]) snprintf(go.label, sizeof go.label, "Move to %s", name);
                else snprintf(go.label, sizeof go.label, dir < 0 ? "Move left to station" : "Move right to station");
                out[n++] = go;
            }
        }
        out[n++] = (TrainOption){ TRAIN_FUEL, 0, "Fuel" };
    }
    if (def->vehicle.cargo_slots > 0) out[n++] = (TrainOption){ TRAIN_CARGO, 0, "Open" };
    if (riding(g, v)) {
        out[n++] = (TrainOption){ TRAIN_OUT, 0, "Get out" };
    } else if (def->vehicle.seats > 0 && !g->ride) {
        int seat = train_seat_at(g, v, x);
        /* (The seat clicked; or, with someone in it, the other.) */
        if (it->seats[seat] != 0 && def->vehicle.seats > 1) seat = 1 - seat;
        if (it->seats[seat] == 0) out[n++] = (TrainOption){ TRAIN_SIT, seat, "" };
        if (it->seats[seat] == 0) snprintf(out[n - 1].label, sizeof out[n - 1].label, def->vehicle.engine || def->vehicle.hand ? "Get in" : "Sit");
    }
    /* Taking it up is always the last of them. */
    out[n] = (TrainOption){ TRAIN_REMOVE, 0, "" };
    snprintf(out[n].label, sizeof out[n].label, "Remove %s", def->vehicle.engine ? "locomotive" : def->vehicle.width > 1 ? "car" : def->label);
    for (char *c = out[n].label + 7; *c; c++) *c = (char)tolower((unsigned char)*c);
    return n + 1;
}

/* Brings it to a stop at the square it is coming to (or where it stands). */
static void halt(Game *g, int v)
{
    Vehicle *it = &g->vehicles[v];
    if (it->dir == 0) return;
    if (it->step <= 0.0f) {
        it->dir = 0;
        it->has_dest = 0;
        return;
    }
    int y = it->y;
    if (!train_next(&g->world, it->x, it->y, it->dir, &y)) return;
    it->has_dest = 1;
    it->dest_x = world_wrap_x(&g->world, it->x + it->dir);
    it->dest_y = y;
}

bool train_do(Game *g, int v, TrainOption option)
{
    TrainOption can[TRAIN_MAX_OPTIONS];
    int n = train_options(g, v, g->vehicles[v].x + (option.what == TRAIN_SIT && option.arg == 0 ? -1 : 1), can);
    bool offered = false;
    for (int i = 0; i < n; i++) offered = offered || (can[i].what == option.what && (option.what != TRAIN_GO || can[i].arg == option.arg));
    if (!offered) return false;
    Vehicle *it = &g->vehicles[v];
    switch (option.what) {
    case TRAIN_GO: {
        int members[MAX_VEHICLES], count = train_group(g, v, members);
        /* (One engine to a train does the pulling.) */
        for (int i = 0; i < count; i++)
            if (members[i] != v) g->vehicles[members[i]].dir = 0;
        it->dir = (int8_t)(option.arg < 0 ? -1 : 1);
        it->has_dest = train_station(g, v, it->dir, &it->dest_x, &it->dest_y, NULL);
        return true;
    }
    case TRAIN_STOP: {
        int members[MAX_VEHICLES], count = train_group(g, v, members);
        for (int i = 0; i < count; i++) halt(g, members[i]);
        return true;
    }
    case TRAIN_SIT:
        if (option.arg < 0 || option.arg >= VEHICLE_SEATS || option.arg >= kind(it)->vehicle.seats || it->seats[option.arg] != 0) return false;
        game_cancel_actions(g);
        game_close_bench(g);
        it->seats[option.arg] = (int8_t)(g->live + 1);
        g->ride = v + 1;
        g->ride_seat = option.arg;
        seat_rider(g, v, option.arg);
        return true;
    case TRAIN_OUT:
        train_unseat(g, -1);
        return true;
    case TRAIN_FUEL:
    case TRAIN_CARGO:
        g->open_vehicle = v + 1;
        g->open_vehicle_cargo = option.what == TRAIN_CARGO;
        return true;
    case TRAIN_REMOVE:
        train_remove(g, v);
        return true;
    }
    return false;
}

/* Whether the train a vehicle is part of has track to go one square further. */
static bool clear_ahead(const Game *g, const int *members, int count, int dir)
{
    const Vehicle *lead = &g->vehicles[members[dir > 0 ? count - 1 : 0]];
    int x, y;
    return cell(g, lead, dir * (half(lead) + 1), &x, &y) && !world_is_solid(&g->world, x, y) && !world_is_solid(&g->world, x, y - 1);
}

/* Whether an engine has fire, taking the next thing from what it was given if the last has burnt out. */
static bool fired(Game *g, Vehicle *it)
{
    if (it->burn > 0) return true;
    if (it->fuel == 0 || it->fuel >= MAX_CONTAINERS) return false;
    ItemStack *slot = &g->inventory.containers[it->fuel].slots[0];
    if (slot->count <= 0 || item_def((ItemId)slot->id)->fuel <= 0) return false;
    it->burn = item_def((ItemId)slot->id)->fuel;
    if (--slot->count == 0) *slot = (ItemStack){ 0 };
    return true;
}

static void settle(Game *g, const int *members, int count)
{
    for (int i = 0; i < count; i++) {
        g->vehicles[members[i]].pull = 0;
        g->vehicles[members[i]].step = 0.0f;
    }
}

static void drive(Game *g, int v, float dt)
{
    Vehicle *it = &g->vehicles[v];
    const ItemDef *def = kind(it);
    int members[MAX_VEHICLES], count = train_group(g, v, members);
    /* (Of two engines pulling one train, the first is the one that does.) */
    for (int i = 0; i < count; i++)
        if (members[i] < v && g->vehicles[members[i]].dir != 0) return;
    float left = dt * def->vehicle.speed;
    while (it->dir != 0 && left > 0.0f) {
        if (it->step <= 0.0f && (!clear_ahead(g, members, count, it->dir) || (def->vehicle.engine && !fired(g, it)))) {
            it->dir = 0;
            it->has_dest = 0;
            break;
        }
        float need = 1.0f - it->step;
        if (left < need) {
            it->step += left;
            break;
        }
        left -= need;
        /* A square further on, all of them, and whoever is in them. */
        for (int i = 0; i < count; i++) {
            Vehicle *one = &g->vehicles[members[i]];
            int y = one->y;
            if (!train_next(&g->world, one->x, one->y, it->dir, &y)) continue;
            one->x = world_wrap_x(&g->world, one->x + it->dir);
            one->y = y;
            for (int s = 0; s < VEHICLE_SEATS; s++) seat_rider(g, members[i], s);
        }
        it->step = 0.0f;
        if (it->has_dest && it->x == it->dest_x && it->y == it->dest_y) {
            it->dir = 0;
            it->has_dest = 0;
        }
    }
    if (it->dir == 0) {
        settle(g, members, count);
        return;
    }
    for (int i = 0; i < count; i++) {
        g->vehicles[members[i]].pull = it->dir;
        g->vehicles[members[i]].step = it->step;
    }
}

void train_update(Game *g, float dt)
{
    /* Whoever is on something worked by hand says which way it goes: along with the keys, and stopped by up or down. */
    if (g->ride > 0 && g->ride <= MAX_VEHICLES && g->vehicles[g->ride - 1].used && kind(&g->vehicles[g->ride - 1])->vehicle.hand && game_alive(g)) {
        Vehicle *it = &g->vehicles[g->ride - 1];
        if (g->move_dx != 0) {
            if (it->dir != (g->move_dx < 0 ? -1 : 1) && it->step <= 0.0f) it->dir = (int8_t)(g->move_dx < 0 ? -1 : 1), it->has_dest = 0;
            else if (it->dir != (g->move_dx < 0 ? -1 : 1)) halt(g, g->ride - 1);
        } else if (g->move_dy != 0) {
            halt(g, g->ride - 1);
        }
    }
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *it = &g->vehicles[i];
        if (!it->used || it->dir == 0) continue;
        if (!kind(it)->vehicle.engine && !kind(it)->vehicle.hand) {
            it->dir = 0;
            continue;
        }
        drive(g, i, dt);
    }
    /* What is open is shut when its vehicle has gone off, or been taken up. */
    if (g->open_vehicle > 0) {
        const Vehicle *open = &g->vehicles[g->open_vehicle - 1];
        if (!open->used || (!riding(g, g->open_vehicle - 1) && !near(g, open->x, open->y, REACH_X + half(open)))) g->open_vehicle = 0;
    }
}

void train_tick(Game *g)
{
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *it = &g->vehicles[i];
        if (it->used && it->dir != 0 && it->burn > 0) it->burn--;
    }
}

void train_visual(const Game *g, int v, float *x, float *y)
{
    const Vehicle *it = &g->vehicles[v];
    int row = it->y;
    *x = (float)it->x;
    *y = (float)it->y;
    if (it->pull == 0 || it->step <= 0.0f || !train_next(&g->world, it->x, it->y, it->pull, &row)) return;
    *x += (float)it->pull * it->step;
    *y += (float)(row - it->y) * it->step;
}

bool train_rider_visual(const Game *g, float *x, float *y)
{
    if (g->ride < 1 || g->ride > MAX_VEHICLES || !g->vehicles[g->ride - 1].used) return false;
    const Vehicle *it = &g->vehicles[g->ride - 1];
    float vx, vy;
    train_visual(g, g->ride - 1, &vx, &vy);
    /* (As far from the middle of it as their seat is, and as much higher or lower as the track is there.) */
    *x = (float)g->player.feet.x + (vx - (float)it->x);
    *y = (float)g->player.feet.y + (vy - (float)it->y);
    return true;
}
