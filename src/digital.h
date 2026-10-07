#ifndef DIGITAL_H
#define DIGITAL_H

#include "game.h"

/* Storage networks. Blocks that say they are part of one ("network" in their
 * definitions), joined by cable, are a network if there is a controller among
 * them and it has electricity. What the network stores is what is in the
 * storage cells in its drives, and in whatever its storage buses stand
 * beside; it is got at from a terminal, or from a distance with something
 * "remote" in hand near one of its access points. Import and export buses
 * move things in and out by themselves; and what the benches beside its
 * assemblers make, the network can be asked to make, on its crafting CPUs. */

#define MAX_NET_JOBS 16
#define MAX_JOB_STEPS 16
#define MAX_NET_KINDS 512 /* kinds of thing a network is listed as holding */

/* What one storage cell has in it. */
typedef struct CellStore {
    bool used;
    int types;
    struct {
        uint16_t id;
        uint32_t count;
    } held[CELL_TYPES];
} CellStore;

/* A crafting job: recipes to make in turn, so many times each. */
typedef struct NetJob {
    bool used;
    int x, y;        /* the network's controller */
    ItemId wanted;   /* what was asked for, and how many */
    int amount;
    struct {
        int16_t recipe; /* which of content.recipes */
        int32_t crafts, done;
    } steps[MAX_JOB_STEPS];
    int step_count, step;
    int timer;       /* ticks the one being made has still to take; 0 with none in hand */
    bool waiting;    /* it has not what the next one takes, or nowhere to put what it made */
} NetJob;

typedef struct {
    ItemId item;
    long count;
    bool craftable; /* the network can be asked to make it */
} NetItem;

typedef struct {
    bool found;      /* there is a network there, with a controller */
    bool online;     /* and it has electricity */
    TilePos controller;
    long bytes_used, bytes_total;
    int types, type_room; /* kinds of thing in its cells, and how many more there is room for */
    int devices, cpus, cpus_busy, coprocessors, drives, cells;
} NetInfo;

void digital_init(Game *g);
void digital_free(Game *g);
void digital_tick(Game *g);

/* The network the block at a square is part of. */
bool digital_info(Game *g, int x, int y, NetInfo *out);
/* What it holds, most of first: fills `out`, returns how many kinds. Things it could make and has none of are listed too, with a count of 0. */
int digital_list(Game *g, int x, int y, NetItem *out, int max);
long digital_count(Game *g, int x, int y, ItemId item);
/* Puts things into it, or takes them out: each returns how many were moved. */
int digital_insert(Game *g, int x, int y, ItemId item, int count);
int digital_extract(Game *g, int x, int y, ItemId item, int count);
/* The player's side of that: a stack from their bar into the network (so many
 * of it; all for 0), and so many of a thing out of it into what they carry. */
int digital_deposit(Game *g, int x, int y, int bar_slot, int count);
int digital_withdraw(Game *g, int x, int y, ItemId item, int count);
/* A recipe by which the network could make a thing; NULL if it has none. */
const Recipe *digital_recipe_for(Game *g, int x, int y, ItemId item);
/* Asks for so many of a thing to be made. False - with a word of why - if it cannot be. */
bool digital_request(Game *g, int x, int y, ItemId item, int amount, char *why, size_t size);
/* Its jobs: fills `out` with pointers to them, returns how many. Cancelling one gives up what it had in hand. */
int digital_jobs(Game *g, int x, int y, const NetJob **out, int max);
bool digital_cancel(Game *g, const NetJob *job);
/* Whether the character has something remote in hand and an access point of a network that is running in range: the square of that point. */
bool digital_remote(Game *g, TilePos *at);

#endif
