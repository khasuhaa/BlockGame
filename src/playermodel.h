#ifndef PLAYERMODEL_H
#define PLAYERMODEL_H

#include "raylib.h"

/* The character, built of boxes: a cube for the head and a block each for
 * the body, the arms and the legs, seen dead level: face on when they stand
 * idle, square side on when they go anywhere. Each box is wrapped in its part of
 * a skin (64 by 32, laid out as the boxes unfold: see playermodel.c); what
 * is worn is more skins of the same layout, drawn on the same boxes a little
 * larger, wherever they are not clear. */

/* How far round from facing the viewer they stand when they face along the world, in degrees. */
#define PLAYERMODEL_SIDE 90.0f

typedef struct {
    int facing;        /* 1 looking right, -1 looking left */
    float lean;        /* the body tipped forward at the hips, in degrees: 90 is flat out, on all fours */
    float head;        /* the head tipped back on the neck, in degrees: to look ahead while on all fours */
    float pitch;       /* the whole of them tipped over about the feet: -90 is flat on their back */
    float arm[2];      /* each arm swung forward from hanging, in degrees: near side, far side */
    float leg[2];      /* each leg swung forward */
    float sink;        /* how far the hips are let down, in the model's units (12 is sat on the ground) */
    float turn;        /* how far round toward the viewer they are turned, in degrees; PLAYERMODEL_SIDE is face on */
    Color tint;        /* WHITE for as they are */
    float jet;         /* how long the flames under a pack on their back are, in the model's units (9 at most); 0 for none */
} BodyPose;

/* Draws them with their feet at a point of the world, `scale` world units to
 * one of the model's (a tile is 16 of those). `skin` is the body; `worn` is
 * what is on over it, inmost first. */
/* Whether what is drawn next is drawn in depth - each part as far forward or back of `z` as it is in the body -
 * or flat, as a picture of it. (In depth nothing of it is left out for being turned away: what is behind is
 * hidden by what is in front.) */
void playermodel_depth(bool on, float z);
/* (`packs` says which of the things worn are packs as well, a bit for each: a box on the back, whose picture is
 * laid out at 32,0 of the thing's own - and the flame under it at 52,0.) */
void playermodel_draw(Vector2 feet, float scale, const BodyPose *pose, const char *skin, const char *const *worn, int worn_count, unsigned packs);

#endif
