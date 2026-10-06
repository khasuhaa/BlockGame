#include "benchui.h"
#include "game.h"
#include "hud.h"
#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WORLD_WIDTH 256
#define WORLD_HEIGHT 128
#define TILE_SIZE 32
#define DRAG_THRESHOLD 6.0f /* pixels the mouse must travel before a press becomes a pan */
#define CAMERA_FOLLOW_SPEED 8.0f

/* World-space pixel shown at the centre of the window. */
typedef struct {
    Vector2 center;
    bool follow; /* track the player until the map is dragged */
    bool pressed, dragging;
    Vector2 press_mouse, press_center;
} MapCamera;

static Vector2 view_origin(const MapCamera *cam)
{
    return (Vector2){ floorf(GetScreenWidth() / 2.0f - cam->center.x),
                      floorf(GetScreenHeight() / 2.0f - cam->center.y) };
}

static TilePos tile_at_screen(const MapCamera *cam, Vector2 screen)
{
    Vector2 origin = view_origin(cam);
    return (TilePos){ (int)floorf((screen.x - origin.x) / TILE_SIZE),
                      (int)floorf((screen.y - origin.y) / TILE_SIZE) };
}

static Rectangle tile_rect(Vector2 origin, TilePos t)
{
    return (Rectangle){ origin.x + t.x * TILE_SIZE, origin.y + t.y * TILE_SIZE,
                        TILE_SIZE, TILE_SIZE };
}

static Vector2 player_center(const Game *g)
{
    float x, y;
    game_player_visual_pos(g, &x, &y);
    return (Vector2){ (x + 0.5f) * TILE_SIZE, y * TILE_SIZE };
}

static void handle_input(Game *g, MapCamera *cam, Hud *hud, BenchUi *bench)
{
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_D)) {
        game_cancel_actions(g);
        hud_close_all(hud);
        benchui_close(bench, g);
    }
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (IsKeyPressed(KEY_ONE + i)) inventory_select(&g->inventory, i, -1);
    game_set_move_input(g, (int)IsKeyDown(KEY_D) - (int)IsKeyDown(KEY_A),
                        (int)IsKeyDown(KEY_S) - (int)IsKeyDown(KEY_W));

    if (IsKeyPressed(KEY_SPACE)) cam->follow = true;

    /* A press that starts on the inventory never reaches the world. */
    if (hud_handle_input(hud, &g->inventory)) return;
    if (benchui_handle_input(bench, g, view_origin(cam), TILE_SIZE)) return;

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
            const TileDef *def = tile_def(world_get(&g->world, t.x, t.y));
            /* A bench offers a choice if it can be removed; otherwise it just opens. */
            if (def->bench && def->removable) benchui_show_popup(bench, t);
            else if (def->bench) game_queue_action(g, ACTION_USE, t.x, t.y);
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
    cam->center.x = fminf(fmaxf(cam->center.x, 0.0f), (float)g->world.width * TILE_SIZE);
    cam->center.y = fminf(fmaxf(cam->center.y, 0.0f), (float)g->world.height * TILE_SIZE);
}

static void draw_dirt(int x, int y, int px, int py)
{
    /* Cheap per-tile hash so the dirt is not one flat colour. */
    unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
    int shade = (int)((h >> 8) % 13) - 6;
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, (Color){ 96, 66, 44, 255 });
    DrawRectangle(px, py, TILE_SIZE - 1, TILE_SIZE - 1,
                  (Color){ (unsigned char)(134 + shade), (unsigned char)(96 + shade),
                           (unsigned char)(67 + shade), 255 });
}

static void draw_workbench(int px, int py)
{
    Color dark = { 104, 72, 44, 255 };
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, dark);
    DrawRectangle(px + 1, py + 1, TILE_SIZE - 2, TILE_SIZE - 2, (Color){ 164, 120, 74, 255 });
    DrawRectangle(px, py, TILE_SIZE, 8, dark);
    DrawRectangle(px + 1, py + 19, TILE_SIZE - 2, 2, dark);
    DrawRectangle(px + 15, py + 8, 2, TILE_SIZE - 8, dark);
}

/* One tile of the spawn portal: a shimmering field, framed by stone posts
 * that stand just outside the tile so they show around the player. */
static void draw_portal(int px, int py, bool top)
{
    const int post = 6;
    Color stone = { 92, 94, 112, 255 };
    float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 2.5f + py * 0.05f);
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, (Color){ 58, 34, 110, 255 });
    DrawRectangle(px + 6, py, TILE_SIZE - 12, TILE_SIZE, Fade((Color){ 170, 120, 255, 255 },
                                                              0.25f + 0.35f * pulse));
    DrawRectangle(px - post, py, post, TILE_SIZE, stone);
    DrawRectangle(px + TILE_SIZE, py, post, TILE_SIZE, stone);
    if (top) DrawRectangle(px - post, py - post, TILE_SIZE + 2 * post, post, stone);
}

static void draw_world(const World *w, Vector2 origin)
{
    int x0 = (int)floorf(-origin.x / TILE_SIZE), y0 = (int)floorf(-origin.y / TILE_SIZE);
    int x1 = x0 + GetScreenWidth() / TILE_SIZE + 1, y1 = y0 + GetScreenHeight() / TILE_SIZE + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= w->width) x1 = w->width - 1;
    if (y1 >= w->height) y1 = w->height - 1;

    DrawRectangle((int)origin.x, (int)origin.y, w->width * TILE_SIZE, w->height * TILE_SIZE,
                  (Color){ 135, 190, 235, 255 });

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            TileType t = world_get(w, x, y);
            int px = (int)origin.x + x * TILE_SIZE, py = (int)origin.y + y * TILE_SIZE;
            if (t == TILE_DIRT) draw_dirt(x, y, px, py);
            else if (t == TILE_WORKBENCH) draw_workbench(px, py);
        }
    }
    /* Portals go on last: their frame overlaps the tiles around them. */
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            if (world_get(w, x, y) != TILE_SPAWN_PORTAL) continue;
            draw_portal((int)origin.x + x * TILE_SIZE, (int)origin.y + y * TILE_SIZE,
                        world_get(w, x, y - 1) != TILE_SPAWN_PORTAL);
        }
    }
}

static void draw_action_marker(Vector2 origin, Action a)
{
    Rectangle r = tile_rect(origin, a.target);
    Color c = { 255, 255, 255, 255 };
    if (a.type == ACTION_MINE) c = (Color){ 255, 160, 40, 255 };
    if (a.type == ACTION_USE) c = (Color){ 190, 140, 255, 255 };
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
        draw_action_marker(origin, g->current);
        if (g->player.activity == ACTIVITY_MINING) {
            float progress = g->player.timer / g->player.duration;
            DrawRectangleRec(tile_rect(origin, g->current.target), Fade(BLACK, 0.7f * progress));
        }
    }
    for (int i = 0; i < g->queue_count; i++) draw_action_marker(origin, g->queue[i]);

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

static void draw_drops(const Game *g, Vector2 origin)
{
    const float size = TILE_SIZE * 0.5f;
    Vector2 player = player_center(g);
    for (int i = 0; i < MAX_DROPS; i++) {
        const Drop *d = &g->drops[i];
        if (!d->active) continue;
        /* Lying drops bob a little; collected ones fly into the player. */
        float bob = sinf(d->age * 4.0f + (float)d->x) * 2.0f;
        Vector2 p = { (d->x + 0.5f) * TILE_SIZE, (d->y + 1.0f) * TILE_SIZE - size / 2 - 3 + bob };
        p.x += (player.x - p.x) * d->collect;
        p.y += (player.y - p.y) * d->collect;
        hud_draw_item_icon((ItemId)d->item, (Rectangle){ origin.x + p.x - size / 2,
                                                         origin.y + p.y - size / 2, size, size });
    }
}

static void draw_player(const Game *g, Vector2 origin)
{
    float x, y;
    game_player_visual_pos(g, &x, &y);
    int grip = game_player_grip_side(g);
    const float inset = 3.0f;
    /* When climbing, press against the wall and reach for it. */
    float px = origin.x + x * TILE_SIZE + grip * inset, py = origin.y + y * TILE_SIZE;
    Color skin = { 240, 200, 160, 255 };
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

static void draw_hud(const Game *g, const MapCamera *cam)
{
    DrawRectangle(0, 0, GetScreenWidth(), 52, Fade(BLACK, 0.45f));
    DrawText("A/D: move   W/S: climb   Click: move / mine / place held block / use bench   "
             "Drag: pan   Space: follow   1-8: select",
             10, 8, 16, RAYWHITE);
    const ItemStack *held = inventory_selected(&g->inventory);
    DrawText(TextFormat("seed %u   player %d,%d   queued %d   selected: %s%s", g->world.seed,
                        g->player.feet.x, g->player.feet.y,
                        g->queue_count + (g->has_current ? 1 : 0),
                        held->count > 0 ? item_def((ItemId)held->id)->name : "nothing",
                        cam->follow ? "" : "   (camera detached)"),
             10, 30, 16, LIGHTGRAY);
}

int main(int argc, char **argv)
{
    uint32_t seed = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 10) : (uint32_t)time(NULL);

    Game game;
    if (!game_init(&game, WORLD_WIDTH, WORLD_HEIGHT, seed)) {
        fprintf(stderr, "failed to create world\n");
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "BlockGame");
    SetTargetFPS(60);

    Hud hud = { 0 };
    BenchUi bench = { .selected = -1, .amount = 1 };
    MapCamera cam = { .center = player_center(&game), .follow = true };

    while (!WindowShouldClose()) {
        float dt = fminf(GetFrameTime(), 0.1f);

        handle_input(&game, &cam, &hud, &bench);
        game_update(&game, dt);
        update_camera(&game, &cam, dt);

        Vector2 origin = view_origin(&cam);
        BeginDrawing();
        ClearBackground((Color){ 20, 22, 28, 255 });
        draw_world(&game.world, origin);
        draw_actions(&game, origin);
        draw_drops(&game, origin);
        draw_player(&game, origin);
        if (!cam.dragging && IsCursorOnScreen() && !hud_mouse_over(&hud, &game.inventory) &&
            !benchui_mouse_over(&bench, &game, origin, TILE_SIZE))
            DrawRectangleLinesEx(tile_rect(origin, tile_at_screen(&cam, GetMousePosition())), 1.0f,
                                 Fade(WHITE, 0.5f));
        benchui_draw(&bench, &game, origin, TILE_SIZE);
        draw_hud(&game, &cam);
        hud_draw(&hud, &game.inventory);
        EndDrawing();
    }

    CloseWindow();
    game_free(&game);
    return 0;
}
