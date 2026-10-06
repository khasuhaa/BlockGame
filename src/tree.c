#include "tree.h"

typedef struct {
    int dx;  /* columns from the trunk */
    int off; /* rows below the trunk's top block; -1 is the row above it */
} LeafCell;

void tree_plant(World *w, int x, int y, int kind)
{
    const TreeDef *def = &content.trees[kind];
    world_set_at(w, def->layer, x, y, def->log);
}

int tree_trunk_at(const World *w, int x, int y)
{
    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        int kind = tile_def(world_get_at(w, layer, x, y))->log_of;
        if (kind >= 0 && content.trees[kind].layer == layer) return kind;
    }
    return -1;
}

int tree_leaves_at(const World *w, int x, int y)
{
    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        int kind = tile_def(world_get_at(w, layer, x, y))->leaves_of;
        if (kind >= 0 && content.trees[kind].layer == layer) return kind;
    }
    return -1;
}

static uint32_t next_random(uint32_t *state)
{
    uint32_t x = *state ? *state : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

/* A leaf or trunk block can go where its own layer is empty and nothing
 * solid stands in the foreground. */
static bool is_free(const World *w, const TreeDef *def, int x, int y)
{
    return world_in_bounds(w, x, y) && world_get_at(w, def->layer, x, y) == TILE_AIR &&
           !world_is_solid(w, x, y);
}

/* The places a tree of this height may have leaves, nearest the trunk first.
 * Rows are counted from the top of the trunk, so the crown rides up as the
 * trunk grows, and a row only gets leaves once the trunk reaches down to it. */
static int leaf_cells(const TreeDef *def, int trunk_x, int height, LeafCell *out)
{
    bool mirror = def->mirror_by_column && (trunk_x & 1);
    int n = 0, widest = 0;
    for (int i = 0; i < def->row_count; i++) {
        const TreeRow *row = &def->rows[i];
        if (row->center && row->offset < height && n < TREE_MAX_LEAVES)
            out[n++] = (LeafCell){ 0, row->offset };
        if (row->left > widest) widest = row->left;
        if (row->right > widest) widest = row->right;
    }
    for (int d = 1; d <= widest; d++) {
        for (int i = 0; i < def->row_count; i++) {
            const TreeRow *row = &def->rows[i];
            if (row->offset >= height) continue;
            for (int side = -1; side <= 1; side += 2) {
                int reach = (side < 0) != mirror ? row->left : row->right;
                if (reach >= d && n < TREE_MAX_LEAVES) out[n++] = (LeafCell){ side * d, row->offset };
            }
        }
    }
    return n;
}

TreeGrowth tree_grow(World *w, int x, int y, uint32_t *rng)
{
    TreeGrowth result = { false, false, world_wrap_x(w, x), y };
    int kind = tree_trunk_at(w, x, y);
    if (kind < 0) return result;
    const TreeDef *def = &content.trees[kind];
    int layer = def->layer;

    int top = y, base = y;
    while (world_get_at(w, layer, x, top - 1) == def->log) top--;
    while (world_get_at(w, layer, x, base + 1) == def->log) base++;
    int height = base - top + 1;
    result.top = top;

    LeafCell cells[TREE_MAX_LEAVES];
    int count = leaf_cells(def, result.x, height, cells);

    /* Leaves fill in ring by ring: pick among the nearest empty places. */
    int open[TREE_MAX_LEAVES], open_count = 0, ring = -1;
    for (int i = 0; i < count; i++) {
        int d = cells[i].dx < 0 ? -cells[i].dx : cells[i].dx;
        if (!is_free(w, def, x + cells[i].dx, top + cells[i].off)) continue;
        if (ring < 0) ring = d;
        if (d == ring) open[open_count++] = i;
    }

    TileType above = world_get_at(w, layer, x, top - 1);
    bool can_extend = height < def->max_height && top - 1 >= 0 && !world_is_solid(w, x, top - 1) &&
                      (above == TILE_AIR || above == def->leaves);

    uint32_t roll = next_random(rng);
    if (open_count > 0 && (!can_extend || (roll & 1))) {
        LeafCell c = cells[open[(roll >> 8) % (uint32_t)open_count]];
        world_set_at(w, layer, x + c.dx, top + c.off, def->leaves);
        result.grew = true;
    } else if (can_extend) {
        /* Lift the whole crown, then put it back one row up wherever it fits. */
        bool had[TREE_MAX_LEAVES];
        for (int i = 0; i < count; i++) {
            had[i] = world_get_at(w, layer, x + cells[i].dx, top + cells[i].off) == def->leaves;
            if (had[i]) world_set_at(w, layer, x + cells[i].dx, top + cells[i].off, TILE_AIR);
        }
        world_set_at(w, layer, x, top - 1, def->log);
        for (int i = 0; i < count; i++) {
            int lx = x + cells[i].dx, ly = top + cells[i].off - 1;
            if (had[i] && is_free(w, def, lx, ly)) world_set_at(w, layer, lx, ly, def->leaves);
        }
        result.grew = result.extended = true;
    }
    return result;
}
