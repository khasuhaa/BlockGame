#include "tree.h"

typedef struct {
    int dx;  /* columns from the trunk */
    int off; /* rows below the trunk's top block; -1 is the row above it */
} LeafCell;

static const TileType log_tiles[] = { TILE_AIR, TILE_OAK_LOG, TILE_CHERRY_LOG };
static const TileType leaf_tiles[] = { TILE_AIR, TILE_OAK_LEAVES, TILE_CHERRY_LEAVES };

TreeKind tree_kind_of_log(TileType t)
{
    return t == TILE_OAK_LOG ? TREE_OAK : t == TILE_CHERRY_LOG ? TREE_CHERRY : TREE_NONE;
}

TreeKind tree_kind_of_leaves(TileType t)
{
    return t == TILE_OAK_LEAVES ? TREE_OAK : t == TILE_CHERRY_LEAVES ? TREE_CHERRY : TREE_NONE;
}

void tree_plant(World *w, int x, int y, TreeKind kind)
{
    world_set(w, x, y, log_tiles[kind]);
}

static uint32_t next_random(uint32_t *state)
{
    uint32_t x = *state ? *state : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

/* How far the leaves reach to one side on a given row, as the tree will be
 * when fully grown. Rows are counted from the top of the trunk, so the crown
 * rides up as the trunk grows.
 *
 * Oak: a ball round the top of the trunk - 3 wide above it, then 5, 5, 3.
 * Cherry: rows of 3, 3, 4, 4, 5, 5 down the trunk. The 4-wide rows lean to one
 * side, chosen by the trunk's column. */
static int leaf_reach(TreeKind kind, int trunk_x, int off, int side)
{
    if (kind == TREE_OAK) {
        if (off == -1 || off == 2) return 1;
        return off == 0 || off == 1 ? 2 : 0;
    }
    if (off < 0 || off > 5) return 0;
    if (off < 2) return 1;
    if (off < 4) return ((trunk_x & 1) ? 1 : -1) == side ? 2 : 1;
    return 2;
}

/* The places a tree of this height may have leaves, nearest the trunk first.
 * A row only gets leaves once the trunk is tall enough to reach down to it. */
static int leaf_cells(TreeKind kind, int trunk_x, int height, LeafCell *out)
{
    int n = 0;
    if (kind == TREE_OAK) out[n++] = (LeafCell){ 0, -1 };
    for (int d = 1; d <= 2; d++) {
        for (int off = -1; off <= 5 && off < height; off++) {
            for (int side = -1; side <= 1; side += 2)
                if (leaf_reach(kind, trunk_x, off, side) >= d) out[n++] = (LeafCell){ side * d, off };
        }
    }
    return n;
}

TreeGrowth tree_grow(World *w, int x, int y, uint32_t *rng)
{
    TreeGrowth result = { false, false, world_wrap_x(w, x), y };
    TileType log = world_get(w, x, y);
    TreeKind kind = tree_kind_of_log(log);
    if (kind == TREE_NONE) return result;
    TileType leaf = leaf_tiles[kind];

    int top = y, base = y;
    while (world_get(w, x, top - 1) == log) top--;
    while (world_get(w, x, base + 1) == log) base++;
    int height = base - top + 1;
    result.top = top;

    LeafCell cells[TREE_MAX_LEAVES];
    int count = leaf_cells(kind, result.x, height, cells);

    /* Leaves fill in ring by ring: pick among the nearest empty places. */
    int open[TREE_MAX_LEAVES], open_count = 0, ring = -1;
    for (int i = 0; i < count; i++) {
        int d = cells[i].dx < 0 ? -cells[i].dx : cells[i].dx;
        if (world_get(w, x + cells[i].dx, top + cells[i].off) != TILE_AIR) continue;
        if (!world_in_bounds(w, x, top + cells[i].off)) continue;
        if (ring < 0) ring = d;
        if (d == ring) open[open_count++] = i;
    }

    int max_height = kind == TREE_OAK ? OAK_MAX_HEIGHT : CHERRY_MAX_HEIGHT;
    TileType above = world_get(w, x, top - 1);
    bool can_extend = height < max_height && top - 1 >= 0 && (above == TILE_AIR || above == leaf);

    uint32_t roll = next_random(rng);
    if (open_count > 0 && (!can_extend || (roll & 1))) {
        LeafCell c = cells[open[(roll >> 8) % (uint32_t)open_count]];
        world_set(w, x + c.dx, top + c.off, leaf);
        result.grew = true;
    } else if (can_extend) {
        /* Lift the whole crown, then put it back one row up wherever it fits. */
        bool had[TREE_MAX_LEAVES];
        for (int i = 0; i < count; i++) {
            had[i] = world_get(w, x + cells[i].dx, top + cells[i].off) == leaf;
            if (had[i]) world_set(w, x + cells[i].dx, top + cells[i].off, TILE_AIR);
        }
        world_set(w, x, top - 1, log);
        for (int i = 0; i < count; i++) {
            int lx = x + cells[i].dx, ly = top + cells[i].off - 1;
            if (had[i] && world_in_bounds(w, lx, ly) && world_get(w, lx, ly) == TILE_AIR)
                world_set(w, lx, ly, leaf);
        }
        result.grew = result.extended = true;
    }
    return result;
}
