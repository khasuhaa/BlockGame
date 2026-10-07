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
/* Whether the block that is open gives everything away. */
bool game_trade_free(const Game *g);
/* Things no trade portal deals in: benches and other things to trade at, and
 * whatever takes crystals to make. (One that gives everything away has them all the same.) */
bool trade_barred(ItemId item);
/* Whether an item can be bought and sold at the block that is open. At an
 * ordinary one that is anything with a price that is not barred and that
 * somebody in this world has had in their hands; at one that gives
 * everything away, anything that can be held at all. */
bool game_trade_deals(const Game *g, ItemId item);
/* Everything the player's coins are worth, in the smallest currency. */
long trade_wallet(const Inventory *inv);

/* Buys `count` of an item. Coins are spent smallest first, and a bigger coin
 * is broken into change when the smaller ones do not cover it. */
bool game_trade_buy(Game *g, ItemId item, int count);
/* Sells `count` of an item for the smallest currency. Coins are never
 * exchanged upward on a sale; that is what converting is for. */
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
