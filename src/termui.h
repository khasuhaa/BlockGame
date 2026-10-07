#ifndef TERMUI_H
#define TERMUI_H

#include "game.h"

#define TERMUI_SEARCH_MAX 23

/* The screen of a storage network's terminal: what is stored, to be taken out
 * and put in; what can be made, to be asked for; and the jobs that are running. */
typedef struct {
    bool open;
    TilePos at;       /* the terminal, or the access point it is reached through */
    bool remote;      /* reached from a distance, with something in hand */
    int tab;          /* 0 what is stored, 1 what can be made, 2 the jobs */
    char search[TERMUI_SEARCH_MAX + 1];
    float scroll;
    ItemId chosen;    /* what is being asked for on the second tab */
    int amount;
    char message[64]; /* why the last thing asked for could not be made */
} TermUi;

void termui_open(TermUi *ui, TilePos at, bool remote);
/* Runs and draws one frame of it, over the world. It closes itself. */
void termui_frame(TermUi *ui, Game *g);

#endif
