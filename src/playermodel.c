#include "playermodel.h"

#include "assets.h"
#include "rlgl.h"

#include <math.h>
#include <stdlib.h>

/* The skin: 64 by 32. A box w wide, h tall and d deep, whose corner on the
 * skin is (u, v), unfolds as: its top at (u + d, v), w by d; its underside
 * beside that; then a row beneath of its right side (d by h), its front
 * (w by h), its left side (d by h) and its back (w by h). The head's corner
 * is (0, 0), a leg's (0, 16), the body's (16, 16) and an arm's (40, 16); the
 * other arm and leg are the same, turned about. */
#define SKIN_W 64.0f
#define SKIN_H 32.0f
#define VIEW_YAW PLAYERMODEL_SIDE
#define MAX_QUADS 256

typedef struct {
    float x, y, z;
} V3;

typedef struct {
    Vector2 p[4], uv[4];
    float z[4];
    float depth, shade;
    unsigned int texture;
} Quad;

static Quad quads[MAX_QUADS];
static bool deep;
static float deep_z;

void playermodel_depth(bool on, float z)
{
    deep = on;
    deep_z = z;
}
static int quad_count;

static V3 rot_x(V3 v, float degrees)
{
    float a = degrees * DEG2RAD, c = cosf(a), s = sinf(a);
    return (V3){ v.x, v.y * c - v.z * s, v.y * s + v.z * c };
}

static V3 rot_y(V3 v, float degrees)
{
    float a = degrees * DEG2RAD, c = cosf(a), s = sinf(a);
    return (V3){ v.x * c + v.z * s, v.y, -v.x * s + v.z * c };
}

/* One part: a box, where it hangs from, and how it is turned there. */
typedef struct {
    V3 pivot;      /* in the body, standing */
    V3 corner;     /* the box's least corner, from the pivot */
    V3 size;
    float u, v;    /* its corner on the skin */
    bool mirror;   /* the other arm or leg: the same skin, turned about */
    bool upper;    /* it goes with the body when the body leans */
    float swing;   /* turned forward about its pivot */
} Part;

/* Where a point of a part ends up on the screen, and how near the viewer. */
static V3 place(const Part *part, const BodyPose *pose, V3 local)
{
    V3 v = rot_x(local, -part->swing);
    v.x += part->pivot.x, v.y += part->pivot.y, v.z += part->pivot.z;
    if (part->upper) {
        /* (The body leans about the hips.) */
        v.y -= 12.0f;
        v = rot_x(v, pose->lean);
        v.y += 12.0f;
    }
    v.y -= pose->sink;
    v = rot_x(v, pose->pitch);
    /* (The viewer is on the -z side: they are turned most of the way round, to show their front.) */
    v = rot_y(v, (float)pose->facing * (180.0f - VIEW_YAW + pose->turn));
    return v;
}

static void add_quad(const Part *part, const BodyPose *pose, const V3 corner[4], float u, float v, float w, float h, bool flip, float shade, unsigned int texture)
{
    if (quad_count >= MAX_QUADS) return;
    Quad *q = &quads[quad_count];
    float depth = 0.0f;
    V3 at[4];
    for (int i = 0; i < 4; i++) {
        at[i] = place(part, pose, corner[i]);
        q->p[i] = (Vector2){ at[i].x, -at[i].y };
        q->z[i] = at[i].z;
        depth += at[i].z;
    }
    /* Only a face that is turned toward the viewer. */
    float ax = at[1].x - at[0].x, ay = at[1].y - at[0].y, bx = at[3].x - at[0].x, by = at[3].y - at[0].y;
    if (!deep && ax * by - ay * bx <= 0.0f) return;
    float u0 = u / SKIN_W, u1 = (u + w) / SKIN_W, v0 = v / SKIN_H, v1 = (v + h) / SKIN_H;
    if (flip) {
        float swap = u0;
        u0 = u1;
        u1 = swap;
    }
    /* (Corners are given top left, bottom left, bottom right, top right, as the face is looked at from outside.) */
    q->uv[0] = (Vector2){ u0, v0 };
    q->uv[1] = (Vector2){ u0, v1 };
    q->uv[2] = (Vector2){ u1, v1 };
    q->uv[3] = (Vector2){ u1, v0 };
    q->depth = depth / 4.0f;
    q->shade = shade;
    q->texture = texture;
    quad_count++;
}

/* The six faces of a part, `grow` larger all round (for what is worn over it). */
static void add_part(const Part *part, const BodyPose *pose, float grow, unsigned int texture)
{
    float x0 = part->corner.x - grow, y0 = part->corner.y - grow, z0 = part->corner.z - grow;
    float x1 = part->corner.x + part->size.x + grow, y1 = part->corner.y + part->size.y + grow, z1 = part->corner.z + part->size.z + grow;
    float w = part->size.x, h = part->size.y, d = part->size.z, u = part->u, v = part->v;
    bool m = part->mirror;
    /* (+z is the front of them, +x their left, +y up.) */
    V3 front[4] = { { x1, y1, z1 }, { x1, y0, z1 }, { x0, y0, z1 }, { x0, y1, z1 } };
    V3 back[4] = { { x0, y1, z0 }, { x0, y0, z0 }, { x1, y0, z0 }, { x1, y1, z0 } };
    V3 left[4] = { { x1, y1, z0 }, { x1, y0, z0 }, { x1, y0, z1 }, { x1, y1, z1 } };
    V3 right[4] = { { x0, y1, z1 }, { x0, y0, z1 }, { x0, y0, z0 }, { x0, y1, z0 } };
    V3 top[4] = { { x1, y1, z0 }, { x1, y1, z1 }, { x0, y1, z1 }, { x0, y1, z0 } };
    V3 under[4] = { { x1, y0, z1 }, { x1, y0, z0 }, { x0, y0, z0 }, { x0, y0, z1 } };
    add_quad(part, pose, front, u + d, v + d, w, h, m, 0.94f, texture);
    add_quad(part, pose, back, u + d + w + d, v + d, w, h, m, 0.72f, texture);
    add_quad(part, pose, left, m ? u : u + d + w, v + d, d, h, m, 0.82f, texture);
    add_quad(part, pose, right, m ? u + d + w : u, v + d, d, h, m, 0.82f, texture);
    add_quad(part, pose, top, u + d, v, w, d, m, 1.0f, texture);
    add_quad(part, pose, under, u + d + w, v, w, d, m, 0.6f, texture);
}

static int by_depth(const void *a, const void *b)
{
    float x = ((const Quad *)a)->depth, y = ((const Quad *)b)->depth;
    return x > y ? -1 : x < y ? 1 : 0;
}

void playermodel_draw(Vector2 feet, float scale, const BodyPose *pose, const char *skin, const char *const *worn, int worn_count)
{
    /* Head, body, the arm and the leg on the near side, and the ones on the far side. */
    Part parts[6] = {
        { { 0, 24, 0 }, { -4, 0, -4 }, { 8, 8, 8 }, 0, 0, false, true, pose->head },
        { { 0, 12, 0 }, { -4, 0, -2 }, { 8, 12, 4 }, 16, 16, false, true, 0.0f },
        { { -6, 22, 0 }, { -2, -10, -2 }, { 4, 12, 4 }, 40, 16, false, true, pose->arm[0] },
        { { 6, 22, 0 }, { -2, -10, -2 }, { 4, 12, 4 }, 40, 16, true, true, pose->arm[1] },
        { { -2, 12, 0 }, { -2, -12, -2 }, { 4, 12, 4 }, 0, 16, false, false, pose->leg[0] },
        { { 2, 12, 0 }, { -2, -12, -2 }, { 4, 12, 4 }, 0, 16, true, false, pose->leg[1] },
    };
    /* (Facing left it is the other arm and leg that are nearer.) */
    if (pose->facing < 0) {
        float swap = parts[2].swing;
        parts[2].swing = parts[3].swing, parts[3].swing = swap;
        swap = parts[4].swing;
        parts[4].swing = parts[5].swing, parts[5].swing = swap;
    }
    quad_count = 0;
    Texture2D body = assets_texture(skin);
    for (int i = 0; i < 6 && body.id; i++) add_part(&parts[i], pose, 0.0f, body.id);
    for (int k = 0; k < worn_count; k++) {
        Texture2D over = worn[k] && worn[k][0] ? assets_texture(worn[k]) : (Texture2D){ 0 };
        /* (Each thing worn a little larger than the last, so that one goes over another.) */
        for (int i = 0; i < 6 && over.id; i++) add_part(&parts[i], pose, 0.45f + 0.2f * (float)k, over.id);
    }
    qsort(quads, (size_t)quad_count, sizeof quads[0], by_depth);
    for (int i = 0; i < quad_count; i++) {
        const Quad *q = &quads[i];
        Color c = { (unsigned char)((float)pose->tint.r * q->shade), (unsigned char)((float)pose->tint.g * q->shade),
                    (unsigned char)((float)pose->tint.b * q->shade), pose->tint.a };
        rlSetTexture(q->texture);
        rlBegin(RL_QUADS);
        rlColor4ub(c.r, c.g, c.b, c.a);
        rlNormal3f(0.0f, 0.0f, 1.0f);
        for (int k = 0; k < 4; k++) {
            rlTexCoord2f(q->uv[k].x, q->uv[k].y);
            /* (The viewer is on the side the body's z runs away from: nearer is less of it.) */
            if (deep) rlVertex3f(feet.x + q->p[k].x * scale, feet.y + q->p[k].y * scale, deep_z - q->z[k] * scale);
            else rlVertex2f(feet.x + q->p[k].x * scale, feet.y + q->p[k].y * scale);
        }
        rlEnd();
        rlSetTexture(0);
    }
}
