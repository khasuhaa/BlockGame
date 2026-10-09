#ifndef MOBMODEL_H
#define MOBMODEL_H

#include "content.h"
#include "raylib.h"

/* A creature, built of boxes as the character is: each part a box hung from a point of the body, wrapped in its
 * part of a skin (laid out as the boxes unfold: see playermodel.c), and moved as the creature moves - legs that
 * stride, wings that beat, a tail that sways, a body that goes along in a wave. Which boxes, where, how big and how
 * they move is the creature's own, in the data ("model"): a big creature is built big, of more and larger parts,
 * and is not a small one drawn large. */

/* Draws a creature with its feet (or, for one that floats, its underside) at a point of the world, `scale` world
 * units to one of the model's (a tile is 16 of those). `stride` is how far through a step it is, 0 to 1, and
 * `walking` whether it is taking one; `time` is seconds, for what moves all the while. `z` is how far back it
 * stands; with `deep` false it is drawn flat. False if the creature has no model. */
bool mobmodel_draw(const MobDef *def, Vector2 feet, float scale, int facing, bool walking, float stride, float time, bool deep, float z, Color tint);

#endif
