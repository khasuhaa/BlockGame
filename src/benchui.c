#include "benchui.h"

#include "hud.h"

#include <stddef.h>

#define MENU_WIDTH 300.0f
#define MENU_HEIGHT 204.0f
#define MENU_PAD 10.0f
#define LIST_CELL 48.0f
#define LIST_GAP 4.0f
#define POPUP_WIDTH 136.0f
#define POPUP_BUTTON_HEIGHT 26.0f
#define UI_DRAG_THRESHOLD 6.0f
#define WHEEL_SCROLL 30.0f

static const Color panel_color = { 28, 30, 38, 240 };
static const Color edge_color = { 120, 122, 134, 255 };
static const Color button_color = { 70, 130, 80, 255 };

/* ---- layout ------------------------------------------------------------ */

static Rectangle menu_rect(const Game *g, const View *v)
{
    Vector2 tile = view_tile_pos(v, (float)g->bench_pos.x, (float)g->bench_pos.y);
    return (Rectangle){ tile.x + v->tile_size + 14.0f, tile.y + v->tile_size - MENU_HEIGHT,
                        MENU_WIDTH, MENU_HEIGHT };
}

static Rectangle list_rect(Rectangle menu)
{
    return (Rectangle){ menu.x + MENU_PAD, menu.y + MENU_PAD, menu.width - 2 * MENU_PAD,
                        LIST_CELL + 4.0f };
}

static Rectangle slider_rect(Rectangle menu)
{
    return (Rectangle){ menu.x + MENU_PAD + 6, menu.y + 124, menu.width - 2 * MENU_PAD - 62, 20 };
}

static Rectangle craft_rect(Rectangle menu)
{
    return (Rectangle){ menu.x + MENU_PAD, menu.y + menu.height - MENU_PAD - 34,
                        menu.width - 2 * MENU_PAD, 34 };
}

static Rectangle popup_rect(const BenchUi *ui, const View *v)
{
    float h = 2 * POPUP_BUTTON_HEIGHT + 3 * 6.0f;
    Vector2 tile = view_tile_pos(v, (float)ui->popup_tile.x, (float)ui->popup_tile.y);
    return (Rectangle){ tile.x + v->tile_size / 2 - POPUP_WIDTH / 2, tile.y - h - 8.0f,
                        POPUP_WIDTH, h };
}

static Rectangle popup_button(Rectangle popup, int i)
{
    return (Rectangle){ popup.x + 6, popup.y + 6 + i * (POPUP_BUTTON_HEIGHT + 6),
                        popup.width - 12, POPUP_BUTTON_HEIGHT };
}

static int open_recipes(const Game *g, const Recipe **out)
{
    return bench_recipes(world_get(&g->world, g->bench_pos.x, g->bench_pos.y), out, MAX_BENCH_RECIPES);
}

/* The list only scrolls when its items are wider than the menu. */
static float max_scroll(Rectangle list, int count)
{
    float content = count * (LIST_CELL + LIST_GAP) - LIST_GAP;
    return content > list.width ? content - list.width : 0.0f;
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* ---- input ------------------------------------------------------------- */

void benchui_show_popup(BenchUi *ui, TilePos tile)
{
    ui->popup = true;
    ui->popup_tile = tile;
}

void benchui_close(BenchUi *ui, Game *g)
{
    ui->popup = false;
    game_close_bench(g);
}

bool benchui_mouse_over(const BenchUi *ui, const Game *g, const View *v)
{
    Vector2 m = GetMousePosition();
    if (ui->press != BENCH_PRESS_NONE) return true;
    if (ui->popup && CheckCollisionPointRec(m, popup_rect(ui, v))) return true;
    return g->bench_open && CheckCollisionPointRec(m, menu_rect(g, v));
}

static BenchPress press_target(const BenchUi *ui, const Game *g, const View *v, Vector2 m)
{
    if (ui->popup) {
        Rectangle popup = popup_rect(ui, v);
        if (CheckCollisionPointRec(m, popup_button(popup, 0))) return BENCH_PRESS_OPEN;
        if (CheckCollisionPointRec(m, popup_button(popup, 1))) return BENCH_PRESS_REMOVE;
        if (CheckCollisionPointRec(m, popup)) return BENCH_PRESS_PANEL;
    }
    if (!g->bench_open) return BENCH_PRESS_NONE;
    Rectangle menu = menu_rect(g, v);
    if (!CheckCollisionPointRec(m, menu)) return BENCH_PRESS_NONE;
    if (CheckCollisionPointRec(m, list_rect(menu))) return BENCH_PRESS_LIST;
    if (ui->selected < 0) return BENCH_PRESS_PANEL;
    Rectangle slider = slider_rect(menu);
    slider.x -= 8;
    slider.width += 16;
    if (CheckCollisionPointRec(m, slider)) return BENCH_PRESS_SLIDER;
    if (CheckCollisionPointRec(m, craft_rect(menu))) return BENCH_PRESS_CRAFT;
    return BENCH_PRESS_PANEL;
}

bool benchui_handle_input(BenchUi *ui, Game *g, const View *v)
{
    Vector2 m = GetMousePosition();

    if (g->bench_open) {
        bool changed = ui->shown.x != g->bench_pos.x || ui->shown.y != g->bench_pos.y;
        if (!ui->was_open || changed) {
            ui->selected = -1;
            ui->scroll = 0.0f;
            ui->amount = 1;
            ui->shown = g->bench_pos;
        }
        ui->popup = false;
    }
    ui->was_open = g->bench_open;

    const Recipe *recipes[MAX_BENCH_RECIPES];
    int count = g->bench_open ? open_recipes(g, recipes) : 0;
    Rectangle menu = menu_rect(g, v);
    Rectangle list = list_rect(menu);
    if (ui->selected >= count) ui->selected = -1;

    /* The amount never goes past what there are materials for. */
    int cap = ui->selected >= 0 ? recipe_max_crafts(&g->inventory, recipes[ui->selected]) : 0;
    if (ui->amount > cap) ui->amount = cap;
    if (ui->amount < 1) ui->amount = 1;

    if (g->bench_open && CheckCollisionPointRec(m, menu)) {
        Vector2 wheel = GetMouseWheelMoveV();
        ui->scroll -= (wheel.x + wheel.y) * WHEEL_SCROLL;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ui->press = press_target(ui, g, v, m);
        ui->press_mouse = m;
        ui->press_scroll = ui->scroll;
        ui->dragged = false;
    }

    if (ui->press != BENCH_PRESS_NONE && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        float dx = m.x - ui->press_mouse.x;
        if (dx > UI_DRAG_THRESHOLD || dx < -UI_DRAG_THRESHOLD) ui->dragged = true;
        if (ui->press == BENCH_PRESS_LIST && ui->dragged) ui->scroll = ui->press_scroll - dx;
        if (ui->press == BENCH_PRESS_SLIDER) {
            Rectangle slider = slider_rect(menu);
            float t = clampf((m.x - slider.x) / slider.width, 0.0f, 1.0f);
            int value = 1 + (int)(t * (MAX_CRAFT_AMOUNT - 1) + 0.5f);
            ui->amount = value > cap ? (cap > 0 ? cap : 1) : value;
        }
    }
    ui->scroll = clampf(ui->scroll, 0.0f, max_scroll(list, count));

    if (ui->press == BENCH_PRESS_NONE) return false;
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return true;

    BenchPress released = press_target(ui, g, v, m);
    if (ui->press == BENCH_PRESS_LIST && !ui->dragged && released == BENCH_PRESS_LIST) {
        float offset = m.x - list.x + ui->scroll;
        int i = (int)(offset / (LIST_CELL + LIST_GAP));
        if (i >= 0 && i < count && offset - i * (LIST_CELL + LIST_GAP) <= LIST_CELL) {
            ui->selected = i;
            ui->amount = 1;
        }
    } else if (ui->press == BENCH_PRESS_CRAFT && released == BENCH_PRESS_CRAFT) {
        game_craft(g, recipes[ui->selected], ui->amount);
    } else if (ui->press == BENCH_PRESS_OPEN && released == BENCH_PRESS_OPEN) {
        ui->popup = false;
        game_queue_action(g, ACTION_USE, ui->popup_tile.x, ui->popup_tile.y);
    } else if (ui->press == BENCH_PRESS_REMOVE && released == BENCH_PRESS_REMOVE) {
        ui->popup = false;
        game_queue_action(g, ACTION_MINE, ui->popup_tile.x, ui->popup_tile.y);
    }
    ui->press = BENCH_PRESS_NONE;
    return true;
}

/* ---- drawing ----------------------------------------------------------- */

static void draw_centered(const char *text, Rectangle r, int size, Color c)
{
    DrawText(text, (int)(r.x + (r.width - MeasureText(text, size)) / 2),
             (int)(r.y + (r.height - size) / 2), size, c);
}

static void draw_popup(const BenchUi *ui, const View *v)
{
    static const char *labels[2] = { "Open bench", "Remove bench" };
    Rectangle popup = popup_rect(ui, v);
    Vector2 m = GetMousePosition();
    DrawRectangleRec(popup, panel_color);
    DrawRectangleLinesEx(popup, 2.0f, edge_color);
    for (int i = 0; i < 2; i++) {
        Rectangle b = popup_button(popup, i);
        bool hot = CheckCollisionPointRec(m, b);
        DrawRectangleRec(b, hot ? (Color){ 80, 84, 100, 255 } : (Color){ 54, 57, 70, 255 });
        draw_centered(labels[i], b, 16, RAYWHITE);
    }
}

static void draw_list(const BenchUi *ui, const Recipe **recipes, int count, Rectangle list)
{
    bool scrolls = max_scroll(list, count) > 0.0f;
    if (scrolls) BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    for (int i = 0; i < count; i++) {
        Rectangle cell = { list.x + i * (LIST_CELL + LIST_GAP) - ui->scroll, list.y, LIST_CELL,
                           LIST_CELL };
        DrawRectangleRec(cell, (Color){ 44, 47, 58, 255 });
        DrawRectangleLinesEx(cell, i == ui->selected ? 3.0f : 2.0f,
                             i == ui->selected ? RAYWHITE : edge_color);
        hud_draw_item_icon(recipes[i]->output.id, cell);
    }
    if (scrolls) {
        EndScissorMode();
        /* Thin bar under the list showing where the view is. */
        float content = count * (LIST_CELL + LIST_GAP) - LIST_GAP;
        float w = list.width * list.width / content;
        float x = list.x + (list.width - w) * ui->scroll / max_scroll(list, count);
        DrawRectangleRec((Rectangle){ x, list.y + LIST_CELL + 1, w, 3 }, edge_color);
    }
}

static void draw_recipe(const BenchUi *ui, const Game *g, const Recipe *r, Rectangle menu)
{
    const float icon = 36.0f;
    int cap = recipe_max_crafts(&g->inventory, r);
    int amount = ui->amount;
    float x = menu.x + MENU_PAD, y = menu.y + 72;

    /* Inputs with how many are held against how many are needed, then the output. */
    for (int i = 0; i < r->input_count; i++) {
        int have = inventory_count(&g->inventory, r->inputs[i].id);
        int need = r->inputs[i].count * amount;
        hud_draw_item_icon(r->inputs[i].id, (Rectangle){ x, y, icon, icon });
        const char *text = TextFormat("%d/%d", have, need);
        DrawText(text, (int)(x + icon + 2), (int)y + 11, 16,
                 have >= need ? RAYWHITE : (Color){ 235, 90, 90, 255 });
        x += icon + 8 + MeasureText(text, 16);
    }
    DrawText(">", (int)x + 4, (int)y + 9, 20, LIGHTGRAY);
    x += 24;
    hud_draw_item_icon(r->output.id, (Rectangle){ x, y, icon, icon });
    DrawText(TextFormat("x%d %s", r->output.count * amount, item_def(r->output.id)->label),
             (int)(x + icon + 2), (int)y + 11, 16, RAYWHITE);

    /* Slider over 1..99; the part past what can be afforded is greyed out. */
    Rectangle s = slider_rect(menu);
    float mid = s.y + s.height / 2;
    float span = (float)(MAX_CRAFT_AMOUNT - 1);
    float cap_x = s.x + s.width * (float)((cap > 1 ? cap : 1) - 1) / span;
    float knob_x = s.x + s.width * (float)(amount - 1) / span;
    DrawRectangleRec((Rectangle){ s.x, mid - 3, s.width, 6 }, (Color){ 60, 62, 74, 255 });
    DrawRectangleRec((Rectangle){ s.x, mid - 3, cap_x - s.x, 6 }, (Color){ 110, 114, 130, 255 });
    DrawRectangleRec((Rectangle){ s.x, mid - 3, knob_x - s.x, 6 }, button_color);
    DrawRectangleRec((Rectangle){ knob_x - 5, mid - 10, 10, 20 }, cap > 0 ? RAYWHITE : GRAY);
    DrawText(TextFormat("x%d", amount), (int)(s.x + s.width + 14), (int)mid - 8, 16, RAYWHITE);

    Rectangle b = craft_rect(menu);
    if (g->craft_remaining > 0) {
        int done = g->craft_total - g->craft_remaining;
        DrawRectangleRec(b, (Color){ 44, 47, 58, 255 });
        DrawRectangleRec((Rectangle){ b.x, b.y, b.width * done / g->craft_total, b.height },
                         button_color);
        draw_centered(TextFormat("Crafting %d/%d", done, g->craft_total), b, 18, RAYWHITE);
    } else if (cap < 1) {
        draw_centered("Not enough items", b, 18, (Color){ 235, 90, 90, 255 });
    } else {
        bool hot = CheckCollisionPointRec(GetMousePosition(), b);
        DrawRectangleRec(b, hot ? (Color){ 90, 160, 100, 255 } : button_color);
        draw_centered("Craft", b, 18, RAYWHITE);
    }
}

void benchui_draw(const BenchUi *ui, const Game *g, const View *v)
{
    if (ui->popup && !g->bench_open) draw_popup(ui, v);
    if (!g->bench_open) return;

    const Recipe *recipes[MAX_BENCH_RECIPES];
    int count = open_recipes(g, recipes);
    Rectangle menu = menu_rect(g, v);
    float bench_y = view_tile_pos(v, 0.0f, (float)g->bench_pos.y + 0.5f).y;

    /* Pointer joining the menu to its bench. */
    DrawTriangle((Vector2){ menu.x, bench_y + 9 }, (Vector2){ menu.x, bench_y - 9 },
                 (Vector2){ menu.x - 12, bench_y }, edge_color);
    DrawRectangleRec(menu, panel_color);
    DrawRectangleLinesEx(menu, 2.0f, edge_color);

    draw_list(ui, recipes, count, list_rect(menu));
    /* Until input has caught up with a newly opened bench, show no selection. */
    bool current = ui->was_open && ui->shown.x == g->bench_pos.x && ui->shown.y == g->bench_pos.y;
    if (current && ui->selected >= 0 && ui->selected < count) {
        draw_recipe(ui, g, recipes[ui->selected], menu);
    } else {
        Rectangle rest = { menu.x, menu.y + 72, menu.width, menu.height - 72 };
        draw_centered("Select an item to craft", rest, 16, LIGHTGRAY);
    }
}
