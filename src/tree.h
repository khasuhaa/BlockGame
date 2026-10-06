#ifndef TREE_H
#define TREE_H

#include "world.h"

#define TREE_MAX_LEAVES 96

/* A tree is nothing but its blocks: a column of log tiles and the leaf tiles
 * around it, in the layer its definition names. Its kind is an index into
 * content.trees. */

/* Starts a tree as a single trunk block. */
void tree_plant(World *w, int x, int y, int kind);

/* The kind of tree whose trunk or leaves are at a position, in the layer that
 * kind grows in; -1 if there is none. */
int tree_trunk_at(const World *w, int x, int y);
int tree_leaves_at(const World *w, int x, int y);

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
