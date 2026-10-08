#ifndef WORLD3D_H
#define WORLD3D_H

#include "raylib.h"

/* The world drawn in depth. The rules of the game are flat - a grid seen from
 * the side - and so is everything that draws it: each thing says where on
 * that flat picture it goes. Between world3d_begin and world3d_end the
 * picture is the front of a scene looked into through a real camera, whose
 * eye is in front of the middle of the screen: what is drawn plainly lies on
 * the front, exactly where it always did, and what is given a depth stands
 * back from it - a block as a block, a picture as a thing with thickness -
 * and shifts against the front as the view moves.
 *
 * Distances into the scene are in the same units as across it (pixels of the
 * world, before the zoom), and negative: 0 is the front. */

/* `zoom`: screen pixels to one of the world's. The eye is in front of the middle of the screen. */
void world3d_begin(float zoom);
void world3d_end(void);
bool world3d_active(void);
/* The middle of the view, in the world's pixels: what is left of it shows its right side, and so on. */
Vector2 world3d_center(void);
/* Where on the flat picture a point of the screen lies at a depth: for finding what is under the cursor. */
Vector2 world3d_at_depth(Vector2 screen, float zoom, float z);

/* How the textures drawn next are to stand. */
#define W3D_LEFT 1
#define W3D_RIGHT 2
#define W3D_TOP 4
#define W3D_BOTTOM 8
#define W3D_ALL 15
/* On the front, as a flat picture (what everything is until told otherwise). */
void world3d_flat(void);
/* As a flat picture at a depth. */
void world3d_plane(float z);
/* As a solid from one depth back to another: a block with the picture on its
 * face and on such of its sides as `sides` names, if the picture fills its
 * square; otherwise the picture itself given that thickness (or, if it is
 * mostly empty and `slim` is set, a thinner slice of it in the middle). */
void world3d_solid(float front, float back, int sides, bool slim);

/* Draws a texture as DrawTexturePro would, standing as last told. (assets.c draws everything through this.) */
void world3d_texture(Texture2D tex, Rectangle src, Rectangle dst, Color tint);
/* Plain drawing - rectangles, text - done between these lies at a depth instead of on the front. */
void world3d_push(float z);
void world3d_pop(void);

#endif
