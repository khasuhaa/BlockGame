#ifndef LIGHT_H
#define LIGHT_H

#include "game.h"

#define LIGHT_MAX 15

/* How bright the open sky is right now: the sun by day, the moon by night. */
int light_sky(const Game *g);

/* Works out how well lit a rectangle of the world is, 0..LIGHT_MAX per tile,
 * row by row into `out` (width * height bytes). Column x0 may be any column:
 * the world wraps.
 *
 * Light comes from the sky - it beams straight down every column until a
 * block stops it, and fills any open space with no backwall - and from blocks
 * that give light, such as torches and gems. From there it spreads tile to
 * tile, losing a little through air and more through blocks. Sources up to
 * LIGHT_MAX tiles outside the rectangle are counted. */
void light_compute(const Game *g, int x0, int y0, int width, int height, uint8_t *out);

#endif
