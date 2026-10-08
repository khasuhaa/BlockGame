#ifndef TRAIN_H
#define TRAIN_H

#include "game.h"

/* Things that run on rails. A vehicle is not a block: it stands on track - one
 * square of it, or three - and is two squares tall, and goes along the track
 * a square at a time. Track runs from one piece to the next beside it, or
 * beside it and one up or down. Vehicles that touch end to end are coupled:
 * whatever moves one of them moves them all. What a vehicle is - an engine
 * that burns fuel, a car pumped along by hand, somewhere to sit, somewhere to
 * keep things - is its item's "vehicle" in the data. */

/* Whether there is track in a square. */
bool train_rail(const World *w, int x, int y);
/* The track a piece runs on to, to the left (-1) or the right (1): the row
 * it is in, or false if the track ends. */
bool train_next(const World *w, int x, int y, int dir, int *row);

/* The vehicle standing on a square of track, or -1. */
int train_at(const Game *g, int x, int y);
/* The vehicle drawn over a square: on its track, or the square above. */
int train_under(const Game *g, int x, int y);
/* Which seat of it that square is at. */
int train_seat_at(const Game *g, int v, int x);

/* Puts the vehicle a character is holding down on the track at a square (or
 * the one under it), if the character is near enough and there is room; it
 * is slid up to a vehicle it is put down beside, so that they couple.
 * Returns which vehicle it now is, or -1. */
int train_place(Game *g, ItemId item, int x, int y);
/* Takes one up: it, and whatever was in it, is left where it stood. */
void train_remove(Game *g, int v);

/* The vehicles coupled to one, itself among them, from the left end to the right. */
int train_group(const Game *g, int v, int *out);

/* What can be done with a vehicle from where the character is. */
typedef enum {
    TRAIN_GO,     /* arg: -1 left, 1 right */
    TRAIN_STOP,
    TRAIN_SIT,    /* arg: which seat */
    TRAIN_OUT,
    TRAIN_FUEL,   /* open what it burns */
    TRAIN_CARGO,  /* open what it carries */
    TRAIN_REMOVE
} TrainDo;

typedef struct {
    TrainDo what;
    int arg;
    char label[64];
} TrainOption;

#define TRAIN_MAX_OPTIONS 8

/* The choices a click on a vehicle gives, in order - the last is always to
 * take it up. `x` is the square clicked, which picks the seat. None if the
 * character is too far off to do anything with it. */
int train_options(const Game *g, int v, int x, TrainOption *out);
bool train_do(Game *g, int v, TrainOption option);

/* The station a train going one way from a vehicle would come to first, and
 * what is written on it ("" for nothing). */
bool train_station(const Game *g, int v, int dir, int *x, int *y, const char **name);

/* The vehicle whose slots are open, and which of them: its fuel or its cargo. 0 for none. */
uint16_t train_open_container(const Game *g, int *slots);
void train_close(Game *g);

/* Moves what is moving. */
void train_update(Game *g, float dt);
/* Burns what is burning: once a tick. */
void train_tick(Game *g);

/* Where a vehicle is drawn: the middle of its track, in squares, part way to the next as it goes. */
void train_visual(const Game *g, int v, float *x, float *y);
/* Where the character being played is drawn, if they are riding. */
bool train_rider_visual(const Game *g, float *x, float *y);
/* Puts a character out of whatever they are riding (the live one: -1). */
void train_unseat(Game *g, int character);

#endif
