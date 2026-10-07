#include "industry.h"

#include <math.h>
#include <string.h>

#define MAX_ROUTE 2048 /* squares of conveyor looked along from one extractor */
#define MAX_SINKS 32   /* things along them that might take what it has */

static const int beside[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

static Storage *storage_here(Game *g, int x, int y)
{
    x = world_wrap_x(&g->world, x);
    for (int i = 0; i < MAX_STORAGES; i++)
        if (g->storages[i].used && g->storages[i].x == x && g->storages[i].y == y) return &g->storages[i];
    return NULL;
}

/* The block at a square that has to do with any of this: one that holds things, or works. */
static const TileDef *block_here(const World *w, int x, int y)
{
    for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
        const TileDef *def = tile_def(world_get_at(w, layer, x, y));
        if (def->storage_columns > 0 || def->draw > 0 || def->extractor) return def;
    }
    return NULL;
}

static int conveyor_here(const World *w, int x, int y)
{
    int rate = 0;
    if (!world_in_bounds(w, x, y)) return 0;
    for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
        const TileDef *def = tile_def(world_get_at(w, layer, x, y));
        if (def->conveyor > rate) rate = def->conveyor;
    }
    return rate;
}

static int slot_count(const TileDef *def)
{
    int n = def->storage_columns * def->storage_rows;
    return n > CONTAINER_MAX_SLOTS ? CONTAINER_MAX_SLOTS : n;
}

/* ---- slots ------------------------------------------------------------- */

/* Plain things only: nothing with anything inside it, or a meter to read. */
static bool plain(const ItemStack *s)
{
    return s->container == 0 && item_def(s->id)->meter_max == 0;
}

static int room_for_item(const Container *c, int from, int to, ItemId item)
{
    int room = 0, most = item_def(item)->max_stack;
    if (item_def(item)->meter_max != 0) return 0;
    for (int i = from; i < to; i++) {
        const ItemStack *s = &c->slots[i];
        if (s->count == 0) room += most;
        else if (s->id == item && plain(s) && s->count < most) room += most - s->count;
    }
    return room;
}

static int put_item(Container *c, int from, int to, ItemId item, int count)
{
    int most = item_def(item)->max_stack, put = 0;
    /* (Onto stacks of it that are there first, then into empty slots.) */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = from; i < to && put < count; i++) {
            ItemStack *s = &c->slots[i];
            bool same = s->count > 0 && s->id == item && plain(s);
            if (pass == 0 ? !same : s->count != 0) continue;
            int n = most - (pass == 0 ? s->count : 0);
            if (n > count - put) n = count - put;
            if (n <= 0) continue;
            if (pass == 1) *s = (ItemStack){ .id = (uint16_t)item };
            s->count += n;
            put += n;
        }
    }
    return put;
}

static int count_item_in(const Container *c, int from, int to, ItemId item)
{
    int n = 0;
    for (int i = from; i < to; i++)
        if (c->slots[i].count > 0 && c->slots[i].id == item && plain(&c->slots[i])) n += c->slots[i].count;
    return n;
}

static int take_item(Container *c, int from, int to, ItemId item, int count)
{
    int taken = 0;
    for (int i = from; i < to && taken < count; i++) {
        ItemStack *s = &c->slots[i];
        if (s->count == 0 || s->id != item || !plain(s)) continue;
        int n = s->count < count - taken ? s->count : count - taken;
        s->count -= n;
        taken += n;
        if (s->count == 0) *s = (ItemStack){ 0 };
    }
    return taken;
}

/* ---- machines ---------------------------------------------------------- */

/* A recipe a machine could make at all: nothing but plain items in, in no more kinds than it has slots for. */
static bool machine_can_make(const TileDef *def, const Recipe *r)
{
    if (r->crystals > 0 || r->random_total > 0 || r->characters > 0 || r->input_count < 1 || r->input_count > def->automat_inputs) return false;
    if (item_def(r->output.id)->becomes_player || item_def(r->output.id)->meter_max != 0 || item_def(r->output.id)->crystals > 0) return false;
    for (int i = 0; i < r->input_count; i++)
        if (r->inputs[i].full || item_def(r->inputs[i].id)->meter_max != 0) return false;
    return true;
}

/* Whether a bench that makes a recipe stands beside a square, at the tier the recipe wants. */
static bool bench_beside(const World *w, int x, int y, const Recipe *r)
{
    for (int d = 0; d < 4; d++) {
        for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
            const TileDef *def = tile_def(world_get_at(w, layer, x + beside[d][0], y + beside[d][1]));
            if (def->bench && strcmp(def->bench_kind, r->bench) == 0 && def->bench_level >= r->level) return true;
        }
    }
    return false;
}

/* Whether a machine may make a recipe where it stands. */
static bool machine_allows(const World *w, const TileDef *def, int x, int y, const Recipe *r)
{
    if (!def->automat || !machine_can_make(def, r)) return false;
    if (def->automat_kind[0]) return strcmp(def->automat_kind, r->bench) == 0;
    return bench_beside(w, x, y, r);
}

bool game_machine_chooses(const Game *g, int x, int y)
{
    const TileDef *def = block_here(&g->world, x, y);
    return def && def->automat && !def->automat_kind[0];
}

int game_machine_inputs(const Game *g, int x, int y)
{
    const TileDef *def = block_here(&g->world, x, y);
    return def && def->automat ? def->automat_inputs : 0;
}

int game_machine_recipes(const Game *g, int x, int y, const Recipe **out, int max)
{
    const TileDef *def = block_here(&g->world, x, y);
    int n = 0;
    if (!def || !def->automat) return 0;
    for (int i = 0; i < content.recipe_count && n < max; i++)
        if (machine_allows(&g->world, def, x, y, &content.recipes[i])) out[n++] = &content.recipes[i];
    return n;
}

const Recipe *game_machine_recipe(const Game *g, int x, int y)
{
    Storage *st = storage_here((Game *)g, x, y);
    if (!st || st->recipe < 1 || st->recipe > content.recipe_count) return NULL;
    return &content.recipes[st->recipe - 1];
}

bool game_machine_set_recipe(Game *g, int x, int y, const Recipe *r)
{
    const TileDef *def = block_here(&g->world, x, y);
    Storage *st = storage_here(g, x, y);
    if (!def || !st || !def->automat || def->automat_kind[0]) return false;
    if (r && !machine_allows(&g->world, def, x, y, r)) return false;
    st->recipe = r ? (int16_t)(r - content.recipes + 1) : 0;
    st->work = 0; /* (what was in hand is given up: what went into it is gone) */
    return true;
}

static int work_ticks(const TileDef *def, const Recipe *r)
{
    float rate = (float)(content.rules.tick_rate > 0 ? content.rules.tick_rate : 10);
    int ticks = (int)ceilf(game_craft_time(r) / def->automat_speed * rate);
    return ticks < 1 ? 1 : ticks > 30000 ? 30000 : ticks;
}

float game_machine_progress(const Game *g, int x, int y)
{
    const TileDef *def = block_here(&g->world, x, y);
    Storage *st = storage_here((Game *)g, x, y);
    const Recipe *r = game_machine_recipe(g, x, y);
    if (!def || !st || !def->automat || !r || st->work <= 0) return -1.0f;
    return (float)(st->work - 1) / (float)work_ticks(def, r);
}

/* Whether everything a recipe takes is in a machine's input slots. */
static bool has_inputs(const Container *c, const TileDef *def, const Recipe *r)
{
    for (int i = 0; i < r->input_count; i++)
        if (count_item_in(c, 0, def->automat_inputs, r->inputs[i].id) < r->inputs[i].count) return false;
    return true;
}

static void run_automat(Game *g, Storage *st, const TileDef *def)
{
    Container *c = &g->inventory.containers[st->container];
    int in = def->automat_inputs, out = in + 1;
    if (st->recipe > content.recipe_count) st->recipe = 0, st->work = 0;
    const Recipe *r = st->recipe > 0 ? &content.recipes[st->recipe - 1] : NULL;
    /* (What it was set to has to be something it can still make, where it stands.) */
    if (r && !machine_allows(&g->world, def, st->x, st->y, r)) r = NULL, st->recipe = 0, st->work = 0;
    if (st->work == 0) {
        /* Nothing in hand. One with a kind of bench of its own looks at what is in it and takes the first thing that can be made of it. */
        if (def->automat_kind[0]) {
            r = NULL;
            st->recipe = 0;
            for (int i = 0; i < content.recipe_count && !r; i++) {
                const Recipe *maybe = &content.recipes[i];
                if (!machine_allows(&g->world, def, st->x, st->y, maybe) || !has_inputs(c, def, maybe)) continue;
                if (room_for_item(c, in, out, maybe->output.id) < maybe->output.count) continue;
                r = maybe;
                st->recipe = (int16_t)(i + 1);
            }
        }
        /* It starts when what it takes is there, there is room for what it makes, and it has the electricity to. */
        if (!r || !has_inputs(c, def, r) || room_for_item(c, in, out, r->output.id) < r->output.count || st->power < def->draw) return;
        for (int i = 0; i < r->input_count; i++) take_item(c, 0, in, r->inputs[i].id, r->inputs[i].count);
        st->work = 1;
    }
    if (!r) {
        st->work = 0;
        return;
    }
    /* A tick of work, for a tick's electricity. */
    if (st->power < def->draw) return;
    if (st->work <= work_ticks(def, r)) {
        st->power -= def->draw;
        st->work++;
    }
    if (st->work <= work_ticks(def, r)) return;
    /* Done: out it comes, as soon as there is room for it. */
    if (room_for_item(c, in, out, r->output.id) < r->output.count) return;
    put_item(c, in, out, r->output.id, r->output.count);
    st->work = 0;
    if (def->automat_kind[0]) st->recipe = 0;
}

static void run_miner(Game *g, Storage *st, const TileDef *def)
{
    const World *w = &g->world;
    Container *c = &g->inventory.containers[st->container];
    const TileDef *source = NULL;
    for (int d = 0; d < 4 && !source; d++) {
        const TileDef *there = tile_def(world_get(w, st->x + beside[d][0], st->y + beside[d][1]));
        if (there->endless && there->drop != ITEM_NONE) source = there;
    }
    int give = source ? (source->drop_count > 0 ? source->drop_count : 1) : 0;
    if (!source || st->power < def->draw || room_for_item(c, 0, slot_count(def), source->drop) < give) return;
    st->power -= def->draw;
    if (++st->work < def->miner_ticks) return;
    st->work = 0;
    put_item(c, 0, slot_count(def), source->drop, give);
}

/* ---- conveyors --------------------------------------------------------- */

/* The slots of a thing that a conveyor may put an item in: `from` up to `to`. False if it will not have the item at all. */
static bool takes_item(Game *g, const Storage *st, const TileDef *def, ItemId item, int *from, int *to)
{
    if (!st || st->container == 0 || def->extractor || def->miner_ticks > 0 || def->storage_shared || def->storage_columns <= 0) return false;
    const Container *c = &g->inventory.containers[st->container];
    *from = 0;
    *to = slot_count(def);
    if (def->automat) {
        *to = def->automat_inputs;
        if (!def->automat_kind[0]) {
            /* Told what to make: only what that takes, each kind in the slot that is its own. */
            const Recipe *r = st->recipe > 0 && st->recipe <= content.recipe_count ? &content.recipes[st->recipe - 1] : NULL;
            for (int i = 0; r && i < r->input_count; i++) {
                if (r->inputs[i].id != item) continue;
                *from = i;
                *to = i + 1;
                return true;
            }
            return false;
        }
        /* Making whatever it is given: only what something it makes is made of. */
        for (int i = 0; i < content.recipe_count; i++) {
            const Recipe *r = &content.recipes[i];
            if (strcmp(r->bench, def->automat_kind) != 0 || !machine_can_make(def, r)) continue;
            for (int k = 0; k < r->input_count; k++)
                if (r->inputs[k].id == item) return true;
        }
        return false;
    }
    if (def->sorts > 0) {
        /* Sorted: if anything is in its first slots, only those kinds of thing. */
        bool any = false, listed = false;
        for (int i = 0; i < def->sorts && i < *to; i++) {
            if (c->slots[i].count == 0) continue;
            any = true;
            listed = listed || c->slots[i].id == item;
        }
        if (any && !listed) return false;
    }
    return true;
}

static void run_extractor(Game *g, Storage *st)
{
    World *w = &g->world;
    /* The one kind of thing it takes, if it has been given one to go by. */
    ItemId only = ITEM_NONE;
    if (st->container != 0 && g->inventory.containers[st->container].slots[0].count > 0)
        only = g->inventory.containers[st->container].slots[0].id;

    /* Along the conveyors from it, nearest first: what stands beside them that holds things. */
    static int32_t queue[MAX_ROUTE];
    static int16_t slowest[MAX_ROUTE];
    struct {
        Storage *st;
        const TileDef *def;
        int rate;
    } sinks[MAX_SINKS];
    int head = 0, tail = 0, sink_count = 0;
    uint32_t gen = ++g->bfs_gen;
    g->bfs_seen[(size_t)(st->y * w->width + st->x) * 2] = gen;
    for (int d = 0; d < 4; d++) {
        int nx = world_wrap_x(w, st->x + beside[d][0]), ny = st->y + beside[d][1], rate = conveyor_here(w, nx, ny);
        if (rate <= 0 || g->bfs_seen[(size_t)(ny * w->width + nx) * 2] == gen) continue;
        g->bfs_seen[(size_t)(ny * w->width + nx) * 2] = gen;
        slowest[tail] = (int16_t)rate;
        queue[tail++] = ny * w->width + nx;
    }
    while (head < tail) {
        int at = queue[head], ax = at % w->width, ay = at / w->width, rate = slowest[head];
        head++;
        for (int d = 0; d < 4; d++) {
            int nx = world_wrap_x(w, ax + beside[d][0]), ny = ay + beside[d][1];
            if (!world_in_bounds(w, nx, ny)) continue;
            size_t cell = (size_t)(ny * w->width + nx) * 2;
            if (g->bfs_seen[cell] == gen) continue;
            int next = conveyor_here(w, nx, ny);
            if (next > 0) {
                if (tail >= MAX_ROUTE) continue;
                g->bfs_seen[cell] = gen;
                slowest[tail] = (int16_t)(next < rate ? next : rate);
                queue[tail++] = ny * w->width + nx;
                continue;
            }
            const TileDef *def = block_here(w, nx, ny);
            Storage *there = def ? storage_here(g, nx, ny) : NULL;
            if (!there || sink_count >= MAX_SINKS) continue;
            g->bfs_seen[cell] = gen;
            sinks[sink_count].st = there;
            sinks[sink_count].def = def;
            sinks[sink_count++].rate = rate;
        }
    }
    if (sink_count == 0) return;

    /* From whatever holds things beside it: the first thing in there that something along the way will have. */
    for (int d = 0; d < 4; d++) {
        int sx = world_wrap_x(w, st->x + beside[d][0]), sy = st->y + beside[d][1];
        const TileDef *from_def = block_here(w, sx, sy);
        Storage *from = from_def ? storage_here(g, sx, sy) : NULL;
        if (!from || from->container == 0 || from_def->extractor || from_def->storage_shared || from_def->storage_columns <= 0) continue;
        Container *c = &g->inventory.containers[from->container];
        /* (Out of a machine comes only what it has made.) */
        int first = from_def->automat ? from_def->automat_inputs : 0, last = from_def->automat ? first + 1 : slot_count(from_def);
        for (int i = first; i < last; i++) {
            const ItemStack *s = &c->slots[i];
            if (s->count == 0 || !plain(s) || (only != ITEM_NONE && s->id != only)) continue;
            ItemId item = s->id;
            for (int k = 0; k < sink_count; k++) {
                int lo, hi;
                if (sinks[k].st == from || !takes_item(g, sinks[k].st, sinks[k].def, item, &lo, &hi)) continue;
                Container *to = &g->inventory.containers[sinks[k].st->container];
                int n = room_for_item(to, lo, hi, item), have = count_item_in(c, first, last, item);
                if (n > sinks[k].rate) n = sinks[k].rate;
                if (n > have) n = have;
                if (n <= 0) continue;
                put_item(to, lo, hi, item, take_item(c, first, last, item, n));
                return; /* (one lot a tick) */
            }
        }
    }
}

void industry_tick(Game *g)
{
    const World *w = &g->world;
    for (int i = 0; i < MAX_STORAGES; i++) {
        Storage *st = &g->storages[i];
        if (!st->used) continue;
        const TileDef *def = block_here(w, st->x, st->y);
        if (!def) continue;
        if (def->miner_ticks > 0 && st->container != 0) run_miner(g, st, def);
        else if (def->automat && st->container != 0) run_automat(g, st, def);
    }
    /* (Then what has been made is moved on.) */
    for (int i = 0; i < MAX_STORAGES; i++) {
        Storage *st = &g->storages[i];
        if (!st->used) continue;
        const TileDef *def = block_here(w, st->x, st->y);
        if (def && def->extractor) run_extractor(g, st);
    }
}
