#include "water.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RUN 2048 /* the longest stretch of level water evened out in one go */

static uint8_t *cell(const World *w, int x, int y)
{
    return &w->water[(size_t)y * w->width + world_wrap_x(w, x)];
}

int world_water(const World *w, int x, int y)
{
    if (!w->water || !world_in_bounds(w, x, y)) return 0;
    return *cell(w, x, y);
}

void world_set_water(World *w, int x, int y, int level)
{
    if (!w->water || !world_in_bounds(w, x, y)) return;
    *cell(w, x, y) = (uint8_t)(level < 0 ? 0 : level > WATER_FULL ? WATER_FULL : level);
    water_wake(w, x);
}

bool world_is_water(const World *w, int x, int y)
{
    return world_water(w, x, y) >= WATER_FULL / 2;
}

void water_wake(World *w, int x)
{
    if (!w->water_awake) return;
    for (int d = -1; d <= 1; d++) w->water_awake[world_wrap_x(w, x + d)] = 1;
    w->water_stirring = true;
}

void water_fill_sea(World *w)
{
    if (!w->water) return;
    memset(w->water, 0, (size_t)w->width * w->height);
    for (int y = w->sea_row + 1; w->sea_row >= 0 && y < w->height; y++)
        for (int x = 0; x < w->width; x++)
            if (!world_is_solid(w, x, y) && world_get_at(w, LAYER_WALL, x, y) == TILE_AIR) *cell(w, x, y) = WATER_FULL;
    /* It is at rest as it lies. */
    memset(w->water_awake, 0, (size_t)w->width);
    w->water_stirring = false;
}

void water_settle(World *w, int max_steps)
{
    if (!w->water) return;
    for (int x = 0; x < w->width; x++) w->water_awake[x] = 1;
    w->water_stirring = true;
    for (int i = 0; i < max_steps && w->water_stirring; i++) water_step(w);
}

/* Fills the sea up again where it has run off into the ground. Returns how much it took. */
static long top_up_sea(World *w)
{
    long added = 0;
    for (int y = w->sea_row + 1; w->sea_row >= 0 && y < w->height; y++) {
        for (int x = 0; x < w->width; x++) {
            if (world_is_solid(w, x, y) || world_get_at(w, LAYER_WALL, x, y) != TILE_AIR) continue;
            added += WATER_FULL - *cell(w, x, y);
            *cell(w, x, y) = WATER_FULL;
        }
    }
    return added;
}

void water_make_sea(World *w)
{
    if (!w->water) return;
    water_fill_sea(w);
    for (int round = 0; round < 12; round++) {
        water_settle(w, 4000);
        if (top_up_sea(w) == 0) break;
    }
    memset(w->water_awake, 0, (size_t)w->width);
    w->water_stirring = false;
}

long water_total(const World *w)
{
    long total = 0;
    size_t cells = (size_t)w->width * w->height;
    for (size_t i = 0; w->water && i < cells; i++) total += w->water[i];
    return total;
}

int water_add(World *w, int x, int y, int amount)
{
    if (!w->water || !world_in_bounds(w, x, y) || amount <= 0) return amount;
    for (int yy = y; amount > 0 && yy >= 0 && !world_is_solid(w, x, yy); yy--) {
        int room = WATER_FULL - *cell(w, x, yy), n = amount < room ? amount : room;
        *cell(w, x, yy) += (uint8_t)n;
        amount -= n;
    }
    water_wake(w, x);
    return amount;
}

void water_displace(World *w, int x, int y)
{
    if (!w->water || !world_in_bounds(w, x, y)) return;
    int left = *cell(w, x, y);
    if (left == 0) return;
    *cell(w, x, y) = 0;
    /* Up through the water above to the first square with room; then to either side. */
    for (int yy = y - 1; left > 0 && yy >= 0 && !world_is_solid(w, x, yy); yy--) {
        int room = WATER_FULL - *cell(w, x, yy), n = left < room ? left : room;
        *cell(w, x, yy) += (uint8_t)n;
        left -= n;
    }
    for (int d = -1; left > 0 && d <= 1; d += 2) {
        if (world_is_solid(w, x + d, y)) continue;
        int room = WATER_FULL - *cell(w, x + d, y), n = left < room ? left : room;
        *cell(w, x + d, y) += (uint8_t)n;
        left -= n;
    }
    water_wake(w, x);
}

static bool is_open(const World *w, int x, int y)
{
    return world_in_bounds(w, x, y) && !world_is_solid(w, x, y);
}

/* Whether water in a square stays in it: it has ground, or a square full of
 * water, beneath it. Otherwise it falls. */
static bool is_held(const World *w, int x, int y)
{
    return y + 1 >= w->height || world_is_solid(w, x, y + 1) || *cell(w, x, y + 1) >= WATER_FULL;
}

static void move(World *w, int from_x, int from_y, int to_x, int to_y, int n)
{
    *cell(w, from_x, from_y) -= (uint8_t)n;
    *cell(w, to_x, to_y) += (uint8_t)n;
    water_wake(w, from_x);
    water_wake(w, to_x);
}

/* Evens out one level stretch of water, from column `l` to `r` of a row.
 * Water runs from a square to the nearest one that stands two or more
 * eighths lower, an eighth at a time, the closest such pair first - so a dip
 * fills in from round about it, and water set down on level ground spreads
 * into a puddle an eighth deep round where it landed, and no further. It does
 * not feel a difference more than WATER_REACH squares off. At either end,
 * where the floor stops, it runs over the edge. */
#define WATER_REACH 32
static int settle_run(World *w, int l, int r, int y, int flow)
{
    static bool flip; /* which side wins a tie, turn about, so nothing drifts one way */
    int moved = 0;
    for (int n = 0; n < flow; n++) {
        int least = WATER_FULL + 1, most = -1;
        for (int x = l; x <= r; x++) {
            int level = *cell(w, x, y);
            if (level < least) least = level;
            if (level > most) most = level;
        }
        if (most - least < 2) break; /* as level as eighths allow */
        int from = INT_MIN, to = INT_MIN, near = WATER_REACH + 1, drop = 0;
        flip = !flip;
        for (int i = 0; i <= r - l; i++) {
            int x = flip ? r - i : l + i, level = *cell(w, x, y);
            if (level < 2) continue;
            for (int d = 1; d <= near && d <= WATER_REACH; d++) {
                int found = INT_MIN, fall = 0;
                for (int side = 0; side < 2; side++) {
                    int o = (side == 0) != flip ? x - d : x + d;
                    if (o < l || o > r) continue;
                    int diff = level - *cell(w, o, y);
                    if (diff >= 2 && diff > fall) found = o, fall = diff;
                }
                if (found == INT_MIN) continue;
                if (d < near || fall > drop) from = x, to = found, near = d, drop = fall;
                break;
            }
        }
        if (from == INT_MIN) break;
        move(w, from, y, to, y, 1);
        moved++;
    }
    for (int side = 0; side < 2; side++) {
        int end = side == 0 ? l : r, out = side == 0 ? l - 1 : r + 1;
        if (!is_open(w, out, y) || is_held(w, out, y)) continue;
        int have = *cell(w, end, y), room = WATER_FULL - *cell(w, out, y);
        int n = have < room ? have : room;
        if (n > flow) n = flow;
        if (n <= 0) continue;
        move(w, end, y, out, y, n);
        moved += n;
    }
    return moved;
}

int water_step(World *w)
{
    if (!w->water || !w->water_stirring) return 0;
    int width = w->width, height = w->height, moved = 0;
    int flow = content.rules.water_flow > 0 ? content.rules.water_flow : 4;
    uint8_t *awake = malloc((size_t)width);
    int *done = malloc(sizeof(int) * (size_t)height);
    if (!awake || !done) {
        free(awake);
        free(done);
        return 0;
    }
    /* Whatever moves in this step wakes its own column for the next. */
    memcpy(awake, w->water_awake, (size_t)width);
    memset(w->water_awake, 0, (size_t)width);
    w->water_stirring = false;
    for (int y = 0; y < height; y++) done[y] = INT_MIN;

    /* Down first, from the bottom up, so that a whole column drops together. */
    for (int x = 0; x < width; x++) {
        if (!awake[x]) continue;
        for (int y = height - 2; y >= 0; y--) {
            int level = *cell(w, x, y);
            if (level == 0 || world_is_solid(w, x, y + 1)) continue;
            int room = WATER_FULL - *cell(w, x, y + 1), n = level < room ? level : room;
            if (n <= 0) continue;
            move(w, x, y, x, y + 1, n);
            moved += n;
        }
    }
    /* Then sideways, a level stretch at a time. */
    for (int x = 0; x < width; x++) {
        if (!awake[x]) continue;
        for (int y = 0; y < height; y++) {
            if (x <= done[y] || world_is_solid(w, x, y)) continue;
            if (*cell(w, x, y) == 0 && world_water(w, x - 1, y) == 0 && world_water(w, x + 1, y) == 0) continue;
            if (!is_held(w, x, y)) continue;
            int l = x, r = x;
            while (x - l < MAX_RUN / 2 && is_open(w, l - 1, y) && is_held(w, l - 1, y)) l--;
            while (r - x < MAX_RUN / 2 && is_open(w, r + 1, y) && is_held(w, r + 1, y)) r++;
            done[y] = r;
            moved += settle_run(w, l, r, y, flow);
        }
    }
    free(awake);
    free(done);
    return moved;
}
