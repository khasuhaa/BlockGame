#include "mobmodel.h"

#include "assets.h"
#include "rlgl.h"

#include <math.h>
#include <stdlib.h>

#define MAX_QUADS (MAX_MODEL_PARTS * 6)

typedef struct {
    float x, y, z;
} V3;

typedef struct {
    Vector2 p[4], uv[4];
    float z[4];
    float depth, shade;
} Quad;

static Quad quads[MAX_QUADS];
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

static V3 rot_z(V3 v, float degrees)
{
    float a = degrees * DEG2RAD, c = cosf(a), s = sinf(a);
    return (V3){ v.x * c - v.y * s, v.x * s + v.y * c, v.z };
}

/* How far a part is turned just now, and about which way: by what kind of moving it does. */
static float part_angle(const ModelPart *part, bool walking, float stride, float time, int *axis)
{
    float step = walking ? sinf(stride * 2.0f * PI) : 0.0f;
    *axis = 0;
    switch (part->move) {
    case MODEL_LEG: return part->by * step;
    case MODEL_LEG_OTHER: return -part->by * step;
    case MODEL_WING: *axis = 2; return part->by * sinf(time * 9.0f + part->lag);
    case MODEL_WING_OTHER: *axis = 2; return -part->by * sinf(time * 9.0f + part->lag);
    case MODEL_TAIL: *axis = 1; return part->by * sinf(time * 3.0f + part->lag);
    case MODEL_HEAD: return part->by * (walking ? sinf(stride * 4.0f * PI) : sinf(time * 1.3f + part->lag) * 0.5f);
    case MODEL_WAVE: *axis = 1; return part->by * sinf(time * (walking ? 6.0f : 3.0f) + part->lag);
    case MODEL_SWAY: *axis = 2; return part->by * sinf(time * 2.0f + part->lag);
    default: return 0.0f;
    }
}

static V3 place(const ModelPart *part, V3 local, int axis, float angle, float lift, int facing, float turn)
{
    V3 v = axis == 1 ? rot_y(local, angle) : axis == 2 ? rot_z(local, angle) : rot_x(local, -angle);
    v.x += part->pivot[0], v.y += part->pivot[1] + lift, v.z += part->pivot[2];
    /* (The viewer is on the -z side: it goes along the world side on, turned a little toward whoever looks.) */
    return rot_y(v, (float)facing * (90.0f + turn));
}

static void add_quad(const ModelPart *part, const V3 corner[4], float u, float v, float w, float h, bool flip, float shade, float atlas_w, float atlas_h,
                     int axis, float angle, float lift, int facing, float turn)
{
    if (quad_count >= MAX_QUADS) return;
    Quad *q = &quads[quad_count];
    float depth = 0.0f;
    for (int i = 0; i < 4; i++) {
        V3 at = place(part, corner[i], axis, angle, lift, facing, turn);
        q->p[i] = (Vector2){ at.x, -at.y };
        q->z[i] = at.z;
        depth += at.z;
    }
    float u0 = u / atlas_w, u1 = (u + w) / atlas_w, v0 = v / atlas_h, v1 = (v + h) / atlas_h;
    if (flip) {
        float swap = u0;
        u0 = u1;
        u1 = swap;
    }
    q->uv[0] = (Vector2){ u0, v0 };
    q->uv[1] = (Vector2){ u0, v1 };
    q->uv[2] = (Vector2){ u1, v1 };
    q->uv[3] = (Vector2){ u1, v0 };
    q->depth = depth / 4.0f;
    q->shade = shade;
    quad_count++;
}

static int by_depth(const void *a, const void *b)
{
    float x = ((const Quad *)a)->depth, y = ((const Quad *)b)->depth;
    return x > y ? -1 : x < y ? 1 : 0;
}

bool mobmodel_draw(const MobDef *def, Vector2 feet, float scale, int facing, bool walking, float stride, float time, bool deep, float z, Color tint)
{
    if (def->model_part_count <= 0 || !def->model_skin[0]) return false;
    Texture2D skin = assets_texture(def->model_skin);
    if (!skin.id) return false;
    float aw = def->model_w > 0 ? (float)def->model_w : (float)skin.width, ah = def->model_h > 0 ? (float)def->model_h : (float)skin.height;
    /* (What floats rides up and down a little all the while; what walks lifts a little with each stride.) */
    float lift = def->model_bob * sinf(time * 2.2f) + (walking ? def->model_hop * fabsf(sinf(stride * 2.0f * PI)) : 0.0f);
    quad_count = 0;
    for (int i = 0; i < def->model_part_count; i++) {
        const ModelPart *part = &def->model_parts[i];
        int axis;
        float angle = part_angle(part, walking, stride, time, &axis);
        float x0 = part->corner[0], y0 = part->corner[1], z0 = part->corner[2];
        float x1 = x0 + part->size[0], y1 = y0 + part->size[1], z1 = z0 + part->size[2];
        float w = part->size[0], h = part->size[1], d = part->size[2], u = (float)part->u, v = (float)part->v;
        bool m = part->mirror;
        /* (+z is the front of it, +x its left, +y up: the same unfolding as the character's skin.) */
        V3 front[4] = { { x1, y1, z1 }, { x1, y0, z1 }, { x0, y0, z1 }, { x0, y1, z1 } };
        V3 back[4] = { { x0, y1, z0 }, { x0, y0, z0 }, { x1, y0, z0 }, { x1, y1, z0 } };
        V3 left[4] = { { x1, y1, z0 }, { x1, y0, z0 }, { x1, y0, z1 }, { x1, y1, z1 } };
        V3 right[4] = { { x0, y1, z1 }, { x0, y0, z1 }, { x0, y0, z0 }, { x0, y1, z0 } };
        V3 top[4] = { { x1, y1, z0 }, { x1, y1, z1 }, { x0, y1, z1 }, { x0, y1, z0 } };
        V3 under[4] = { { x1, y0, z1 }, { x1, y0, z0 }, { x0, y0, z0 }, { x0, y0, z1 } };
        add_quad(part, front, u + d, v + d, w, h, m, 0.94f, aw, ah, axis, angle, lift, facing, def->model_turn);
        add_quad(part, back, u + d + w + d, v + d, w, h, m, 0.72f, aw, ah, axis, angle, lift, facing, def->model_turn);
        add_quad(part, left, m ? u : u + d + w, v + d, d, h, m, 0.86f, aw, ah, axis, angle, lift, facing, def->model_turn);
        add_quad(part, right, m ? u + d + w : u, v + d, d, h, m, 0.86f, aw, ah, axis, angle, lift, facing, def->model_turn);
        add_quad(part, top, u + d, v, w, d, m, 1.0f, aw, ah, axis, angle, lift, facing, def->model_turn);
        add_quad(part, under, u + d + w, v, w, d, m, 0.6f, aw, ah, axis, angle, lift, facing, def->model_turn);
    }
    /* (Flat, the far faces go down first; in depth the depth test sees to it.) */
    if (!deep) qsort(quads, (size_t)quad_count, sizeof quads[0], by_depth);
    rlSetTexture(skin.id);
    rlBegin(RL_QUADS);
    for (int i = 0; i < quad_count; i++) {
        const Quad *q = &quads[i];
        rlColor4ub((unsigned char)((float)tint.r * q->shade), (unsigned char)((float)tint.g * q->shade), (unsigned char)((float)tint.b * q->shade), tint.a);
        rlNormal3f(0.0f, 0.0f, 1.0f);
        for (int k = 0; k < 4; k++) {
            rlTexCoord2f(q->uv[k].x, q->uv[k].y);
            if (deep) rlVertex3f(feet.x + q->p[k].x * scale, feet.y + q->p[k].y * scale, z - q->z[k] * scale);
            else rlVertex2f(feet.x + q->p[k].x * scale, feet.y + q->p[k].y * scale);
        }
    }
    rlEnd();
    rlSetTexture(0);
    return true;
}
