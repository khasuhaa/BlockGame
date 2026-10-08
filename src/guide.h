#ifndef GUIDE_H
#define GUIDE_H

#include "content.h"

#include <stddef.h>

/* The guide: what the loaded content says about where a thing comes from.
 * Nothing here knows any item by name; it only reads the tables. */

typedef enum {
    SOURCE_BLOCK,      /* left by a block when it is mined */
    SOURCE_CREATURE,   /* left by a creature when it is killed */
    SOURCE_SHEAR,      /* taken off a creature with the tool for it, leaving it alive */
    SOURCE_MILK,       /* milked from a creature */
    SOURCE_FRUIT,      /* borne by a tree */
    SOURCE_CHEST,      /* found inside a block the world was generated with */
    SOURCE_MEDITATION, /* brought by meditating */
    SOURCE_START,      /* a new character has it */
    SOURCE_VILLAGE     /* sold in some villages, and nowhere else */
} SourceKind;

typedef struct {
    SourceKind kind;
    int from;         /* the block, creature or tree; unused for the last two kinds */
    int min, max;     /* how many at a time */
    float chance;     /* chance of getting any; 1 for always */
    int tool_class;   /* the kind of tool that makes shorter work of the block, or -1 */
    char label[64];   /* what the block, creature or tree is called */
    char where[200];  /* a sentence on where that is to be found; empty if nothing in the world says */
    ItemId placed_by; /* for something the world does not have of itself: the item that puts it there;
                         for shearing, the tool */
} ItemSource;

/* The ways an item can be got other than by making or buying it. Returns how many, at most `max`. */
int guide_sources(ItemId item, ItemSource *out, int max);
/* The recipes that make an item, and the ones that use it, as indices into content.recipes. */
int guide_recipes(ItemId item, int *out, int max);
int guide_uses(ItemId item, int *out, int max);
/* The first block of the lowest tier of a kind of bench, or TILE_AIR; and the item that puts it down. */
TileType guide_bench_tile(const char *kind, int level);
ItemId guide_placer(TileType t);
/* Whether a new world has this block in it somewhere; `where` gets a sentence saying where. */
bool guide_tile_natural(TileType t, char *where, size_t size);
/* Whether `text` (any case) is in an item's label. An empty text is in every one. */
bool guide_matches(ItemId item, const char *text);

/* Every item, in the order the book lists them: by the category it is traded
 * under, in the order the categories come, then by what it is called (items
 * of the same name - the tiers of a bench - in the order they were made in);
 * and last the ones with no category. Fills `out` (which needs room for every
 * item) and returns how many. */
int guide_order(ItemId *out);

#endif
