#ifndef PLAYERMODEL_H
#define PLAYERMODEL_H

#include "raylib.h"

/* The character, built of boxes: a cube for the head and a block each for
 * the body, the arms and the legs, seen a little from the front and a little
 * from above, so that they read as solid. Each box is wrapped in its part of
 * a skin (64 by 32, laid out as the boxes unfold: see playermodel.c); what
 * is worn is more skins of the same layout, drawn on the same boxes a little
 * larger, wherever they are not clear. */

typedef struct {
    int facing;        /* 1 looking right, -1 looking left */
    float lean;        /* the body tipped forward at the hips, in degrees: 90 is flat out, on all fours */
    float head;        /* the head tipped back on the neck, in degrees: to look ahead while on all fours */
    float pitch;       /* the whole of them tipped over about the feet: -90 is flat on their back */
    float arm[2];      /* each arm swung forward from hanging, in degrees: near side, far side */
    float leg[2];      /* each leg swung forward */
    float sink;        /* how far the hips are let down, in the model's units (12 is sat on the ground) */
    float turn;        /* how far round toward the viewer they are turned, in degrees; 0 for the usual */
    Color tint;        /* WHITE for as they are */
} BodyPose;

/* Draws them with their feet at a point of the world, `scale` world units to
 * one of the model's (a tile is 16 of those). `skin` is the body; `worn` is
 * what is on over it, inmost first. */
void playermodel_draw(Vector2 feet, float scale, const BodyPose *pose, const char *skin, const char *const *worn, int worn_count);

#endif
