#include "digital.h"

#include "industry.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NET_CELLS 4096 /* squares of one network */
#define MAX_PARTS 32       /* of each kind of thing in one */
#define BUS_RATE 4         /* things a bus moves in a tick */
#define PLANE_TICKS 10     /* ticks between one block and the next, for a plane */
#define MAX_PLAN_DEPTH 8

static const int beside[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

/* A network, as found by following its cables from any part of it. */
typedef struct {
    Storage *controller;
    Storage *drives[MAX_PARTS], *imports[MAX_PARTS], *exports[MAX_PARTS], *breakers[MAX_PARTS], *placers[MAX_PARTS];
    TilePos stores[MAX_PARTS], access[MAX_PARTS], assemblers[MAX_PARTS];
    int drive_count, import_count, export_count, breaker_count, placer_count, store_count, access_count, assembler_count;
    int cpus, coprocessors, devices;
} Net;

void digital_init(Game *g)
{
    g->cells = calloc(MAX_CELLS, sizeof(CellStore));
    g->jobs = calloc(MAX_NET_JOBS, sizeof(NetJob));
}

void digital_free(Game *g)
{
    free(g->cells);
    free(g->jobs);
    g->cells = NULL;
    g->jobs = NULL;
}

static Storage *storage_here(Game *g, int x, int y)
{
    x = world_wrap_x(&g->world, x);
    for (int i = 0; i < MAX_STORAGES; i++)
        if (g->storages[i].used && g->storages[i].x == x && g->storages[i].y == y) return &g->storages[i];
    return NULL;
}

/* What part of a network the block at a square is, if any. */
static const TileDef *part_here(const World *w, int x, int y)
{
    if (!world_in_bounds(w, x, y)) return NULL;
    for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
        const TileDef *def = tile_def(world_get_at(w, layer, x, y));
        if (def->net_role != NET_NONE) return def;
    }
    return NULL;
}

/* Follows the network out from a square. False if there is no part of one there. */
static bool scan(Game *g, int x, int y, Net *net)
{
    const World *w = &g->world;
    static int32_t queue[MAX_NET_CELLS];
    int head = 0, tail = 0;
    memset(net, 0, sizeof *net);
    x = world_wrap_x(w, x);
    if (!part_here(w, x, y)) return false;
    uint32_t gen = ++g->bfs_gen;
    g->bfs_seen[(size_t)(y * w->width + x) * 2] = gen;
    queue[tail++] = y * w->width + x;
    bool bridged = false;
    while (head < tail) {
        int at = queue[head++], ax = at % w->width, ay = at / w->width;
        const TileDef *def = part_here(w, ax, ay);
        Storage *st = storage_here(g, ax, ay);
        if (def->net_role != NET_CABLE) net->devices++;
        switch (def->net_role) {
        case NET_CONTROLLER: if (!net->controller) net->controller = st; break;
        case NET_DRIVE: if (st && net->drive_count < MAX_PARTS) net->drives[net->drive_count++] = st; break;
        case NET_IMPORT: if (st && net->import_count < MAX_PARTS) net->imports[net->import_count++] = st; break;
        case NET_EXPORT: if (st && net->export_count < MAX_PARTS) net->exports[net->export_count++] = st; break;
        case NET_PLANE_BREAK: if (st && net->breaker_count < MAX_PARTS) net->breakers[net->breaker_count++] = st; break;
        case NET_PLANE_PLACE: if (st && net->placer_count < MAX_PARTS) net->placers[net->placer_count++] = st; break;
        case NET_STORAGE_BUS: if (net->store_count < MAX_PARTS) net->stores[net->store_count++] = (TilePos){ ax, ay }; break;
        case NET_ACCESS: if (net->access_count < MAX_PARTS) net->access[net->access_count++] = (TilePos){ ax, ay }; break;
        case NET_ASSEMBLER: if (net->assembler_count < MAX_PARTS) net->assemblers[net->assembler_count++] = (TilePos){ ax, ay }; break;
        case NET_CPU: net->cpus++; break;
        case NET_COPROCESSOR: net->coprocessors++; break;
        default: break;
        }
        for (int d = 0; d < 4; d++) {
            int nx = world_wrap_x(w, ax + beside[d][0]), ny = ay + beside[d][1];
            if (!part_here(w, nx, ny) || tail >= MAX_NET_CELLS) continue;
            size_t cell = (size_t)(ny * w->width + nx) * 2;
            if (g->bfs_seen[cell] == gen) continue;
            g->bfs_seen[cell] = gen;
            queue[tail++] = ny * w->width + nx;
        }
        /* A bridge is joined to every other bridge there is: they are found by the note each keeps of itself. */
        if (def->net_role == NET_BRIDGE && !bridged) {
            bridged = true;
            for (int i = 0; i < MAX_STORAGES && tail < MAX_NET_CELLS; i++) {
                const Storage *other = &g->storages[i];
                if (!other->used) continue;
                const TileDef *there = part_here(w, other->x, other->y);
                size_t cell = (size_t)(other->y * w->width + other->x) * 2;
                if (!there || there->net_role != NET_BRIDGE || g->bfs_seen[cell] == gen) continue;
                g->bfs_seen[cell] = gen;
                queue[tail++] = other->y * w->width + other->x;
            }
        }
    }
    return net->controller != NULL;
}

static bool online(const Net *net)
{
    return net->controller && net->controller->work > 0;
}

/* ---- cells ------------------------------------------------------------- */

/* The storage cells in a drive: each slot of it that has one. Which of the
 * world's cells one is, is what its meter reads; one that reads nothing is
 * given a number when something first goes into it. */
static CellStore *cell_of(Game *g, Storage *drive, int slot, bool make, int *bytes)
{
    if (!drive || drive->container == 0) return NULL;
    ItemStack *s = &g->inventory.containers[drive->container].slots[slot];
    if (s->count == 0 || item_def(s->id)->cell_bytes <= 0) return NULL;
    if (bytes) *bytes = item_def(s->id)->cell_bytes;
    int id = inventory_meter(&g->inventory, s);
    if (id > 0 && id < MAX_CELLS) {
        g->cells[id].used = true;
        return &g->cells[id];
    }
    if (!make) return NULL;
    for (int pass = 0; pass < 2; pass++) {
        for (id = 1; id < MAX_CELLS; id++) {
            if (g->cells[id].used) continue;
            if (!inventory_set_meter(&g->inventory, s, id)) return NULL;
            memset(&g->cells[id], 0, sizeof g->cells[id]);
            g->cells[id].used = true;
            return &g->cells[id];
        }
        /* (None free: the ones with nothing in them are, whatever still points at them.) */
        for (id = 1; id < MAX_CELLS; id++)
            if (g->cells[id].types == 0) g->cells[id].used = false;
    }
    return NULL;
}

static long cell_items(const CellStore *c)
{
    long n = 0;
    for (int i = 0; i < c->types; i++) n += c->held[i].count;
    return n;
}

static long cell_used(const CellStore *c, int bytes)
{
    return (long)c->types * (bytes / 128) + (cell_items(c) + 7) / 8;
}

/* How many more of a thing a cell has room for. */
static long cell_room(const CellStore *c, int bytes, ItemId item)
{
    bool has = false;
    for (int i = 0; i < c->types; i++) has = has || c->held[i].id == item;
    if (!has && c->types >= CELL_TYPES) return 0;
    long free_bytes = bytes - (long)(c->types + (has ? 0 : 1)) * (bytes / 128);
    long room = free_bytes * 8 - cell_items(c);
    return room > 0 ? room : 0;
}

static long cell_add(CellStore *c, int bytes, ItemId item, long count)
{
    long room = cell_room(c, bytes, item);
    if (count > room) count = room;
    if (count <= 0) return 0;
    for (int i = 0; i < c->types; i++) {
        if (c->held[i].id != item) continue;
        c->held[i].count += (uint32_t)count;
        return count;
    }
    c->held[c->types].id = (uint16_t)item;
    c->held[c->types++].count = (uint32_t)count;
    return count;
}

static long cell_take(CellStore *c, ItemId item, long count)
{
    for (int i = 0; i < c->types; i++) {
        if (c->held[i].id != item) continue;
        if (count > (long)c->held[i].count) count = (long)c->held[i].count;
        c->held[i].count -= (uint32_t)count;
        if (c->held[i].count == 0) c->held[i] = c->held[--c->types];
        return count;
    }
    return 0;
}

static int drive_slots(Game *g, const Storage *drive)
{
    const TileDef *def = part_here(&g->world, drive->x, drive->y);
    int n = def ? def->storage_columns * def->storage_rows : 0;
    return n > CONTAINER_MAX_SLOTS ? CONTAINER_MAX_SLOTS : n;
}

/* ---- what a network holds ---------------------------------------------- */

static bool storable(ItemId item)
{
    return item > ITEM_NONE && item < content.item_count && item_def(item)->meter_max == 0 && item_def(item)->crystals == 0;
}

static long net_count(Game *g, Net *net, ItemId item)
{
    long n = 0;
    for (int d = 0; d < net->drive_count; d++) {
        for (int k = 0; k < drive_slots(g, net->drives[d]); k++) {
            const CellStore *c = cell_of(g, net->drives[d], k, false, NULL);
            for (int i = 0; c && i < c->types; i++)
                if (c->held[i].id == item) n += c->held[i].count;
        }
    }
    for (int s = 0; s < net->store_count; s++)
        for (int d = 0; d < 4; d++) n += industry_count(g, net->stores[s].x + beside[d][0], net->stores[s].y + beside[d][1], item);
    return n;
}

static long net_insert(Game *g, Net *net, ItemId item, long count)
{
    long put = 0;
    if (!storable(item) || count <= 0) return 0;
    /* Into a cell that has some of it already; then any cell with room; then whatever a storage bus stands beside. */
    for (int pass = 0; pass < 2 && put < count; pass++) {
        for (int d = 0; d < net->drive_count && put < count; d++) {
            for (int k = 0; k < drive_slots(g, net->drives[d]) && put < count; k++) {
                int bytes = 0;
                CellStore *c = cell_of(g, net->drives[d], k, pass == 1, &bytes);
                if (!c) continue;
                bool has = false;
                for (int i = 0; i < c->types; i++) has = has || c->held[i].id == item;
                if (pass == 0 && !has) continue;
                put += cell_add(c, bytes, item, count - put);
            }
        }
    }
    for (int s = 0; s < net->store_count && put < count; s++)
        for (int d = 0; d < 4 && put < count; d++)
            put += industry_put(g, net->stores[s].x + beside[d][0], net->stores[s].y + beside[d][1], item, (int)(count - put > 30000 ? 30000 : count - put));
    return put;
}

static long net_extract(Game *g, Net *net, ItemId item, long count)
{
    long got = 0;
    for (int d = 0; d < net->drive_count && got < count; d++) {
        for (int k = 0; k < drive_slots(g, net->drives[d]) && got < count; k++) {
            CellStore *c = cell_of(g, net->drives[d], k, false, NULL);
            if (c) got += cell_take(c, item, count - got);
        }
    }
    for (int s = 0; s < net->store_count && got < count; s++)
        for (int d = 0; d < 4 && got < count; d++)
            got += industry_take(g, net->stores[s].x + beside[d][0], net->stores[s].y + beside[d][1], item, (int)(count - got > 30000 ? 30000 : count - got), NULL);
    return got;
}

/* ---- what a network can make ------------------------------------------- */

static bool plain_recipe(const Recipe *r)
{
    if (r->crystals > 0 || r->random_total > 0 || r->characters > 0 || r->input_count < 1) return false;
    if (!storable(r->output.id) || item_def(r->output.id)->becomes_player) return false;
    for (int i = 0; i < r->input_count; i++)
        if (r->inputs[i].full || !storable(r->inputs[i].id)) return false;
    return true;
}

/* Whether a bench that makes a recipe stands beside one of the network's assemblers. */
static bool net_makes(Game *g, const Net *net, const Recipe *r)
{
    const World *w = &g->world;
    if (!plain_recipe(r)) return false;
    for (int a = 0; a < net->assembler_count; a++) {
        for (int d = 0; d < 4; d++) {
            for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
                const TileDef *def = tile_def(world_get_at(w, layer, net->assemblers[a].x + beside[d][0], net->assemblers[a].y + beside[d][1]));
                if (def->bench && strcmp(def->bench_kind, r->bench) == 0 && def->bench_level >= r->level) return true;
            }
        }
    }
    return false;
}

static const Recipe *net_recipe(Game *g, const Net *net, ItemId item)
{
    for (int i = 0; net->assembler_count > 0 && i < content.recipe_count; i++)
        if (content.recipes[i].output.id == item && net_makes(g, net, &content.recipes[i])) return &content.recipes[i];
    return NULL;
}

/* Working out a job: what is in store is used first, and what is short is made, and what that takes in its turn. */
typedef struct {
    Game *g;
    Net *net;
    NetJob *job;
    struct {
        ItemId item;
        long left;
    } ledger[64];
    int ledger_count;
    ItemId short_of;
} Plan;

static long *ledger(Plan *p, ItemId item)
{
    for (int i = 0; i < p->ledger_count; i++)
        if (p->ledger[i].item == item) return &p->ledger[i].left;
    if (p->ledger_count >= 64) return NULL;
    p->ledger[p->ledger_count].item = item;
    p->ledger[p->ledger_count].left = net_count(p->g, p->net, item);
    return &p->ledger[p->ledger_count++].left;
}

static bool plan_item(Plan *p, ItemId item, long need, int depth, bool make_all)
{
    long *have = ledger(p, item);
    if (!have) return false;
    if (!make_all) {
        long use = *have < need ? *have : need;
        *have -= use;
        need -= use;
    }
    if (need <= 0) return true;
    const Recipe *r = depth < MAX_PLAN_DEPTH ? net_recipe(p->g, p->net, item) : NULL;
    if (!r || p->job->step_count >= MAX_JOB_STEPS) {
        p->short_of = item;
        return false;
    }
    long crafts = (need + r->output.count - 1) / r->output.count;
    for (int i = 0; i < r->input_count; i++)
        if (!plan_item(p, r->inputs[i].id, (long)r->inputs[i].count * crafts, depth + 1, false)) return false;
    if (p->job->step_count >= MAX_JOB_STEPS) {
        p->short_of = item;
        return false;
    }
    p->job->steps[p->job->step_count].recipe = (int16_t)(r - content.recipes);
    p->job->steps[p->job->step_count].crafts = (int32_t)crafts;
    p->job->steps[p->job->step_count++].done = 0;
    have = ledger(p, item);
    if (have && !make_all) *have += crafts * r->output.count - need;
    return true;
}

static int jobs_of(const Game *g, const Net *net)
{
    int n = 0;
    for (int i = 0; i < MAX_NET_JOBS; i++)
        n += g->jobs[i].used && g->jobs[i].x == net->controller->x && g->jobs[i].y == net->controller->y;
    return n;
}

/* ---- the outside of it ------------------------------------------------- */

bool digital_info(Game *g, int x, int y, NetInfo *out)
{
    Net net;
    memset(out, 0, sizeof *out);
    if (!scan(g, x, y, &net)) return false;
    out->found = true;
    out->online = online(&net);
    out->controller = (TilePos){ net.controller->x, net.controller->y };
    out->devices = net.devices;
    out->cpus = net.cpus;
    out->coprocessors = net.coprocessors;
    out->cpus_busy = jobs_of(g, &net);
    out->drives = net.drive_count;
    for (int d = 0; d < net.drive_count; d++) {
        for (int k = 0; k < drive_slots(g, net.drives[d]); k++) {
            int bytes = 0;
            const ItemStack *s = &g->inventory.containers[net.drives[d]->container].slots[k];
            if (s->count == 0 || item_def(s->id)->cell_bytes <= 0) continue;
            bytes = item_def(s->id)->cell_bytes;
            const CellStore *c = cell_of(g, net.drives[d], k, false, NULL);
            out->cells++;
            out->bytes_total += bytes;
            out->type_room += CELL_TYPES;
            if (!c) continue;
            out->bytes_used += cell_used(c, bytes);
            out->types += c->types;
            out->type_room -= c->types;
        }
    }
    return true;
}

static int by_count(const void *a, const void *b)
{
    const NetItem *x = a, *y = b;
    if (x->count != y->count) return x->count > y->count ? -1 : 1;
    return (int)x->item - (int)y->item;
}

int digital_list(Game *g, int x, int y, NetItem *out, int max)
{
    Net net;
    static long counts[MAX_ITEMS];
    if (!scan(g, x, y, &net) || !online(&net)) return 0;
    memset(counts, 0, sizeof counts);
    for (int d = 0; d < net.drive_count; d++) {
        for (int k = 0; k < drive_slots(g, net.drives[d]); k++) {
            const CellStore *c = cell_of(g, net.drives[d], k, false, NULL);
            for (int i = 0; c && i < c->types; i++)
                if (c->held[i].id < content.item_count) counts[c->held[i].id] += c->held[i].count;
        }
    }
    for (int s = 0; s < net.store_count; s++) {
        for (int d = 0; d < 4; d++) {
            ItemId kinds[CONTAINER_MAX_SLOTS];
            int sx = net.stores[s].x + beside[d][0], sy = net.stores[s].y + beside[d][1], n = industry_kinds(g, sx, sy, kinds, CONTAINER_MAX_SLOTS);
            for (int i = 0; i < n; i++) counts[kinds[i]] += industry_count(g, sx, sy, kinds[i]);
        }
    }
    int n = 0;
    for (ItemId i = 1; i < content.item_count && n < max; i++) {
        bool makes = net.assembler_count > 0 && net_recipe(g, &net, i) != NULL;
        if (counts[i] <= 0 && !makes) continue;
        out[n++] = (NetItem){ i, counts[i], makes };
    }
    qsort(out, (size_t)n, sizeof out[0], by_count);
    return n;
}

long digital_count(Game *g, int x, int y, ItemId item)
{
    Net net;
    return scan(g, x, y, &net) && online(&net) ? net_count(g, &net, item) : 0;
}

int digital_insert(Game *g, int x, int y, ItemId item, int count)
{
    Net net;
    return scan(g, x, y, &net) && online(&net) ? (int)net_insert(g, &net, item, count) : 0;
}

int digital_extract(Game *g, int x, int y, ItemId item, int count)
{
    Net net;
    return scan(g, x, y, &net) && online(&net) ? (int)net_extract(g, &net, item, count) : 0;
}

int digital_deposit(Game *g, int x, int y, int bar_slot, int count)
{
    if (bar_slot < 0 || bar_slot >= INVENTORY_SLOTS) return 0;
    ItemStack *s = &g->inventory.slots[bar_slot];
    if (s->count == 0 || s->container != 0 || !storable(s->id)) return 0;
    int want = count > 0 && count < s->count ? count : s->count;
    int put = digital_insert(g, x, y, s->id, want);
    s->count -= (uint16_t)put;
    if (s->count == 0) *s = (ItemStack){ 0 };
    return put;
}

int digital_withdraw(Game *g, int x, int y, ItemId item, int count)
{
    int got = digital_extract(g, x, y, item, count);
    if (got > 0) game_give_items(g, item, got);
    return got;
}

const Recipe *digital_recipe_for(Game *g, int x, int y, ItemId item)
{
    Net net;
    return scan(g, x, y, &net) && online(&net) ? net_recipe(g, &net, item) : NULL;
}

bool digital_request(Game *g, int x, int y, ItemId item, int amount, char *why, size_t size)
{
    Net net;
    if (size > 0) why[0] = '\0';
    if (!scan(g, x, y, &net) || !online(&net)) return snprintf(why, size, "No network"), false;
    if (amount < 1 || !net_recipe(g, &net, item)) return snprintf(why, size, "Cannot be made here"), false;
    if (net.cpus < 1) return snprintf(why, size, "No crafting CPU"), false;
    if (jobs_of(g, &net) >= net.cpus) return snprintf(why, size, "Every CPU is busy"), false;
    NetJob *job = NULL;
    for (int i = 0; i < MAX_NET_JOBS && !job; i++)
        if (!g->jobs[i].used) job = &g->jobs[i];
    if (!job) return snprintf(why, size, "Too many jobs"), false;
    memset(job, 0, sizeof *job);
    Plan plan = { .g = g, .net = &net, .job = job };
    if (!plan_item(&plan, item, amount, 0, true)) {
        if (plan.short_of != ITEM_NONE) snprintf(why, size, "Missing %s", item_def(plan.short_of)->label);
        else snprintf(why, size, "Too much to work out");
        memset(job, 0, sizeof *job);
        return false;
    }
    job->used = true;
    job->x = net.controller->x;
    job->y = net.controller->y;
    job->wanted = item;
    job->amount = amount;
    return true;
}

int digital_jobs(Game *g, int x, int y, const NetJob **out, int max)
{
    Net net;
    int n = 0;
    if (!scan(g, x, y, &net)) return 0;
    for (int i = 0; i < MAX_NET_JOBS && n < max; i++)
        if (g->jobs[i].used && g->jobs[i].x == net.controller->x && g->jobs[i].y == net.controller->y) out[n++] = &g->jobs[i];
    return n;
}

bool digital_cancel(Game *g, const NetJob *job)
{
    for (int i = 0; i < MAX_NET_JOBS; i++) {
        if (&g->jobs[i] != job || !g->jobs[i].used) continue;
        g->jobs[i].used = false;
        return true;
    }
    return false;
}

bool digital_remote(Game *g, TilePos *at)
{
    const World *w = &g->world;
    const ItemStack *held = inventory_selected(&g->inventory);
    if (held->count == 0 || !item_def(held->id)->remote) return false;
    long best = -1;
    for (int i = 0; i < MAX_STORAGES; i++) {
        const Storage *st = &g->storages[i];
        const TileDef *def = st->used ? part_here(w, st->x, st->y) : NULL;
        if (!def || def->net_role != NET_ACCESS) continue;
        long dx = world_dx(w, g->player.feet.x, st->x), dy = st->y - g->player.feet.y, far = dx * dx + dy * dy;
        if (far > (long)def->net_range * def->net_range || (best >= 0 && far >= best)) continue;
        Net net;
        if (!scan(g, st->x, st->y, &net) || !online(&net)) continue;
        best = far;
        *at = (TilePos){ st->x, st->y };
    }
    return best >= 0;
}

/* ---- the tick ---------------------------------------------------------- */

static ItemId filter_of(Game *g, const Storage *st)
{
    if (st->container == 0) return ITEM_NONE;
    const ItemStack *s = &g->inventory.containers[st->container].slots[0];
    return s->count > 0 ? (ItemId)s->id : ITEM_NONE;
}

static void run_job(Game *g, Net *net, NetJob *job)
{
    if (job->step >= job->step_count) {
        job->used = false;
        return;
    }
    const Recipe *r = &content.recipes[job->steps[job->step].recipe];
    if (job->timer == 0) {
        /* The next one: what it takes comes out of the store, all of it or none. */
        job->waiting = false;
        for (int i = 0; i < r->input_count; i++)
            if (net_count(g, net, r->inputs[i].id) < r->inputs[i].count) job->waiting = true;
        if (job->waiting || !net_makes(g, net, r)) {
            job->waiting = true;
            return;
        }
        for (int i = 0; i < r->input_count; i++) net_extract(g, net, r->inputs[i].id, r->inputs[i].count);
        float rate = (float)(content.rules.tick_rate > 0 ? content.rules.tick_rate : 10);
        job->timer = (int)ceilf(game_craft_time(r) * rate / (float)(1 + net->coprocessors));
        if (job->timer < 1) job->timer = 1;
    }
    if (job->timer > 1) {
        job->timer--;
        return;
    }
    /* Made: into the store it goes, when there is room for all of it. */
    long room_needed = r->output.count;
    long put = net_insert(g, net, r->output.id, room_needed);
    if (put < room_needed) {
        /* (No room: what did go in comes out again, and it waits.) */
        net_extract(g, net, r->output.id, put);
        job->waiting = true;
        return;
    }
    job->waiting = false;
    job->timer = 0;
    if (++job->steps[job->step].done >= job->steps[job->step].crafts) job->step++;
    if (job->step >= job->step_count) job->used = false;
}

static void run_net(Game *g, Net *net)
{
    World *w = &g->world;
    /* Import buses: out of whatever holds things beside each, into the store. */
    for (int b = 0; b < net->import_count; b++) {
        ItemId only = filter_of(g, net->imports[b]);
        int budget = BUS_RATE;
        for (int d = 0; d < 4 && budget > 0; d++) {
            int sx = net->imports[b]->x + beside[d][0], sy = net->imports[b]->y + beside[d][1];
            ItemId kinds[CONTAINER_MAX_SLOTS];
            int n = industry_kinds(g, sx, sy, kinds, CONTAINER_MAX_SLOTS);
            for (int i = 0; i < n && budget > 0; i++) {
                if (only != ITEM_NONE && kinds[i] != only) continue;
                int have = industry_count(g, sx, sy, kinds[i]), want = have < budget ? have : budget;
                long put = net_insert(g, net, kinds[i], want);
                if (put > 0) industry_take(g, sx, sy, kinds[i], (int)put, NULL);
                budget -= (int)put;
            }
        }
    }
    /* Export buses: the kind of thing each has been given to go by, out of the store into whatever holds things beside it. */
    for (int b = 0; b < net->export_count; b++) {
        ItemId item = filter_of(g, net->exports[b]);
        int budget = BUS_RATE;
        if (item == ITEM_NONE) continue;
        for (int d = 0; d < 4 && budget > 0; d++) {
            int sx = net->exports[b]->x + beside[d][0], sy = net->exports[b]->y + beside[d][1];
            long have = net_count(g, net, item);
            int want = have < budget ? (int)have : budget;
            if (want <= 0) break;
            int put = industry_put(g, sx, sy, item, want);
            if (put > 0) net_extract(g, net, item, put);
            budget -= put;
        }
    }
    if (g->tick % PLANE_TICKS == 0) {
        /* Planes that break: a plain block beside each, into the store, if there is room for what it leaves. */
        for (int b = 0; b < net->breaker_count; b++) {
            for (int d = 0; d < 4; d++) {
                int bx = world_wrap_x(w, net->breakers[b]->x + beside[d][0]), by = net->breakers[b]->y + beside[d][1];
                TileType t = world_get(w, bx, by);
                const TileDef *def = tile_def(t);
                if (t == TILE_AIR || !def->solid || !def->removable || def->endless || def->bench || def->trade || def->storage_columns > 0 ||
                    def->height > 1 || def->pair != TILE_AIR || def->pair_of != TILE_AIR || def->net_role != NET_NONE || def->drop == ITEM_NONE)
                    continue;
                int n = def->drop_count > 0 ? def->drop_count : 1;
                if (net_insert(g, net, def->drop, n) < n) continue;
                world_set(w, bx, by, TILE_AIR);
                break;
            }
        }
        /* Planes that place: the block each has been given to go by, out of the store into an open square beside it. */
        for (int b = 0; b < net->placer_count; b++) {
            ItemId item = filter_of(g, net->placers[b]);
            TileType block = item != ITEM_NONE ? item_def(item)->places : TILE_AIR;
            if (block == TILE_AIR || !tile_def(block)->solid || tile_def(block)->layer != LAYER_FOREGROUND || tile_def(block)->height > 1 ||
                tile_def(block)->pair != TILE_AIR || net_count(g, net, item) < 1)
                continue;
            for (int d = 0; d < 4; d++) {
                int bx = world_wrap_x(w, net->placers[b]->x + beside[d][0]), by = net->placers[b]->y + beside[d][1];
                if (!world_in_bounds(w, bx, by) || world_get(w, bx, by) != TILE_AIR || world_is_solid(w, bx, by) || part_here(w, bx, by)) continue;
                bool taken = false;
                for (int c = 0; c < g->character_count; c++) {
                    TilePos feet = c == g->live ? g->player.feet : g->characters[c].player.feet;
                    taken = taken || (feet.x == bx && (feet.y == by || feet.y - 1 == by));
                }
                if (taken || net_extract(g, net, item, 1) < 1) continue;
                world_set(w, bx, by, block);
                break;
            }
        }
    }
    /* Jobs: as many at once as there are CPUs. */
    int running = 0;
    for (int i = 0; i < MAX_NET_JOBS; i++) {
        NetJob *job = &g->jobs[i];
        if (!job->used || job->x != net->controller->x || job->y != net->controller->y) continue;
        if (running++ < net->cpus) run_job(g, net, job);
    }
}

void digital_tick(Game *g)
{
    const World *w = &g->world;
    bool any = false;
    for (int i = 0; i < MAX_STORAGES; i++) {
        Storage *st = &g->storages[i];
        const TileDef *def = st->used ? part_here(w, st->x, st->y) : NULL;
        if (!def || def->net_role != NET_CONTROLLER) continue;
        any = true;
        Net net;
        /* (One controller to a network: a second one on the same cables does nothing.) */
        if (!scan(g, st->x, st->y, &net) || net.controller != st) {
            st->work = 0;
            continue;
        }
        /* It runs while it has the electricity for the tick. */
        if (st->power < def->draw) {
            st->work = 0;
            continue;
        }
        st->power -= def->draw;
        st->work = 1;
        run_net(g, &net);
    }
    /* (A job whose network is gone is gone with it.) */
    for (int i = 0; i < MAX_NET_JOBS; i++) {
        NetJob *job = &g->jobs[i];
        if (!job->used) continue;
        const TileDef *def = any ? part_here(w, job->x, job->y) : NULL;
        if (!def || def->net_role != NET_CONTROLLER) job->used = false;
    }
}
