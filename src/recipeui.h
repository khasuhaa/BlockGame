#ifndef RECIPEUI_H
#define RECIPEUI_H

#include "content.h"

#define RECIPEUI_SEARCH_MAX 31
#define RECIPEUI_HISTORY 64

/* The recipe book: every item in a grid that can be searched; pick one to
 * see what makes it and where its raw materials come from, and pick anything
 * shown there to look that up in turn. */
typedef struct {
    char search[RECIPEUI_SEARCH_MAX + 1];
    ItemId shown;                     /* the item being looked at, or ITEM_NONE */
    ItemId history[RECIPEUI_HISTORY]; /* the ones looked at on the way to it */
    int depth;
    float grid_scroll, detail_scroll; /* pixels */
    float grid_height, detail_height; /* how tall each was when last drawn */
} RecipeUi;

/* Call when the book is opened: keys typed before then are not part of a search. */
void recipeui_open(RecipeUi *ui);
/* Runs and draws one frame of it, filling the screen. `escape` is the key
 * that backs out. Returns false once it is to be closed. */
bool recipeui_frame(RecipeUi *ui, bool escape);

#endif
