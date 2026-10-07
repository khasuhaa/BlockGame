#include "assets.h"
#include "benchui.h"
#include "game.h"
#include "hud.h"
#include "light.h"
#include "menu.h"
#include "mob.h"
#include "playerui.h"
#include "power.h"
#include "raylib.h"
#include "rlgl.h"
#include "save.h"
#include "tradeui.h"
#include "view.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* How much of a sign's two blocks its writing may take up, and the size it is written at. */
#define SIGN_TEXT_SIZE 10
#define SIGN_TEXT_WIDTH (2 * TILE_SIZE - 6)

static void sign_open(const Game *g, TilePos at);

#define DATA_DIR "data"
#define PROFILE_PATH SAVE_DIR "/profile.txt"
#define TILE_SIZE 32
#define DRAG_THRESHOLD 6.0f /* pixels the mouse must travel before a press becomes a pan */
#define CAMERA_FOLLOW_SPEED 8.0f
#define AUTOSAVE_SECONDS 120.0f
#define BACKGROUND_DIM 0.18f /* how much darker the background layer is drawn */
#define WALL_DIM 0.5f        /* and the backwall behind that */
#define DROP_WARNING 10.0f   /* seconds before it vanishes that a dropped item starts to blink */
#define MOB_GLIDE 14.0f      /* how closely a creature's picture keeps up with where it is */
#define ZOOM_SPEED 10.0f     /* how fast the view eases to a new zoom */
#define MAP_REFRESH 0.5f     /* seconds between redrawings of the world map */

/* How big a block is drawn, in pixels, at each step of the mouse wheel, and
 * where the view fades into the world map, are in the rules. */
#define ZOOM_LEVELS (content.rules.zoom_count)
#define ZOOM_DEFAULT (content.rules.zoom_start)
#define MAP_FADE_FROM (content.rules.map_from)
#define MAP_FADE_TO (content.rules.map_to)

static float zoom_step(int level)
{
    if (ZOOM_LEVELS == 0) return (float)TILE_SIZE; /* no steps in the data: no zooming */
    return content.rules.zoom[level < 0 ? 0 : level >= ZOOM_LEVELS ? ZOOM_LEVELS - 1 : level];
}

/* World-space pixel shown at the centre of the window. */
typedef struct {
    Vector2 center;
    bool follow; /* track the player until the map is dragged */
    bool pressed, dragging;
    Vector2 press_mouse, press_center;
    /* Zooming holds still whatever was under the cursor when the wheel was
     * turned: that point of the world, and where on the screen it was. */
    bool anchored;
    Vector2 anchor_world, anchor_screen;
    int zoom_level; /* which of zoom_levels the wheel has asked for */
    float scale;    /* pixels per block being drawn now, easing toward that */
} MapCamera;

/* Refreshed from the camera at the start of each frame. The world is drawn
 * as if a block were always TILE_SIZE pixels, onto a screen `seen` in size,
 * and the whole picture is then scaled by `zoom`. `view` is that unscaled
 * layout; `ui_view` is the same thing on the real screen, for the interface
 * that is pinned to places in the world but not scaled with it. */
static View view, ui_view;
static float zoom = 1.0f;
static Vector2 seen;

static void update_view(const MapCamera *cam, const World *w)
{
    zoom = (cam->scale > 0.0f ? cam->scale : (float)TILE_SIZE) / TILE_SIZE;
    seen = (Vector2){ GetScreenWidth() / zoom, GetScreenHeight() / zoom };
    /* Snapped to whole pixels of the screen, so the blocks stay crisp and
     * nothing shimmers against them as the camera glides. */
    view.origin = (Vector2){ floorf((seen.x / 2.0f - cam->center.x) * zoom) / zoom,
                             floorf((seen.y / 2.0f - cam->center.y) * zoom) / zoom };
    view.tile_size = TILE_SIZE;
    view.center_x = cam->center.x / TILE_SIZE;
    view.world_width = w->width;
    ui_view = view;
    ui_view.origin = (Vector2){ view.origin.x * zoom, view.origin.y * zoom };
    ui_view.tile_size = TILE_SIZE * zoom;
}

static Vector2 view_origin(const MapCamera *cam)
{
    (void)cam;
    return view.origin;
}

/* The tile under a screen point. Its x is not wrapped into the world. */
static TilePos tile_at_screen(const MapCamera *cam, Vector2 screen)
{
    Vector2 origin = view_origin(cam);
    return (TilePos){ (int)floorf((screen.x / zoom - origin.x) / TILE_SIZE),
                      (int)floorf((screen.y / zoom - origin.y) / TILE_SIZE) };
}

static Rectangle tile_rect(Vector2 origin, TilePos t)
{
    (void)origin;
    Vector2 p = view_tile_pos(&view, (float)t.x, (float)t.y);
    return (Rectangle){ p.x, p.y, TILE_SIZE, TILE_SIZE };
}

/* World-space pixel at the middle of the player, on the copy of the world
 * nearest the camera. */
static Vector2 player_center(const Game *g)
{
    float x, y;
    game_player_visual_pos(g, &x, &y);
    return (Vector2){ (view_near_x(&view, x) + 0.5f) * TILE_SIZE, y * TILE_SIZE };
}

static void sync_storage_slots(const Game *g, Hud *hud)
{
    Rectangle rects[HUD_MAX_EXTRA_SLOTS];
    uint16_t container;
    hud->extra_count = benchui_storage_slots(g, &ui_view, rects, HUD_MAX_EXTRA_SLOTS, &container);
    for (int i = 0; i < hud->extra_count; i++)
        hud->extra[i] = (HudSlot){ rects[i], { SLOT_CONTAINER, container, i } };
}

/* The whole world, a pixel to a block, redrawn every so often while it shows. */
#define MAX_MAP_PORTALS 128
#define MAP_ICON_REACH 16.0f /* pixels round an icon on the map that count as a click on it */

static struct {
    Texture2D texture;
    Color *pixels;
    int width, height;
    float age;
    /* The portals the player has uncovered, each by the lowest of its blocks. */
    TilePos portals[MAX_MAP_PORTALS];
    int portal_count;
} world_map;

/* Where a point of the world is on the screen while the map is up. */
static Vector2 map_point(const MapCamera *cam, float x, float y)
{
    float cx = cam->center.x / TILE_SIZE, cy = cam->center.y / TILE_SIZE;
    return (Vector2){ GetScreenWidth() / 2.0f + (view_near_x(&view, x) - cx) * cam->scale,
                      GetScreenHeight() / 2.0f + (y - cy) * cam->scale };
}

/* The portal whose icon on the map is under a point of the screen, or -1. */
static int map_portal_at(const MapCamera *cam, Vector2 screen)
{
    int best = -1;
    float nearest = MAP_ICON_REACH * MAP_ICON_REACH;
    for (int i = 0; i < world_map.portal_count; i++) {
        Vector2 at = map_point(cam, world_map.portals[i].x + 0.5f, world_map.portals[i].y - 0.5f);
        float dx = at.x - screen.x, dy = at.y - screen.y;
        if (dx * dx + dy * dy < nearest) nearest = dx * dx + dy * dy, best = i;
    }
    return best;
}

/* The portal at a place, by its lowest block; false if there is none there that the player has uncovered. */
static bool portal_under(const Game *g, int x, int y, TilePos *out)
{
    x = world_wrap_x(&g->world, x);
    if (!game_is_portal(g, x, y) || game_explored(g, x, y) == 0) return false;
    while (game_is_portal(g, x, y + 1)) y++;
    *out = (TilePos){ x, y };
    return true;
}

/* The panel for making a first character, at the top of the screen. */
static Rectangle create_panel(void)
{
    return (Rectangle){ GetScreenWidth() / 2.0f - 210.0f, 130.0f, 420.0f, 172.0f };
}

static Rectangle create_button(void)
{
    Rectangle p = create_panel();
    return (Rectangle){ p.x + 16, p.y + p.height - 48, p.width - 32, 34 };
}

/* With nobody in the world there is nothing to do but look about, pick a
 * portal, and make somebody. Returns true once a character has been made. */
static bool handle_nobody(Game *g, MapCamera *cam, char *name, TilePos *create_at)
{
    /* The name, as it is typed. */
    size_t len = strlen(name);
    for (int c = GetCharPressed(); c > 0; c = GetCharPressed())
        if (c >= 32 && c < 127 && len + 1 < CHARACTER_NAME_MAX) name[len++] = (char)c, name[len] = '\0';
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && len > 0) name[len - 1] = '\0';

    Vector2 mouse = GetMousePosition();
    bool on_panel = CheckCollisionPointRec(mouse, create_panel());
    bool make = IsKeyPressed(KEY_ENTER) ||
                (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !cam->dragging && CheckCollisionPointRec(mouse, create_button()));
    if (make && game_create_character(g, *create_at, name) >= 0) {
        cam->pressed = cam->dragging = false;
        cam->follow = true;
        cam->anchored = false;
        cam->zoom_level = ZOOM_DEFAULT;
        return true;
    }

    /* The view: the wheel, and dragging. */
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        int before = cam->zoom_level;
        cam->zoom_level += wheel > 0.0f ? -1 : 1;
        if (cam->zoom_level < 0) cam->zoom_level = 0;
        if (cam->zoom_level >= ZOOM_LEVELS) cam->zoom_level = ZOOM_LEVELS > 0 ? ZOOM_LEVELS - 1 : 0;
        if (cam->zoom_level != before) {
            cam->anchored = true;
            cam->anchor_screen = mouse;
            cam->anchor_world = (Vector2){ cam->center.x + (mouse.x - GetScreenWidth() / 2.0f) / zoom,
                                           cam->center.y + (mouse.y - GetScreenHeight() / 2.0f) / zoom };
        }
    }
    cam->follow = false;
    bool on_map = zoom_step(cam->zoom_level) < MAP_FADE_FROM;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !on_panel) {
        cam->pressed = true;
        cam->dragging = false;
        cam->press_mouse = mouse;
        cam->press_center = cam->center;
    }
    if (cam->pressed && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        float dx = mouse.x - cam->press_mouse.x, dy = mouse.y - cam->press_mouse.y;
        if (!cam->dragging && dx * dx + dy * dy > DRAG_THRESHOLD * DRAG_THRESHOLD) {
            cam->dragging = true;
            cam->anchored = false;
            cam->press_center = cam->center;
            cam->press_mouse = mouse;
            dx = dy = 0.0f;
        }
        if (cam->dragging)
            cam->center = (Vector2){ cam->press_center.x - dx / zoom, cam->press_center.y - dy / zoom };
    }
    if (cam->pressed && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        /* A click on a portal - its mark on the map, or the thing itself -
         * makes it the one the character will be made at, and goes in on it. */
        TilePos picked;
        bool found = false;
        if (!cam->dragging && on_map) {
            int portal = map_portal_at(cam, mouse);
            if (portal >= 0) picked = world_map.portals[portal], found = true;
        } else if (!cam->dragging) {
            TilePos t = tile_at_screen(cam, mouse);
            found = portal_under(g, t.x, t.y, &picked);
        }
        if (found) {
            *create_at = picked;
            cam->zoom_level = ZOOM_DEFAULT;
            cam->anchored = false;
            cam->center = (Vector2){ (view_near_x(&view, (float)picked.x) + 0.5f) * TILE_SIZE, ((float)picked.y - 2.0f) * TILE_SIZE };
        }
        cam->pressed = false;
        cam->dragging = false;
    }
    return false;
}

static void draw_nobody(const Game *g, const char *name, TilePos create_at)
{
    Rectangle p = create_panel(), b = create_button();
    DrawRectangleRec(p, (Color){ 28, 30, 40, 244 });
    DrawRectangleLinesEx(p, 2.0f, (Color){ 244, 196, 48, 255 });
    const char *title = g->started ? "Nobody is left. Make a new character" : "Make your first character";
    DrawText(title, (int)(p.x + (p.width - MeasureText(title, 20)) / 2), (int)p.y + 12, 20, RAYWHITE);
    /* The name, with a cursor that blinks. */
    Rectangle field = { p.x + 16, p.y + 44, p.width - 32, 32 };
    DrawRectangleRec(field, (Color){ 14, 15, 20, 255 });
    DrawRectangleLinesEx(field, 1.0f, (Color){ 120, 122, 134, 255 });
    bool blink = fmodf((float)GetTime(), 1.0f) < 0.5f;
    if (name[0]) DrawText(TextFormat("%s%s", name, blink ? "_" : ""), (int)field.x + 8, (int)field.y + 7, 18, RAYWHITE);
    else DrawText(blink ? "Type a name_" : "Type a name", (int)field.x + 8, (int)field.y + 7, 18, GRAY);
    bool spawn = create_at.x == g->world.spawn_x;
    const char *where = TextFormat("At the %s, X %d, Y %d", spawn ? "spawn portal" : "portal", create_at.x,
                                   world_row_to_y(&g->world, create_at.y));
    DrawText(where, (int)(p.x + (p.width - (float)MeasureText(where, 10)) / 2), (int)p.y + 90, 10, LIGHTGRAY);
    bool hot = CheckCollisionPointRec(GetMousePosition(), b);
    DrawRectangleRec(b, hot ? (Color){ 90, 160, 100, 255 } : (Color){ 70, 130, 80, 255 });
    const char *label = "Create character  (Enter)";
    DrawText(label, (int)(b.x + (b.width - MeasureText(label, 18)) / 2), (int)b.y + 8, 18, RAYWHITE);
}

static void handle_input(Game *g, MapCamera *cam, Hud *hud, BenchUi *bench, TradeUi *trade, PlayerUi *player,
                         const Keybinds *keys)
{
    const int *k = keys->keys;
    if (IsKeyPressed(k[BIND_UP]) || IsKeyPressed(k[BIND_LEFT]) || IsKeyPressed(k[BIND_DOWN]) ||
        IsKeyPressed(k[BIND_RIGHT])) {
        game_cancel_actions(g);
        hud_close_all(hud);
        benchui_close(bench, g);
    }
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (IsKeyPressed(KEY_ONE + i)) inventory_select_slot(&g->inventory, i);
    game_set_move_input(g, (int)IsKeyDown(k[BIND_RIGHT]) - (int)IsKeyDown(k[BIND_LEFT]),
                        (int)IsKeyDown(k[BIND_DOWN]) - (int)IsKeyDown(k[BIND_UP]));

    if (IsKeyPressed(k[BIND_FOLLOW])) cam->follow = true;

    /* The slots of an open chest join the inventory's, so items drag between them. */
    sync_storage_slots(g, hud);

    /* Food that is selected gets an "Eat" button, greyed out on a full stomach. */
    const ItemStack *chosen = inventory_selected(&g->inventory);
    hud->eat = chosen->count < 1 || !game_is_food(chosen->id) ? HUD_EAT_NONE
               : game_can_eat(g, chosen->id) ? HUD_EAT_READY : HUD_EAT_FULL;

    /* A press that starts on the inventory never reaches the world. */
    bool on_hud = hud_handle_input(hud, &g->inventory);
    if (hud->eat_clicked) game_eat(g);
    /* A stack dragged out of the inventory and let go over the world is
     * dropped into it - but not one let go over a menu. */
    if (hud->discard && !playerui_mouse_over(player) && !benchui_mouse_over(bench, g, &ui_view) &&
        !tradeui_mouse_over(trade, g, &ui_view))
        game_discard(g, hud->discard_slot, hud->discard_count);
    if (on_hud) return;
    bool on_players = playerui_handle_input(player, g);
    if (player->switched) {
        /* Another character is being played: the view and the menus go over to them. */
        hud_close_all(hud);
        cam->follow = true;
        cam->anchored = false;
    }
    if (on_players) return;
    if (tradeui_handle_input(trade, g, &ui_view)) return;
    bool on_bench = benchui_handle_input(bench, g, &ui_view);
    if (bench->teleported) {
        /* Gone with the player. */
        bench->teleported = false;
        cam->follow = true;
        cam->anchored = false;
        cam->center = (Vector2){ (g->player.feet.x + 0.5f) * TILE_SIZE, (float)g->player.feet.y * TILE_SIZE };
    }
    if (on_bench) return;

    /* The wheel zooms, when it is not scrolling a menu under the cursor. */
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && !hud_mouse_over(hud, &g->inventory) && !tradeui_mouse_over(trade, g, &ui_view) &&
        !playerui_mouse_over(player) &&
        !benchui_mouse_over(bench, g, &ui_view)) {
        int before = cam->zoom_level;
        cam->zoom_level += wheel > 0.0f ? -1 : 1;
        if (cam->zoom_level < 0) cam->zoom_level = 0;
        if (cam->zoom_level >= ZOOM_LEVELS) cam->zoom_level = ZOOM_LEVELS > 0 ? ZOOM_LEVELS - 1 : 0;
        if (cam->zoom_level != before) {
            /* Zoom about the cursor, not the middle of the screen. The camera
             * lets go of the player to do it, as it does when dragged. */
            Vector2 at = GetMousePosition();
            cam->anchored = true;
            cam->anchor_screen = at;
            cam->anchor_world = (Vector2){ cam->center.x + (at.x - GetScreenWidth() / 2.0f) / zoom,
                                           cam->center.y + (at.y - GetScreenHeight() / 2.0f) / zoom };
            cam->follow = false;
        }
        /* On the world map there is nothing to have open. */
        if (zoom_step(cam->zoom_level) < MAP_FADE_FROM) {
            hud_close_all(hud);
            benchui_close(bench, g);
        }
    }
    bool on_map = zoom_step(cam->zoom_level) < MAP_FADE_FROM;

    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        hud_close_all(hud);
        benchui_close(bench, g);
        player->open = false;
        cam->pressed = true;
        cam->dragging = false;
        cam->press_mouse = mouse;
        cam->press_center = cam->center;
    }
    if (cam->pressed && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        float dx = mouse.x - cam->press_mouse.x, dy = mouse.y - cam->press_mouse.y;
        if (!cam->dragging && dx * dx + dy * dy > DRAG_THRESHOLD * DRAG_THRESHOLD) {
            cam->dragging = true;
            cam->follow = false;
            cam->anchored = false;
            cam->press_center = cam->center;
            cam->press_mouse = mouse;
            dx = dy = 0.0f;
        }
        if (cam->dragging)
            cam->center = (Vector2){ cam->press_center.x - dx / zoom, cam->press_center.y - dy / zoom };
    }
    if (cam->pressed && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        /* The world map can be dragged about, and the only thing on it that a
         * click does anything to is a portal: the view goes in on it, and its
         * menu comes up. */
        int portal = !cam->dragging && on_map ? map_portal_at(cam, mouse) : -1;
        if (portal >= 0) {
            TilePos at = world_map.portals[portal];
            cam->zoom_level = ZOOM_DEFAULT;
            cam->follow = false;
            cam->anchored = false;
            cam->center = (Vector2){ (view_near_x(&view, (float)at.x) + 0.5f) * TILE_SIZE, ((float)at.y - 2.0f) * TILE_SIZE };
            game_open_bench(g, at.x, at.y);
        }
        if (!cam->dragging && !on_map) {
            TilePos t = tile_at_screen(cam, mouse);
            t.x = world_wrap_x(&g->world, t.x);
            const TileDef *def = tile_def(game_bench_at(g, t.x, t.y));
            bool opens = def->bench || def->trade || def->storage_columns > 0;
            int drop = game_drop_at(g, (mouse.x / zoom - view.origin.x) / TILE_SIZE,
                                    (mouse.y / zoom - view.origin.y) / TILE_SIZE);
            /* An item under the cursor comes before the tile behind it.
             * Something that opens offers a choice if it can also be removed;
             * otherwise it just opens. */
            int mob = game_mob_at(g, (mouse.x / zoom - view.origin.x) / TILE_SIZE,
                                  (mouse.y / zoom - view.origin.y) / TILE_SIZE);
            if (drop >= 0) game_click_drop(g, drop);
            /* A creature under the cursor is hunted. */
            else if (mob >= 0) game_click_mob(g, mob);
            /* A second click on a square with something queued takes it back. */
            else if (game_unqueue_tile(g, t.x, t.y)) {}
            else if (opens && def->removable) benchui_show_popup(bench, t);
            /* A bench or a portal shows its menu from wherever the player is;
             * a chest has to be walked to. */
            else if (opens && game_open_bench(g, t.x, t.y)) {}
            else if (opens) game_queue_action(g, ACTION_USE, t.x, t.y);
            /* A sign is read and written on, not dug at. */
            else if (tile_def(world_get(&g->world, t.x, t.y))->written ||
                     tile_def(tile_def(world_get(&g->world, t.x, t.y))->pair_of)->written)
                sign_open(g, t);
            else game_click_tile(g, t.x, t.y);
        }
        cam->pressed = false;
        cam->dragging = false;
    }
}

static void update_camera(const Game *g, MapCamera *cam, float dt)
{
    if (cam->follow) {
        Vector2 target = player_center(g);
        float k = fminf(1.0f, dt * CAMERA_FOLLOW_SPEED);
        cam->center.x += (target.x - cam->center.x) * k;
        cam->center.y += (target.y - cam->center.y) * k;
    }
    if (cam->follow) cam->anchored = false;
    cam->scale += (zoom_step(cam->zoom_level) - cam->scale) * fminf(1.0f, dt * ZOOM_SPEED);
    if (cam->anchored) {
        /* Keep the point that was under the cursor under it as the zoom eases in. */
        float z = cam->scale / TILE_SIZE;
        cam->center = (Vector2){ cam->anchor_world.x - (cam->anchor_screen.x - GetScreenWidth() / 2.0f) / z,
                                 cam->anchor_world.y - (cam->anchor_screen.y - GetScreenHeight() / 2.0f) / z };
        if (fabsf(zoom_step(cam->zoom_level) - cam->scale) < 0.01f) cam->anchored = false;
    }
    /* No limit sideways: the world goes all the way round. */
    cam->center.y = fminf(fmaxf(cam->center.y, 0.0f), (float)g->world.height * TILE_SIZE);
}

static Color mix(Color a, Color b, float t)
{
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                    (unsigned char)(a.b + (b.b - a.b) * t), 255 };
}

/* Sky, with the sun crossing it by day and the moon by night. */
static void draw_sky(const Game *g, Vector2 origin)
{
    const World *w = &g->world;
    float light = game_daylight(g);
    Color sky = mix((Color){ 12, 16, 40, 255 }, (Color){ 135, 190, 235, 255 }, light);
    /* (Drawn straight to the screen, not through the zoom.) */
    DrawRectangle(0, (int)(origin.y * zoom), GetScreenWidth(), (int)(w->height * TILE_SIZE * zoom) + 1, sky);

    bool day;
    float phase = game_day_phase(g, &day);
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    Vector2 at = { sw * (0.08f + 0.84f * phase), sh * 0.5f - sinf(phase * PI) * sh * 0.36f };
    if (day) {
        DrawCircleV(at, 34, Fade((Color){ 255, 236, 150, 255 }, 0.35f));
        DrawCircleV(at, 24, (Color){ 255, 228, 110, 255 });
    } else {
        DrawCircleV(at, 20, (Color){ 226, 230, 240, 255 });
        DrawCircleV((Vector2){ at.x + 8, at.y - 5 }, 17, sky);
    }
}

/* The tile layers, back to front: the backwall, well dimmed, and the
 * background, dimmed a little, wherever the foreground does not hide them;
 * then the foreground itself. */
/* Which sides of a block have something it joins up with: 1 for above, 2
 * right, 4 below, 8 left. Standing on solid ground counts as joined below. */
static int tile_joined(const World *w, int layer, int x, int y)
{
    static const int side[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
    const TileDef *def = tile_def(world_get_at(w, layer, x, y));
    int joined = 0;
    for (int i = 0; i < 4; i++) {
        TileType other = world_get_at(w, layer, x + side[i][0], y + side[i][1]);
        for (int k = 0; k < def->join_count; k++)
            if (other != TILE_AIR && other == def->joins[k]) joined |= 1 << i;
    }
    if (world_is_solid(w, x, y + 1)) joined |= 4;
    return joined;
}

/* One block of a layer, in whichever of its looks applies. */
static void draw_block(const Game *g, int layer, TileType t, int x, int y, float px, float py, float dim)
{
    if (tile_def(t)->mimics != MIMIC_NONE && layer == LAYER_FOREGROUND) t = world_looks_like(&g->world, x, y);
    const TileDef *def = tile_def(t);
    int wx = world_wrap_x(&g->world, x);
    if (def->steps) {
        /* Steps rise toward the next flight up or away from the next one
         * down; failing either, toward a block beside them. As drawn they rise to the right. */
        const World *w = &g->world;
        bool right = world_is_step(w, x + 1, y - 1) || world_is_step(w, x - 1, y + 1);
        bool left = world_is_step(w, x - 1, y - 1) || world_is_step(w, x + 1, y + 1);
        if (right == left) {
            right = world_is_solid(w, x + 1, y) || !world_is_solid(w, x - 1, y);
            left = !right;
        }
        assets_draw_tile_facing(t, wx, y, px, py, TILE_SIZE, dim, left && !right);
    } else if (!def->solid && (def->texture_mount[0][0] || def->texture_mount[1][0] || def->texture_mount[2][0])) {
        /* Something put up with no backwall behind it shows what it is fixed to instead. (In the foreground, a backwall holds nothing up.) */
        int mount = layer == LAYER_BACKGROUND ? world_mount(&g->world, x, y) : world_mount_block(&g->world, x, y);
        /* (Something two blocks wide is put up one way: by what holds its left half, or failing that its right.
         * A fixing to one side shows on the half that is on that side.) */
        bool right_half = def->pair_of != TILE_AIR && world_get_at(&g->world, layer, x - 1, y) == def->pair_of;
        if (def->pair != TILE_AIR || right_half) {
            int head = right_half ? x - 1 : x;
            mount = world_mount_block(&g->world, head, y);
            if (mount == MOUNT_NONE) mount = world_mount_block(&g->world, head + 1, y);
            if ((mount == MOUNT_LEFT && right_half) || (mount == MOUNT_RIGHT && !right_half)) mount = MOUNT_WALL;
        }
        assets_draw_tile_mounted(t, wx, y, px, py, TILE_SIZE, dim, mount);
    } else if (def->wire) {
        /* Wire runs to whatever beside it carries or takes electricity. */
        static const int beside[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
        int joined = 0;
        for (int d = 0; d < 4; d++) {
            int nx = x + beside[d][0], ny = y + beside[d][1];
            bool thing = false;
            for (int l = 0; l < BLOCK_LAYERS; l++) {
                const TileDef *n = tile_def(world_get_at(&g->world, l, nx, ny));
                thing = thing || n->gen_power > 0 || n->battery > 0 || n->electric > 0 || n->motor_power > 0;
            }
            if (thing || world_is_wired(&g->world, nx, ny)) joined |= 1 << d;
        }
        assets_draw_tile_joined(t, wx, y, px, py, TILE_SIZE, dim, joined);
    } else if (def->join_count > 0)
        assets_draw_tile_joined(t, wx, y, px, py, TILE_SIZE, dim, tile_joined(&g->world, layer, x, y));
    else {
        /* A tall block has a top to it, and a foot. */
        TilePart part = TILE_PART_BODY;
        if (def->height > 1 && world_get_at(&g->world, layer, x, y) == t) {
            int bottom, count = world_structure(&g->world, layer, x, y, &bottom);
            part = y == bottom - count + 1 ? TILE_PART_TOP : y == bottom ? TILE_PART_BASE : TILE_PART_BODY;
        }
        /* Something with a fire in it shows the fire while it has fuel. */
        assets_draw_tile_lit(t, wx, y, px, py, TILE_SIZE, dim, part, def->lit_count > 0 && game_fuel_at(g, wx, y) > 0);
    }
    /* A two-tone flower, in the colours this one is. */
    if (def->two_tone) assets_draw_tile_tones(t, px, py, TILE_SIZE, dim, world_meta(&g->world, wx, y));
}

static void draw_world(const Game *g, Vector2 origin)
{
    const World *w = &g->world;
    /* Columns run past either end of the world: reads wrap round it. */
    int x0 = (int)floorf(-origin.x / TILE_SIZE), y0 = (int)floorf(-origin.y / TILE_SIZE);
    int x1 = x0 + (int)seen.x / TILE_SIZE + 1, y1 = y0 + (int)seen.y / TILE_SIZE + 1;
    if (y0 < 0) y0 = 0;
    if (y1 >= w->height) y1 = w->height - 1;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            TileType t = world_get(w, x, y), behind = world_get_at(w, LAYER_BACKGROUND, x, y);
            int wx = world_wrap_x(w, x);
            float px = origin.x + (float)(x * TILE_SIZE), py = origin.y + (float)(y * TILE_SIZE);
            /* (Paint is a wash of its colour over the backwall or the block it is on, as bright as that is lit.) */
            int wall_paint = world_paint(w, true, x, y), block_paint = world_paint(w, false, x, y);
            if (!tile_def(t)->solid || tile_def(t)->see_through) {
                /* (A window is a hole in the backwall: there, what is behind the wall shows, and the wall does not.) */
                bool hole = tile_def(behind)->window;
                if (!hole)
                    assets_draw_tile(world_get_at(w, LAYER_WALL, x, y), wx, y, px, py, TILE_SIZE, WALL_DIM,
                                     TILE_PART_BODY);
                if (!hole && wall_paint > 0 && wall_paint <= content.tint_count && world_get_at(w, LAYER_WALL, x, y) != TILE_AIR) {
                    const unsigned char *c = content.tints[wall_paint - 1];
                    float lit = 1.0f - WALL_DIM;
                    DrawRectangleRec((Rectangle){ px, py, TILE_SIZE, TILE_SIZE },
                                     (Color){ (unsigned char)(c[0] * lit), (unsigned char)(c[1] * lit), (unsigned char)(c[2] * lit), 170 });
                }
                if (!tile_def(behind)->front) draw_block(g, LAYER_BACKGROUND, behind, x, y, px, py, BACKGROUND_DIM);
            }
            if (!tile_def(t)->front) draw_block(g, LAYER_FOREGROUND, t, x, y, px, py, 0.0f);
            if (block_paint > 0 && block_paint <= content.tint_count && tile_def(t)->solid) {
                const unsigned char *c = content.tints[block_paint - 1];
                DrawRectangleRec((Rectangle){ px, py, TILE_SIZE, TILE_SIZE }, (Color){ c[0], c[1], c[2], 170 });
            }
            int wet = world_water(w, x, y);
            if (wet > 0) {
                /* Water tints what is in it. A square with water over it is
                 * drawn full; the top one stands as high as it holds, with a
                 * lighter line along its surface. */
                const int *c = content.rules.water_color;
                Color water = { (unsigned char)c[0], (unsigned char)c[1], (unsigned char)c[2], (unsigned char)c[3] };
                bool top = world_water(w, x, y - 1) == 0;
                float depth = top ? (float)TILE_SIZE * (float)wet / WATER_FULL : (float)TILE_SIZE;
                DrawRectangleRec((Rectangle){ px, py + TILE_SIZE - depth, TILE_SIZE, depth }, water);
                if (top) DrawRectangleRec((Rectangle){ px, py + TILE_SIZE - depth, TILE_SIZE, 3 }, Fade(WHITE, 0.35f));
            }
        }
    }
    /* Signs: what is written on each, in one line across the two blocks it takes up. */
    for (int y = y0; y <= y1; y++) {
        for (int x = x0 - 1; x <= x1; x++) {
            const SignText *text = tile_def(world_get(w, x, y))->written ? game_sign_at(g, x, y) : NULL;
            if (!text || !text->text[0]) continue;
            float px = origin.x + (float)(x * TILE_SIZE), py = origin.y + (float)(y * TILE_SIZE);
            /* Written smaller when it would not otherwise fit, so that it never runs off the board. */
            float wide = (float)MeasureText(text->text, SIGN_TEXT_SIZE);
            float scale = wide > SIGN_TEXT_WIDTH ? (float)SIGN_TEXT_WIDTH / wide : 1.0f;
            rlPushMatrix();
            rlTranslatef(px + (float)TILE_SIZE, py + TILE_SIZE / 2.0f, 0.0f);
            rlScalef(scale, scale, 1.0f);
            DrawText(text->text, (int)(-wide / 2.0f), -SIGN_TEXT_SIZE / 2, SIGN_TEXT_SIZE, (Color){ 40, 26, 14, 255 });
            rlPopMatrix();
        }
    }
    /* Shelves and cabinets: what is kept in each, drawn small on it, four to a block. */
    for (int i = 0; i < MAX_STORAGES; i++) {
        const Storage *st = &g->storages[i];
        if (!st->used || st->container == 0 || st->y < y0 || st->y > y1) continue;
        const TileDef *def = tile_def(world_get(w, st->x, st->y));
        if (!def->shows_contents) def = tile_def(world_get_at(w, LAYER_BACKGROUND, st->x, st->y));
        if (!def->shows_contents) continue;
        float bx = origin.x + ((float)x0 + (float)world_dx(w, world_wrap_x(w, x0), st->x)) * TILE_SIZE, by = origin.y + (float)st->y * TILE_SIZE;
        if (bx < origin.x + (float)(x0 - 1) * TILE_SIZE || bx > origin.x + (float)(x1 + 1) * TILE_SIZE) continue;
        const ItemStack *kept = g->inventory.containers[st->container].slots;
        float half = TILE_SIZE / 2.0f;
        for (int k = 0; k < 4; k++)
            if (kept[k].count > 0)
                hud_draw_item_icon((ItemId)kept[k].id, (Rectangle){ bx + (float)(k % 2) * half + 1, by + (float)(k / 2) * half + 1, half - 2, half - 2 });
    }
    /* Electricity: how full each battery is, as a bar over it; and what is
     * on its way along the wires, as sparks running down them. */
    for (int i = 0; i < MAX_STORAGES; i++) {
        const Storage *st = &g->storages[i];
        int holds = 0, has = st->used ? game_power_at(g, st->x, st->y, &holds) : 0;
        if (holds <= 0) continue;
        float bx = origin.x + ((float)x0 + (float)world_dx(w, world_wrap_x(w, x0), st->x)) * TILE_SIZE, by = origin.y + (float)st->y * TILE_SIZE;
        if (st->y < y0 || st->y > y1 || bx < origin.x + (float)(x0 - 1) * TILE_SIZE || bx > origin.x + (float)(x1 + 1) * TILE_SIZE) continue;
        DrawRectangleRec((Rectangle){ bx + 2, by - 7, TILE_SIZE - 4, 5 }, (Color){ 12, 13, 18, 255 });
        DrawRectangleRec((Rectangle){ bx + 3, by - 6, (TILE_SIZE - 6) * (float)has / (float)holds, 3 }, (Color){ 250, 214, 60, 255 });
    }
    {
        static Spark sparks[512];
        int count = game_sparks(g, sparks, 512);
        float part = g->tick_timer < 0.0f ? 0.0f : g->tick_timer > 1.0f ? 1.0f : g->tick_timer;
        for (int i = 0; i < count; i++) {
            const Spark *k = &sparks[i];
            float fx = (float)x0 + (float)world_dx(w, world_wrap_x(w, x0), k->x) + (float)world_dx(w, k->x, k->to_x) * part;
            float fy = (float)k->y + (float)(k->to_y - k->y) * part;
            if (fx < (float)x0 - 1 || fx > (float)x1 + 1 || fy < (float)y0 - 1 || fy > (float)y1 + 1) continue;
            float sx = origin.x + (fx + 0.5f) * TILE_SIZE, sy = origin.y + (fy + 0.5f) * TILE_SIZE;
            DrawRectangleRec((Rectangle){ sx - 4, sy - 4, 8, 8 }, (Color){ 255, 240, 120, 110 });
            DrawRectangleRec((Rectangle){ sx - 2, sy - 2, 4, 4 }, (Color){ 255, 255, 220, 255 });
        }
    }
    /* Snow: lying on the top of each column, as deep as it has fallen. */
    if (content.rules.snow_max > 0) {
        for (int x = x0; x <= x1; x++) {
            int deep = game_snow(g, x), top = deep > 0 ? game_sky_floor(g, x, false) : -1;
            if (top < y0 || top > y1 + 1) continue;
            float h = (float)TILE_SIZE * (float)deep / 8.0f;
            float px = origin.x + (float)(x * TILE_SIZE), py = origin.y + (float)(top * TILE_SIZE);
            DrawRectangleRec((Rectangle){ px, py - h, TILE_SIZE, h }, (Color){ 240, 244, 250, 255 });
            DrawRectangleRec((Rectangle){ px, py - h, TILE_SIZE, 2 }, WHITE);
            DrawRectangleRec((Rectangle){ px, py - 2, TILE_SIZE, 2 }, (Color){ 206, 216, 232, 255 });
        }
    }
    /* Tiles marked "front" go on last: their textures overhang their
     * neighbours. The top and bottom tiles of a column of one can have textures
     * of their own. */
    for (int y = y0; y <= y1 + 1; y++) {
        for (int x = x0 - 1; x <= x1 + 1; x++) {
            float px = origin.x + (float)(x * TILE_SIZE), py = origin.y + (float)(y * TILE_SIZE);
            for (int layer = BLOCK_LAYERS - 1; layer >= 0; layer--) {
                TileType t = world_get_at(w, layer, x, y);
                if (!tile_def(t)->front) continue;
                if (layer == LAYER_BACKGROUND && tile_def(world_get(w, x, y))->solid) continue;
                assets_draw_tile(t, world_wrap_x(w, x), y, px, py, TILE_SIZE,
                                 layer == LAYER_BACKGROUND ? BACKGROUND_DIM : 0.0f,
                                 assets_tile_part(t, world_get_at(w, layer, x, y - 1),
                                                  world_get_at(w, layer, x, y + 1)));
            }
        }
    }
}

/* Rain, or in a season of snow, snow: falling down every column that is open
 * to the sky, as far as the ground or the water. It is only to look at. */
static void draw_weather(const Game *g, Vector2 origin)
{
    float amount = game_rain(g);
    if (amount <= 0.0f) return;
    bool snow = content.rules.season_count > 0 && content.rules.seasons[game_season(g)].snows;
    int x0 = (int)floorf(-origin.x / TILE_SIZE), x1 = x0 + (int)seen.x / TILE_SIZE + 1;
    float top = -origin.y - TILE_SIZE, time = (float)GetTime();
    for (int x = x0; x <= x1; x++) {
        int floor = game_sky_floor(g, x, true);
        float bottom = floor < 0 ? -origin.y + seen.y : (float)floor * TILE_SIZE;
        if (bottom <= top) continue;
        for (int k = 0; k < (int)(4.0f * amount + 0.5f); k++) {
            unsigned h = (unsigned)x * 73856093u ^ (unsigned)k * 19349663u;
            float speed = snow ? 60.0f + (float)(h % 40u) : 620.0f + (float)(h % 200u);
            float span = bottom - top + 40.0f;
            float y = top + fmodf((float)(h % 1000u) / 1000.0f * span + time * speed, span);
            float fx = (float)(x * TILE_SIZE) + (float)((h >> 10) % (unsigned)TILE_SIZE);
            if (y > bottom) continue;
            if (snow) {
                fx += sinf(time * 1.5f + (float)(h % 628u) / 100.0f) * 5.0f;
                DrawRectangleRec((Rectangle){ origin.x + fx, origin.y + y, 3, 3 }, (Color){ 250, 252, 255, 230 });
            } else {
                DrawLineEx((Vector2){ origin.x + fx, origin.y + y }, (Vector2){ origin.x + fx - 3, origin.y + y + 14 }, 1.5f,
                           (Color){ 150, 190, 240, 150 });
            }
        }
    }
}

/* Darkens the world by how little light reaches each tile. The darkness of a
 * tile's corner is the average of the tiles that meet there, so light fades
 * smoothly instead of in squares. */
static void draw_light(const Game *g, Vector2 origin)
{
    static uint8_t *light;
    static int capacity;
    int x0 = (int)floorf(-origin.x / TILE_SIZE) - 1, y0 = (int)floorf(-origin.y / TILE_SIZE) - 1;
    int cols = (int)seen.x / TILE_SIZE + 4, rows = (int)seen.y / TILE_SIZE + 4;
    if (cols * rows > capacity) {
        uint8_t *bigger = realloc(light, (size_t)cols * rows);
        if (!bigger) return;
        light = bigger;
        capacity = cols * rows;
    }
    light_compute(g, x0, y0, cols, rows, light);

    for (int cy = 1; cy < rows - 1; cy++) {
        for (int cx = 1; cx < cols - 1; cx++) {
            Color corner[4]; /* top left, bottom left, bottom right, top right */
            static const int at[4][2] = { { -1, -1 }, { -1, 0 }, { 0, 0 }, { 0, -1 } };
            int darkest = 0;
            for (int k = 0; k < 4; k++) {
                int bx = cx + at[k][0], by = cy + at[k][1];
                int sum = light[by * cols + bx] + light[by * cols + bx + 1] +
                          light[(by + 1) * cols + bx] + light[(by + 1) * cols + bx + 1];
                int dark = 255 - sum * 255 / (4 * LIGHT_MAX);
                corner[k] = (Color){ 4, 5, 14, (unsigned char)dark };
                if (dark > darkest) darkest = dark;
            }
            if (darkest == 0) continue;
            Rectangle r = { origin.x + (float)((x0 + cx) * TILE_SIZE), origin.y + (float)((y0 + cy) * TILE_SIZE),
                            TILE_SIZE, TILE_SIZE };
            /* (raylib takes the corners anticlockwise from the top left.) */
            DrawRectangleGradientEx(r, corner[0], corner[1], corner[2], corner[3]);
        }
    }
}

/* Fog of war: black over what the player has never been near, thinning out
 * smoothly at the edge of what they have. */
static void draw_fog(const Game *g, Vector2 origin)
{
    if (!g->fog) return;
    int x0 = (int)floorf(-origin.x / TILE_SIZE) - 1, y0 = (int)floorf(-origin.y / TILE_SIZE) - 1;
    int cols = (int)seen.x / TILE_SIZE + 4, rows = (int)seen.y / TILE_SIZE + 4;
    for (int cy = 1; cy < rows - 1; cy++) {
        for (int cx = 1; cx < cols - 1; cx++) {
            Color corner[4]; /* top left, bottom left, bottom right, top right */
            static const int at[4][2] = { { -1, -1 }, { -1, 0 }, { 0, 0 }, { 0, -1 } };
            int darkest = 0;
            for (int k = 0; k < 4; k++) {
                int bx = x0 + cx + at[k][0], by = y0 + cy + at[k][1];
                int sum = game_explored(g, bx, by) + game_explored(g, bx + 1, by) +
                          game_explored(g, bx, by + 1) + game_explored(g, bx + 1, by + 1);
                int dark = 255 - sum / 4;
                corner[k] = (Color){ 0, 0, 0, (unsigned char)dark };
                if (dark > darkest) darkest = dark;
            }
            if (darkest == 0) continue;
            Rectangle r = { origin.x + (float)((x0 + cx) * TILE_SIZE), origin.y + (float)((y0 + cy) * TILE_SIZE),
                            TILE_SIZE, TILE_SIZE };
            DrawRectangleGradientEx(r, corner[0], corner[1], corner[2], corner[3]);
        }
    }
}

/* ---- the world map ----------------------------------------------------- */

static Color scaled(Color c, float k)
{
    return (Color){ (unsigned char)(c.r * k), (unsigned char)(c.g * k), (unsigned char)(c.b * k), 255 };
}

static void world_map_refresh(const Game *g)
{
    const World *w = &g->world;
    if (world_map.width != w->width || world_map.height != w->height) {
        if (world_map.texture.id) UnloadTexture(world_map.texture);
        free(world_map.pixels);
        world_map.pixels = malloc(sizeof(Color) * (size_t)w->width * w->height);
        Image blank = GenImageColor(w->width, w->height, BLACK);
        world_map.texture = LoadTextureFromImage(blank);
        UnloadImage(blank);
        world_map.width = w->width;
        world_map.height = w->height;
    }
    if (!world_map.pixels) return;

    /* Which blocks are portals: the kind of bench a player can be carried to. */
    bool portal[MAX_TILES] = { false };
    for (int t = 1; t < content.tile_count && content.rules.teleport_bench[0]; t++)
        portal[t] = content.tiles[t].bench && strcmp(content.tiles[t].bench_kind, content.rules.teleport_bench) == 0;
    world_map.portal_count = 0;

    const int *wc = content.rules.water_color;
    Color sky = { 135, 190, 235, 255 }, water = { (unsigned char)wc[0], (unsigned char)wc[1], (unsigned char)wc[2], 255 };
    for (int y = 0; y < w->height; y++) {
        for (int x = 0; x < w->width; x++) {
            /* Each block is one colour: what is in front, else what is behind it. */
            TileType front = world_get(w, x, y), behind = world_get_at(w, LAYER_BACKGROUND, x, y);
            TileType wall = world_get_at(w, LAYER_WALL, x, y);
            Color c = sky;
            if (assets_tile_color(front).a) c = assets_tile_color(front);
            else if (assets_tile_color(behind).a) c = scaled(assets_tile_color(behind), 1.0f - BACKGROUND_DIM);
            else if (wall != TILE_AIR) c = scaled(assets_tile_color(wall), 1.0f - WALL_DIM);
            else if (world_is_water(w, x, y)) c = water;
            /* A portal is marked at its foot, once the player has seen it. */
            for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
                if (!portal[world_get_at(w, layer, x, y)] || portal[world_get_at(w, layer, x, y + 1)]) continue;
                if (game_explored(g, x, y) > 0 && world_map.portal_count < MAX_MAP_PORTALS)
                    world_map.portals[world_map.portal_count++] = (TilePos){ x, y };
            }
            /* Fog of war covers the map as it does the world. */
            world_map.pixels[(size_t)y * w->width + x] = scaled(c, (float)game_explored(g, x, y) / 255.0f);
        }
    }
    UpdateTexture(world_map.texture, world_map.pixels);
    world_map.age = 0.0f;
}

/* Draws the world map over the view, `alpha` of the way faded in, with a
 * marker where the player is. */
static void draw_world_map(const Game *g, const MapCamera *cam, float alpha, float dt)
{
    world_map.age += dt;
    if (!world_map.texture.id || world_map.age >= MAP_REFRESH || world_map.width != g->world.width)
        world_map_refresh(g);
    if (!world_map.texture.id) return;

    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight(), scale = cam->scale;
    float cx = cam->center.x / TILE_SIZE, cy = cam->center.y / TILE_SIZE;
    DrawRectangle(0, 0, (int)sw, (int)sh, Fade((Color){ 8, 9, 14, 255 }, alpha));
    /* The world goes round: as many copies side by side as the screen shows. */
    int base = (int)floorf(cx / (float)world_map.width);
    for (int k = -2; k <= 2; k++) {
        Rectangle to = { sw / 2 - (cx - (float)(base + k) * world_map.width) * scale, sh / 2 - cy * scale,
                         world_map.width * scale, world_map.height * scale };
        if (to.x > sw || to.x + to.width < 0) continue;
        DrawTexturePro(world_map.texture, (Rectangle){ 0, 0, (float)world_map.width, (float)world_map.height },
                       to, (Vector2){ 0, 0 }, 0.0f, Fade(WHITE, alpha));
    }

    /* Portals: a little doorway of light each, brighter under the cursor. They can be clicked. */
    int hot = map_portal_at(cam, GetMousePosition());
    for (int i = 0; i < world_map.portal_count; i++) {
        Vector2 p = map_point(cam, world_map.portals[i].x + 0.5f, world_map.portals[i].y - 0.5f);
        float glow = 0.5f + 0.5f * sinf((float)GetTime() * 3.0f + (float)i);
        float grow = i == hot ? 2.0f : 0.0f;
        DrawRectangleRounded((Rectangle){ p.x - 7 - grow, p.y - 10 - grow, 14 + 2 * grow, 20 + 2 * grow }, 0.5f, 6,
                             Fade(BLACK, alpha));
        DrawRectangleRounded((Rectangle){ p.x - 5 - grow, p.y - 8 - grow, 10 + 2 * grow, 16 + 2 * grow }, 0.5f, 6,
                             Fade((Color){ 150, 90, 230, 255 }, alpha));
        DrawRectangleRounded((Rectangle){ p.x - 2.5f, p.y - 5, 5, 10 }, 0.6f, 6,
                             Fade((Color){ 226, 200, 255, 255 }, alpha * (0.6f + 0.4f * glow)));
        if (i == hot) DrawRing(p, 15.0f, 17.0f, 0.0f, 360.0f, 32, Fade(RAYWHITE, alpha));
    }

    /* Where each character is: a head, and round the one being played a ring that breathes. */
    Game *all = (Game *)g;
    int played = g->controlled;
    for (int n = 0; n < g->character_count; n++) {
        int c = (played + 1 + n) % g->character_count;
        game_use_character(all, c);
        float px, py;
        game_player_visual_pos(g, &px, &py);
        Vector2 at = { sw / 2 + (view_near_x(&view, px) + 0.5f - cx) * scale, sh / 2 + (py - 0.5f - cy) * scale };
        float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 4.0f);
        if (c == played)
            DrawRing(at, 11.0f + 3.0f * pulse, 13.5f + 3.0f * pulse, 0.0f, 360.0f, 32, Fade(WHITE, alpha * (1.0f - 0.5f * pulse)));
        DrawRectangleRec((Rectangle){ at.x - 7, at.y - 7, 14, 14 }, Fade(BLACK, alpha));
        DrawRectangleRec((Rectangle){ at.x - 6, at.y - 6, 12, 12 }, Fade((Color){ 240, 200, 160, 255 }, alpha));
        DrawRectangleRec((Rectangle){ at.x - 6, at.y - 6, 12, 4 }, Fade((Color){ 90, 60, 40, 255 }, alpha));
        DrawRectangleRec((Rectangle){ at.x - 3, at.y, 2, 2 }, Fade((Color){ 40, 40, 60, 255 }, alpha));
        DrawRectangleRec((Rectangle){ at.x + 2, at.y, 2, 2 }, Fade((Color){ 40, 40, 60, 255 }, alpha));
    }

    const char *text = "World map  -  drag to look around, scroll to zoom back in, click a portal to go to its menu";
    DrawText(text, (int)(sw - MeasureText(text, 16)) / 2, (int)sh - 110, 16, Fade(RAYWHITE, alpha));
}

/* Screen position of the middle of a drop, with its little hover. */
static Vector2 drop_screen_pos(const Game *g, int i, Vector2 origin)
{
    const Drop *d = &g->drops[i];
    float x, y;
    game_drop_center(g, i, &x, &y);
    /* Fruit drifts slowly on its leaf; things on the ground bob. */
    float bob = d->attached ? sinf(d->age * 2.0f + (float)d->x) * 3.0f
                            : sinf(d->age * 4.0f + (float)d->x) * 2.0f;
    return (Vector2){ origin.x + view_near_x(&view, x) * TILE_SIZE, origin.y + y * TILE_SIZE + bob };
}

static void draw_action_marker(const Game *g, Vector2 origin, Action a)
{
    Rectangle r = tile_rect(origin, a.target);
    Color c = { 255, 255, 255, 255 };
    if (a.type == ACTION_ATTACK) {
        /* Rings the creature, wherever it has got to. */
        const Mob *m = &g->mobs[a.drop];
        if (!m->active || m->id != a.drop_id) return;
        float mx, my;
        game_mob_pos(g, a.drop, &mx, &my);
        Vector2 at = { origin.x + (view_near_x(&view, mx) + 0.5f) * TILE_SIZE, origin.y + (my + 0.5f) * TILE_SIZE };
        DrawRing(at, TILE_SIZE * 0.55f, TILE_SIZE * 0.55f + 2.5f, 0.0f, 360.0f, 32, (Color){ 235, 70, 70, 255 });
        return;
    }
    if (a.type == ACTION_COLLECT) {
        /* Rings the item itself, wherever in its tile it happens to be. */
        const Drop *d = &g->drops[a.drop];
        Vector2 at = { r.x + r.width / 2, r.y + r.height / 2 };
        if (d->active && d->id == a.drop_id) at = drop_screen_pos(g, a.drop, origin);
        DrawRing(at, TILE_SIZE * 0.42f, TILE_SIZE * 0.42f + 2.5f, 0.0f, 360.0f, 32,
                 (Color){ 255, 220, 70, 255 });
        return;
    }
    if (a.type == ACTION_MINE) c = (Color){ 255, 160, 40, 255 };
    if (a.type == ACTION_USE) c = (Color){ 190, 140, 255, 255 };
    if (a.type == ACTION_PLANT) {
        /* The seed, shown where the tree will come up: on top of the soil. */
        r.y -= TILE_SIZE;
        hud_draw_item_icon((ItemId)a.item, r);
        DrawRectangleRec(r, Fade(WHITE, 0.25f));
        DrawRectangleLinesEx(r, 2.0f, (Color){ 110, 220, 120, 255 });
        return;
    }
    if (a.type == ACTION_PLACE) {
        /* A faded copy of the block that is going to be put here. */
        c = (Color){ 110, 220, 120, 255 };
        hud_draw_item_icon((ItemId)a.item, (Rectangle){ r.x - 3, r.y - 3, r.width + 6, r.height + 6 });
        DrawRectangleRec(r, Fade(WHITE, 0.35f));
    }
    DrawRectangleLinesEx(r, 2.0f, c);
}

static void draw_actions(const Game *g, Vector2 origin)
{
    if (g->has_current) {
        draw_action_marker(g, origin, g->current);
        if (g->player.activity == ACTIVITY_MINING) {
            float progress = g->player.timer / g->player.duration;
            DrawRectangleRec(tile_rect(origin, g->current.target), Fade(BLACK, 0.7f * progress));
        }
    }
    for (int i = 0; i < g->queue_count; i++) draw_action_marker(g, origin, g->queue[i]);

    for (int i = 0; i < MAX_DENIALS; i++) {
        const Denial *d = &g->denials[i];
        if (d->ttl <= 0.0f) continue;
        Rectangle r = tile_rect(origin, d->pos);
        Color c = Fade((Color){ 230, 40, 40, 255 }, d->ttl / DENIAL_TIME);
        DrawRectangleRec(r, Fade(c, 0.35f * d->ttl / DENIAL_TIME));
        DrawLineEx((Vector2){ r.x + 6, r.y + 6 },
                   (Vector2){ r.x + r.width - 6, r.y + r.height - 6 }, 3.0f, c);
        DrawLineEx((Vector2){ r.x + r.width - 6, r.y + 6 },
                   (Vector2){ r.x + 6, r.y + r.height - 6 }, 3.0f, c);
    }
}

/* Creatures, each drawn facing the way it is going. */
static void draw_mobs(const Game *g, Vector2 origin)
{
    for (int i = 0; i < MAX_MOBS; i++) {
        const Mob *m = &g->mobs[i];
        if (!m->active) continue;
        const MobDef *def = &content.mobs[m->kind];
        float x, y;
        game_mob_pos(g, i, &x, &y);
        /* The creature is drawn gliding after where the game has it, so that a
         * way made of block-long steps comes out as one flowing movement. */
        static struct {
            uint32_t id;
            float x, y;
        } glide[MAX_MOBS];
        float near_x = view_near_x(&view, x);
        if (glide[i].id != m->id || fabsf(glide[i].x - near_x) > 3.0f || fabsf(glide[i].y - y) > 3.0f) {
            glide[i].id = m->id;
            glide[i].x = near_x;
            glide[i].y = y;
        }
        float follow = fminf(1.0f, GetFrameTime() * MOB_GLIDE);
        glide[i].x += (near_x - glide[i].x) * follow;
        glide[i].y += (y - glide[i].y) * follow;
        Rectangle r = { origin.x + glide[i].x * TILE_SIZE, origin.y + glide[i].y * TILE_SIZE, TILE_SIZE, TILE_SIZE };
        if (r.x < -TILE_SIZE || r.x > seen.x) continue;
        /* On the move it strides through its walking frames, twice to the
         * block, and bobs with each stride. */
        const char *file = m->baby ? def->texture_baby : def->texture;
        int frames = m->baby ? def->walk_baby_count : def->walk_count;
        /* One that has been shorn looks it until it has grown back. */
        if (mob_shorn(m) && def->texture_shorn[0]) {
            file = def->texture_shorn;
            frames = 0;
        }
        float stride;
        if (game_mob_walking(g, i, &stride)) {
            if (frames > 0) {
                int frame = (int)(stride * 2.0f * (float)frames) % frames;
                file = m->baby ? def->walk_baby[frame] : def->walk[frame];
            }
            if (m->to_x != m->x) r.y -= def->hop * TILE_SIZE * fabsf(sinf(stride * 2.0f * PI));
        }
        /* A big one is drawn as long as it is, round the square it is in. */
        if (m->size > 1) {
            float side = (float)m->size * TILE_SIZE;
            r = (Rectangle){ r.x + TILE_SIZE / 2.0f - side / 2, r.y + TILE_SIZE / 2.0f - side / 2, side, side };
        }
        assets_draw_sprite(file, r, m->dir < 0);
        /* One that has been hurt shows how much it has left. */
        if (def->health > 1 && m->hp > 0 && m->hp < def->health) {
            Rectangle bar = { r.x + r.width / 2 - 14, r.y + r.height / 2 - (m->size > 1 ? r.height / 4 : TILE_SIZE / 2.0f) - 7, 28, 4 };
            DrawRectangleRec((Rectangle){ bar.x - 1, bar.y - 1, bar.width + 2, bar.height + 2 }, (Color){ 12, 13, 18, 255 });
            DrawRectangleRec(bar, (Color){ 96, 22, 28, 255 });
            DrawRectangleRec((Rectangle){ bar.x, bar.y, bar.width * (float)m->hp / (float)def->health, bar.height },
                             (Color){ 226, 70, 70, 255 });
        }
    }
}

/* The item layer, in front of the blocks. */
static void draw_drops(const Game *g, Vector2 origin)
{
    const float size = TILE_SIZE * 0.62f;
    Vector2 player = player_center(g);
    for (int i = 0; i < MAX_DROPS; i++) {
        const Drop *d = &g->drops[i];
        if (!d->active) continue;
        Vector2 p = drop_screen_pos(g, i, origin);
        if (p.x < -TILE_SIZE || p.x > seen.x + TILE_SIZE) continue;
        /* Collected ones fly into the player. */
        p.x += (origin.x + player.x - p.x) * d->collect;
        p.y += (origin.y + player.y - p.y) * d->collect;
        /* One about to be gone for good blinks for its last few seconds. */
        float left = DROP_LIFETIME - d->age;
        if (DROP_LIFETIME > 0.0f && !d->attached && d->collect == 0.0f && left < DROP_WARNING &&
            fmodf(left, 0.5f) < 0.2f)
            continue;
        Rectangle at = { p.x - size / 2, p.y - size / 2, size, size };
        if (item_def((ItemId)d->item)->two_tone) {
            ItemStack one = { d->item, 1, d->container };
            assets_draw_item_tones((ItemId)d->item, at, inventory_meter(&g->inventory, &one));
        } else {
            hud_draw_item_icon((ItemId)d->item, at);
        }
    }
}

static const Color SKIN = { 240, 200, 160, 255 }, HAIR = { 90, 60, 40, 255 }, SHIRT = { 50, 90, 190, 255 },
                   TROUSERS = { 40, 60, 120, 255 }, EYE = { 40, 40, 60, 255 };

/* The player's head, `size` across, with its top-left corner at (x, y),
 * looking the way `facing` says. */
static void draw_player_head(float x, float y, float size, int facing, bool eyes_shut)
{
    DrawRectangleRec((Rectangle){ x, y, size, size }, SKIN);
    DrawRectangleRec((Rectangle){ x, y, size, size * 0.28f }, HAIR);
    /* Hair down the back of the head. */
    DrawRectangleRec((Rectangle){ facing > 0 ? x : x + size - size * 0.2f, y, size * 0.2f, size * 0.55f }, HAIR);
    float eye = facing > 0 ? x + size * 0.62f : x + size * 0.24f;
    if (eyes_shut) DrawRectangleRec((Rectangle){ eye - 1, y + size * 0.52f, size * 0.2f, 2 }, EYE);
    else DrawRectangleRec((Rectangle){ eye, y + size * 0.42f, size * 0.14f, size * 0.18f }, EYE);
}

/* Asleep: stretched out on the ground, toward whichever side there is room. */
static void draw_player_asleep(const Game *g, float px, float py)
{
    const Player *p = &g->player;
    int side = !world_is_solid(&g->world, p->feet.x - p->facing, p->feet.y) ? -p->facing
               : !world_is_solid(&g->world, p->feet.x + p->facing, p->feet.y) ? p->facing : 0;
    float ground = py + TILE_SIZE, breath = sinf((float)GetTime() * 1.6f);
    if (side == 0) {
        /* Nowhere to lie: slumped where they sat. */
        DrawRectangleRec((Rectangle){ px + 4, ground - 16, TILE_SIZE - 8, 16 }, SHIRT);
        draw_player_head(px + 7, ground - 32, 18, p->facing, true);
    } else {
        /* Legs where they stood, head on the square beside. */
        float body = 12.0f + breath;
        float from = side > 0 ? px + 2 : px - 10, len = TILE_SIZE + 8;
        DrawRectangleRec((Rectangle){ from, ground - body, len, body }, SHIRT);
        DrawRectangleRec((Rectangle){ side > 0 ? px + 2 : px + TILE_SIZE - 16, ground - 10, 14, 10 }, TROUSERS);
        float hx = side > 0 ? px + TILE_SIZE + 8 : px - 10 - 16;
        draw_player_head(hx, ground - 17, 16, -side, true);
    }
    /* Zs drifting up from the head. */
    float hx = px + TILE_SIZE / 2 + side * (TILE_SIZE * 0.9f);
    for (int k = 0; k < 3; k++) {
        float t = fmodf((float)GetTime() * 0.5f + k / 3.0f, 1.0f);
        int size = 10 + (int)(t * 8.0f);
        DrawText("z", (int)(hx + 6 + t * 14.0f + sinf(t * 6.0f) * 3.0f), (int)(ground - 30 - t * 34.0f), size,
                 Fade(RAYWHITE, (1.0f - t) * 0.9f));
    }
}

/* Meditating: sat cross-legged, eyes shut, with rings of calm spreading out. */
static void draw_player_meditating(const Game *g, float px, float py)
{
    float ground = py + TILE_SIZE, cx = px + TILE_SIZE / 2;
    float time = (float)GetTime(), lift = 2.0f + sinf(time * 1.8f) * 1.5f;
    for (int k = 0; k < 3; k++) {
        float t = fmodf(time * 0.45f + k / 3.0f, 1.0f);
        DrawRing((Vector2){ cx, ground - 22 }, 12.0f + t * 30.0f, 14.0f + t * 30.0f, 0.0f, 360.0f, 40,
                 Fade((Color){ 170, 220, 255, 255 }, (1.0f - t) * 0.55f));
    }
    float y = ground - lift;
    DrawRectangleRec((Rectangle){ cx - 17, y - 9, 34, 9 }, TROUSERS);   /* crossed legs */
    DrawRectangleRec((Rectangle){ cx - 9, y - 26, 18, 18 }, SHIRT);     /* body */
    DrawRectangleRec((Rectangle){ cx - 18, y - 13, 6, 5 }, SKIN);       /* hands on the knees */
    DrawRectangleRec((Rectangle){ cx + 12, y - 13, 6, 5 }, SKIN);
    DrawRectangleRec((Rectangle){ cx - 13, y - 24, 4, 12 }, SHIRT);     /* arms */
    DrawRectangleRec((Rectangle){ cx + 9, y - 24, 4, 12 }, SHIRT);
    draw_player_head(cx - 10, y - 46, 20, g->player.facing, true);

    /* How far along the meditation is, in a little box over the head. */
    Rectangle box = { cx - 34, y - 78, 68, 22 };
    DrawRectangleRec(box, (Color){ 30, 32, 42, 235 });
    DrawRectangleLinesEx(box, 1.0f, (Color){ 150, 200, 240, 255 });
    DrawTriangle((Vector2){ cx - 5, box.y + box.height }, (Vector2){ cx, box.y + box.height + 6 },
                 (Vector2){ cx + 5, box.y + box.height }, (Color){ 150, 200, 240, 255 });
    Rectangle bar = { box.x + 5, box.y + 12, box.width - 10, 5 };
    DrawRectangleRec(bar, (Color){ 22, 40, 60, 255 });
    DrawRectangleRec((Rectangle){ bar.x, bar.y, bar.width * g->vitals.meditation, bar.height },
                     (Color){ 150, 210, 255, 255 });
    const char *text = TextFormat("%d%%", (int)(g->vitals.meditation * 100.0f));
    DrawText(text, (int)(cx - MeasureText(text, 10) / 2.0f), (int)box.y + 1, 10, RAYWHITE);
}

static void draw_player(const Game *g, Vector2 origin)
{
    float x, y;
    game_player_visual_pos(g, &x, &y);
    int grip = game_player_grip_side(g);
    const float inset = 3.0f;
    const Player *p = &g->player;
    /* When climbing, press against the wall and reach for it. */
    float px = origin.x + view_near_x(&view, x) * TILE_SIZE + grip * inset, py = origin.y + y * TILE_SIZE;
    bool settled = p->activity == ACTIVITY_IDLE && !p->climbing;
    if (g->vitals.respawn > 0) {
        /* Dead: laid out on the ground, the colour gone from them. */
        float ground = py + TILE_SIZE;
        Color grey = { 120, 124, 134, 255 }, pale = { 176, 178, 186, 255 };
        DrawRectangleRec((Rectangle){ px - 6, ground - 10, TILE_SIZE + 4, 10 }, grey);
        DrawRectangleRec((Rectangle){ px + TILE_SIZE - 2, ground - 14, 14, 14 }, pale);
        DrawRectangleRec((Rectangle){ px + TILE_SIZE - 2, ground - 14, 14, 4 }, (Color){ 70, 66, 62, 255 });
        DrawLineEx((Vector2){ px + TILE_SIZE + 3, ground - 8 }, (Vector2){ px + TILE_SIZE + 7, ground - 4 }, 1.5f, EYE);
        DrawLineEx((Vector2){ px + TILE_SIZE + 7, ground - 8 }, (Vector2){ px + TILE_SIZE + 3, ground - 4 }, 1.5f, EYE);
        return;
    }
    if (g->vitals.rest == REST_SLEEP && settled) {
        draw_player_asleep(g, px, py);
        return;
    }
    if (g->vitals.rest == REST_MEDITATE && settled) {
        draw_player_meditating(g, px, py);
        return;
    }
    if (game_player_crawling(g)) {
        /* On hands and knees: one tile tall, head leading. */
        float half = TILE_SIZE / 2.0f;
        float head_x = p->facing > 0 ? px + TILE_SIZE - half : px;
        float shuffle = p->activity == ACTIVITY_STEPPING ? sinf(p->timer / p->duration * PI * 2.0f) * 2.0f : 0.0f;
        DrawRectangleRec((Rectangle){ px + 1, py + half, TILE_SIZE - 2, half - 5 }, SHIRT);
        DrawRectangleRec((Rectangle){ px + 3 + shuffle, py + TILE_SIZE - 6, 6, 6 }, TROUSERS);
        DrawRectangleRec((Rectangle){ px + TILE_SIZE - 9 - shuffle, py + TILE_SIZE - 6, 6, 6 }, TROUSERS);
        draw_player_head(head_x + 1, py + 3, half - 2, p->facing, false);
        return;
    }
    /* Hanging from a block overhead: arms up to it. */
    if (p->climbing && p->activity != ACTIVITY_MINING && grip == 0 &&
        world_is_solid(&g->world, p->feet.x, p->feet.y - 2) &&
        (p->activity != ACTIVITY_STEPPING || world_is_solid(&g->world, p->step_to.x, p->step_to.y - 2))) {
        DrawRectangleRec((Rectangle){ px + inset + 2, py - TILE_SIZE - 5, 5, 10 }, SKIN);
        DrawRectangleRec((Rectangle){ px + TILE_SIZE - inset - 7, py - TILE_SIZE - 5, 5, 10 }, SKIN);
    }

    /* Walking: the legs swing and the body bobs, one stride to the block.
     * Working: a small nod in time with it. */
    bool walking = p->activity == ACTIVITY_STEPPING && p->step.dx != 0 && grip == 0 && !p->climbing &&
                   !p->step.climb && !p->step_hop;
    float t = p->activity != ACTIVITY_IDLE && p->duration > 0.0f ? p->timer / p->duration : 0.0f;
    float swing = walking ? sinf(t * PI * 2.0f) : 0.0f;
    float bob = walking ? fabsf(sinf(t * PI * 2.0f)) * 1.5f : 0.0f;
    /* Afloat: kicking, and riding up and down a little. */
    if (game_player_swimming(g) && grip == 0) {
        swing = sinf((float)GetTime() * 7.0f) * 0.7f;
        bob = sinf((float)GetTime() * 2.5f) * 1.5f;
    }
    if (p->activity == ACTIVITY_MINING || p->activity == ACTIVITY_PLACING) bob = -fabsf(sinf((float)GetTime() * 9.0f)) * 1.5f;
    /* A blow: the arm swings through with it. */
    if (p->activity == ACTIVITY_ATTACKING) swing = (float)p->facing * sinf(t * PI) * 2.0f;
    float legs = 11.0f, body_w = TILE_SIZE - 2 * inset, leg_w = body_w / 2 - 1;
    float top = py - bob;
    /* Legs, the far one a shade darker. */
    DrawRectangleRec((Rectangle){ px + inset + 1 - swing * 4.0f, py + TILE_SIZE - legs, leg_w, legs },
                     (Color){ 32, 48, 100, 255 });
    DrawRectangleRec((Rectangle){ px + inset + body_w - leg_w - 1 + swing * 4.0f, py + TILE_SIZE - legs, leg_w, legs },
                     TROUSERS);
    DrawRectangleRec((Rectangle){ px + inset, top, body_w, TILE_SIZE - legs + bob }, SHIRT);
    /* An arm, swinging against the legs. */
    DrawRectangleRec((Rectangle){ px + TILE_SIZE / 2 - 3 + swing * 3.0f, top + 3, 6, 13 }, (Color){ 40, 74, 160, 255 });
    DrawRectangleRec((Rectangle){ px + TILE_SIZE / 2 - 3 + swing * 3.0f, top + 14, 6, 4 }, SKIN);
    draw_player_head(px + inset, top - TILE_SIZE + inset, body_w, p->facing, false);
    if (grip != 0) {
        float hx = grip > 0 ? px + TILE_SIZE - inset - 4 : px + inset - 4;
        DrawRectangleRec((Rectangle){ hx, py - 6, 8, 6 }, SKIN);
        DrawRectangleRec((Rectangle){ hx, py + TILE_SIZE - 10, 8, 6 }, TROUSERS);
    }
}

/* With more than one character about, a small arrow over the one being played. */
static void draw_played_marker(const Game *g, Vector2 origin)
{
    float x, y;
    game_player_visual_pos(g, &x, &y);
    float cx = origin.x + (view_near_x(&view, x) + 0.5f) * TILE_SIZE, top = origin.y + (y - 1.0f) * TILE_SIZE - 8.0f;
    float bob = sinf((float)GetTime() * 4.0f) * 2.0f;
    DrawTriangle((Vector2){ cx - 6, top - 9 + bob }, (Vector2){ cx, top + bob }, (Vector2){ cx + 6, top - 9 + bob },
                 (Color){ 244, 196, 48, 255 });
}

/* While the player sleeps or meditates the world runs fast: arrows say so. */
static void draw_fast_forward(const Game *g)
{
    if (g->vitals.rest == REST_NONE || game_tick_speed(g) <= g->tick_rate) return;
    float cx = GetScreenWidth() / 2.0f, y = 20.0f, time = (float)GetTime();
    const char *text = TextFormat("x%d", game_tick_speed(g) / g->tick_rate);
    float w = 86.0f + (float)MeasureText(text, 20);
    Rectangle box = { cx - w / 2, y - 8, w, 40 };
    DrawRectangleRec(box, (Color){ 22, 30, 48, 220 });
    DrawRectangleLinesEx(box, 2.0f, (Color){ 244, 196, 48, 255 });
    for (int k = 0; k < 3; k++) {
        /* Three arrows, lighting up one after another. */
        float glow = 0.35f + 0.65f * fmaxf(0.0f, sinf(time * 6.0f - k * 1.1f));
        float ax = box.x + 12 + k * 18.0f;
        DrawTriangle((Vector2){ ax, y }, (Vector2){ ax, y + 24 }, (Vector2){ ax + 16, y + 12 },
                     Fade((Color){ 244, 196, 48, 255 }, glow));
    }
    DrawText(text, (int)(box.x + 72), (int)y + 2, 20, RAYWHITE);
}

/* Crystals have a box of their own in the corner: they belong to the player,
 * not to this world or its inventory. */
static void draw_crystals(const Game *g)
{
    const char *text = TextFormat("%d", g->crystals);
    float w = (float)MeasureText(text, 20) + 52.0f;
    Rectangle box = { GetScreenWidth() - w - 12.0f, 12.0f, w, 36.0f };
    DrawRectangleRec(box, (Color){ 22, 30, 48, 235 });
    DrawRectangleLinesEx(box, 2.0f, (Color){ 120, 220, 255, 255 });
    assets_draw_image(content.trade.crystal_icon, (Rectangle){ box.x + 8, box.y + 5, 26, 26 });
    DrawText(text, (int)box.x + 42, (int)box.y + 8, 20, RAYWHITE);
}

/* One line of the readout: white text on its own dark strip. */
static void draw_readout_line(int row, const char *text)
{
    const int size = 16, x = 4, y = 4 + row * (size + 4);
    DrawRectangle(x - 2, y - 2, MeasureText(text, size) + 8, size + 4, Fade((Color){ 20, 20, 24, 255 }, 0.6f));
    DrawText(text, x + 2, y, size, (Color){ 224, 224, 224, 255 });
}

/* The readout in the top left corner: how the game is running, where the
 * player is and what is about. F3 hides it and brings it back. */
/* A sign being written on: what it says as it is being typed. */
static struct {
    bool open;
    TilePos at;
    char text[SIGN_TEXT_MAX + 1];
} sign_edit;

static void sign_open(const Game *g, TilePos at)
{
    const SignText *now = game_sign_at(g, at.x, at.y);
    memset(&sign_edit, 0, sizeof sign_edit);
    sign_edit.open = true;
    sign_edit.at = at;
    if (now) snprintf(sign_edit.text, sizeof sign_edit.text, "%s", now->text);
    while (GetCharPressed() > 0) {
    }
}

static Rectangle sign_panel(void)
{
    return (Rectangle){ (GetScreenWidth() - 380) / 2.0f, (GetScreenHeight() - 170) / 2.0f, 380, 170 };
}

static Rectangle sign_button(int i)
{
    Rectangle p = sign_panel();
    float w = (p.width - 32 - 2 * 8) / 3.0f;
    return (Rectangle){ p.x + 16 + (float)i * (w + 8), p.y + p.height - 50, w, 34 };
}

/* Typing goes on the sign, up to as many letters as one holds. */
static void sign_input(Game *g)
{
    size_t len = strlen(sign_edit.text);
    for (int c = GetCharPressed(); c > 0; c = GetCharPressed()) {
        if (c < 32 || c >= 127 || len >= SIGN_TEXT_MAX) continue;
        sign_edit.text[len++] = (char)c;
        sign_edit.text[len] = '\0';
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && len > 0) sign_edit.text[len - 1] = '\0';
    Vector2 m = GetMousePosition();
    bool click = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    bool gone = game_sign_at(g, sign_edit.at.x, sign_edit.at.y) == NULL && !tile_def(world_get(&g->world, sign_edit.at.x, sign_edit.at.y))->written &&
                tile_def(world_get(&g->world, sign_edit.at.x, sign_edit.at.y))->pair_of == TILE_AIR;
    if (gone || IsKeyPressed(KEY_ESCAPE) || (click && CheckCollisionPointRec(m, sign_button(2)))) {
        sign_edit.open = false; /* left as it was */
    } else if (IsKeyPressed(KEY_ENTER) || (click && CheckCollisionPointRec(m, sign_button(0)))) {
        game_sign_write(g, sign_edit.at.x, sign_edit.at.y, sign_edit.text);
        sign_edit.open = false;
    } else if (click && CheckCollisionPointRec(m, sign_button(1))) {
        game_queue_action(g, ACTION_MINE, sign_edit.at.x, sign_edit.at.y);
        sign_edit.open = false;
    }
}

static void sign_draw(void)
{
    static const char *labels[3] = { "Done", "Take down", "Cancel" };
    if (!sign_edit.open) return;
    Rectangle p = sign_panel();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.4f));
    DrawRectangleRec(p, (Color){ 28, 30, 38, 245 });
    DrawRectangleLinesEx(p, 2.0f, (Color){ 120, 122, 134, 255 });
    DrawText("Sign", (int)p.x + 16, (int)p.y + 14, 24, RAYWHITE);
    const char *room = TextFormat("%d / %d", (int)strlen(sign_edit.text), SIGN_TEXT_MAX);
    DrawText(room, (int)(p.x + p.width - 16) - MeasureText(room, 14), (int)p.y + 20, 14, GRAY);
    bool blink = (int)(GetTime() * 2.0) % 2 == 0;
    Rectangle box = { p.x + 16, p.y + 56, p.width - 32, 40 };
    DrawRectangleRec(box, (Color){ 18, 19, 24, 255 });
    DrawRectangleLinesEx(box, 2.0f, RAYWHITE);
    DrawText(TextFormat("%s%s", sign_edit.text, blink ? "_" : ""), (int)box.x + 10, (int)box.y + 10, 20, RAYWHITE);
    for (int i = 0; i < 3; i++) {
        Rectangle b = sign_button(i);
        bool hot = CheckCollisionPointRec(GetMousePosition(), b);
        DrawRectangleRec(b, hot ? (Color){ 80, 84, 100, 255 } : (Color){ 54, 57, 70, 255 });
        DrawRectangleLinesEx(b, 2.0f, (Color){ 120, 122, 134, 255 });
        DrawText(labels[i], (int)(b.x + (b.width - (float)MeasureText(labels[i], 18)) / 2), (int)b.y + 8, 18, RAYWHITE);
    }
}

static bool readout_shown = true;

static void draw_hud(const Game *g, const MapCamera *cam)
{
    draw_crystals(g);
    if (IsKeyPressed(KEY_F3)) readout_shown = !readout_shown;
    if (!readout_shown) return;

    const World *w = &g->world;
    bool day;
    float phase = game_day_phase(g, &day);
    int row = 0;
    draw_readout_line(row++, TextFormat("%d fps   %d ticks/s%s", GetFPS(), game_tick_speed(g), cam->follow ? "" : "   camera free"));
    /* Where the character is - or, with nobody, where the view is. */
    int at_x = g->character_count > 0 ? g->player.feet.x : world_wrap_x(w, (int)floorf(cam->center.x / TILE_SIZE));
    int at_y = g->character_count > 0 ? g->player.feet.y : (int)floorf(cam->center.y / TILE_SIZE);
    draw_readout_line(row++, TextFormat("XY: %d / %d", at_x, world_row_to_y(w, at_y)));
    draw_readout_line(row++, TextFormat("Biome: %s", content.biomes[world_biome(w, at_x)].label));
    draw_readout_line(row++, TextFormat("Mobs: %d / %d land, %d / %d water", mobs_near_player(g), content.mob_cap,
                                        mobs_near_player_in_water(g), content.mob_water_cap));
    /* The date: the season, and the time passed as years, days and the ticks of today. */
    int year, date, tick;
    game_date(g, &year, &date, &tick);
    const RulesDef *u = &content.rules;
    bool snows = u->season_count > 0 && u->seasons[game_season(g)].snows;
    draw_readout_line(row++, TextFormat("%s%sYear %d, Day %d, Tick %d (%s %d%%)%s", u->season_count > 0 ? u->seasons[game_season(g)].label : "",
                                        u->season_count > 0 ? "   " : "", year, date, tick, day ? "day" : "night",
                                        (int)(phase * 100.0f), game_rain(g) > 0.0f ? (snows ? "   snow" : "   rain") : ""));
}

/* Everything that exists only while a world is open. */
typedef struct {
    Game game;
    MapCamera cam;
    Hud hud;
    BenchUi bench;
    TradeUi trade;
    /* What the profile file holds: the player's crystals and shared chest. */
    int saved_crystals;
    Container saved_shared;
    PauseMenu pause;
    bool paused;
    PlayerUi player_ui;
    /* With nobody in the world: the name being typed for a first character,
     * and the portal they will be made at. */
    char new_name[CHARACTER_NAME_MAX];
    TilePos create_at;
    float since_save;
    char name[WORLD_NAME_MAX + 1];
    char path[256];
} Session;

static void session_begin(Session *s, const MainMenu *menu)
{
    snprintf(s->name, sizeof s->name, "%s", menu->name);
    snprintf(s->path, sizeof s->path, "%s", menu->path);
    s->hud = (Hud){ 0 };
    s->bench = (BenchUi){ .selected = -1, .amount = 1 };
    s->trade = (TradeUi){ 0 };
    s->player_ui = (PlayerUi){ 0 };
    s->new_name[0] = '\0';
    s->create_at = (TilePos){ s->game.world.spawn_x, s->game.world.spawn_y };
    /* Crystals come with the player into whichever world is opened. */
    profile_load(PROFILE_PATH, &s->game);
    s->saved_crystals = s->game.crystals;
    s->saved_shared = s->game.inventory.containers[SHARED_CONTAINER];
    s->paused = false;
    s->since_save = 0.0f;
    s->cam = (MapCamera){ .follow = true, .zoom_level = ZOOM_DEFAULT, .scale = zoom_step(ZOOM_DEFAULT) };
    s->cam.center = (Vector2){ (s->game.player.feet.x + 0.5f) * TILE_SIZE,
                               (float)s->game.player.feet.y * TILE_SIZE };
}

static void session_save_profile(Session *s)
{
    if (!DirectoryExists(SAVE_DIR)) MakeDirectory(SAVE_DIR);
    if (!profile_save(PROFILE_PATH, &s->game)) return;
    s->saved_crystals = s->game.crystals;
    s->saved_shared = s->game.inventory.containers[SHARED_CONTAINER];
}

static void session_save(Session *s)
{
    if (!DirectoryExists(SAVE_DIR)) MakeDirectory(SAVE_DIR);
    if (!save_game(&s->game, s->path)) fprintf(stderr, "could not save %s\n", s->path);
    session_save_profile(s);
    s->since_save = 0.0f;
}

static void session_update(Session *s, const Keybinds *keys, float dt)
{
    update_view(&s->cam, &s->game.world);
    if (sign_edit.open) {
        /* Writing on a sign: the keys are for that, and the world goes on meanwhile. */
        game_set_move_input(&s->game, 0, 0);
        sign_input(&s->game);
        game_update(&s->game, dt);
        update_camera(&s->game, &s->cam, dt);
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        s->paused = true;
        pause_menu_open(&s->pause);
        game_set_move_input(&s->game, 0, 0);
        return;
    }
    playerui_set_view(&ui_view, zoom_step(s->cam.zoom_level) >= MAP_FADE_FROM);
    if (s->game.character_count == 0) {
        /* Nobody to play: somebody has to be made first. */
        hud_close_all(&s->hud);
        benchui_close(&s->bench, &s->game);
        s->player_ui = (PlayerUi){ 0 };
        if (handle_nobody(&s->game, &s->cam, s->new_name, &s->create_at)) {
            s->new_name[0] = '\0';
            s->create_at = (TilePos){ s->game.world.spawn_x, s->game.world.spawn_y };
        }
    } else {
        handle_input(&s->game, &s->cam, &s->hud, &s->bench, &s->trade, &s->player_ui, keys);
    }
    game_update(&s->game, dt);
    /* What belongs to the player is written down as soon as it changes, not
     * only when the world is saved. */
    if (s->game.crystals != s->saved_crystals ||
        memcmp(&s->saved_shared, &s->game.inventory.containers[SHARED_CONTAINER], sizeof s->saved_shared) != 0)
        session_save_profile(s);
    update_camera(&s->game, &s->cam, dt);

    s->since_save += dt;
    if (s->since_save >= AUTOSAVE_SECONDS) session_save(s);
}

static void session_draw(Session *s)
{
    Game *g = &s->game;
    update_view(&s->cam, &g->world);
    Vector2 origin = view_origin(&s->cam);
    bool on_map = zoom_step(s->cam.zoom_level) < MAP_FADE_FROM;

    ClearBackground((Color){ 20, 22, 28, 255 });
    draw_sky(g, origin);
    if (s->cam.scale > MAP_FADE_TO) {
        /* The world itself, scaled by the zoom. */
        BeginMode2D((Camera2D){ .zoom = zoom });
        assets_set_season(game_season(g));
        draw_world(g, origin);
        draw_mobs(g, origin);
        /* Every character, the one being played last so that it is on top. */
        for (int n = 0; n < g->character_count; n++) {
            int c = (g->controlled + 1 + n) % g->character_count;
            game_use_character(g, c);
            draw_player(g, origin);
            if (g->character_count > 1 && c == g->controlled && game_alive(g)) draw_played_marker(g, origin);
        }
        draw_drops(g, origin);
        draw_weather(g, origin);
        /* An arrow in the air: a quick streak from the bow to what it was loosed at. */
        if (g->shot.live) {
            float t = g->shot.age / 0.2f;
            Vector2 a = { origin.x + view_near_x(&view, g->shot.from_x) * TILE_SIZE, origin.y + g->shot.from_y * TILE_SIZE };
            Vector2 b = { a.x + (g->shot.to_x - g->shot.from_x) * TILE_SIZE, origin.y + g->shot.to_y * TILE_SIZE };
            Vector2 head = { a.x + (b.x - a.x) * fminf(1.0f, t * 2.0f), a.y + (b.y - a.y) * fminf(1.0f, t * 2.0f) };
            Vector2 tail = { a.x + (b.x - a.x) * fmaxf(0.0f, t * 2.0f - 0.35f), a.y + (b.y - a.y) * fmaxf(0.0f, t * 2.0f - 0.35f) };
            if (t < 0.6f) DrawLineEx(tail, head, 2.0f, (Color){ 240, 232, 200, 255 });
        }
        /* Darkness falls over the world, but not over the interface. */
        draw_light(g, origin);
        draw_fog(g, origin);
        /* What each of them has queued. */
        for (int n = 0; n < g->character_count; n++) {
            game_use_character(g, (g->controlled + 1 + n) % g->character_count);
            draw_actions(g, origin);
        }
        if (!on_map && !s->paused && !s->cam.dragging && IsCursorOnScreen() &&
            !hud_mouse_over(&s->hud, &g->inventory) && !benchui_mouse_over(&s->bench, g, &ui_view) &&
            !playerui_mouse_over(&s->player_ui) &&
            !tradeui_mouse_over(&s->trade, g, &ui_view))
            DrawRectangleLinesEx(tile_rect(origin, tile_at_screen(&s->cam, GetMousePosition())), 1.0f,
                                 Fade(WHITE, 0.5f));
        EndMode2D();
    }
    /* Zoomed far enough out, the world map fades in over it. */
    if (s->cam.scale < MAP_FADE_FROM) {
        float span = MAP_FADE_FROM - MAP_FADE_TO;
        float alpha = span > 0.0f ? (MAP_FADE_FROM - s->cam.scale) / span : 1.0f;
        draw_world_map(g, &s->cam, alpha > 1.0f ? 1.0f : alpha, GetFrameTime());
    }
    if (!on_map) {
        benchui_draw(&s->bench, g, &ui_view);
        tradeui_draw(&s->trade, g, &ui_view);
    }
    playerui_set_view(&ui_view, !on_map);
    if (g->character_count == 0) {
        /* Nobody in the world: just the readout, and the making of somebody. */
        draw_hud(g, &s->cam);
        if (!s->paused) draw_nobody(g, s->new_name, s->create_at);
        return;
    }
    sync_storage_slots(g, &s->hud);
    draw_hud(g, &s->cam);
    draw_fast_forward(g);
    playerui_draw(&s->player_ui, g);
    hud_draw(&s->hud, &g->inventory);
    sign_draw();
}

int main(void)
{
    char error[256];
    if (!content_load(DATA_DIR, error, sizeof error)) {
        fprintf(stderr, "could not load game data: %s\n", error);
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "BlockGame");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); /* Escape opens the pause menu instead of quitting */
    assets_load(DATA_DIR);

    Keybinds keys;
    keybinds_load(&keys);
    MainMenu menu = { 0 };
    static Session session;
    bool playing = false, quit = false;

    while (!quit && !WindowShouldClose()) {
        float dt = fminf(GetFrameTime(), 0.1f);
        if (playing && !session.paused) session_update(&session, &keys, dt);

        BeginDrawing();
        if (!playing) {
            MainMenuResult r = main_menu_frame(&menu);
            bool ok = true;
            if (r == MAIN_MENU_QUIT) quit = true;
            if (r == MAIN_MENU_LOAD) ok = load_game(&session.game, menu.path);
            if (r == MAIN_MENU_CREATE) {
                uint32_t seed = (uint32_t)time(NULL) ^ (uint32_t)(GetTime() * 1000000.0);
                WorldOptions options = main_menu_world_options(&menu);
                ok = game_init_with(&session.game, main_menu_world_width(&menu), content.worldgen.height, seed,
                                    WORLD_TERRAIN, &options);
                /* A new world has nobody in it: its first character is made by the player. */
                if (ok) game_clear_characters(&session.game);
            }
            if (r == MAIN_MENU_LOAD || r == MAIN_MENU_CREATE) {
                if (ok) {
                    session_begin(&session, &menu);
                    if (r == MAIN_MENU_CREATE) session_save(&session);
                    menu.creating = false;
                    menu.error[0] = '\0';
                    playing = true;
                } else {
                    snprintf(menu.error, sizeof menu.error, "Could not open \"%s\".", menu.name);
                }
            }
        } else {
            session_draw(&session);
            if (session.paused) {
                PauseResult r = pause_menu_frame(&session.pause, &session.game, &keys, session.name);
                if (r == PAUSE_RESUME) session.paused = false;
                if (r == PAUSE_QUIT) {
                    session_save(&session);
                    game_free(&session.game);
                    playing = false;
                    menu.listed = false; /* show the world as it was left */
                }
            }
        }
        EndDrawing();
    }

    if (playing) {
        session_save(&session);
        game_free(&session.game);
    }
    assets_unload();
    CloseWindow();
    return 0;
}
