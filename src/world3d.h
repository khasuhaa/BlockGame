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
/* As a thing with depth, somewhere between one depth and another. What kind of thing says how it is made:
 *
 *   W3D_AUTO   a picture that fills its square is a block of material (W3D_BLOCK); one that is mostly empty and
 *              branching, a plant (W3D_CROSS); any other, a built thing (W3D_FRAME).
 *   W3D_BLOCK  material all the way from front to back, with the picture on its face and again on each of its
 *              sides (those `sides` names), a square of it to a square of depth: earth, stone, a tree's trunk.
 *              A picture with holes in it is cut through from front to back.
 *   W3D_BOX    one thing, not a material: the picture on its face, and its sides plain - coloured as the edge of
 *              the picture nearest each is. A chest, a furnace.
 *   W3D_FRAME  built up from the picture's own parts. Every stretch of it across becomes a box as deep as it is
 *              wide - so a table's top is a slab and a post is square - and a part much thinner than the whole
 *              (a leg, a wheel) stands once at the front and once at the back.
 *   W3D_BODY   the same, with every part in the middle: a creature.
 *   W3D_MASS   the same, made as deep as the room it is given: each part that much deeper in proportion, and none
 *              doubled. Something that is stood on, and so is as deep as the ground is.
 *   W3D_PANEL  a flat thing with a little thickness: a ladder, a sign, something lying on the ground.
 *   W3D_CROSS  two flat pictures crossed, as plants are. */
typedef enum { W3D_AUTO, W3D_BLOCK, W3D_BOX, W3D_FRAME, W3D_BODY, W3D_PANEL, W3D_CROSS, W3D_MASS } World3dModel;
void world3d_solid(float front, float back, int sides, World3dModel model);

/* Draws a texture as DrawTexturePro would, standing as last told. (assets.c draws everything through this.) */
void world3d_texture(Texture2D tex, Rectangle src, Rectangle dst, Color tint);
/* Plain drawing - rectangles, text - done between these lies at a depth instead of on the front. */
void world3d_push(float z);
void world3d_pop(void);

#endif
