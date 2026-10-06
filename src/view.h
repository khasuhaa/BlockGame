#ifndef VIEW_H
#define VIEW_H

#include "raylib.h"

#include <math.h>

/* How the wrapping world is laid out on screen this frame. */
typedef struct {
    Vector2 origin;  /* screen position of tile (0, 0) */
    float tile_size;
    float center_x;  /* camera centre, in tiles; may lie outside 0..world_width */
    int world_width;
} View;

/* The copy of column `x` that is nearest the camera. The world repeats every
 * world_width columns, so anything drawn there is drawn where it is visible. */
static inline float view_near_x(const View *v, float x)
{
    float w = (float)v->world_width;
    return x + w * floorf((v->center_x - x) / w + 0.5f);
}

/* Screen position of the top-left corner of a tile. */
static inline Vector2 view_tile_pos(const View *v, float x, float y)
{
    return (Vector2){ v->origin.x + view_near_x(v, x) * v->tile_size,
                      v->origin.y + y * v->tile_size };
}

#endif
