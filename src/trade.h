#ifndef TRADE_H
#define TRADE_H

#include "game.h"

/* Buying and selling at a trade block. Everything here needs one to be open,
 * and returns false, changing nothing, if the deal cannot be done. */

/* True if the block the player has open is one to trade at. */
bool game_trade_open(const Game *g);

/* What one of an item costs, in the smallest currency; 0 if it is not traded. */
int trade_price(ItemId item);
/* What selling one gives: a share of that. */
int trade_sell_price(ItemId item);
/* Villages. The village a square is in - a number of its own, never 0 - or 0 for none: it is in one if it is
 * within reach of a block that is a village's heart. */
uint32_t game_village_at(const Game *g, int x, int y);
/* Whether the block that is open does any trade where it stands: where trade is only done in villages, one
 * anywhere else does none (but one that gives everything away works anywhere). */
bool game_trade_works(const Game *g);
/* What one of an item costs at the block that is open, and what selling one there gives: in a village, the
 * village's own prices, which are not quite any other's and change from time to time. */
int game_trade_price(const Game *g, ItemId item);
int game_trade_sell_price(const Game *g, ItemId item);
/* Whether the block that is open has an item to sell, as well as buying it: a speciality is sold only in the
 * villages that deal in it, though any village will buy one - and pays well for it. */
bool game_trade_stocks(const Game *g, ItemId item);
/* Whether the block that is open gives everything away. */
bool game_trade_free(const Game *g);
/* How far the block that is open has been raised: it deals in nothing of a
 * higher tier. 0 if it deals in every tier. */
int game_trade_level(const Game *g);
/* Things no trade portal deals in: things to trade at, whatever takes crystals
 * to make, and what is in a category kept for where everything is given away. (One that gives everything away has them all the same.) */
bool trade_barred(ItemId item);
/* Whether an item can be bought and sold at the block that is open. At an
 * ordinary one that is anything with a price that is not barred, of a tier
 * the block has been raised to, that somebody in this world has had in
 * their hands; at one that gives
 * everything away, anything that can be held at all. */
bool game_trade_deals(const Game *g, ItemId item);
/* The items of a category that the open block deals in or will, in the order
 * they are defined within each tier, lowest tier first: each keeps its place
 * whether or not it can be had yet, and `found`, if wanted, says which have (only those can be
 * bought and sold). Where everything is given away, things with no category
 * of their own - the coins - are in the last one kept for such places. Returns how many, at most `max`. */
int game_trade_listing(const Game *g, int category, ItemId *out, bool *found, int max);
/* A sum as the coins that make it up, using the biggest there are: `out` (room for MAX_CURRENCIES) gets how many of
 * each currency, smallest first. Returns how many currencies there are. */
int trade_coins(long amount, int *out);
/* The same in words: "1 platinum coin, 55 gold coins". */
void trade_price_text(long amount, char *out, size_t size);
/* Everything the player's coins are worth, in the smallest currency. */
long trade_wallet(const Inventory *inv);
/* A sum paid in coin, with change (false, and nothing done, if it cannot be made up); and a sum handed over, in the biggest coins. */
bool trade_pay(Game *g, long price);
void trade_give(Game *g, long amount);

/* Buys `count` of an item. Coins are spent smallest first, and a bigger coin
 * is broken into change when the smaller ones do not cover it. */
bool game_trade_buy(Game *g, ItemId item, int count);
/* Sells `count` of an item, for the biggest coins that make up what it fetches. */
bool game_trade_sell(Game *g, ItemId item, int count);
/* Exchanges between two currencies next to each other in value: as many of
 * the lower as make one of the higher, or one of the higher for that many. */
bool game_trade_convert(Game *g, int from, int to);

/* The same three done on the spot, with the player at the trade block;
 * the ones above walk the player there first if need be. */
bool trade_buy_now(Game *g, ItemId item, int count);
bool trade_sell_now(Game *g, ItemId item, int count);
bool trade_convert_now(Game *g, int from, int to);

#endif
