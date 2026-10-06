#ifndef TREE_H
#define TREE_H

#include "world.h"

typedef enum {
    TREE_NONE = 0,
    TREE_OAK,
    TREE_CHERRY
} TreeKind;

#define OAK_MAX_HEIGHT 7    /* trunk blocks: a bare stick with a ball of leaves on top */
#define CHERRY_MAX_HEIGHT 8 /* trunk blocks: six rows of leaves over two of bare trunk */
#define TREE_MAX_LEAVES 24

/* A tree is nothing but its blocks: a column of log tiles and the leaf tiles
 * around it. These say which kind of tree a tile belongs to, if any. */
TreeKind tree_kind_of_log(TileType t);
TreeKind tree_kind_of_leaves(TileType t);

/* Starts a tree as a single trunk block. */
void tree_plant(World *w, int x, int y, TreeKind kind);

typedef struct {
    bool grew;     /* something changed */
    bool extended; /* the trunk got taller and the leaves moved up with it */
    int x;         /* trunk column */
    int top;       /* row of the trunk's top block before growing */
} TreeGrowth;

/* One step of growth for the tree whose trunk passes through (x, y): either a
 * new leaf, added outward from the trunk, or one more block of trunk. When
 * the trunk extends, every leaf moves up a row with it; a leaf with a block
 * in its way is lost. Does nothing once the tree is fully grown. */
TreeGrowth tree_grow(World *w, int x, int y, uint32_t *rng);

#endif
