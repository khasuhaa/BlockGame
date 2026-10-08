#ifndef DAO_H
#define DAO_H

#include "game.h"

/* Cultivation (see DaoDef in content.h, and Dao in game.h): the road a character goes up by gathering qi.
 *
 * A character who has studied a manual gathers qi while they sit in meditation: faster where the qi of the
 * place is thicker, by a better manual, with roots in its element, with more meridians open, on a better
 * foundation - and slower for pill toxin. With a stage's measure of it, and whatever else that stage asks,
 * they may try to break through to the next. Coming into a new realm brings a tribulation: so many bolts of
 * lightning, one after another, which they live through or do not. */

/* Whether the game has any of this: there are realms to go through. */
bool dao_on(void);
/* How many stages there are, all the realms together. Past the last is immortality. */
int dao_stages(void);
/* Which realm a stage is in (-1 for none: a mortal; the number of realms for an immortal), and which stage of it, from 1. */
int dao_realm_of(int stage, int *within);
/* What a character's stage is called: "Qi Refining III". */
void game_dao_label(const Game *g, char *out, size_t size);
/* How much qi the stage they are at holds. */
float game_dao_capacity(const Game *g);
/* The qi of a square: the world's own (world_qi), and what any formation lit round it adds. */
float game_qi_at(const Game *g, int x, int y);
/* The qi they would gather each tick, sitting where they are; 0 for somebody with no manual. */
float game_dao_rate(const Game *g);
/* The chance their next breakthrough has. */
float game_dao_chance(const Game *g);
/* What the step they are on asks for besides qi: fills `out` (room for 4) and returns how many. */
int game_dao_needs(const Game *g, RecipeItem *out);
/* Whether they may try it now: qi enough, everything asked for in hand, and no tribulation overhead. */
bool game_dao_ready(const Game *g);
/* Tries it. False if they may not; true if it was tried, whichever way it went. */
bool game_breakthrough(Game *g);
/* Whether something to do with cultivation can be taken just now, and taking it. (For game_eat.) */
bool dao_item(const ItemDef *def);
bool game_dao_can_take(const Game *g, ItemId item);
void dao_take(Game *g, ItemId item);
/* What cultivation gives the character in hand: added to `all` like any other boons. */
void dao_boons(const Game *g, Boons *all);
/* A tick of it for the character in hand; and, once a tick, of the formations of the world. */
void dao_tick(Game *g);
void dao_world_tick(Game *g);
/* Whether a technique can be used by them now, and what using it costs. */
bool game_technique_ready(const Game *g, ItemId item);
void dao_spend(Game *g, ItemId item);
/* Death, for somebody who has got far enough to survive one: true if they have, this once. */
bool dao_revive(Game *g);
/* Uses a formation that is a gate: takes them to the nearest other that is lit. False if there is none. */
bool game_formation_gate(Game *g, int x, int y);
/* Whether the formation whose heart is in a square is lit, and how far it reaches. */
int game_formation_reach(const Game *g, int x, int y);

/* Which of the techniques they have learned an item is (0 to DAO_KNOWN - 1), or -1; and the one in hand to be cast, or ITEM_NONE. */
int dao_known_slot(const Game *g, ItemId item);
ItemId game_technique_armed(const Game *g);
/* Uses the technique in a slot of the bar: one for themselves is done at once; one for a place is taken in hand (or put down again). */
bool game_technique_use(Game *g, int slot);
/* Adds to what they understand of an element or a craft (0 to DAO_COMP - 1). */
void dao_learn(Game *g, int what, int by);
/* Whether a square is inside a lit formation that keeps wild creatures out (0), or sets them wandering (1). */
bool game_formation_rule(const Game *g, int x, int y, int rule);
#endif
