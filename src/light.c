#include "light.h"

#include <stdlib.h>
#include <string.h>

int light_sky(const Game *g)
{
    const RulesDef *u = &content.rules;
    float day = game_daylight(g);
    return (int)((float)u->light_moon + (float)(u->light_sun - u->light_moon) * day + 0.5f);
}

/* Whether a block is giving light where it is: one that needs a fire has to have one. */
static bool shines(const Game *g, const TileDef *def, int x, int y)
{
    if (def->light <= 0 && (def->beam_dir == 0 || def->beam_light <= 0)) return false;
    return !def->light_needs_fuel || game_fuel_at(g, x, y) > 0;
}

int light_glow(const Game *g, int x, int y)
{
    if (content.rules.light_glow <= 0) return 0;
    const TileDef *front = tile_def(world_get(&g->world, x, y));
    int best = 0;
    for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
        const TileDef *def = layer == LAYER_FOREGROUND ? front : tile_def(world_get_at(&g->world, layer, x, y));
        /* (Not what is out of sight behind a block.) */
        if (layer != LAYER_FOREGROUND && front->solid && !front->see_through) continue;
        if (!shines(g, def, x, y)) continue;
        int bright = (def->light > def->beam_light ? def->light : def->beam_light) + content.rules.light_glow;
        if (bright > best) best = bright;
    }
    return best > LIGHT_MAX ? LIGHT_MAX : best;
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
        /* The beam: lit from the top of the place down to the first block. (A place under another has a sky of
         * its own or none: worked out again whenever the rows pass from one into the next.) */
        int stop = 0, top = 0, place = -2;
        for (int cy = 0; cy < bh; cy++) {
            int x = bx + cx, y = by + cy;
            int v = 0;
            int in = y < 0 ? -1 : world_region_at(w, y);
            if (in != place) {
                place = in;
                top = y < 0 ? 0 : world_sky_top(w, y);
                stop = top < 0 ? 0 : top;
                while (top >= 0 && stop < w->height && !world_is_solid(w, bx + cx, stop)) stop++;
            }
            if (top >= 0 && (y < 0 || y < stop)) v = sky;
            else if (top >= 0 && y < w->height && !world_is_solid(w, x, y) &&
                     (world_get_at(w, LAYER_WALL, x, y) == TILE_AIR ||
                      /* (or a window in what is behind it, if its glass is clear) */
                      (tile_def(world_get_at(w, LAYER_BACKGROUND, x, y))->window && !tile_def(world_get_at(w, LAYER_BACKGROUND, x, y))->window_dark)))
                v = sky; /* open air with nothing behind it */
            /* Under water the sky is dimmer the deeper it is. */
            if (v == sky && u->light_water_depth > 0 && world_is_water(w, x, y)) {
                /* (How deep: the water standing over this square.) */
                int over = 1;
                while (over < sky * u->light_water_depth && world_is_water(w, x, y - over)) over++;
                v = sky - over / u->light_water_depth;
                if (v < 0) v = 0;
            }
            if (y >= 0 && y < w->height) {
                int e = emitted(g, x, y);
                if (e > v) v = e;
            }
            level[cy * bw + cx] = (uint8_t)(v > LIGHT_MAX ? LIGHT_MAX : v);
        }
    }

    /* Lamps that throw their light one way: a quarter circle of it, up or
     * down from the lamp, a level dimmer with each row, and stopped by
     * whatever is solid. A square is lit if the one it comes through was. */
    for (int cy = 0; cy < bh; cy++) {
        for (int cx = 0; cx < bw; cx++) {
            int x = bx + cx, y = by + cy, dir = 0, bright = 0;
            for (int layer = 0; layer < BLOCK_LAYERS && y >= 0 && y < w->height; layer++) {
                const TileDef *def = tile_def(world_get_at(w, layer, x, y));
                if (def->beam_dir != 0 && def->beam_light > bright) {
                    dir = def->beam_dir;
                    bright = def->beam_light > LIGHT_MAX ? LIGHT_MAX : def->beam_light;
                }
            }
            if (dir == 0) continue;
            enum { REACH = LIGHT_MAX };
            bool lit[REACH + 1][2 * REACH + 1] = { { false } };
            lit[0][REACH] = true;
            for (int d = 1; d < bright && d <= REACH; d++) {
                int ry = cy + dir * d;
                if (ry < 0 || ry >= bh) break;
                for (int off = -d; off <= d; off++) {
                    int rx = cx + off;
                    /* (Through the square a row nearer the lamp, a step in toward its line.) */
                    int from = off > 0 ? off - 1 : off < 0 ? off + 1 : 0;
                    if (abs(from) > d - 1) from = off > 0 ? d - 1 : -(d - 1);
                    if (rx < 0 || rx >= bw || !lit[d - 1][REACH + from]) continue;
                    if (by + ry >= 0 && world_is_solid(w, bx + rx, by + ry)) continue;
                    lit[d][REACH + off] = true;
                    int v = bright - d * u->light_air_falloff;
                    if (v > level[ry * bw + rx]) level[ry * bw + rx] = (uint8_t)v;
                }
            }
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
            /* (Some places are never quite dark.) */
            int least = u->light_minimum, dim = world_dimension_at(w, y0 + cy);
            if (dim >= 0 && dim < content.dimension_count && content.dimensions[dim].ambient > least) least = content.dimensions[dim].ambient;
            out[cy * width + cx] = (uint8_t)(v < least ? least : v);
        }
    }
    free(level);
}
