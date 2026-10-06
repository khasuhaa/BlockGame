#include "light.h"

#include <stdlib.h>
#include <string.h>

int light_sky(const Game *g)
{
    const RulesDef *u = &content.rules;
    float day = game_daylight(g);
    return (int)((float)u->light_moon + (float)(u->light_sun - u->light_moon) * day + 0.5f);
}

/* Light a tile gives off of its own accord. */
static int emitted(const Game *g, int x, int y)
{
    int best = 0;
    for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
        const TileDef *def = tile_def(world_get_at(&g->world, layer, x, y));
        if (def->light <= best) continue;
        /* A fire that has gone out gives none. */
        if (def->light_needs_fuel && game_fuel_at(g, x, y) <= 0) continue;
        best = def->light;
    }
    return best;
}

void light_compute(const Game *g, int x0, int y0, int width, int height, uint8_t *out)
{
    const World *w = &g->world;
    const RulesDef *u = &content.rules;
    /* Work on a bigger rectangle, so light from just outside spreads in. */
    const int pad = LIGHT_MAX;
    int bw = width + 2 * pad, bh = height + 2 * pad;
    int bx = x0 - pad, by = y0 - pad;
    uint8_t *level = calloc((size_t)bw, (size_t)bh);
    if (!level) {
        memset(out, LIGHT_MAX, (size_t)width * height);
        return;
    }
    int sky = light_sky(g);

    for (int cx = 0; cx < bw; cx++) {
        /* The beam: lit from the top of the world down to the first block. */
        int stop = 0;
        while (stop < w->height && !world_is_solid(w, bx + cx, stop)) stop++;
        for (int cy = 0; cy < bh; cy++) {
            int x = bx + cx, y = by + cy;
            int v = 0;
            if (y < 0 || y < stop) v = sky;
            else if (y < w->height && !world_is_solid(w, x, y) &&
                     world_get_at(w, LAYER_WALL, x, y) == TILE_AIR)
                v = sky; /* open air with nothing behind it */
            if (y >= 0 && y < w->height) {
                int e = emitted(g, x, y);
                if (e > v) v = e;
            }
            level[cy * bw + cx] = (uint8_t)(v > LIGHT_MAX ? LIGHT_MAX : v);
        }
    }

    /* Spread, brightest first: by the time a level is reached, every tile
     * that will ever have it already does. */
    static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (int l = LIGHT_MAX; l > 1; l--) {
        for (int cy = 0; cy < bh; cy++) {
            for (int cx = 0; cx < bw; cx++) {
                if (level[cy * bw + cx] != l) continue;
                for (int d = 0; d < 4; d++) {
                    int nx = cx + dirs[d][0], ny = cy + dirs[d][1];
                    if (nx < 0 || ny < 0 || nx >= bw || ny >= bh) continue;
                    bool solid = by + ny >= 0 && world_is_solid(w, bx + nx, by + ny);
                    int n = l - (solid ? u->light_solid_falloff : u->light_air_falloff);
                    if (n > level[ny * bw + nx]) level[ny * bw + nx] = (uint8_t)n;
                }
            }
        }
    }

    for (int cy = 0; cy < height; cy++) {
        for (int cx = 0; cx < width; cx++) {
            int v = level[(cy + pad) * bw + cx + pad];
            out[cy * width + cx] = (uint8_t)(v < u->light_minimum ? u->light_minimum : v);
        }
    }
    free(level);
}
