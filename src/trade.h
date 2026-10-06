#ifndef TRADE_H
#define TRADE_H

#include "game.h"

/* Buying and selling at a trade block. Everything here needs one to be open,
 * and returns false, changing nothing, if the deal cannot be done. */

/* True if the block the player has open is one to trade at. */
bool game_trade_open(const Game *g);

/* What one of an item costs, in the smallest currency; 0 if it is not traded. */
int trade_price(ItemId item);
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

#endif
