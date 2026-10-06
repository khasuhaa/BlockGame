#include "assets.h"
#include "benchui.h"
#include "game.h"
#include "hud.h"
#include "light.h"
#include "menu.h"
#include "mob.h"
#include "raylib.h"
#include "save.h"
#include "tradeui.h"
#include "view.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DATA_DIR "data"
#define PROFILE_PATH SAVE_DIR "/profile.txt"
#define TILE_SIZE 32
#define DRAG_THRESHOLD 6.0f /* pixels the mouse must travel before a press becomes a pan */
#define CAMERA_FOLLOW_SPEED 8.0f
#define AUTOSAVE_SECONDS 120.0f
#define BACKGROUND_DIM 0.18f /* how much darker the background layer is drawn */
#define WALL_DIM 0.5f        /* and the backwall behind that */

/* World-space pixel shown at the centre of the window. */
typedef struct {
    Vector2 center;
    bool follow; /* track the player until the map is dragged */
    bool pressed, dragging;
    Vector2 press_mouse, press_center;
} MapCamera;

/* Refreshed from the camera at the start of each frame. */
static View view;

static void update_view(const MapCamera *cam, const World *w)
{
    view.origin = (Vector2){ floorf(GetScreenWidth() / 2.0f - cam->center.x),
                             floorf(GetScreenHeight() / 2.0f - cam->center.y) };
    view.tile_size = TILE_SIZE;
    view.center_x = cam->center.x / TILE_SIZE;
    view.world_width = w->width;
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
    return (TilePos){ (int)floorf((screen.x - origin.x) / TILE_SIZE),
                      (int)floorf((screen.y - origin.y) / TILE_SIZE) };
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
    hud->extra_count = benchui_storage_slots(g, &view, rects, HUD_MAX_EXTRA_SLOTS, &container);
    for (int i = 0; i < hud->extra_count; i++)
        hud->extra[i] = (HudSlot){ rects[i], { SLOT_CONTAINER, container, i } };
}

static void handle_input(Game *g, MapCamera *cam, Hud *hud, BenchUi *bench, TradeUi *trade,
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
        if (IsKeyPressed(KEY_ONE + i)) inventory_select(&g->inventory, i, -1);
    game_set_move_input(g, (int)IsKeyDown(k[BIND_RIGHT]) - (int)IsKeyDown(k[BIND_LEFT]),
                        (int)IsKeyDown(k[BIND_DOWN]) - (int)IsKeyDown(k[BIND_UP]));

    if (IsKeyPressed(k[BIND_FOLLOW])) cam->follow = true;

    /* The slots of an open chest join the inventory's, so items drag between them. */
    sync_storage_slots(g, hud);

    /* A press that starts on the inventory never reaches the world. */
    if (hud_handle_input(hud, &g->inventory)) return;
    if (tradeui_handle_input(trade, g, &view)) return;
    if (benchui_handle_input(bench, g, &view)) return;

    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        hud_close_all(hud);
        benchui_close(bench, g);
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
        }
        if (cam->dragging)
            cam->center = (Vector2){ cam->press_center.x - dx, cam->press_center.y - dy };
    }
    if (cam->pressed && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (!cam->dragging) {
            TilePos t = tile_at_screen(cam, mouse);
            t.x = world_wrap_x(&g->world, t.x);
            const TileDef *def = tile_def(game_bench_at(g, t.x, t.y));
            bool opens = def->bench || def->trade || def->storage_columns > 0;
            int drop = game_drop_at(g, (mouse.x - view.origin.x) / TILE_SIZE,
                                    (mouse.y - view.origin.y) / TILE_SIZE);
            /* An item under the cursor comes before the tile behind it.
             * Something that opens offers a choice if it can also be removed;
             * otherwise it just opens. */
            int mob = game_mob_at(g, (mouse.x - view.origin.x) / TILE_SIZE,
                                  (mouse.y - view.origin.y) / TILE_SIZE);
            if (drop >= 0) game_click_drop(g, drop);
            /* A creature under the cursor is hunted. */
            else if (mob >= 0) game_click_mob(g, mob);
            /* A second click on a square with something queued takes it back. */
            else if (game_unqueue_tile(g, t.x, t.y)) {}
            else if (opens && def->removable) benchui_show_popup(bench, t);
            else if (opens) game_queue_action(g, ACTION_USE, t.x, t.y);
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
    DrawRectangle(0, (int)origin.y, GetScreenWidth(), w->height * TILE_SIZE, sky);

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
static void draw_world(const World *w, Vector2 origin)
{
    /* Columns run past either end of the world: reads wrap round it. */
    int x0 = (int)floorf(-origin.x / TILE_SIZE), y0 = (int)floorf(-origin.y / TILE_SIZE);
    int x1 = x0 + GetScreenWidth() / TILE_SIZE + 1, y1 = y0 + GetScreenHeight() / TILE_SIZE + 1;
    if (y0 < 0) y0 = 0;
    if (y1 >= w->height) y1 = w->height - 1;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            TileType t = world_get(w, x, y), behind = world_get_at(w, LAYER_BACKGROUND, x, y);
            int wx = world_wrap_x(w, x);
            float px = origin.x + (float)(x * TILE_SIZE), py = origin.y + (float)(y * TILE_SIZE);
            if (!tile_def(t)->solid) {
                assets_draw_tile(world_get_at(w, LAYER_WALL, x, y), wx, y, px, py, TILE_SIZE, WALL_DIM,
                                 TILE_PART_BODY);
                if (!tile_def(behind)->front)
                    assets_draw_tile(behind, wx, y, px, py, TILE_SIZE, BACKGROUND_DIM, TILE_PART_BODY);
            }
            if (!tile_def(t)->front) assets_draw_tile(t, wx, y, px, py, TILE_SIZE, 0.0f, TILE_PART_BODY);
            if (world_is_water(w, x, y)) {
                /* Water tints what is in it; its surface is a lighter line. */
                const int *c = content.rules.water_color;
                Color water = { (unsigned char)c[0], (unsigned char)c[1], (unsigned char)c[2], (unsigned char)c[3] };
                DrawRectangle((int)px, (int)py, TILE_SIZE, TILE_SIZE, water);
                if (y == w->sea_row + 1) DrawRectangle((int)px, (int)py, TILE_SIZE, 3, Fade(WHITE, 0.35f));
            }
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

/* Darkens the world by how little light reaches each tile. The darkness of a
 * tile's corner is the average of the tiles that meet there, so light fades
 * smoothly instead of in squares. */
static void draw_light(const Game *g, Vector2 origin)
{
    static uint8_t *light;
    static int capacity;
    int x0 = (int)floorf(-origin.x / TILE_SIZE) - 1, y0 = (int)floorf(-origin.y / TILE_SIZE) - 1;
    int cols = GetScreenWidth() / TILE_SIZE + 4, rows = GetScreenHeight() / TILE_SIZE + 4;
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
        hud_draw_item_icon((ItemId)a.item, (Rectangle){ r.x - 6, r.y - 6, r.width + 12,
                                                        r.height + 12 });
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
        Rectangle r = { origin.x + view_near_x(&view, x) * TILE_SIZE, origin.y + y * TILE_SIZE, TILE_SIZE,
                        TILE_SIZE };
        if (r.x < -TILE_SIZE || r.x > GetScreenWidth()) continue;
        assets_draw_sprite(m->baby ? def->texture_baby : def->texture, r, m->dir < 0);
    }
}

/* The item layer, in front of the blocks. */
static void draw_drops(const Game *g, Vector2 origin)
{
    const float size = TILE_SIZE * 0.55f;
    Vector2 player = player_center(g);
    for (int i = 0; i < MAX_DROPS; i++) {
        const Drop *d = &g->drops[i];
        if (!d->active) continue;
        Vector2 p = drop_screen_pos(g, i, origin);
        if (p.x < -TILE_SIZE || p.x > GetScreenWidth() + TILE_SIZE) continue;
        /* Collected ones fly into the player. */
        p.x += (origin.x + player.x - p.x) * d->collect;
        p.y += (origin.y + player.y - p.y) * d->collect;
        hud_draw_item_icon((ItemId)d->item, (Rectangle){ p.x - size / 2, p.y - size / 2, size, size });
    }
}

static void draw_player(const Game *g, Vector2 origin)
{
    float x, y;
    game_player_visual_pos(g, &x, &y);
    int grip = game_player_grip_side(g);
    const float inset = 3.0f;
    /* When climbing, press against the wall and reach for it. */
    float px = origin.x + view_near_x(&view, x) * TILE_SIZE + grip * inset, py = origin.y + y * TILE_SIZE;
    Color skin = { 240, 200, 160, 255 };
    if (game_player_crawling(g)) {
        /* On hands and knees: one tile tall, head leading. */
        float half = TILE_SIZE / 2.0f;
        float head_x = g->player.facing > 0 ? px + TILE_SIZE - half : px;
        DrawRectangleRec((Rectangle){ px + 1, py + half, TILE_SIZE - 2, half }, (Color){ 50, 90, 190, 255 });
        DrawRectangleRec((Rectangle){ head_x + 1, py + 4, half - 2, half }, skin);
        return;
    }
    /* Hanging from a block overhead: arms up to it. */
    const Player *p = &g->player;
    if (p->climbing && p->activity != ACTIVITY_MINING && grip == 0 &&
        world_is_solid(&g->world, p->feet.x, p->feet.y - 2) &&
        (p->activity != ACTIVITY_STEPPING || world_is_solid(&g->world, p->step_to.x, p->step_to.y - 2))) {
        DrawRectangleRec((Rectangle){ px + inset + 2, py - TILE_SIZE - 5, 5, 10 }, skin);
        DrawRectangleRec((Rectangle){ px + TILE_SIZE - inset - 7, py - TILE_SIZE - 5, 5, 10 }, skin);
    }
    DrawRectangleRec((Rectangle){ px + inset, py, TILE_SIZE - 2 * inset, TILE_SIZE },
                     (Color){ 50, 90, 190, 255 });
    DrawRectangleRec((Rectangle){ px + inset, py - TILE_SIZE + inset, TILE_SIZE - 2 * inset,
                                  TILE_SIZE - inset },
                     skin);
    if (grip != 0) {
        float hx = grip > 0 ? px + TILE_SIZE - inset - 4 : px + inset - 4;
        DrawRectangleRec((Rectangle){ hx, py - 6, 8, 6 }, skin);
        DrawRectangleRec((Rectangle){ hx, py + TILE_SIZE - 10, 8, 6 }, (Color){ 40, 60, 120, 255 });
    }
}

/* Crystals have a box of their own in the corner: they belong to the player,
 * not to this world or its inventory. */
static void draw_crystals(const Game *g)
{
    const char *text = TextFormat("%d", g->crystals);
    float w = (float)MeasureText(text, 20) + 52.0f;
    Rectangle box = { GetScreenWidth() - w - 12.0f, 64.0f, w, 36.0f };
    DrawRectangleRec(box, (Color){ 22, 30, 48, 235 });
    DrawRectangleLinesEx(box, 2.0f, (Color){ 120, 220, 255, 255 });
    assets_draw_image(content.trade.crystal_icon, (Rectangle){ box.x + 8, box.y + 5, 26, 26 });
    DrawText(text, (int)box.x + 42, (int)box.y + 8, 20, RAYWHITE);
}

static void draw_hud(const Game *g, const MapCamera *cam)
{
    draw_crystals(g);
    DrawRectangle(0, 0, GetScreenWidth(), 52, Fade(BLACK, 0.45f));
    DrawText("Click: move / mine / place held block / use bench / pick up item   Drag: pan   "
             "1-8: select   Esc: menu",
             10, 8, 16, RAYWHITE);
    const ItemStack *held = inventory_selected(&g->inventory);
    bool day;
    float phase = game_day_phase(g, &day);
    DrawText(TextFormat("%s %d%%   X %d  Y %d   queued %d   selected: %s%s",
                        day ? "Day" : "Night", (int)(phase * 100.0f),
                        g->player.feet.x, world_row_to_y(&g->world, g->player.feet.y),
                        g->queue_count + (g->has_current ? 1 : 0),
                        held->count > 0 ? item_def((ItemId)held->id)->label : "nothing",
                        cam->follow ? "" : "   (camera detached)"),
             10, 30, 16, LIGHTGRAY);
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
    /* Crystals come with the player into whichever world is opened. */
    profile_load(PROFILE_PATH, &s->game);
    s->saved_crystals = s->game.crystals;
    s->saved_shared = s->game.inventory.containers[SHARED_CONTAINER];
    s->paused = false;
    s->since_save = 0.0f;
    s->cam = (MapCamera){ .follow = true };
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
    if (IsKeyPressed(KEY_ESCAPE)) {
        s->paused = true;
        pause_menu_open(&s->pause);
        game_set_move_input(&s->game, 0, 0);
        return;
    }
    handle_input(&s->game, &s->cam, &s->hud, &s->bench, &s->trade, keys);
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

    ClearBackground((Color){ 20, 22, 28, 255 });
    draw_sky(g, origin);
    draw_world(&g->world, origin);
    draw_mobs(g, origin);
    draw_player(g, origin);
    draw_drops(g, origin);
    /* Darkness falls over the world, but not over the interface. */
    draw_light(g, origin);
    draw_actions(g, origin);
    if (!s->paused && !s->cam.dragging && IsCursorOnScreen() &&
        !hud_mouse_over(&s->hud, &g->inventory) && !benchui_mouse_over(&s->bench, g, &view) &&
        !tradeui_mouse_over(&s->trade, g, &view))
        DrawRectangleLinesEx(tile_rect(origin, tile_at_screen(&s->cam, GetMousePosition())), 1.0f,
                             Fade(WHITE, 0.5f));
    benchui_draw(&s->bench, g, &view);
    tradeui_draw(&s->trade, g, &view);
    sync_storage_slots(g, &s->hud);
    draw_hud(g, &s->cam);
    hud_draw(&s->hud, &g->inventory);
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
                ok = game_init(&session.game, content.worldgen.width, content.worldgen.height, seed,
                               WORLD_TERRAIN);
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
