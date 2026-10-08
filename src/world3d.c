#include "world3d.h"

#include "rlgl.h"

#include <math.h>
#include <stdlib.h>

#define FOVY 40.0f       /* how wide the eye sees, top to bottom, in degrees: the wider, the nearer it is and the stronger the depth */
#define UNIT 32.0f       /* the scene is handed to the camera in blocks, not pixels, to keep within what it can see to */
#define MAX_TEXTURES 8192
#define SIDE_SHADE 0.80f /* how bright the sides of things are beside their faces */
#define TOP_SHADE 0.92f
#define UNDER_SHADE 0.62f

typedef struct {
    short a, b, at, of; /* a run of edge from a to b, on line `at`, coloured as texel row or column `of` */
    bool upright;       /* it runs up and down (between two columns), not across */
    bool low;           /* it faces down, or right: for how it is lit */
} Edge;

/* What is known of a texture's shape: whether it fills its square, how much of it is there, and where its edges run. */
/* A part of a picture: so many rows of one stretch across it. */
typedef struct {
    short x0, x1, y0, y1;
} Part;

#define MAX_PARTS 200

typedef struct {
    bool known, full;
    float cover;   /* how much of it is there */
    float branchy; /* how many of its rows are in more than one piece */
    int widest;    /* the longest stretch across it, in texels */
    int count;
    Edge *edges;
    int part_count; /* 0 if it is too much in bits to be built of parts */
    Part *parts;
} Shape;

static Shape shapes[MAX_TEXTURES];
static bool active;
static double near_was, far_was;
static Vector2 center;
static Shader cutout;
static bool cutout_tried;
static int mode; /* 0 flat, 1 plane, 2 solid */
static float mode_front, mode_back;
static int mode_sides;
static World3dModel mode_model;

static const char *cutout_fs =
    "#version 330\n"
    "in vec2 fragTexCoord; in vec4 fragColor; uniform sampler2D texture0; uniform vec4 colDiffuse; out vec4 finalColor;\n"
    "void main() { vec4 c = texture(texture0, fragTexCoord) * colDiffuse * fragColor; if (c.a < 0.02) discard; finalColor = c; }\n";

bool world3d_active(void)
{
    return active;
}

Vector2 world3d_center(void)
{
    return center;
}

static float eye_distance(float zoom)
{
    return ((float)GetScreenHeight() / zoom / 2.0f) / tanf(FOVY * DEG2RAD / 2.0f);
}

Vector2 world3d_at_depth(Vector2 screen, float zoom, float z)
{
    Vector2 mid = { (float)GetScreenWidth() / zoom / 2.0f, (float)GetScreenHeight() / zoom / 2.0f };
    float d = eye_distance(zoom), k = (d - z) / d;
    return (Vector2){ mid.x + (screen.x / zoom - mid.x) * k, mid.y + (screen.y / zoom - mid.y) * k };
}

void world3d_begin(float zoom)
{
    if (!cutout_tried) {
        /* (What is clear in a picture must leave no mark on what is behind it.) */
        cutout_tried = true;
        cutout = LoadShaderFromMemory(NULL, cutout_fs);
    }
    center = (Vector2){ (float)GetScreenWidth() / zoom / 2.0f, (float)GetScreenHeight() / zoom / 2.0f };
    float d = eye_distance(zoom);
    Camera3D eye = { .position = { center.x / UNIT, -center.y / UNIT, d / UNIT }, .target = { center.x / UNIT, -center.y / UNIT, 0.0f },
                     .up = { 0.0f, 1.0f, 0.0f }, .fovy = FOVY, .projection = CAMERA_PERSPECTIVE };
    /* (It sees no nearer than a little in front of the scene and no further than a little behind it: the narrower
     * that is, the finer it can tell one depth from another, however far off the eye is.) */
    near_was = rlGetCullDistanceNear(), far_was = rlGetCullDistanceFar();
    rlSetClipPlanes(d / UNIT > 4.5 ? d / UNIT - 4.0 : 0.5, d / UNIT + 8.0);
    BeginMode3D(eye);
    if (cutout.id > 0) BeginShaderMode(cutout);
    rlDisableBackfaceCulling();
    /* The flat picture has its y running down and is in pixels: turned over and scaled, it is the front of the scene. */
    rlPushMatrix();
    rlScalef(1.0f / UNIT, -1.0f / UNIT, 1.0f / UNIT);
    active = true;
    mode = 0;
}

void world3d_end(void)
{
    if (!active) return;
    active = false;
    mode = 0;
    rlPopMatrix();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    if (cutout.id > 0) EndShaderMode();
    EndMode3D();
    rlSetClipPlanes(near_was, far_was);
}

void world3d_flat(void)
{
    mode = 0;
}

void world3d_plane(float z)
{
    mode = 1;
    mode_front = z;
}

void world3d_solid(float front, float back, int sides, World3dModel model)
{
    mode = 2;
    mode_front = front;
    mode_back = back;
    mode_sides = sides;
    mode_model = model;
}

void world3d_push(float z)
{
    rlPushMatrix();
    rlTranslatef(0.0f, 0.0f, z);
}

void world3d_pop(void)
{
    rlPopMatrix();
}

/* Reads a texture back to see what shape is drawn on it. */
static const Shape *shape_of(Texture2D tex)
{
    static const Shape none = { .known = true, .full = true, .cover = 1.0f };
    if (tex.id == 0 || tex.id >= MAX_TEXTURES || tex.width > 512 || tex.height > 512) return &none;
    Shape *s = &shapes[tex.id];
    if (s->known) return s;
    s->known = true;
    s->full = true;
    s->cover = 1.0f;
    Image img = LoadImageFromTexture(tex);
    if (!img.data) return s;
    Color *px = LoadImageColors(img);
    int w = img.width, h = img.height, there = 0;
    #define SOLID(X, Y) ((X) >= 0 && (Y) >= 0 && (X) < w && (Y) < h && px[(Y) * w + (X)].a >= 128)
    for (int i = 0; i < w * h; i++) there += px[i].a >= 128;
    s->full = there == w * h;
    s->cover = w * h > 0 ? (float)there / (float)(w * h) : 1.0f;
    /* Its edges: wherever something is beside nothing, in runs as long as they go. */
    for (int pass = 0; pass < 2 && !s->full; pass++) {
        /* (First to count them, then to keep them.) */
        int n = 0;
        for (int x = 0; x <= w; x++) {
            for (int y = 0; y < h;) {
                bool in = SOLID(x, y), out = SOLID(x - 1, y);
                if (in == out) {
                    y++;
                    continue;
                }
                int from = y;
                while (y < h && SOLID(x, y) == in && SOLID(x - 1, y) == out) y++;
                if (pass == 1) s->edges[n] = (Edge){ (short)from, (short)y, (short)x, (short)(in ? x : x - 1), true, out };
                n++;
            }
        }
        for (int y = 0; y <= h; y++) {
            for (int x = 0; x < w;) {
                bool in = SOLID(x, y), out = SOLID(x, y - 1);
                if (in == out) {
                    x++;
                    continue;
                }
                int from = x;
                while (x < w && SOLID(x, y) == in && SOLID(x, y - 1) == out) x++;
                if (pass == 1) s->edges[n] = (Edge){ (short)from, (short)x, (short)y, (short)(in ? y : y - 1), false, out };
                n++;
            }
        }
        if (pass == 0) {
            s->edges = malloc(sizeof(Edge) * (size_t)(n > 0 ? n : 1));
            if (!s->edges) break;
        } else {
            s->count = n;
        }
    }
    /* Its parts: each row is so many stretches across; rows that are the same stretches go together. */
    if (!s->full) {
        Part *parts = malloc(sizeof(Part) * MAX_PARTS);
        int n = 0, rows = 0, split = 0, open_from = 0, open_count = 0;
        bool fits = parts != NULL;
        for (int y = 0; y < h && fits; y++) {
            short runs[64][2];
            int count = 0;
            for (int x = 0; x < w;) {
                if (!SOLID(x, y)) {
                    x++;
                    continue;
                }
                int from = x;
                while (x < w && SOLID(x, y)) x++;
                if (count < 64) runs[count][0] = (short)from, runs[count++][1] = (short)x;
                if (x - from > s->widest) s->widest = x - from;
            }
            rows += count > 0;
            split += count > 1;
            /* The same as the row above: those parts are a row taller. */
            bool same = count == open_count && count > 0;
            for (int k = 0; same && k < count; k++) same = parts[open_from + k].x0 == runs[k][0] && parts[open_from + k].x1 == runs[k][1];
            if (same) {
                for (int k = 0; k < count; k++) parts[open_from + k].y1 = (short)(y + 1);
                continue;
            }
            if (n + count > MAX_PARTS) {
                fits = false;
                break;
            }
            open_from = n, open_count = count;
            for (int k = 0; k < count; k++) parts[n++] = (Part){ runs[k][0], runs[k][1], (short)y, (short)(y + 1) };
        }
        s->branchy = rows > 0 ? (float)split / (float)rows : 0.0f;
        if (fits && n > 0) s->parts = parts, s->part_count = n;
        else free(parts);
    }
    #undef SOLID
    UnloadImageColors(px);
    UnloadImage(img);
    return s;
}

static Color shaded(Color c, float by)
{
    return (Color){ (unsigned char)((float)c.r * by), (unsigned char)((float)c.g * by), (unsigned char)((float)c.b * by), c.a };
}

typedef struct {
    float x, y, z, u, v;
} Corner;

static void quad(Texture2D tex, Corner a, Corner b, Corner c, Corner d, Color tint)
{
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    rlNormal3f(0.0f, 0.0f, 1.0f);
    rlTexCoord2f(a.u, a.v);
    rlVertex3f(a.x, a.y, a.z);
    rlTexCoord2f(b.u, b.v);
    rlVertex3f(b.x, b.y, b.z);
    rlTexCoord2f(c.u, c.v);
    rlVertex3f(c.x, c.y, c.z);
    rlTexCoord2f(d.u, d.v);
    rlVertex3f(d.x, d.y, d.z);
    rlEnd();
    rlSetTexture(0);
}

void world3d_texture(Texture2D tex, Rectangle src, Rectangle dst, Color tint)
{
    if (!active || mode == 0 || tex.id == 0) {
        DrawTexturePro(tex, src, dst, (Vector2){ 0, 0 }, 0.0f, tint);
        return;
    }
    bool flip = src.width < 0.0f;
    float sw = fabsf(src.width), u0 = src.x / (float)tex.width, u1 = (src.x + sw) / (float)tex.width;
    float v0 = src.y / (float)tex.height, v1 = (src.y + src.height) / (float)tex.height;
    if (flip) {
        float swap = u0;
        u0 = u1;
        u1 = swap;
    }
    float x0 = dst.x, x1 = dst.x + dst.width, y0 = dst.y, y1 = dst.y + dst.height;
    float front = mode_front, back = mode == 1 ? mode_front : mode_back;
    const Shape *shape = mode == 2 ? shape_of(tex) : NULL;
    bool whole = fabsf(sw - (float)tex.width) < 0.5f && fabsf(src.height - (float)tex.height) < 0.5f && src.x == 0.0f && src.y == 0.0f;
    World3dModel model = mode == 2 ? mode_model : W3D_PANEL;
    if (mode == 2 && model == W3D_AUTO) model = shape->full ? W3D_BLOCK : shape->cover < 0.5f && shape->branchy > 0.25f ? W3D_CROSS : W3D_FRAME;
    if (mode == 2 && !shape->full && (model == W3D_FRAME || model == W3D_BODY || model == W3D_MASS) && (shape->part_count == 0 || !whole)) model = model == W3D_MASS ? W3D_BLOCK : W3D_PANEL;
    if (mode == 2 && shape->full && model != W3D_BOX) model = W3D_BLOCK;
    if (mode == 2 && model == W3D_BOX && !shape->full) model = whole && shape->part_count > 0 ? W3D_FRAME : W3D_PANEL;
    float px = dst.width / (float)tex.width, py = dst.height / (float)tex.height;

    if (mode == 2 && model == W3D_CROSS) {
        /* A plant: the picture twice, crossed about its middle. */
        float mid = (front + back) / 2.0f, reach = fminf(dst.width / 2.0f, (front - back) / 2.0f) * 0.9f, xm = (x0 + x1) / 2.0f, half = dst.width / 2.0f * 0.72f;
        quad(tex, (Corner){ xm - half, y0, mid + reach, u0, v0 }, (Corner){ xm - half, y1, mid + reach, u0, v1 }, (Corner){ xm + half, y1, mid - reach, u1, v1 }, (Corner){ xm + half, y0, mid - reach, u1, v0 }, tint);
        quad(tex, (Corner){ xm - half, y0, mid - reach, u0, v0 }, (Corner){ xm - half, y1, mid - reach, u0, v1 }, (Corner){ xm + half, y1, mid + reach, u1, v1 }, (Corner){ xm + half, y0, mid + reach, u1, v0 }, shaded(tint, SIDE_SHADE + 0.1f));
        return;
    }
    if (mode == 2 && (model == W3D_FRAME || model == W3D_BODY || model == W3D_MASS)) {
        /* A built thing: a box for every part of the picture, as deep as the part is wide - but none deeper than
         * the whole of it, which is as deep as its widest part, or as there is room for. */
        float room = front - back, natural = (float)shape->widest * px, whole_deep = model == W3D_MASS ? room : fminf(natural, room), mid = (front + back) / 2.0f;
        float stretch = model == W3D_MASS && natural > 0.0f ? room / natural : 1.0f;
        for (int i = 0; i < shape->part_count; i++) {
            const Part *p = &shape->parts[i];
            float wide = (float)(p->x1 - p->x0) * px, deep = fminf(wide * stretch, whole_deep);
            float xa = flip ? x1 - (float)p->x1 * px : x0 + (float)p->x0 * px, xb = xa + wide, ya = y0 + (float)p->y0 * py, yb = y0 + (float)p->y1 * py;
            float ua = (float)p->x0 / (float)tex.width, ub = (float)p->x1 / (float)tex.width, va = (float)p->y0 / (float)tex.height, vb = (float)p->y1 / (float)tex.height;
            if (flip) {
                float swap = ua;
                ua = ub;
                ub = swap;
            }
            /* (The colour of each side is the colour of the picture along that edge of the part.) */
            float ul = ((float)(flip ? p->x1 - 1 : p->x0) + 0.5f) / (float)tex.width, ur = ((float)(flip ? p->x0 : p->x1 - 1) + 0.5f) / (float)tex.width;
            float vt = ((float)p->y0 + 0.5f) / (float)tex.height, vu = ((float)p->y1 - 0.5f) / (float)tex.height;
            /* Something much thinner than the whole stands at the front of it and again at the back: legs, wheels. */
            bool twice = model == W3D_FRAME && deep < whole_deep * 0.4f && whole_deep > 0.0f;
            for (int copy = 0; copy < (twice ? 2 : 1); copy++) {
                float zf = twice ? (copy == 0 ? mid + whole_deep / 2.0f : mid - whole_deep / 2.0f + deep) : mid + deep / 2.0f, zb = zf - deep;
                quad(tex, (Corner){ xa, ya, zf, ua, va }, (Corner){ xa, yb, zf, ua, vb }, (Corner){ xb, yb, zf, ub, vb }, (Corner){ xb, ya, zf, ub, va }, tint);
                if (xa > center.x)
                    quad(tex, (Corner){ xa, ya, zb, ul, va }, (Corner){ xa, yb, zb, ul, vb }, (Corner){ xa, yb, zf, ul, vb }, (Corner){ xa, ya, zf, ul, va }, shaded(tint, SIDE_SHADE));
                if (xb < center.x)
                    quad(tex, (Corner){ xb, ya, zf, ur, va }, (Corner){ xb, yb, zf, ur, vb }, (Corner){ xb, yb, zb, ur, vb }, (Corner){ xb, ya, zb, ur, va }, shaded(tint, SIDE_SHADE));
                if (ya > center.y)
                    quad(tex, (Corner){ xa, ya, zb, ua, vt }, (Corner){ xa, ya, zf, ua, vt }, (Corner){ xb, ya, zf, ub, vt }, (Corner){ xb, ya, zb, ub, vt }, shaded(tint, TOP_SHADE));
                if (yb < center.y)
                    quad(tex, (Corner){ xa, yb, zf, ua, vu }, (Corner){ xa, yb, zb, ua, vu }, (Corner){ xb, yb, zb, ub, vu }, (Corner){ xb, yb, zf, ub, vu }, shaded(tint, UNDER_SHADE));
            }
        }
        return;
    }
    if (mode == 2 && model == W3D_PANEL) {
        /* Flat, with a little thickness, in the middle of the room it has. */
        float mid = (front + back) / 2.0f, half = fminf((front - back) / 2.0f, 2.0f * px);
        front = mid + half, back = mid - half;
    }
    /* The face of it. */
    quad(tex, (Corner){ x0, y0, front, u0, v0 }, (Corner){ x0, y1, front, u0, v1 }, (Corner){ x1, y1, front, u1, v1 }, (Corner){ x1, y0, front, u1, v0 }, tint);
    if (mode != 2 || front <= back) return;
    if (shape->full && model == W3D_BOX) {
        /* One thing: its sides plain, each the colour the picture is along that edge. */
        float bu = (u1 - u0) * 0.07f, bv = (v1 - v0) * 0.07f;
        if (mode_sides & W3D_LEFT)
            quad(tex, (Corner){ x0, y0, back, u0, v0 }, (Corner){ x0, y1, back, u0, v1 }, (Corner){ x0, y1, front, u0 + bu, v1 }, (Corner){ x0, y0, front, u0 + bu, v0 }, shaded(tint, SIDE_SHADE));
        if (mode_sides & W3D_RIGHT)
            quad(tex, (Corner){ x1, y0, front, u1 - bu, v0 }, (Corner){ x1, y1, front, u1 - bu, v1 }, (Corner){ x1, y1, back, u1, v1 }, (Corner){ x1, y0, back, u1, v0 }, shaded(tint, SIDE_SHADE));
        if (mode_sides & W3D_TOP)
            quad(tex, (Corner){ x0, y0, back, u0, v0 }, (Corner){ x0, y0, front, u0, v0 + bv }, (Corner){ x1, y0, front, u1, v0 + bv }, (Corner){ x1, y0, back, u1, v0 }, shaded(tint, TOP_SHADE));
        if (mode_sides & W3D_BOTTOM)
            quad(tex, (Corner){ x0, y1, front, u0, v1 - bv }, (Corner){ x0, y1, back, u0, v1 }, (Corner){ x1, y1, back, u1, v1 }, (Corner){ x1, y1, front, u1, v1 - bv }, shaded(tint, UNDER_SHADE));
        return;
    }
    if (shape->full) {
        /* Material: its sides, a square of the picture to each square of depth. */
        float deep = dst.width > 0.0f ? dst.width : front - back;
        for (float z = front; z > back + 0.01f; z -= deep) {
            float far = z - deep < back ? back : z - deep;
            if (mode_sides & W3D_LEFT)
                quad(tex, (Corner){ x0, y0, far, u0, v0 }, (Corner){ x0, y1, far, u0, v1 }, (Corner){ x0, y1, z, u1, v1 }, (Corner){ x0, y0, z, u1, v0 }, shaded(tint, SIDE_SHADE));
            if (mode_sides & W3D_RIGHT)
                quad(tex, (Corner){ x1, y0, z, u0, v0 }, (Corner){ x1, y1, z, u0, v1 }, (Corner){ x1, y1, far, u1, v1 }, (Corner){ x1, y0, far, u1, v0 }, shaded(tint, SIDE_SHADE));
            /* (The top of a block is what its picture has along its top, and the underside what it has along its foot:
             * so that grass is green on top, and the earth under it shows beneath.) */
            float band = (v1 - v0) * 0.19f;
            if (mode_sides & W3D_TOP)
                quad(tex, (Corner){ x0, y0, far, u0, v0 }, (Corner){ x0, y0, z, u0, v0 + band }, (Corner){ x1, y0, z, u1, v0 + band }, (Corner){ x1, y0, far, u1, v0 }, shaded(tint, TOP_SHADE));
            if (mode_sides & W3D_BOTTOM)
                quad(tex, (Corner){ x0, y1, z, u0, v1 - band }, (Corner){ x0, y1, far, u0, v1 }, (Corner){ x1, y1, far, u1, v1 }, (Corner){ x1, y1, z, u1, v1 - band }, shaded(tint, UNDER_SHADE));
        }
        return;
    }
    /* A picture with thickness: the same again at the back of it, and its edges joined between. */
    quad(tex, (Corner){ x0, y0, back, u0, v0 }, (Corner){ x0, y1, back, u0, v1 }, (Corner){ x1, y1, back, u1, v1 }, (Corner){ x1, y0, back, u1, v0 }, shaded(tint, UNDER_SHADE));
    if (!whole) return;
    for (int i = 0; i < shape->count; i++) {
        const Edge *e = &shape->edges[i];
        if (e->upright) {
            float x = flip ? x1 - (float)e->at * px : x0 + (float)e->at * px, ya = y0 + (float)e->a * py, yb = y0 + (float)e->b * py;
            float u = ((float)e->of + 0.5f) / (float)tex.width, va = (float)e->a / (float)tex.height, vb = (float)e->b / (float)tex.height;
            /* (Only the side that is turned toward the middle of the view can be seen.) */
            bool faces_left = e->low == flip;
            if (faces_left ? x < center.x : x > center.x) continue;
            quad(tex, (Corner){ x, ya, front, u, va }, (Corner){ x, yb, front, u, vb }, (Corner){ x, yb, back, u, vb }, (Corner){ x, ya, back, u, va }, shaded(tint, SIDE_SHADE));
        } else {
            float y = y0 + (float)e->at * py, xa = flip ? x1 - (float)e->a * px : x0 + (float)e->a * px, xb = flip ? x1 - (float)e->b * px : x0 + (float)e->b * px;
            float v = ((float)e->of + 0.5f) / (float)tex.height, ua = (float)e->a / (float)tex.width, ub = (float)e->b / (float)tex.width;
            bool faces_up = !e->low;
            if (faces_up ? y < center.y : y > center.y) continue;
            quad(tex, (Corner){ xa, y, front, ua, v }, (Corner){ xb, y, front, ub, v }, (Corner){ xb, y, back, ub, v }, (Corner){ xa, y, back, ua, v }, shaded(tint, faces_up ? TOP_SHADE : UNDER_SHADE));
        }
    }
}
