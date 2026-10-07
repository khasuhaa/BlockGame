#include "trade.h"

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

bool game_trade_free(const Game *g)
{
    return game_trade_open(g) && tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y))->trade_free;
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
            barred[i] = def->places != TILE_AIR && (puts->bench || puts->trade);
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
    if (trade_price(item) <= 0 || trade_barred(item)) return false;
    return !content.trade.discover || g->discovered[item];
}

long trade_wallet(const Inventory *inv)
{
    long total = 0;
    for (int i = 0; i < content.trade.currency_count; i++)
        total += (long)inventory_count(inv, content.trade.currencies[i].item) *
                 content.trade.currencies[i].value;
    return total;
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

bool trade_buy_now(Game *g, ItemId item, int count)
{
    int price = trade_price(item), delta[MAX_CURRENCIES];
    if (count < 1 || !game_trade_deals(g, item)) return false;
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
    int price = trade_sell_price(item);
    if (count < 1 || !game_trade_deals(g, item) || t->currency_count == 0) return false;
    if (inventory_count(&g->inventory, item) < count) return false;
    /* (Where everything is free, a thing handed back is worth nothing.) */
    if (!game_trade_free(g) && price <= 0) return false;
    inventory_remove(&g->inventory, item, count);
    if (!game_trade_free(g)) game_give_items(g, t->currencies[0].item, price * count / t->currencies[0].value);
    return true;
}

bool trade_convert_now(Game *g, int from, int to)
{
    const TradeDef *t = &content.trade;
    if (!game_trade_open(g) || from < 0 || to < 0 || from >= t->currency_count ||
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
