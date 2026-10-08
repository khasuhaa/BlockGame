#include "trade.h"

#include <stdio.h>
#include <stdlib.h>

bool game_trade_open(const Game *g)
{
    return g->bench_open && tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y))->trade;
}

int trade_price(ItemId item)
{
    const ItemDef *def = item_def(item);
    if (def->category < 0) return 0;
    return def->price > 0 ? def->price : content.trade.default_price;
}

int trade_sell_price(ItemId item)
{
    return (int)((float)trade_price(item) * content.trade.sell_share + 0.0001f);
}

/* ---- villages ------------------------------------------------------------ */

static uint32_t mix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

/* A number for a name: the same whatever order things were loaded in. */
static uint32_t name_hash(const char *name)
{
    uint32_t h = 2166136261u;
    for (; *name; name++) h = (h ^ (uint32_t)(unsigned char)*name) * 16777619u;
    return h;
}

uint32_t game_village_at(const Game *g, int x, int y)
{
    /* Where the hearts are: looked for through the whole world, and again every so often, for they can be moved. */
    enum { MOST = 96, EVERY = 200 };
    static struct {
        const Game *game;
        uint32_t seed;
        int width, height, count, reach[MOST];
        uint64_t tick;
        TilePos at[MOST];
    } known;
    const World *w = &g->world;
    bool stale = known.game != g || known.seed != w->seed || known.width != w->width || known.height != w->height ||
                 g->tick < known.tick || g->tick - known.tick >= EVERY;
    bool any = false;
    for (int t = 1; t < content.tile_count && !any; t++) any = content.tiles[t].village > 0;
    if (!any) return 0;
    if (stale) {
        known.game = g, known.seed = w->seed, known.width = w->width, known.height = w->height, known.tick = g->tick, known.count = 0;
        size_t cells = (size_t)w->width * w->height;
        for (int layer = 0; layer < BLOCK_LAYERS; layer++)
            for (size_t i = 0; i < cells && known.count < MOST; i++) {
                int reach = tile_def((TileType)w->layers[layer][i])->village;
                if (reach <= 0) continue;
                /* (The top of a tall one only: one heart, one village.) */
                if (i >= (size_t)w->width && w->layers[layer][i - w->width] == w->layers[layer][i]) continue;
                known.reach[known.count] = reach;
                known.at[known.count++] = (TilePos){ (int)(i % (size_t)w->width), (int)(i / (size_t)w->width) };
            }
    }
    uint32_t best = 0;
    int nearest = 0;
    for (int i = 0; i < known.count; i++) {
        int dx = abs(world_dx(w, known.at[i].x, x)), dy = abs(known.at[i].y - y), far = dx > dy ? dx : dy;
        if (dx > known.reach[i] || dy > known.reach[i] / 2 + 6 || (best != 0 && far >= nearest)) continue;
        nearest = far;
        best = mix32(w->seed ^ (uint32_t)known.at[i].x * 73856093u ^ (uint32_t)known.at[i].y * 19349663u) | 1u;
    }
    return best;
}

static uint32_t open_village(const Game *g)
{
    return game_trade_open(g) ? game_village_at(g, g->bench_pos.x, g->bench_pos.y) : 0;
}

bool game_trade_works(const Game *g)
{
    if (!game_trade_open(g)) return false;
    return !content.trade.villages_only || game_trade_free(g) || open_village(g) != 0;
}

/* Whether a village is one of the few that deal in a speciality: the ones for which it comes out highest. */
static bool village_deals(uint32_t village, ItemId item)
{
    if (village == 0 || !item_def(item)->speciality) return false;
    uint32_t mine = mix32(village ^ name_hash(item_def(item)->name));
    int above = 0;
    for (ItemId i = 1; i < content.item_count; i++)
        if (i != item && content.items[i].speciality && mix32(village ^ name_hash(content.items[i].name)) > mine) above++;
    return above < content.trade.village_specialities;
}

bool game_trade_stocks(const Game *g, ItemId item)
{
    if (!game_trade_deals(g, item)) return false;
    return !item_def(item)->speciality || game_trade_free(g) || village_deals(open_village(g), item);
}

int game_trade_price(const Game *g, ItemId item)
{
    const TradeDef *t = &content.trade;
    int price = trade_price(item);
    uint32_t village = open_village(g);
    if (village == 0 || price <= 0 || game_trade_free(g)) return price;
    /* A speciality is worth a good deal more anywhere that does not deal in it. */
    if (item_def(item)->speciality) return village_deals(village, item) ? price : price + price / 2;
    if (t->village_swing <= 0.0f) return price;
    /* Which things are dearer or cheaper here, and by how much: settled by the village and the thing, and
     * - where prices move - by which stretch of days it is. */
    const RulesDef *u = &content.rules;
    int day = u->day_ticks + u->night_ticks > 0 ? (int)(g->tick / (uint64_t)(u->day_ticks + u->night_ticks)) : 0;
    uint32_t h = mix32(village ^ name_hash(item_def(item)->name) ^ (t->village_days > 0 ? (uint32_t)(day / t->village_days) * 2654435761u : 0u));
    if ((float)(h % 1000u) / 1000.0f >= t->village_share) return price;
    float swing = ((float)((h >> 10) % 2001u) / 1000.0f - 1.0f) * t->village_swing;
    int changed = (int)((float)price * (1.0f + swing) + 0.5f);
    return changed < 1 ? 1 : changed;
}

int game_trade_sell_price(const Game *g, ItemId item)
{
    return (int)((float)game_trade_price(g, item) * content.trade.sell_share + 0.0001f);
}

bool game_trade_free(const Game *g)
{
    return game_trade_open(g) && tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y))->trade_free;
}

int game_trade_level(const Game *g)
{
    return game_trade_open(g) ? tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y))->trade_level : 0;
}

bool trade_barred(ItemId item)
{
    /* Worked out once for the content as it stands, and again if it changes. */
    static uint8_t barred[MAX_ITEMS];
    static int for_items = -1, for_recipes = -1, for_tiles = -1;
    if (for_items != content.item_count || for_recipes != content.recipe_count || for_tiles != content.tile_count) {
        for_items = content.item_count;
        for_recipes = content.recipe_count;
        for_tiles = content.tile_count;
        for (ItemId i = 0; i < content.item_count; i++) {
            const ItemDef *def = &content.items[i];
            const TileDef *puts = tile_def(def->places);
            barred[i] = (def->places != TILE_AIR && puts->trade) || (def->category >= 0 && content.trade.categories[def->category].free_only);
        }
        for (int r = 0; r < content.recipe_count; r++)
            if (content.recipes[r].crystals > 0 && content.recipes[r].output.id < MAX_ITEMS) barred[content.recipes[r].output.id] = 1;
    }
    return item <= ITEM_NONE || item >= content.item_count || barred[item];
}

bool game_trade_deals(const Game *g, ItemId item)
{
    if (!game_trade_open(g) || item <= ITEM_NONE || item >= content.item_count) return false;
    const ItemDef *def = &content.items[item];
    if (game_trade_free(g)) return def->crystals == 0 && !def->becomes_player;
    if (!game_trade_works(g)) return false;
    /* A speciality: sold where the village deals in it, and bought from anybody who has one, in any village. */
    if (def->speciality) {
        uint32_t village = open_village(g);
        return village != 0 && trade_price(item) > 0 && (village_deals(village, item) || inventory_count(&g->inventory, item) > 0);
    }
    if (trade_price(item) <= 0 || trade_barred(item)) return false;
    int level = game_trade_level(g);
    if (level > 0 && def->trade_tier > level) return false;
    return !content.trade.discover || g->discovered[item];
}

int trade_coins(long amount, int *out)
{
    const TradeDef *t = &content.trade;
    for (int i = t->currency_count - 1; i >= 0; i--) {
        int value = t->currencies[i].value > 0 ? t->currencies[i].value : 1;
        out[i] = (int)(amount / value);
        amount -= (long)out[i] * value;
    }
    return t->currency_count;
}

void trade_price_text(long amount, char *out, size_t size)
{
    int parts[MAX_CURRENCIES], n = trade_coins(amount, parts);
    size_t used = 0;
    if (size > 0) out[0] = '\0';
    for (int i = n - 1; i >= 0 && used < size; i--) {
        if (parts[i] == 0) continue;
        char label[CONTENT_NAME_LEN];
        snprintf(label, sizeof label, "%s", item_def(content.trade.currencies[i].item)->label);
        if (label[0] >= 'A' && label[0] <= 'Z') label[0] = (char)(label[0] - 'A' + 'a');
        used += (size_t)snprintf(out + used, size - used, "%s%d %s%s", used > 0 ? ", " : "", parts[i], label, parts[i] == 1 ? "" : "s");
    }
    if (used == 0 && size > 0) snprintf(out, size, "nothing");
}

long trade_wallet(const Inventory *inv)
{
    long total = 0;
    for (int i = 0; i < content.trade.currency_count; i++)
        total += (long)inventory_count(inv, content.trade.currencies[i].item) *
                 content.trade.currencies[i].value;
    return total;
}

int game_trade_listing(const Game *g, int category, ItemId *out, bool *found, int max)
{
    int n = 0, last = content.trade.category_count - 1;
    /* (The last of those kept for where everything is given away, if there is one.) */
    for (int c = content.trade.category_count - 1; c >= 0; c--)
        if (content.trade.categories[c].free_only) { last = c; break; }
    bool free = game_trade_free(g);
    int top = 1;
    for (int i = 1; i < content.item_count; i++)
        if (content.items[i].trade_tier > top) top = content.items[i].trade_tier;
    for (int tier = 1; tier <= top; tier++) {
        for (int i = 1; i < content.item_count && n < max; i++) {
            int in = content.items[i].category;
            if (in < 0 && free) in = last;
            if (in != category || content.items[i].trade_tier != tier) continue;
            bool deals = game_trade_deals(g, (ItemId)i);
            /* (What it will never deal in has no place kept for it.) */
            if (!deals && (trade_price((ItemId)i) <= 0 || trade_barred((ItemId)i) || content.items[i].speciality)) continue;
            if (found) found[n] = deals;
            out[n++] = (ItemId)i;
        }
    }
    return n;
}

/* Works out which coins change hands to pay `price`: `delta` gets, for each
 * currency, how many are gained (change) or lost. Small coins go first; if
 * they run out, the smallest coin that covers the rest is broken. */
static bool plan_payment(const Inventory *inv, long price, int *delta)
{
    const TradeDef *t = &content.trade;
    int held[MAX_CURRENCIES];
    for (int i = 0; i < t->currency_count; i++) {
        held[i] = inventory_count(inv, t->currencies[i].item);
        delta[i] = 0;
    }

    long owed = price;
    for (int i = 0; i < t->currency_count && owed > 0; i++) {
        long use = owed / t->currencies[i].value;
        if (use > held[i]) use = held[i];
        delta[i] -= (int)use;
        owed -= use * t->currencies[i].value;
    }
    if (owed == 0) return true;

    /* Every coin still held is now worth more than what is owed. */
    for (int i = 0; i < t->currency_count; i++) {
        if (held[i] + delta[i] <= 0) continue;
        long change = t->currencies[i].value - owed;
        delta[i]--;
        for (int c = i - 1; c >= 0; c--) {
            delta[c] += (int)(change / t->currencies[c].value);
            change %= t->currencies[c].value;
        }
        return change == 0;
    }
    return false;
}

bool trade_pay(Game *g, long price)
{
    int delta[MAX_CURRENCIES];
    if (price <= 0) return true;
    if (!plan_payment(&g->inventory, price, delta)) return false;
    for (int i = 0; i < content.trade.currency_count; i++)
        if (delta[i] < 0) inventory_remove(&g->inventory, content.trade.currencies[i].item, -delta[i]);
    for (int i = 0; i < content.trade.currency_count; i++)
        if (delta[i] > 0) game_give_items(g, content.trade.currencies[i].item, delta[i]);
    return true;
}

void trade_give(Game *g, long amount)
{
    int parts[MAX_CURRENCIES], kinds = trade_coins(amount, parts);
    for (int i = kinds - 1; i >= 0; i--)
        if (parts[i] > 0) game_give_items(g, content.trade.currencies[i].item, parts[i]);
}

bool trade_buy_now(Game *g, ItemId item, int count)
{
    int price = game_trade_price(g, item), delta[MAX_CURRENCIES];
    if (count < 1 || !game_trade_stocks(g, item)) return false;
    if (game_trade_free(g)) {
        game_give_items(g, item, count);
        return true;
    }
    if (!plan_payment(&g->inventory, (long)price * count, delta)) return false;

    /* Coins out before anything comes in, so the change has room to land. */
    for (int i = 0; i < content.trade.currency_count; i++)
        if (delta[i] < 0) inventory_remove(&g->inventory, content.trade.currencies[i].item, -delta[i]);
    for (int i = 0; i < content.trade.currency_count; i++)
        if (delta[i] > 0) game_give_items(g, content.trade.currencies[i].item, delta[i]);
    game_give_items(g, item, count);
    return true;
}

bool trade_sell_now(Game *g, ItemId item, int count)
{
    const TradeDef *t = &content.trade;
    int price = game_trade_sell_price(g, item);
    if (count < 1 || !game_trade_deals(g, item) || t->currency_count == 0) return false;
    if (inventory_count(&g->inventory, item) < count) return false;
    /* (Where everything is free, a thing handed back is worth nothing.) */
    if (!game_trade_free(g) && price <= 0) return false;
    inventory_remove(&g->inventory, item, count);
    if (!game_trade_free(g)) {
        /* Paid in the biggest coins that make it up: a great sum in the smallest would not fit in anybody's pockets. */
        int parts[MAX_CURRENCIES], kinds = trade_coins((long)price * count, parts);
        for (int i = kinds - 1; i >= 0; i--)
            if (parts[i] > 0) game_give_items(g, t->currencies[i].item, parts[i]);
    }
    return true;
}

bool trade_convert_now(Game *g, int from, int to)
{
    const TradeDef *t = &content.trade;
    if (!game_trade_works(g) || from < 0 || to < 0 || from >= t->currency_count ||
        to >= t->currency_count || (from - to != 1 && to - from != 1))
        return false;
    int lower = from < to ? from : to, higher = from < to ? to : from;
    int rate = t->currencies[higher].value / t->currencies[lower].value;
    int give = from == lower ? rate : 1, get = from == lower ? 1 : rate;
    if (rate < 1 || inventory_count(&g->inventory, t->currencies[from].item) < give) return false;
    inventory_remove(&g->inventory, t->currencies[from].item, give);
    game_give_items(g, t->currencies[to].item, get);
    return true;
}

bool game_trade_buy(Game *g, ItemId item, int count)
{
    return game_trade_open(g) && game_bench_work(g, WORK_BUY, item, count);
}

bool game_trade_sell(Game *g, ItemId item, int count)
{
    return game_trade_open(g) && game_bench_work(g, WORK_SELL, item, count);
}

bool game_trade_convert(Game *g, int from, int to)
{
    return game_trade_open(g) && game_bench_work(g, WORK_CONVERT, from, to);
}
