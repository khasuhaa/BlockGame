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
#include "playermodel.h"
#include "termui.h"
#include "dao.h"
#include "daoui.h"
#include "tradeui.h"
#include "train.h"
#include "trainui.h"
#include "world3d.h"
#include "digital.h"
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

/* The screen of a storage network, when one is open. */
static TermUi term_ui;

/* The menu over something on the rails. */
static TrainUi train_ui;

/* The terminal of a storage network at a square, if there is one there. */
static bool terminal_at(const Game *g, TilePos t)
{
    for (int layer = 0; layer < BLOCK_LAYERS; layer++)
        if (tile_def(world_get_at(&g->world, layer, t.x, t.y))->net_role == NET_TERMINAL) return true;
    return false;
}
static float pair_shift(const World *w, int head, int y);


#define DATA_DIR "data"
#define PROFILE_PATH TextFormat("%s/profile.txt", main_menu_saves())
#define TILE_SIZE 32
#define DRAG_THRESHOLD 6.0f /* pixels the mouse must travel before a press becomes a pan */
#define CAMERA_FOLLOW_SPEED 8.0f
#define AUTOSAVE_SECONDS 120.0f
#define BACKGROUND_DIM 0.18f /* how much darker the background layer is drawn */
#define WALL_DIM 0.5f        /* and the backwall behind that */

/* How deep the world is, back from its front. Ground and anything else solid is a block that goes all the way
 * back; characters, creatures and what lies about are in the front half, with the thin things of the foreground
 * behind them; the background layer - trees, benches - is the back half; and the backwall closes the back. */
#define Z_FRONT 0.0f
#define Z_MID (-(float)TILE_SIZE)
#define Z_BACK (-2.0f * (float)TILE_SIZE)
#define Z_BODY (-0.36f * (float)TILE_SIZE) /* the middle of a character, front to back */
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
static DaoUi dao_ui;
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
    /* And to thirty-seconds of a pixel of the world, which a number can hold exactly: then the edge one block ends
     * at and the edge the next begins at come out the very same, at any zoom, and nothing shows through between
     * them - nor between the squares of darkness laid over them. */
    view.origin = (Vector2){ roundf(view.origin.x * 32.0f) / 32.0f, roundf(view.origin.y * 32.0f) / 32.0f };
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

/* The station whose board is over a square: the board is three squares wide, and the station is its middle. */
static bool station_under(const Game *g, TilePos t, TilePos *out)
{
    for (int dx = -1; dx <= 1; dx++) {
        if (!tile_def(world_get(&g->world, t.x + dx, t.y))->station) continue;
        *out = (TilePos){ world_wrap_x(&g->world, t.x + dx), t.y };
        return true;
    }
    return false;
}

/* Puts up the menu of whatever on the rails is under a square, if there is anything to be done with it from here. */
static bool rail_menu(const Game *g, TilePos t)
{
    TrainOption scratch[TRAIN_MAX_OPTIONS];
    const ItemStack *held = inventory_selected(&g->inventory);
    int vehicle = train_under(g, t.x, t.y);
    if (vehicle < 0 || (held->count > 0 && item_def((ItemId)held->id)->vehicle.is && !g->ride)) return false;
    if (train_options(g, vehicle, t.x, scratch) == 0) return false;
    trainui_show(&train_ui, vehicle, t.x);
    return true;
}

/* The tile under a screen point. Its x is not wrapped into the world. */
/* The sign drawn over a point of the world, given in blocks: where it is drawn, not the squares it is in. */
static bool sign_under(const Game *g, float fx, float fy, TilePos *out)
{
    int y = (int)floorf(fy);
    for (int head = (int)floorf(fx) - 2; head <= (int)floorf(fx) + 1; head++) {
        const TileDef *def = tile_def(world_get(&g->world, head, y));
        if (!def->written || def->pair == TILE_AIR) continue;
        float left = (float)head + pair_shift(&g->world, head, y) / TILE_SIZE;
        if (fx < left || fx >= left + 2.0f) continue;
        *out = (TilePos){ world_wrap_x(&g->world, head), y };
        return true;
    }
    return false;
}

/* Where a point of the screen lies in the world, in blocks, at a depth into it. */
static Vector2 world_at(Vector2 screen, float z)
{
    Vector2 p = world3d_at_depth(screen, zoom, z);
    return (Vector2){ (p.x - view.origin.x) / TILE_SIZE, (p.y - view.origin.y) / TILE_SIZE };
}

/* The world being looked at, for telling what a point of the screen is on. */
static const Game *looked_at;

/* The square under a point of the screen: the block whose face is there; failing that, what is there where the
 * characters are; failing that, what stands in the background behind it. (The view has depth: these are not
 * all the same square.) */
static TilePos tile_at_screen(const MapCamera *cam, Vector2 screen)
{
    (void)cam;
    Vector2 front = world_at(screen, Z_FRONT), body = world_at(screen, Z_BODY), behind = world_at(screen, Z_MID);
    TilePos face = { (int)floorf(front.x), (int)floorf(front.y) }, mid = { (int)floorf(body.x), (int)floorf(body.y) };
    TilePos back = { (int)floorf(behind.x), (int)floorf(behind.y) };
    if (!looked_at) return mid;
    const World *w = &looked_at->world;
    if (world_in_bounds(w, 0, face.y) && tile_def(world_get(w, face.x, face.y))->solid) return face;
    if (world_in_bounds(w, 0, mid.y) && (world_get(w, mid.x, mid.y) != TILE_AIR || world_get_at(w, LAYER_BACKGROUND, mid.x, mid.y) == TILE_AIR)) return mid;
    if (world_in_bounds(w, 0, back.y) && world_get_at(w, LAYER_BACKGROUND, back.x, back.y) != TILE_AIR && !tile_def(world_get(w, back.x, back.y))->solid)
        return back;
    return mid;
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
    /* (Something on the rails that is open has its slots there, as a chest has.) */
    hud->extra_count = trainui_storage_slots(g, &ui_view, rects, HUD_MAX_EXTRA_SLOTS - RELIC_SLOTS, &container);
    if (hud->extra_count == 0) hud->extra_count = benchui_storage_slots(g, &ui_view, rects, HUD_MAX_EXTRA_SLOTS - RELIC_SLOTS, &container);
    for (int i = 0; i < hud->extra_count; i++)
        hud->extra[i] = (HudSlot){ rects[i], { SLOT_CONTAINER, container, i } };
    hud->storage_count = hud->extra_count;
    /* The character's relics, beside their armour, wherever there are relics to be had. */
    uint16_t relics = hud->armor_open ? game_relic_box((Game *)g) : 0;
    for (int i = 0; relics != 0 && i < RELIC_SLOTS && hud->extra_count < HUD_MAX_EXTRA_SLOTS; i++)
        hud->extra[hud->extra_count++] = (HudSlot){ hud_relic_rect(i), { SLOT_CONTAINER, relics, i } };
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
        /* (Work in hand at a bench is not dropped for a key: it has a button to stop it.) */
        if (!game_bench_busy(g)) {
            game_cancel_actions(g);
            hud_close_all(hud);
            benchui_close(bench, g);
        }
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
    /* Something to wear gets an "Equip" button in the same place: it goes on, and what was on comes off into its slot. */
    bool wearable = chosen->count > 0 && hud->eat == HUD_EAT_NONE && item_def((ItemId)chosen->id)->equip != EQUIP_NONE;
    /* (And something that is taken for what it does, not to fill a stomach, is drunk.) */
    bool potion = chosen->count > 0 && item_def((ItemId)chosen->id)->food <= 0.0f &&
                  (item_def((ItemId)chosen->id)->effect_ticks != 0 || item_def((ItemId)chosen->id)->cures || item_def((ItemId)chosen->id)->recalls);
    hud->use_label = wearable ? "Equip" : potion && hud->eat == HUD_EAT_READY ? "Drink" : potion ? "Taken" : NULL;
    /* (A manual is studied; a pill, and the like, is taken.) */
    if (chosen->count > 0 && dao_item(item_def((ItemId)chosen->id)) && item_def((ItemId)chosen->id)->food <= 0.0f)
        hud->use_label = hud->eat != HUD_EAT_READY ? "Not now" : item_def((ItemId)chosen->id)->manual ? "Study" : "Take";
    if (wearable) hud->eat = HUD_EAT_READY;

    /* A press that starts on the inventory never reaches the world. */
    bool on_hud = hud_handle_input(hud, &g->inventory);
    if (hud->eat_clicked && wearable) inventory_equip_selected(&g->inventory);
    else if (hud->eat_clicked) game_eat(g);
    /* A stack dragged out of the inventory and let go over the world is
     * dropped into it - but not one let go over a menu. */
    if (hud->discard && !playerui_mouse_over(player) && !benchui_mouse_over(bench, g, &ui_view) &&
        !tradeui_mouse_over(trade, g, &ui_view))
        game_discard(g, hud->discard_slot, hud->discard_count);
    if (on_hud) return;
    if (daoui_handle_input(&dao_ui, g)) return;
    bool on_players = playerui_handle_input(player, g);
    if (player->switched) {
        /* Another character is being played: the view and the menus go over to them. */
        hud_close_all(hud);
        cam->follow = true;
        cam->anchored = false;
    }
    if (player->tapped) {
        /* Their own button: the view comes back to them, wherever it had been taken. */
        cam->follow = true;
        cam->anchored = false;
    }
    if (on_players) return;
    if (tradeui_handle_input(trade, g, &ui_view)) return;
    if (trainui_handle_input(&train_ui, g, &ui_view)) return;
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
            if (!game_bench_busy(g)) benchui_close(bench, g);
        }
    }
    bool on_map = zoom_step(cam->zoom_level) < MAP_FADE_FROM;

    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        hud_close_all(hud);
        /* A click elsewhere closes what is open - but not a bench with work in hand: that stays up till it is done. */
        if (!game_bench_busy(g)) benchui_close(bench, g);
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
            TilePos t = tile_at_screen(cam, mouse), sign;
            t.x = world_wrap_x(&g->world, t.x);
            const TileDef *def = tile_def(game_bench_at(g, t.x, t.y));
            bool opens = def->bench || def->trade || def->storage_columns > 0 || def->leads_dim >= 0;
            int drop = game_drop_at(g, world_at(mouse, Z_BODY).x, world_at(mouse, Z_BODY).y);
            /* (Fruit hangs further back than what lies about: it is looked for where it is.) */
            if (drop < 0) {
                int hung = game_drop_at(g, world_at(mouse, Z_MID).x, world_at(mouse, Z_MID).y);
                if (hung >= 0 && g->drops[hung].attached) drop = hung;
            }
            /* An item under the cursor comes before the tile behind it.
             * Something that opens offers a choice if it can also be removed;
             * otherwise it just opens. */
            int mob = game_mob_at(g, world_at(mouse, Z_BODY).x, world_at(mouse, Z_BODY).y);
            /* What has not been uncovered cannot be clicked on: nothing in it, and not the square. */
            if (game_explored(g, t.x, t.y) == 0) {}
            /* (Something lying there that there is no room for is not in the way of what is behind it.) */
            else if (drop >= 0 && game_can_collect(g, drop)) game_click_drop(g, drop);
            /* A creature under the cursor is hunted. */
            else if (mob >= 0) game_click_mob(g, mob);
            /* A second click on a square with something queued takes it back. */
            else if (game_unqueue_tile(g, t.x, t.y)) {}
            /* Something on the rails, near enough to do anything with: its menu. (With another to put down in hand, that
             * is put down; from too far off, the click is somewhere to go.) */
            else if (rail_menu(g, t)) {}
            /* Something in hand that reaches a storage network from a distance: its screen comes up. */
            else if (digital_remote(g, &sign)) termui_open(&term_ui, sign, true);
            /* A terminal: its screen, from close by; from further off the character goes to it. */
            else if (terminal_at(g, t) && !IsKeyDown(KEY_LEFT_SHIFT)) {
                long far_x = world_dx(&g->world, g->player.feet.x, t.x), far_y = t.y - g->player.feet.y;
                if (far_x * far_x + far_y * far_y <= 36) termui_open(&term_ui, t, false);
                else game_queue_action(g, ACTION_MOVE, t.x + (far_x > 0 ? -1 : 1), t.y);
            }
            /* With something that turns things in hand, a click on a thing with a front turns it. */
            else if (inventory_selected(&g->inventory)->count > 0 && item_def((ItemId)inventory_selected(&g->inventory)->id)->turns &&
                     (tile_def(world_get(&g->world, t.x, t.y))->turns_to != TILE_AIR || tile_def(world_get_at(&g->world, LAYER_BACKGROUND, t.x, t.y))->turns_to != TILE_AIR))
                game_queue_action(g, ACTION_USE, t.x, t.y);
            /* Something that is used by a click - a lever, a button - is used; unless what is
             * in hand is what digs it, when the click is a dig like any other. */
            else if (def->use_becomes != TILE_AIR && !(inventory_selected(&g->inventory)->count > 0 && def->tool_class >= 0 &&
                                                       item_def((ItemId)inventory_selected(&g->inventory)->id)->tool_class == def->tool_class))
                game_queue_action(g, ACTION_USE, t.x, t.y);
            else if (opens && def->removable) benchui_show_popup(bench, t);
            /* A bed has the same little menu: to sleep in it, or to take it up. */
            else if (!opens && game_bed_at(g, t.x, t.y) != TILE_AIR) benchui_show_popup(bench, t);
            /* A bench or a portal shows its menu from wherever the player is;
             * a chest has to be walked to. */
            else if (opens && game_open_bench(g, t.x, t.y)) {}
            else if (opens) game_queue_action(g, ACTION_USE, t.x, t.y);
            /* A sign is read and written on, not dug at. */
            else if (sign_under(g, world_at(mouse, Z_BODY).x, world_at(mouse, Z_BODY).y, &sign))
                sign_open(g, sign);
            /* So is a station's board, which is three squares wide. */
            else if (station_under(g, t, &sign)) sign_open(g, sign);
            else game_click_tile(g, t.x, t.y);
        }
        cam->pressed = false;
        cam->dragging = false;
    }
}

static void update_camera(const Game *g, MapCamera *cam, float dt)
{
    /* Gone to another place altogether - through a portal, or out of the bottom of the sky: the view goes with
     * them at once, whatever it was doing. */
    static const Game *seen_game;
    static int seen_travels;
    if (seen_game != g || seen_travels != g->travels) {
        if (seen_game == g) {
            cam->follow = true;
            cam->anchored = cam->dragging = false;
            cam->center = (Vector2){ ((float)g->player.feet.x + 0.5f) * TILE_SIZE, (float)g->player.feet.y * TILE_SIZE };
        }
        seen_game = g;
        seen_travels = g->travels;
    }
    if (cam->follow) {
        Vector2 target = player_center(g);
        float k = fminf(1.0f, dt * CAMERA_FOLLOW_SPEED);
        /* (Gone to another place altogether, the view goes with them at once.) */
        if (fabsf(target.y - cam->center.y) > 80.0f * TILE_SIZE) k = 1.0f;
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
    /* In a world with more places than one, the view keeps to the place it is in: nothing past its top or its
     * bottom is to be seen - not the edge of it, nor the sky of the place over it. */
    if (g->world.region_count > 0) {
        const World *w = &g->world;
        int row = (int)floorf(cam->center.y / TILE_SIZE), r0, r1, in = world_region_at(w, row < 0 ? 0 : row >= w->height ? w->height - 1 : row);
        world_region_rows(w, in, &r0, &r1);
        float half = (float)GetScreenHeight() / 2.0f / (cam->scale / TILE_SIZE);
        float lo = (float)(r0 + (in >= 0 ? 1 : 0)) * TILE_SIZE + half, hi = (float)r1 * TILE_SIZE - half;
        if (in < 0) lo = 0.0f;
        if (lo > hi) cam->center.y = in < 0 ? fminf(cam->center.y, hi > 0.0f ? hi : 0.0f) : (lo + hi) / 2.0f;
        else cam->center.y = fminf(fmaxf(cam->center.y, lo), hi);
    }
}

static Color mix(Color a, Color b, float t)
{
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                    (unsigned char)(a.b + (b.b - a.b) * t), 255 };
}

/* Sky, with the sun crossing it by day and the moon by night. */
/* Only the place the middle of the view is in is drawn: the others the world has are not to be seen from it. */
static void view_rows(const World *w, int *y0, int *y1)
{
    int mid = (*y0 + *y1) / 2, r0, r1;
    world_region_rows(w, world_region_at(w, mid < 0 ? 0 : mid >= w->height ? w->height - 1 : mid), &r0, &r1);
    if (*y0 < r0) *y0 = r0;
    if (*y1 >= r1) *y1 = r1 - 1;
}

static void draw_sky(const Game *g, Vector2 origin)
{
    const World *w = &g->world;
    float light = game_daylight(g);
    Color sky = mix((Color){ 12, 16, 40, 255 }, (Color){ 135, 190, 235, 255 }, light);
    /* (Drawn straight to the screen, not through the zoom.) */
    DrawRectangle(0, (int)(origin.y * zoom), GetScreenWidth(), (int)(w->height * TILE_SIZE * zoom) + 1, sky);
    /* (A place with no sky has the dark behind it; and some have a sky of their own colours.) */
    for (int i = 0; i < w->region_count; i++) {
        const DimensionDef *d = w->regions[i].dim >= 0 ? &content.dimensions[w->regions[i].dim] : NULL;
        Color back = { 10, 9, 14, 255 };
        if (d && d->has_sky_color)
            back = mix((Color){ d->sky_night[0], d->sky_night[1], d->sky_night[2], 255 }, (Color){ d->sky_day[0], d->sky_day[1], d->sky_day[2], 255 }, d->sky ? light : 1.0f);
        else if (world_sky_top(w, w->regions[i].y0) >= 0) continue;
        DrawRectangle(0, (int)((origin.y + (float)w->regions[i].y0 * TILE_SIZE) * zoom), GetScreenWidth(),
                      (int)((float)w->regions[i].height * TILE_SIZE * zoom) + 1, back);
    }

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

/* Something two blocks wide that is put up against a block - a sign - is
 * drawn centred on the block that holds it, where that is one under it or
 * over it: half a block to the left of the two squares it is in if that is
 * the block at its left half, half a block to the right if only its right
 * half has one. `head` is its left square. How far across to draw it, in pixels. */
static float pair_shift(const World *w, int head, int y)
{
    int mount = world_mount_block(w, head, y), way = -1;
    if (mount == MOUNT_NONE) {
        mount = world_mount_block(w, head + 1, y);
        way = 1;
    }
    if (mount != MOUNT_FLOOR && mount != MOUNT_CEILING) return 0.0f;
    /* (Not into a block that is in the way.) */
    if (world_is_solid(w, way < 0 ? head - 1 : head + 2, y)) return 0.0f;
    return (float)way * TILE_SIZE / 2.0f;
}

/* Set while those are being drawn: they go on after the rest of the blocks, since they lie across the squares. */
static bool drawing_pairs;

/* Whether a block of the foreground is a thing that stands in the front half of the world - a chest - and not
 * material that goes all the way back. */
static bool stands_alone(const TileDef *def)
{
    if (def->model != 0) return def->model != W3D_BLOCK;
    return def->solid && def->storage_columns > 0;
}

/* How a block of a layer stands in the depth of the world, for whatever is drawn of it next. */
static void block_depth(const Game *g, int layer, const TileDef *def, int x, int y, float px, float py)
{
    const World *w = &g->world;
    if (!world3d_active()) return;
    Vector2 mid = world3d_center();
    /* Only the sides that are turned toward the middle of the view. */
    int sides = (px > mid.x ? W3D_LEFT : 0) | (px + TILE_SIZE < mid.x ? W3D_RIGHT : 0) | (py > mid.y ? W3D_TOP : 0) | (py + TILE_SIZE < mid.y ? W3D_BOTTOM : 0);
    /* What kind of thing it is: what the data says, or else what can be told of it. A plant is anything that is
     * sown, or grows, or is the top of something that does; a thing that is worked at or holds things is one
     * thing and not material. (The picture settles the rest: see world3d.h.) */
    bool plant = def->soil_count > 0 || def->grows_into != TILE_AIR || def->grows_top != TILE_AIR || def->topped_by != TILE_AIR || def->cut_to != TILE_AIR;
    /* Only what is built and put down to be used has a shape made for it: benches, what holds things, beds, lamps,
     * and the columns and stairs things are built of. Anything else that is not a block is its picture, a pixel deep. */
    bool thing = def->bench || def->trade || def->storage_columns > 0 || def->sleep_speed > 0.0f || def->light > 0 || def->supports || def->steps ||
                 (def->pair_of != TILE_AIR && tile_def(def->pair_of)->sleep_speed > 0.0f);
    World3dModel model = def->model != 0 ? (World3dModel)def->model : plant ? W3D_CROSS : thing ? W3D_BOX : W3D_PANEL;
    if (layer == LAYER_FOREGROUND && def->solid && !stands_alone(def)) {
        /* Material goes all the way back; and shows no side that is against more of it. */
        static const int beside[4][3] = { { -1, 0, W3D_LEFT }, { 1, 0, W3D_RIGHT }, { 0, -1, W3D_TOP }, { 0, 1, W3D_BOTTOM } };
        for (int i = 0; i < 4; i++) {
            const TileDef *next = tile_def(world_get(w, x + beside[i][0], y + beside[i][1]));
            if (next->solid && !next->see_through && !stands_alone(next) && world_in_bounds(w, 0, y + beside[i][1])) sides &= ~beside[i][2];
        }
        world3d_solid(Z_FRONT, Z_BACK, sides, W3D_BLOCK);
    } else if (layer == LAYER_FOREGROUND && def->solid) {
        /* A solid thing: the front half is its own. */
        world3d_solid(Z_FRONT - 0.04f * TILE_SIZE, Z_MID + 0.04f * TILE_SIZE, sides, model);
    } else if (layer == LAYER_FOREGROUND) {
        /* A thing of the foreground that can be walked through: flat, behind whoever walks through it. */
        world3d_solid(-0.72f * TILE_SIZE, -0.96f * TILE_SIZE, sides, def->model != 0 ? model : W3D_PANEL);
    } else if (def->wire && tile_def(world_get(w, x, y))->solid && !tile_def(world_get(w, x, y))->see_through) {
        world3d_plane(Z_FRONT); /* wire over the face of a block */
    } else if (world_is_solid(w, x, y) && !tile_def(world_get(w, x, y))->solid) {
        /* A square of something of the background that is stood on all the same - the foot of a portal: it is as
         * deep as the ground it stands in for, front to back. */
        world3d_solid(Z_FRONT, Z_BACK, sides, model == W3D_BOX || model == W3D_BLOCK ? model : W3D_MASS);
    } else if (def->rail) {
        /* Track lies where what runs on it runs: under the characters' feet, not back among the trees. */
        world3d_solid(-0.1f * TILE_SIZE, -0.66f * TILE_SIZE, sides, W3D_FRAME);
    } else if (def->window) {
        world3d_solid(Z_BACK + 0.12f * TILE_SIZE, Z_BACK, sides, W3D_PANEL); /* a window is in the backwall */
    } else if (model == W3D_PANEL && def->climbable) {
        world3d_solid(Z_BACK + 0.3f * TILE_SIZE, Z_BACK + 0.04f * TILE_SIZE, sides, W3D_PANEL); /* something climbed is against the back */
    } else if (model == W3D_PANEL) {
        /* Any other flat thing stands where the flat things of the foreground do: just behind whoever walks past. */
        world3d_solid(-0.72f * TILE_SIZE, -0.96f * TILE_SIZE, sides, W3D_PANEL);
    } else {
        /* The background layer: the back half. */
        world3d_solid(Z_MID - 0.02f * TILE_SIZE, Z_BACK + 0.02f * TILE_SIZE, sides, model);
    }
}

/* One block of a layer, in whichever of its looks applies. */
static void draw_block(const Game *g, int layer, TileType t, int x, int y, float px, float py, float dim)
{
    if (!drawing_pairs && (tile_def(t)->pair_floats || (tile_def(t)->pair_of != TILE_AIR && tile_def(tile_def(t)->pair_of)->pair_floats))) return;
    if (tile_def(t)->mimics != MIMIC_NONE && layer == LAYER_FOREGROUND) t = world_looks_like(&g->world, x, y);
    const TileDef *def = tile_def(t);
    int wx = world_wrap_x(&g->world, x);
    block_depth(g, layer, def, x, y, px, py);
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
    } else if (def->rail && def->texture_ramp[0] && !train_rail(&g->world, x + 1, y) && train_rail(&g->world, x + 1, y - 1)) {
        /* Track that climbs to the next piece: a block up, to the right... */
        assets_draw_sprite(def->texture_ramp, (Rectangle){ px, py - TILE_SIZE, TILE_SIZE, 2 * TILE_SIZE }, false);
    } else if (def->rail && def->texture_ramp[0] && !train_rail(&g->world, x - 1, y) && train_rail(&g->world, x - 1, y - 1)) {
        /* ...or to the left. */
        assets_draw_sprite(def->texture_ramp, (Rectangle){ px, py - TILE_SIZE, TILE_SIZE, 2 * TILE_SIZE }, true);
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
    world3d_flat();
}

static void draw_world(const Game *g, Vector2 origin)
{
    const World *w = &g->world;
    /* Columns run past either end of the world: reads wrap round it. */
    int x0 = (int)floorf(-origin.x / TILE_SIZE), y0 = (int)floorf(-origin.y / TILE_SIZE);
    int x1 = x0 + (int)seen.x / TILE_SIZE + 1, y1 = y0 + (int)seen.y / TILE_SIZE + 1;
    /* (What is further back is seen smaller: there is more of it round the edges of the view.) */
    int more_x = (x1 - x0) / 8 + 2, more_y = (y1 - y0) / 8 + 2;
    x0 -= more_x, x1 += more_x, y0 -= more_y, y1 += more_y;
    if (y0 < 0) y0 = 0;
    if (y1 >= w->height) y1 = w->height - 1;
    view_rows(w, &y0, &y1);

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            TileType t = world_get(w, x, y), behind = world_get_at(w, LAYER_BACKGROUND, x, y);
            int wx = world_wrap_x(w, x);
            float px = origin.x + (float)(x * TILE_SIZE), py = origin.y + (float)(y * TILE_SIZE);
            /* (Paint is a wash of its colour over the backwall or the block it is on, as bright as that is lit.) */
            int wall_paint = world_paint(w, true, x, y), block_paint = world_paint(w, false, x, y);
            if (!tile_def(t)->solid || tile_def(t)->see_through || stands_alone(tile_def(t))) {
                /* (A window is a hole in the backwall: there, what is behind the wall shows, and the wall does not.) */
                bool hole = tile_def(behind)->window;
                /* (The backwall is the back of the world.) */
                world3d_plane(Z_BACK);
                if (!hole)
                    assets_draw_tile(world_get_at(w, LAYER_WALL, x, y), wx, y, px, py, TILE_SIZE, WALL_DIM,
                                     TILE_PART_BODY);
                world3d_flat();
                if (!hole && wall_paint > 0 && wall_paint <= content.tint_count && world_get_at(w, LAYER_WALL, x, y) != TILE_AIR) {
                    const unsigned char *c = content.tints[wall_paint - 1];
                    float lit = 1.0f - WALL_DIM;
                    world3d_push(Z_BACK);
                    DrawRectangleRec((Rectangle){ px, py, TILE_SIZE, TILE_SIZE },
                                     (Color){ (unsigned char)(c[0] * lit), (unsigned char)(c[1] * lit), (unsigned char)(c[2] * lit), 170 });
                    world3d_pop();
                }
                /* (Something giving light is not dimmed for being behind.) */
                bool shining = (tile_def(behind)->light > 0 || tile_def(behind)->beam_light > 0) && light_glow(g, x, y) > 0;
                if (!tile_def(behind)->front) draw_block(g, LAYER_BACKGROUND, behind, x, y, px, py, shining ? 0.0f : BACKGROUND_DIM);
            }
            if (!tile_def(t)->front) draw_block(g, LAYER_FOREGROUND, t, x, y, px, py, 0.0f);
            /* (Wire on a block lies over the face of it.) */
            if (tile_def(behind)->wire && tile_def(t)->solid && !tile_def(t)->see_through) draw_block(g, LAYER_BACKGROUND, behind, x, y, px, py, 0.0f);
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
    /* Signs, and anything else two blocks wide that is put up against a block: each centred on the block that holds it. */
    drawing_pairs = true;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0 - 2; x <= x1 + 1; x++) {
            for (int layer = BLOCK_LAYERS - 1; layer >= 0; layer--) {
                TileType t = world_get_at(w, layer, x, y);
                const TileDef *def = tile_def(t);
                if (!def->pair_floats || def->pair == TILE_AIR) continue;
                /* (Not one hidden behind a block.) */
                if (layer != LAYER_FOREGROUND && tile_def(world_get(w, x, y))->solid && !tile_def(world_get(w, x, y))->see_through) continue;
                float px = origin.x + (float)(x * TILE_SIZE) + pair_shift(w, x, y), py = origin.y + (float)(y * TILE_SIZE);
                float dim = layer == LAYER_FOREGROUND ? 0.0f : BACKGROUND_DIM;
                draw_block(g, layer, t, x, y, px, py, dim);
                if (world_get_at(w, layer, x + 1, y) == def->pair) draw_block(g, layer, def->pair, x + 1, y, px + TILE_SIZE, py, dim);
            }
        }
    }
    drawing_pairs = false;
    /* What is written on each sign, in one line across it. */
    for (int y = y0; y <= y1; y++) {
        for (int x = x0 - 2; x <= x1 + 1; x++) {
            const SignText *text = tile_def(world_get(w, x, y))->written && !tile_def(world_get(w, x, y))->station ? game_sign_at(g, x, y) : NULL;
            if (!text || !text->text[0]) continue;
            float px = origin.x + (float)(x * TILE_SIZE) + pair_shift(w, x, y), py = origin.y + (float)(y * TILE_SIZE);
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
                block_depth(g, layer, tile_def(t), x, y, px, py);
                assets_draw_tile(t, world_wrap_x(w, x), y, px, py, TILE_SIZE,
                                 layer == LAYER_BACKGROUND ? BACKGROUND_DIM : 0.0f,
                                 assets_tile_part(t, world_get_at(w, layer, x, y - 1),
                                                  world_get_at(w, layer, x, y + 1)));
                world3d_flat();
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
    /* How dark each amount of light is shown, in quarters of a level (a corner is the mean of four squares). */
    unsigned char shade[4 * LIGHT_MAX + 1];
    float deepest = content.rules.light_darkness > 0.0f ? content.rules.light_darkness : 1.0f;
    float contrast = content.rules.light_contrast > 0.0f ? content.rules.light_contrast : 1.0f;
    for (int i = 0; i <= 4 * LIGHT_MAX; i++)
        shade[i] = (unsigned char)(255.0f * deepest * (1.0f - powf((float)i / (4 * LIGHT_MAX), contrast)) + 0.5f);
    for (int cy = 1; cy < rows - 1; cy++) {
        for (int cx = 1; cx < cols - 1; cx++) {
            Color corner[4]; /* top left, bottom left, bottom right, top right */
            static const int at[4][2] = { { -1, -1 }, { -1, 0 }, { 0, 0 }, { 0, -1 } };
            int darkest = 0;
            for (int k = 0; k < 4; k++) {
                int bx = cx + at[k][0], by = cy + at[k][1];
                int sum = light[by * cols + bx] + light[by * cols + bx + 1] +
                          light[(by + 1) * cols + bx] + light[(by + 1) * cols + bx + 1];
                int dark = shade[sum];
                corner[k] = (Color){ 2, 2, 6, (unsigned char)dark };
                if (dark > darkest) darkest = dark;
            }
            if (darkest == 0) continue;
            Rectangle r = { origin.x + (float)((x0 + cx) * TILE_SIZE), origin.y + (float)((y0 + cy) * TILE_SIZE),
                            TILE_SIZE, TILE_SIZE };
            /* (raylib takes the corners anticlockwise from the top left.) */
            DrawRectangleGradientEx(r, corner[0], corner[1], corner[2], corner[3]);
        }
    }

    /* Something that gives light is shown brighter than what it lights. It is
     * the thing itself that is: drawn again over the dark, as bright as it
     * glows, and then once more added on top, so that it is brighter than
     * daylight makes anything. Nothing round it is touched. (Where someone or
     * something stands in front of it, it is left as it is, behind them.) */
    /* (These are drawn where they stand, behind whatever is in front of them: the darkness was not.) */
    rlDrawRenderBatchActive();
    if (world3d_active()) rlEnableDepthTest();
    static uint8_t *taken;
    static int taken_capacity;
    if (cols * rows > taken_capacity) {
        uint8_t *bigger = realloc(taken, (size_t)cols * rows);
        if (!bigger) return;
        taken = bigger;
        taken_capacity = cols * rows;
    }
    memset(taken, 0, (size_t)cols * rows);
#define TAKE(tx, ty) do { \
        int kx = world_dx(&g->world, world_wrap_x(&g->world, x0), world_wrap_x(&g->world, (tx))), ky = (ty) - y0; \
        if (kx >= 0 && kx < cols && ky >= 0 && ky < rows) taken[ky * cols + kx] = 1; \
    } while (0)
    Game *all = (Game *)g;
    for (int n = 0; n < g->character_count; n++) {
        game_use_character(all, (g->controlled + 1 + n) % g->character_count);
        for (int up = 0; up < 2; up++) {
            TAKE(g->player.feet.x, g->player.feet.y - up);
            TAKE(g->player.step_to.x, g->player.step_to.y - up);
        }
    }
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!g->mobs[i].active) continue;
        float mx, my;
        game_mob_pos(g, i, &mx, &my);
        for (int up = 0; up < 2; up++) {
            TAKE((int)floorf(mx), (int)floorf(my) - up);
            TAKE((int)ceilf(mx), (int)floorf(my) - up);
        }
    }
#undef TAKE
    for (int cy = 1; cy < rows - 1; cy++) {
        for (int cx = 1; cx < cols - 1; cx++) {
            int x = x0 + cx, y = y0 + cy, glow = light_glow(g, x, y);
            if (glow <= 0 || taken[cy * cols + cx]) continue;
            float px = origin.x + (float)(x * TILE_SIZE), py = origin.y + (float)(y * TILE_SIZE);
            for (int layer = BLOCK_LAYERS - 1; layer >= 0; layer--) {
                TileType t = world_get_at(&g->world, layer, x, y);
                const TileDef *def = tile_def(t);
                if (def->light <= 0 && def->beam_light <= 0) continue;
                if (def->light_needs_fuel && game_fuel_at(g, world_wrap_x(&g->world, x), y) <= 0) continue;
                if (glow > light[cy * cols + cx]) draw_block(g, layer, t, x, y, px, py, (float)shade[4 * glow] / 255.0f);
                if (content.rules.light_bloom > 0.0f) {
                    BeginBlendMode(BLEND_ADDITIVE);
                    draw_block(g, layer, t, x, y, px, py, 1.0f - content.rules.light_bloom * (float)glow / LIGHT_MAX);
                    EndBlendMode();
                }
            }
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
            /* (What swims or flies is in the middle of it; what walks stands on its feet.) */
            bool afloat = content.mobs[m->kind].swims || content.mobs[m->kind].flies;
            r = (Rectangle){ r.x + TILE_SIZE / 2.0f - side / 2, afloat ? r.y + TILE_SIZE / 2.0f - side / 2 : r.y + TILE_SIZE - side, side, side };
        }
        /* (A creature is its picture, a pixel deep, in among the characters.) */
        world3d_solid(Z_BODY + 0.26f * TILE_SIZE, Z_BODY - 0.26f * TILE_SIZE, W3D_ALL, W3D_PANEL);
        assets_draw_sprite(file, r, m->dir < 0);
        world3d_flat();
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
            world3d_solid(Z_BODY + 0.07f * TILE_SIZE, Z_BODY - 0.07f * TILE_SIZE, W3D_ALL, W3D_PANEL);
            assets_draw_item_tones((ItemId)d->item, at, inventory_meter(&g->inventory, &one));
            world3d_flat();
        } else if (d->attached && d->collect == 0.0f) {
            /* Fruit hangs on its tree, which is in the back half of the world: just in front of the leaves, behind whoever walks by. */
            world3d_solid(Z_MID + 0.04f * TILE_SIZE, Z_MID - 0.10f * TILE_SIZE, W3D_ALL, W3D_PANEL);
            hud_draw_item_icon((ItemId)d->item, at);
            world3d_flat();
        } else {
            world3d_solid(Z_BODY + 0.07f * TILE_SIZE, Z_BODY - 0.07f * TILE_SIZE, W3D_ALL, W3D_PANEL);
            hud_draw_item_icon((ItemId)d->item, at);
            world3d_flat();
        }
    }
}

/* The player's head, `size` across, with its top-left corner at (x, y),
 * looking the way `facing` says. */
/* The character as they are dressed: their own skin, and over it what they have on. */
static void draw_body(const Game *g, float x, float ground, BodyPose pose)
{
    /* (Inmost first: what is on the legs, the feet, the body, then the head.) */
    static const int order[ARMOR_SLOTS] = { EQUIP_LEGS, EQUIP_BOOTS, EQUIP_CHEST, EQUIP_HEAD };
    const char *worn[ARMOR_SLOTS];
    int count = 0;
    unsigned packs = 0;
    for (int i = 0; i < ARMOR_SLOTS; i++) {
        const ItemStack *on = &g->inventory.armor[order[i]];
        if (on->count <= 0 || !item_def((ItemId)on->id)->worn[0]) continue;
        if (item_def((ItemId)on->id)->pack) packs |= 1u << count;
        worn[count++] = item_def((ItemId)on->id)->worn;
    }
    /* (In the world they have depth, and stand at their place in it.) */
    playermodel_depth(world3d_active(), Z_BODY);
    playermodel_draw((Vector2){ x, ground }, TILE_SIZE / 16.0f, &pose, content.rules.player_skin, worn, count, packs);
    playermodel_depth(false, 0.0f);
}

/* Which side a sleeper lies down toward: wherever there is room, behind them for choice; 0 for nowhere. */
static int sleeping_side(const Game *g)
{
    const Player *p = &g->player;
    return !world_is_solid(&g->world, p->feet.x - p->facing, p->feet.y) ? -p->facing
           : !world_is_solid(&g->world, p->feet.x + p->facing, p->feet.y) ? p->facing : 0;
}

/* Zs drifting up from a sleeper's head. */
static void draw_sleep(float px, float ground, int side)
{
    float hx = px + TILE_SIZE / 2 + side * (TILE_SIZE * 0.9f);
    for (int k = 0; k < 3; k++) {
        float t = fmodf((float)GetTime() * 0.5f + k / 3.0f, 1.0f);
        int size = 10 + (int)(t * 8.0f);
        DrawText("z", (int)(hx + 6 + t * 14.0f + sinf(t * 6.0f) * 3.0f), (int)(ground - 30 - t * 34.0f), size,
                 Fade(RAYWHITE, (1.0f - t) * 0.9f));
    }
}

/* Round someone meditating: rings of calm spreading out behind them... */
static void draw_calm(float cx, float ground)
{
    for (int k = 0; k < 3; k++) {
        float t = fmodf((float)GetTime() * 0.45f + k / 3.0f, 1.0f);
        DrawRing((Vector2){ cx, ground - 22 }, 12.0f + t * 30.0f, 14.0f + t * 30.0f, 0.0f, 360.0f, 40,
                 Fade((Color){ 170, 220, 255, 255 }, (1.0f - t) * 0.55f));
    }
}

/* ...and how far along the meditation is, in a little box over the head. */
static void draw_meditation(const Game *g, float cx, float y)
{
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

/* How each character was last drawn, so that the next frame can carry on from it. */
typedef struct {
    BodyPose pose;
    float side;   /* how far round to the right (or, below nothing, the left) of facing the viewer */
    float shift, bob, phase, idle, last_x, last_y;
    bool set;
} BodyEase;

static BodyEase body_ease[MAX_CHARACTERS];

/* What is on the rails. */
static void draw_vehicles(const Game *g, Vector2 origin)
{
    for (int i = 0; i < MAX_VEHICLES; i++) {
        const Vehicle *v = &g->vehicles[i];
        if (!v->used) continue;
        const ItemDef *def = item_def((ItemId)v->item);
        Texture2D tex = assets_texture(def->vehicle.texture);
        float x, y;
        train_visual(g, i, &x, &y);
        /* (Drawn at the size it was drawn at, standing on the bottom of its square of track, about the middle of it.) */
        float wide = tex.id ? (float)tex.width : (float)(def->vehicle.width * TILE_SIZE), tall = tex.id ? (float)tex.height : 2.0f * TILE_SIZE;
        float px = origin.x + view_near_x(&view, x) * TILE_SIZE + TILE_SIZE / 2.0f - wide / 2.0f, py = origin.y + (y + 1.0f) * TILE_SIZE - tall;
        /* (As deep as the room there is for it; whoever rides is inside.) */
        world3d_solid(-0.08f * TILE_SIZE, -0.66f * TILE_SIZE, W3D_ALL, W3D_FRAME);
        assets_draw_sprite(def->vehicle.texture, (Rectangle){ px, py, wide, tall }, false);
        world3d_flat();
        /* An engine with its fire lit: a glow at the firebox. */
        if (def->vehicle.engine && v->dir != 0 && v->burn > 0)
            DrawRectangleRec((Rectangle){ px + 6.0f, py + tall - 28.0f, 6.0f, 4.0f }, (Color){ 240, 120, 40, 255 });
    }
}

/* The boards of the stations, over whatever has pulled in at them, and what each is called. */
static void draw_stations(const Game *g, Vector2 origin)
{
    const World *w = &g->world;
    int x0 = (int)floorf(-origin.x / TILE_SIZE) - 2, y0 = (int)floorf(-origin.y / TILE_SIZE) - 1;
    int x1 = x0 + (int)((float)GetScreenWidth() / zoom / TILE_SIZE) + 5, y1 = y0 + (int)((float)GetScreenHeight() / zoom / TILE_SIZE) + 3;
    if (y0 < 0) y0 = 0;
    if (y1 >= w->height) y1 = w->height - 1;
    view_rows(w, &y0, &y1);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            TileType t = world_get(w, x, y);
            if (!tile_def(t)->station) continue;
            float px = origin.x + (float)(x * TILE_SIZE), py = origin.y + (float)(y * TILE_SIZE);
            assets_draw_tile(t, world_wrap_x(w, x), y, px, py, TILE_SIZE, 0.0f, TILE_PART_BODY);
            const SignText *text = game_sign_at(g, x, y);
            if (!text || !text->text[0]) continue;
            /* Written smaller when it would not otherwise fit on the board. */
            float wide = (float)MeasureText(text->text, SIGN_TEXT_SIZE), room = 3.0f * TILE_SIZE - 22.0f;
            float scale = wide > room ? room / wide : 1.0f;
            rlPushMatrix();
            rlTranslatef(px + TILE_SIZE / 2.0f, py + 10.0f, 0.0f);
            rlScalef(scale, scale, 1.0f);
            DrawText(text->text, (int)(-wide / 2.0f), -SIGN_TEXT_SIZE / 2, SIGN_TEXT_SIZE, (Color){ 40, 26, 14, 255 });
            rlPopMatrix();
        }
    }
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
    float cx = px + TILE_SIZE / 2.0f, ground = py + TILE_SIZE;
    bool settled = p->activity == ACTIVITY_IDLE && !p->climbing;
    bool dead = g->vitals.respawn > 0;
    if (!dead && g->ride > 0 && g->ride <= MAX_VEHICLES && g->vehicles[g->ride - 1].used) {
        /* Riding: sat in a seat, looking out; or stood on a handcar, working the handle while it goes. */
        const Vehicle *on = &g->vehicles[g->ride - 1];
        if (item_def((ItemId)on->item)->vehicle.hand) {
            /* (Stood at their end of the deck, facing the pump; while they work it, bending to it - faster as it goes faster.) */
            bool working = g->controlled == g->live && g->move_dx != 0 && g->move_dy == 0;
            float pump = working ? sinf((float)GetTime() * (5.0f + on->speed)) * 26.0f : 0.0f;
            draw_body(g, cx, ground - 26.0f,
                      (BodyPose){ .facing = 1, .lean = working ? 14.0f + pump * 0.35f : 0.0f, .arm = { 68.0f + pump, 68.0f + pump }, .tint = WHITE });
        } else {
            /* (Only as much of them as is above the sill: a carriage is not as tall inside as they are.) */
            float vx, vy;
            train_visual(g, g->ride - 1, &vx, &vy);
            float top = (origin.y + (vy - 1.0f) * TILE_SIZE) * zoom;
            rlDrawRenderBatchActive();
            BeginScissorMode(0, (int)top, GetScreenWidth(), (int)(26.0f * zoom));
            draw_body(g, cx, ground - 9.0f, (BodyPose){ .facing = 1, .turn = PLAYERMODEL_SIDE, .sink = 10.0f, .leg = { 82.0f, 82.0f }, .arm = { 20.0f, 20.0f }, .tint = WHITE });
            EndScissorMode();
        }
        return;
    }
    bool asleep = !dead && g->vitals.rest == REST_SLEEP && settled, meditating = !dead && g->vitals.rest == REST_MEDITATE && settled;
    int lie = asleep ? sleeping_side(g) : 0;
    /* What they are doing says how they should stand; they are then moved toward that a little each frame, so
     * that nothing about them jumps from one pose to the next. */
    BodyEase *e = &body_ease[g->live % MAX_CHARACTERS];
    float dt = GetFrameTime(), now = (float)GetTime();
    if (dt > 0.1f) dt = 0.1f;
    float t = p->activity != ACTIVITY_IDLE && p->duration > 0.0f ? p->timer / p->duration : 0.0f;
    bool stepping = p->activity == ACTIVITY_STEPPING, crawling = game_player_crawling(g);
    bool swimming = game_player_swimming(g) && grip == 0 && !crawling;
    bool holding = !crawling && (p->climbing || grip != 0) && !game_player_flying(g);
    bool walking = stepping && p->step.dx != 0 && !holding && !p->step.climb && !p->step_hop && !crawling && !swimming;
    /* The stride keeps its own time: it quickens with the pace, but only so far, and does not start again at each block. */
    float moved = fabsf(x - e->last_x) + fabsf(y - e->last_y);
    float pace = dt > 0.0f && moved < 2.0f ? moved / dt : 0.0f;
    e->last_x = x, e->last_y = y;
    float cadence = fminf(fmaxf(pace * 0.3f, 1.1f), 1.8f);
    if (stepping) e->phase = fmodf(e->phase + dt * cadence * 2.0f * PI, 2.0f * PI);
    float swing = sinf(e->phase);
    /* Stood doing nothing, they turn to face out of the screen; to go anywhere or do anything, to the side. */
    e->idle = p->activity == ACTIVITY_IDLE && !holding && !crawling && !swimming ? e->idle + dt : 0.0f;
    if (game_player_flying(g)) swimming = false;
    /* (Holding on to a wall they face it - but not while they work at something on the other side of them.) */
    bool busy = p->activity == ACTIVITY_MINING || p->activity == ACTIVITY_PLACING || p->activity == ACTIVITY_ATTACKING;
    int facing = grip != 0 && !busy ? grip : p->facing;
    BodyPose to = { .tint = WHITE };
    float side = e->idle > 0.18f ? 0.0f : (float)facing * PLAYERMODEL_SIDE, shift = 0.0f, bob = 0.0f, jet = 0.0f;
    int rise = stepping ? p->step_to.y - p->feet.y : 0;
    if (dead) {
        /* Dead: laid out on the ground, the colour gone from them. */
        to.pitch = -90.0f, side = -PLAYERMODEL_SIDE, shift = -12.0f;
    } else if (asleep && lie != 0) {
        /* Asleep: on their back, feet where they stood and head on the square beside. */
        to.pitch = -90.0f, side = -(float)lie * PLAYERMODEL_SIDE, shift = lie > 0 ? -12.0f : 12.0f;
    } else if (asleep) {
        /* Nowhere to lie: slumped where they sat. */
        to.lean = 25.0f, to.sink = 10.0f, to.leg[0] = to.leg[1] = 80.0f, to.arm[0] = to.arm[1] = 20.0f, side = 0.0f;
    } else if (meditating) {
        /* Sat cross-legged, a little off the ground, rising and settling. */
        to.sink = 10.0f, to.leg[0] = to.leg[1] = 82.0f, to.arm[0] = to.arm[1] = 35.0f, side = 0.0f;
        bob = 2.0f + sinf(now * 1.8f) * 1.5f;
    } else if (crawling) {
        /* On hands and knees, so that all of them fits under a block, head leading. */
        float shuffle = stepping ? swing : 0.0f;
        to.lean = 90.0f, to.head = 65.0f, to.sink = 6.0f, shift = -(float)p->facing * 4.0f;
        to.arm[0] = 150.0f + shuffle * 20.0f, to.arm[1] = 150.0f - shuffle * 20.0f;
        to.leg[0] = -60.0f - shuffle * 12.0f, to.leg[1] = -60.0f + shuffle * 12.0f;
    } else if (game_player_flying(g)) {
        /* In the air on a jetpack: upright and hanging from it while it hovers, leant into the way it is going when it goes. */
        bool going = stepping && p->step.dx != 0;
        to.lean = going ? 32.0f : 4.0f, to.head = going ? 18.0f : 0.0f;
        to.arm[0] = to.arm[1] = going ? -18.0f : 10.0f;
        to.leg[0] = going ? -14.0f : 6.0f, to.leg[1] = going ? -24.0f : -6.0f;
        bob = sinf(now * 5.0f) * 1.2f;
        if (!going && !stepping) side = 0.0f;
        /* (What is holding them up: two jets of flame from the pack, which the body's own drawing puts under it.) */
        jet = 7.0f + 2.0f * sinf(now * 31.0f);
    } else if (swimming) {
        /* Afloat: leaning into it, kicking, and riding up and down a little. */
        float kick = sinf(now * 5.0f);
        to.lean = stepping ? 55.0f : 20.0f, to.head = stepping ? 35.0f : 0.0f;
        to.leg[0] = kick * 22.0f, to.leg[1] = -kick * 22.0f;
        to.arm[0] = 60.0f + kick * 25.0f, to.arm[1] = 60.0f - kick * 25.0f;
        bob = sinf(now * 2.5f) * 1.5f;
    } else if (holding && grip == 0 && p->activity != ACTIVITY_MINING && world_is_solid(&g->world, p->feet.x, p->feet.y - 2) &&
               (!stepping || world_is_solid(&g->world, p->step_to.x, p->step_to.y - 2))) {
        /* Hanging from a block overhead: arms up to it, one hand past the other. */
        float reach = stepping ? swing * 14.0f : 0.0f;
        to.arm[0] = 172.0f + reach, to.arm[1] = 172.0f - reach;
        to.leg[0] = reach * 0.5f, to.leg[1] = -reach * 0.5f;
    } else if (holding) {
        /* On a wall, or on something behind them: reaching up and holding on, one hand over the other. */
        float reach = stepping ? swing * 25.0f : 0.0f;
        to.arm[0] = 125.0f + reach, to.arm[1] = 125.0f - reach;
        to.leg[0] = 18.0f - reach * 0.6f, to.leg[1] = 18.0f + reach * 0.6f;
    } else if (stepping && p->step_hop) {
        /* A hop across a gap: off one foot, knees up in the middle of it, the arms thrown with it. */
        float air = sinf(t * PI);
        to.lean = 12.0f;
        to.leg[0] = 12.0f + air * 48.0f, to.leg[1] = -8.0f - air * 34.0f;
        to.arm[0] = -air * 52.0f, to.arm[1] = air * 62.0f;
    } else if (stepping && rise < 0) {
        /* Up onto a block: leaning to it, the leading knee brought up and over, the hands out to the edge of it. */
        float over = sinf(t * PI);
        to.lean = 6.0f + (1.0f - t) * 16.0f;
        to.leg[0] = over * 78.0f, to.leg[1] = -over * 20.0f;
        to.arm[0] = 28.0f + (1.0f - t) * 46.0f, to.arm[1] = 16.0f + (1.0f - t) * 40.0f;
    } else if (stepping && rise > 0) {
        /* Down off one, or falling: leant back from it, arms out and up for balance, the knees giving to land. */
        to.lean = -5.0f;
        to.leg[0] = 26.0f - t * 10.0f, to.leg[1] = -14.0f;
        to.arm[0] = 128.0f - t * 30.0f, to.arm[1] = 104.0f - t * 30.0f;
    } else if (walking) {
        /* Walking: arm and leg swing against each other, and the body rides up as each foot passes under it. */
        to.lean = 5.0f;
        to.leg[0] = swing * 30.0f, to.leg[1] = -swing * 30.0f;
        to.arm[0] = -swing * 24.0f, to.arm[1] = swing * 24.0f;
        bob = (1.0f - cosf(e->phase * 2.0f)) * 0.6f;
    } else if (e->idle > 0.0f) {
        /* Stood still: breathing. */
        bob = (sinf(now * 1.8f) + 1.0f) * 0.2f;
    }
    if (p->activity == ACTIVITY_MINING) {
        /* At a block: the near arm drawn back slowly and brought down hard, the body going with it. */
        float u = fmodf(now * 2.4f, 1.0f);
        float up = u < 0.62f ? u / 0.62f : 1.0f - (u - 0.62f) / 0.38f;
        up = u < 0.62f ? up * up * (3.0f - 2.0f * up) : up * up;
        to.arm[0] = 40.0f + up * 105.0f;
        if (!crawling) to.lean += 9.0f - up * 7.0f, to.arm[1] += 12.0f;
    }
    /* Setting something down: a reach out and back. A blow: the arm swings through. */
    if (p->activity == ACTIVITY_PLACING) to.arm[0] = 55.0f + sinf(t * PI) * 40.0f;
    if (p->activity == ACTIVITY_ATTACKING) to.arm[0] = 150.0f - sinf(t * PI * 0.5f) * 120.0f, to.lean += sinf(t * PI) * 8.0f;
    /* Just attacked: for a moment the arms go up and about, and they start back from it. */
    if (!dead && game_flail(g) > 0.0f) {
        float wave = sinf(now * 36.0f);
        to.arm[0] = 138.0f + wave * 46.0f, to.arm[1] = 138.0f - wave * 46.0f;
        to.leg[0] += wave * 12.0f, to.leg[1] -= wave * 12.0f;
        to.lean -= 12.0f;
        to.head += 10.0f;
    }
    if (!e->set) {
        e->pose = to, e->side = side, e->shift = shift, e->bob = bob;
        e->set = true;
    }
    float limb = 1.0f - expf(-26.0f * dt), body = 1.0f - expf(-14.0f * dt), round = 1.0f - expf(-11.0f * dt);
    for (int i = 0; i < 2; i++) {
        e->pose.arm[i] += (to.arm[i] - e->pose.arm[i]) * limb;
        e->pose.leg[i] += (to.leg[i] - e->pose.leg[i]) * limb;
    }
    e->pose.lean += (to.lean - e->pose.lean) * body;
    e->pose.head += (to.head - e->pose.head) * body;
    e->pose.sink += (to.sink - e->pose.sink) * body;
    /* (Lying down and getting up take a little longer than the rest.) */
    e->pose.pitch += (to.pitch - e->pose.pitch) * (1.0f - expf(-8.0f * dt));
    e->shift += (shift - e->shift) * body;
    e->bob += (bob - e->bob) * limb;
    e->side += (side - e->side) * round;
    BodyPose pose = e->pose;
    pose.tint = dead ? (Color){ 130, 134, 144, 255 } : WHITE;
    pose.facing = e->side < 0.0f ? -1 : 1;
    pose.turn = PLAYERMODEL_SIDE - fabsf(e->side);
    if (meditating) draw_calm(cx, ground);
    pose.jet = jet;
    draw_body(g, cx + e->shift, ground - e->bob, pose);
    if (asleep) draw_sleep(px, ground, lie);
    if (meditating) draw_meditation(g, cx, ground - e->bob);
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

/* With something in hand that reads them, what the thing under the cursor does for whoever has it on: its own
 * effects, and its set - how much of it is on, what each number of pieces gives, and which those pieces are. */
static void draw_identified(const Game *g, const Hud *hud)
{
    if (!game_identifies(g)) return;
    Vector2 m = GetMousePosition();
    ItemId item = hud_item_at(hud, &g->inventory, m);
    if (item == ITEM_NONE) return;
    const ItemDef *def = item_def(item);
    static char own[12][BOON_LINE], bonus[MAX_SET_TIERS][8][BOON_LINE];
    int own_count = game_item_lines(item, own, 12), bonus_count[MAX_SET_TIERS] = { 0 };
    const SetDef *set = def->set >= 0 && def->set < content.set_count ? &content.sets[def->set] : NULL;
    if (own_count == 0 && !set) return;
    int pieces = set ? game_set_pieces(g, def->set) : 0, members = 0;
    ItemId member[16];
    for (int i = 1; set && i < content.item_count && members < 16; i++)
        if (content.items[i].set == def->set) member[members++] = (ItemId)i;
    const Color got = { 140, 226, 140, 255 }, not_yet = { 150, 154, 170, 255 }, gold = { 226, 196, 102, 255 };
    /* How big it has to be. */
    float wide = (float)MeasureText(def->label, 18), tall = 30.0f + (float)own_count * 18.0f;
    for (int i = 0; i < own_count; i++) wide = fmaxf(wide, (float)MeasureText(own[i], 14));
    if (set) {
        tall += 30.0f;
        wide = fmaxf(wide, (float)MeasureText(TextFormat("%s set   %d of %d", set->label, pieces, members), 16));
        for (int t = 0; t < set->tier_count; t++) {
            bonus_count[t] = game_boon_lines(&set->tiers[t].boons, bonus[t], 8);
            tall += (float)bonus_count[t] * 18.0f;
            for (int i = 0; i < bonus_count[t]; i++) wide = fmaxf(wide, 34.0f + (float)MeasureText(bonus[t][i], 14));
        }
        tall += 8.0f + (float)members * 16.0f;
        for (int i = 0; i < members; i++) wide = fmaxf(wide, 14.0f + (float)MeasureText(item_def(member[i])->label, 12));
    }
    Rectangle box = { m.x + 18.0f, m.y - tall - 28.0f, wide + 24.0f, tall + 14.0f };
    if (box.x + box.width > (float)GetScreenWidth() - 6.0f) box.x = (float)GetScreenWidth() - 6.0f - box.width;
    if (box.y < 6.0f) box.y = 6.0f;
    DrawRectangleRec(box, (Color){ 24, 26, 34, 245 });
    DrawRectangleLinesEx(box, 2.0f, def->relic ? gold : (Color){ 120, 122, 134, 255 });
    float x = box.x + 12.0f, y = box.y + 9.0f;
    DrawText(def->label, (int)x, (int)y, 18, RAYWHITE);
    y += 26.0f;
    for (int i = 0; i < own_count; i++, y += 18.0f) DrawText(own[i], (int)x, (int)y, 14, (Color){ 200, 220, 255, 255 });
    if (!set) return;
    y += 8.0f;
    DrawLineEx((Vector2){ x, y }, (Vector2){ box.x + box.width - 12.0f, y }, 1.0f, (Color){ 70, 74, 90, 255 });
    y += 6.0f;
    DrawText(TextFormat("%s set   %d of %d", set->label, pieces, members), (int)x, (int)y, 16, gold);
    y += 22.0f;
    for (int t = 0; t < set->tier_count; t++) {
        bool on = pieces >= set->tiers[t].pieces;
        for (int i = 0; i < bonus_count[t]; i++, y += 18.0f) {
            if (i == 0) DrawText(TextFormat("%d:", set->tiers[t].pieces), (int)x, (int)y, 14, on ? got : not_yet);
            DrawText(bonus[t][i], (int)x + 34, (int)y, 14, on ? got : not_yet);
        }
    }
    y += 6.0f;
    /* Its pieces: the ones that are on, lit. */
    const Inventory *inv = &g->inventory;
    for (int i = 0; i < members; i++, y += 16.0f) {
        bool on = false;
        for (int k = 0; k < ARMOR_SLOTS; k++) on = on || (inv->armor[k].count > 0 && inv->armor[k].id == member[i]);
        for (int k = 0; g->relic_box > 0 && g->relic_box < MAX_CONTAINERS && k < RELIC_SLOTS; k++)
            on = on || (inv->containers[g->relic_box].slots[k].count > 0 && inv->containers[g->relic_box].slots[k].id == member[i]);
        DrawRectangle((int)x, (int)y + 4, 5, 5, on ? got : (Color){ 70, 74, 90, 255 });
        DrawText(item_def(member[i])->label, (int)x + 14, (int)y, 12, on ? RAYWHITE : not_yet);
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
    draw_readout_line(row++, TextFormat("Biome: %s", content.biomes[world_biome_at(w, at_x, at_y)].label));
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
    term_ui = (TermUi){ 0 };
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
    if (!DirectoryExists(main_menu_saves())) MakeDirectory(main_menu_saves());
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
    if (term_ui.open) {
        /* At a terminal: the keys and the mouse are for that, and the world goes on meanwhile. */
        game_set_move_input(&s->game, 0, 0);
        game_update(&s->game, dt);
        update_camera(&s->game, &s->cam, dt);
        return;
    }
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
        looked_at = g;
        world3d_begin(zoom);
        assets_set_season(game_season(g));
        draw_world(g, origin);
        draw_mobs(g, origin);
        /* Every character, the one being played last so that it is on top: first whoever is riding, then what they
         * are riding in - so that they are seen through its windows - then everyone else. */
        for (int pass = 0; pass < 2; pass++) {
            for (int n = 0; n < g->character_count; n++) {
                int c = (g->controlled + 1 + n) % g->character_count;
                game_use_character(g, c);
                if ((g->ride != 0) != (pass == 0)) continue;
                draw_player(g, origin);
                if (g->character_count > 1 && c == g->controlled && game_alive(g)) draw_played_marker(g, origin);
            }
            if (pass == 0) draw_vehicles(g, origin);
        }
        if (g->character_count == 0) draw_vehicles(g, origin);
        draw_stations(g, origin);
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
        /* Darkness falls over the world, but not over the interface. (It is laid over the front of everything, and
         * leaves no mark on the depth of things: what glows is drawn again through it, where it stands.) */
        rlDrawRenderBatchActive();
        rlDisableDepthMask();
        /* (Over everything, whatever its depth: weighed against the faces of the blocks it lies on, it would win and
         * lose by turns as the view moved in and out.) */
        rlDisableDepthTest();
        draw_light(g, origin);
        rlDrawRenderBatchActive();
        rlDisableDepthTest();
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
        rlDrawRenderBatchActive();
        rlEnableDepthMask();
        world3d_end();
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
        trainui_draw(&train_ui, g, &ui_view);
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
    daoui_draw(&dao_ui, g);
    if (g->lightning > 0) {
        /* A bolt of a tribulation: down the column it struck, and the whole view white for a moment. */
        Vector2 at = view_tile_pos(&ui_view, (float)g->lightning_x + 0.5f, (float)g->player.feet.y);
        float bx = at.x;
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 255, 255, 240, (unsigned char)(g->lightning * 18) });
        for (float by = 0.0f; by < at.y; by += 24.0f) {
            float nx = at.x + (float)((int)(by * 7.0f + (float)g->tick * 13.0f) % 31 - 15);
            if (by + 24.0f >= at.y) nx = at.x;
            DrawLineEx((Vector2){ bx, by }, (Vector2){ nx, by + 24.0f }, 4.0f, (Color){ 255, 250, 200, 255 });
            bx = nx;
        }
    }
    {
        /* What the character has taken that is still working: each with the time it has left, down the right-hand side. */
        Effect on[MAX_EFFECTS];
        int n = game_effects(g, on, MAX_EFFECTS), rate = g->tick_rate > 0 ? g->tick_rate : 20;
        for (int i = 0; i < n; i++) {
            Rectangle row = { (float)GetScreenWidth() - 96.0f, 84.0f + (float)i * 28.0f, 84.0f, 24.0f };
            int secs = on[i].ticks / rate;
            const char *left = on[i].ticks < 0 ? "" : TextFormat("%d:%02d", secs / 60, secs % 60);
            DrawRectangleRec(row, (Color){ 16, 17, 22, 190 });
            hud_draw_item_icon((ItemId)on[i].item, (Rectangle){ row.x + 2, row.y + 1, 22, 22 });
            DrawText(left, (int)row.x + 30, (int)row.y + 5, 16, on[i].ticks > 0 && secs < 10 ? (Color){ 240, 150, 120, 255 } : RAYWHITE);
        }
    }
    draw_identified(g, &s->hud);
    sign_draw();
    termui_frame(&term_ui, g);
}

int main(void)
{
    char error[256];
    /* The game's own data, and on top of it the mods that are on. */
    static MainMenu menu;
    mods_scan(&menu.mods, MODS_DIR, SAVE_DIR);
    /* (Which are on is what the playset in use says.) */
    playsets_load(&menu.playsets, &menu.mods, SAVE_DIR);
    playsets_apply(&menu.playsets, &menu.mods);
    if (!mods_load_content(&menu.mods, DATA_DIR, error, sizeof error)) {
        fprintf(stderr, "could not load game data: %s\n", error);
        return 1;
    }
    if (error[0]) {
        fprintf(stderr, "a mod was left out: %s\n", error);
        snprintf(menu.error, sizeof menu.error, "%.90s", error);
    }
    char saves[256];
    playsets_saves(&menu.playsets, SAVE_DIR, saves, sizeof saves);
    main_menu_set_saves(saves);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "BlockGame");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); /* Escape opens the pause menu instead of quitting */
    const char *texture_dirs[MAX_MODS + 1];
    assets_load_from(texture_dirs, mods_texture_dirs(&menu.mods, DATA_DIR, texture_dirs));

    Keybinds keys;
    keybinds_load(&keys);
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
            if (r == MAIN_MENU_MODS) {
                /* Other mods on: other content, other textures, and that playset's own worlds. */
                menu.error[0] = '\0';
                playsets_apply(&menu.playsets, &menu.mods);
                if (!mods_load_content(&menu.mods, DATA_DIR, error, sizeof error)) {
                    fprintf(stderr, "could not load game data: %s\n", error);
                    quit = true;
                } else {
                    if (error[0]) snprintf(menu.error, sizeof menu.error, "%.90s", error);
                    mods_remember(&menu.mods, SAVE_DIR);
                    assets_load_from(texture_dirs, mods_texture_dirs(&menu.mods, DATA_DIR, texture_dirs));
                    playsets_saves(&menu.playsets, SAVE_DIR, saves, sizeof saves);
                    main_menu_set_saves(saves);
                    menu.listed = false;
                    menu.selected = 0;
                }
            }
            if (r == MAIN_MENU_LOAD) ok = load_game(&session.game, menu.path);
            if (r == MAIN_MENU_CREATE) {
                uint32_t seed = (uint32_t)time(NULL) ^ (uint32_t)(GetTime() * 1000000.0);
                WorldOptions options = main_menu_world_options(&menu);
                ok = game_init_with(&session.game, main_menu_world_width(&menu), content.worldgen.height, seed,
                                    main_menu_world_debug(&menu) ? WORLD_DEBUG : WORLD_TERRAIN, &options);
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
