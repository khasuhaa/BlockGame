#include "benchui.h"

#include "assets.h"
#include "hud.h"
#include "power.h"

#include <stddef.h>
#include <stdio.h>

#define MENU_WIDTH 340.0f
#define MENU_HEIGHT 262.0f
#define MENU_HEADER 32.0f /* the bench's name, level and upgrade button */
#define MENU_PAD 10.0f
#define LIST_CELL 48.0f
#define LIST_GAP 4.0f
#define POPUP_WIDTH 224.0f
#define FUEL_ROW 44.0f
#define MAX_FUEL_ROWS 8
#define POPUP_BUTTON_HEIGHT 26.0f
#define UI_DRAG_THRESHOLD 6.0f
#define STORAGE_SLOT 52.0f
#define STORAGE_GAP 4.0f
#define WHEEL_SCROLL 30.0f

static const Color panel_color = { 28, 30, 38, 240 };
static const Color edge_color = { 120, 122, 134, 255 };
static const Color button_color = { 70, 130, 80, 255 };

/* ---- layout ------------------------------------------------------------ */

static TileType open_bench(const Game *g)
{
    return game_bench_at(g, g->bench_pos.x, g->bench_pos.y);
}

/* The panel flying out of the open block: sized for crafting, or for a
 * storage block's grid of slots. */
static Rectangle menu_rect(const Game *g, const View *v)
{
    const TileDef *def = tile_def(open_bench(g));
    float w = MENU_WIDTH, h = MENU_HEIGHT;
    if (def->storage_columns > 0) {
        w = 2 * MENU_PAD + def->storage_columns * (STORAGE_SLOT + STORAGE_GAP) - STORAGE_GAP;
        h = MENU_HEADER + 2 * MENU_PAD + def->storage_rows * (STORAGE_SLOT + STORAGE_GAP) - STORAGE_GAP;
    }
    Vector2 tile = view_tile_pos(v, (float)g->bench_pos.x, (float)g->bench_pos.y);
    return (Rectangle){ tile.x + v->tile_size + 14.0f, tile.y + v->tile_size - h, w, h };
}

int benchui_storage_slots(const Game *g, const View *v, Rectangle *rects, int max, uint16_t *container)
{
    *container = game_open_storage(g);
    if (*container == 0) return 0;
    const TileDef *def = tile_def(open_bench(g));
    Rectangle menu = menu_rect(g, v);
    int n = 0;
    for (int row = 0; row < def->storage_rows; row++) {
        for (int col = 0; col < def->storage_columns && n < max; col++)
            rects[n++] = (Rectangle){ menu.x + MENU_PAD + col * (STORAGE_SLOT + STORAGE_GAP),
                                      menu.y + MENU_HEADER + MENU_PAD + row * (STORAGE_SLOT + STORAGE_GAP),
                                      STORAGE_SLOT, STORAGE_SLOT };
    }
    return n;
}

static Rectangle list_rect(Rectangle menu)
{
    return (Rectangle){ menu.x + MENU_PAD, menu.y + MENU_HEADER + MENU_PAD,
                        menu.width - 2 * MENU_PAD, LIST_CELL + 4.0f };
}

static Rectangle slider_rect(Rectangle menu)
{
    return (Rectangle){ menu.x + MENU_PAD + 6, menu.y + MENU_HEADER + 144,
                        menu.width - 2 * MENU_PAD - 62, 20 };
}

static Rectangle craft_rect(Rectangle menu)
{
    return (Rectangle){ menu.x + MENU_PAD, menu.y + menu.height - MENU_PAD - 34,
                        menu.width - 2 * MENU_PAD, 34 };
}

/* While something is being made, the right-hand end of that is the button that hurries it for crystals. */
static Rectangle rush_rect(Rectangle menu)
{
    Rectangle b = craft_rect(menu);
    return (Rectangle){ b.x + b.width - 92, b.y, 92, b.height };
}

/* And the left-hand end is the button that gives the work up. */
static Rectangle stop_rect(Rectangle menu)
{
    Rectangle b = craft_rect(menu);
    return (Rectangle){ b.x, b.y, 58, b.height };
}

/* The fuel gauge on the side of a bench that burns fuel, and the list of
 * things to burn that it opens. */
static Rectangle fuel_tab_rect(Rectangle menu)
{
    return (Rectangle){ menu.x + menu.width + 4, menu.y, 46, 112 };
}

/* The items held that can be burned, in the order they are listed. */
static int fuel_items(const Game *g, ItemId *out)
{
    int n = 0;
    for (int i = 1; i < content.item_count && n < MAX_FUEL_ROWS; i++)
        if (content.items[i].fuel > 0 && inventory_count(&g->inventory, i) > 0) out[n++] = i;
    return n;
}

static Rectangle fuel_panel_rect(const Game *g, Rectangle menu)
{
    ItemId items[MAX_FUEL_ROWS];
    int rows = fuel_items(g, items);
    Rectangle tab = fuel_tab_rect(menu);
    return (Rectangle){ tab.x + tab.width + 4, menu.y, 210,
                        MENU_HEADER + (rows > 0 ? rows : 1) * FUEL_ROW + MENU_PAD };
}

static Rectangle fuel_row_rect(Rectangle panel, int i)
{
    return (Rectangle){ panel.x + MENU_PAD, panel.y + MENU_HEADER + i * FUEL_ROW,
                        panel.width - 2 * MENU_PAD, FUEL_ROW - 4 };
}

static Rectangle upgrade_rect(Rectangle menu)
{
    return (Rectangle){ menu.x + menu.width - MENU_PAD - 132, menu.y + 7, 132, 26 };
}

/* The teleport button rides on top of a portal's menu, at its left end. */
static Rectangle teleport_rect(Rectangle menu)
{
    return (Rectangle){ menu.x, menu.y - 34.0f, 196.0f, 30.0f };
}

/* What the two buttons of the little menu over a block say: "Open ..." and
 * "Remove ..." in the block's own word for itself; "Sleep" for a bed. */
static void popup_labels(const BenchUi *ui, const Game *g, char labels[2][2 * CONTENT_NAME_LEN])
{
    TileType bed = game_bed_at(g, ui->popup_tile.x, ui->popup_tile.y);
    TileType t = game_bench_at(g, ui->popup_tile.x, ui->popup_tile.y);
    if (t == TILE_AIR) t = bed;
    const char *name = tile_def(t)->popup_name;
    if (game_bench_at(g, ui->popup_tile.x, ui->popup_tile.y) == TILE_AIR && bed != TILE_AIR) snprintf(labels[0], 2 * CONTENT_NAME_LEN, "Sleep");
    else snprintf(labels[0], 2 * CONTENT_NAME_LEN, "Open %s", name);
    snprintf(labels[1], 2 * CONTENT_NAME_LEN, "Remove %s", name);
}

/* (As wide as what it says needs.) */
static Rectangle popup_rect(const BenchUi *ui, const Game *g, const View *v)
{
    char labels[2][2 * CONTENT_NAME_LEN];
    popup_labels(ui, g, labels);
    float h = 2 * POPUP_BUTTON_HEIGHT + 3 * 6.0f, wide = POPUP_WIDTH;
    for (int i = 0; i < 2; i++)
        if ((float)MeasureText(labels[i], 16) + 36.0f > wide) wide = (float)MeasureText(labels[i], 16) + 36.0f;
    Vector2 tile = view_tile_pos(v, (float)ui->popup_tile.x, (float)ui->popup_tile.y);
    return (Rectangle){ tile.x + v->tile_size / 2 - wide / 2, tile.y - h - 8.0f, wide, h };
}

static Rectangle popup_button(Rectangle popup, int i)
{
    return (Rectangle){ popup.x + 6, popup.y + 6 + i * (POPUP_BUTTON_HEIGHT + 6),
                        popup.width - 12, POPUP_BUTTON_HEIGHT };
}

static int open_recipes(const Game *g, const Recipe **out)
{
    return bench_recipes(open_bench(g), out, MAX_BENCH_RECIPES);
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

/* A trade block has a menu of its own; this one stays out of its way. */
static bool is_trade(const Game *g)
{
    return g->bench_open && tile_def(open_bench(g))->trade;
}

bool benchui_mouse_over(const BenchUi *ui, const Game *g, const View *v)
{
    Vector2 m = GetMousePosition();
    if (is_trade(g)) return false;
    if (ui->press != BENCH_PRESS_NONE) return true;
    if (ui->popup && CheckCollisionPointRec(m, popup_rect(ui, g, v))) return true;
    if (!g->bench_open) return false;
    Rectangle menu = menu_rect(g, v);
    if (game_bench_fuel(g, NULL) >= 0) {
        if (CheckCollisionPointRec(m, fuel_tab_rect(menu))) return true;
        if (ui->fuel_open && CheckCollisionPointRec(m, fuel_panel_rect(g, menu))) return true;
    }
    if (game_teleport_cost(g) >= 0 && CheckCollisionPointRec(m, teleport_rect(menu))) return true;
    return CheckCollisionPointRec(m, menu);
}

static BenchPress press_target(const BenchUi *ui, const Game *g, const View *v, Vector2 m)
{
    if (ui->popup) {
        Rectangle popup = popup_rect(ui, g, v);
        if (CheckCollisionPointRec(m, popup_button(popup, 0))) return BENCH_PRESS_OPEN;
        if (CheckCollisionPointRec(m, popup_button(popup, 1))) return BENCH_PRESS_REMOVE;
        if (CheckCollisionPointRec(m, popup)) return BENCH_PRESS_PANEL;
    }
    if (!g->bench_open) return BENCH_PRESS_NONE;
    Rectangle menu = menu_rect(g, v);
    if (game_bench_fuel(g, NULL) >= 0) {
        if (CheckCollisionPointRec(m, fuel_tab_rect(menu))) return BENCH_PRESS_FUEL_TAB;
        if (ui->fuel_open && CheckCollisionPointRec(m, fuel_panel_rect(g, menu)))
            return BENCH_PRESS_FUEL_ITEM;
    }
    if (game_teleport_cost(g) >= 0 && CheckCollisionPointRec(m, teleport_rect(menu))) return BENCH_PRESS_TELEPORT;
    if (!CheckCollisionPointRec(m, menu)) return BENCH_PRESS_NONE;
    if (tile_def(open_bench(g))->storage_columns > 0) return BENCH_PRESS_PANEL;
    if (tile_def(open_bench(g))->upgrade_to != TILE_AIR &&
        CheckCollisionPointRec(m, upgrade_rect(menu)))
        return BENCH_PRESS_UPGRADE;
    if (CheckCollisionPointRec(m, list_rect(menu))) return BENCH_PRESS_LIST;
    if (ui->selected < 0) return BENCH_PRESS_PANEL;
    Rectangle slider = slider_rect(menu);
    slider.x -= 8;
    slider.width += 16;
    if (CheckCollisionPointRec(m, slider)) return BENCH_PRESS_SLIDER;
    if (game_craft_rush_cost(g) >= 0 && CheckCollisionPointRec(m, rush_rect(menu))) return BENCH_PRESS_RUSH;
    if (g->craft_remaining > 0 && CheckCollisionPointRec(m, stop_rect(menu))) return BENCH_PRESS_STOP;
    if (CheckCollisionPointRec(m, craft_rect(menu))) return BENCH_PRESS_CRAFT;
    return BENCH_PRESS_PANEL;
}

bool benchui_handle_input(BenchUi *ui, Game *g, const View *v)
{
    Vector2 m = GetMousePosition();
    if (is_trade(g)) {
        ui->popup = false;
        ui->was_open = false;
        ui->press = BENCH_PRESS_NONE;
        return false;
    }

    if (g->bench_open) {
        bool changed = ui->shown.x != g->bench_pos.x || ui->shown.y != g->bench_pos.y;
        if (!ui->was_open || changed) {
            ui->selected = -1;
            ui->scroll = 0.0f;
            ui->amount = 1;
            ui->fuel_open = false;
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
    /* What is being made is what is shown, so that how it is coming along can be seen. */
    for (int i = 0; g->bench_open && g->craft_remaining > 0 && i < count; i++)
        if (recipes[i] == g->craft_recipe) ui->selected = i;

    /* The amount never goes past what there are materials for. */
    int cap = ui->selected >= 0 ? recipe_max_crafts(&g->inventory, recipes[ui->selected], g->crystals) : 0;
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
        /* A recipe the bench is not yet good enough for cannot be picked. */
        if (i >= 0 && i < count && offset - i * (LIST_CELL + LIST_GAP) <= LIST_CELL &&
            !recipe_locked(open_bench(g), recipes[i])) {
            ui->selected = i;
            ui->amount = 1;
        }
    } else if (ui->press == BENCH_PRESS_FUEL_TAB && released == BENCH_PRESS_FUEL_TAB) {
        ui->fuel_open = !ui->fuel_open;
    } else if (ui->press == BENCH_PRESS_FUEL_ITEM && released == BENCH_PRESS_FUEL_ITEM) {
        /* Each click on a row burns one of that item. */
        ItemId items[MAX_FUEL_ROWS];
        int rows = fuel_items(g, items);
        Rectangle panel = fuel_panel_rect(g, menu);
        for (int i = 0; i < rows; i++)
            if (CheckCollisionPointRec(m, fuel_row_rect(panel, i))) game_add_fuel(g, items[i]);
    } else if (ui->press == BENCH_PRESS_TELEPORT && released == BENCH_PRESS_TELEPORT) {
        if (game_teleport(g)) ui->teleported = true;
    } else if (ui->press == BENCH_PRESS_UPGRADE && released == BENCH_PRESS_UPGRADE) {
        game_upgrade_bench(g);
    } else if (ui->press == BENCH_PRESS_RUSH && released == BENCH_PRESS_RUSH) {
        game_craft_rush(g);
    } else if (ui->press == BENCH_PRESS_STOP && released == BENCH_PRESS_STOP) {
        game_craft_stop(g);
    } else if (ui->press == BENCH_PRESS_CRAFT && released == BENCH_PRESS_CRAFT) {
        game_craft(g, recipes[ui->selected], ui->amount);
    } else if (ui->press == BENCH_PRESS_OPEN && released == BENCH_PRESS_OPEN) {
        ui->popup = false;
        /* A bench shows its menu from here; a chest is walked to; a bed is gone to, and slept in. */
        if (game_bench_at(g, ui->popup_tile.x, ui->popup_tile.y) == TILE_AIR)
            game_sleep_in(g, ui->popup_tile.x, ui->popup_tile.y);
        else if (!game_open_bench(g, ui->popup_tile.x, ui->popup_tile.y))
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

static void draw_popup(const BenchUi *ui, const Game *g, const View *v)
{
    char labels[2][2 * CONTENT_NAME_LEN];
    popup_labels(ui, g, labels);
    Rectangle popup = popup_rect(ui, g, v);
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

/* A padlock, for recipes that need a better bench. */
static void draw_lock(Rectangle cell)
{
    Color metal = { 214, 190, 96, 255 }, dark = { 60, 52, 28, 255 };
    float cx = cell.x + cell.width / 2, cy = cell.y + cell.height / 2 - 3;
    DrawRing((Vector2){ cx, cy - 3 }, 4.5f, 7.5f, 180.0f, 360.0f, 16, metal);
    DrawRectangleRec((Rectangle){ cx - 9, cy - 3, 18, 13 }, metal);
    DrawRectangleRec((Rectangle){ cx - 1.5f, cy + 1, 3, 5 }, dark);
}

/* The bench's name and level, and the button that upgrades it. */
static void draw_header(const Game *g, Rectangle menu)
{
    const TileDef *def = tile_def(open_bench(g));
    bool levelled = def->upgrade_to != TILE_AIR || def->bench_level > 1;
    const char *name = levelled ? TextFormat("%s  Tier %d", def->label, def->bench_level) : def->label;
    /* Shrink the name rather than let it run under the upgrade button. */
    float space = (def->upgrade_to != TILE_AIR ? upgrade_rect(menu).x - 8 : menu.x + menu.width - MENU_PAD) -
                  (menu.x + MENU_PAD);
    int size = 16;
    while (size > 10 && MeasureText(name, size) > space) size--;
    DrawText(name, (int)(menu.x + MENU_PAD), (int)menu.y + 12 + (16 - size) / 2, size, RAYWHITE);
    if (def->upgrade_to == TILE_AIR) return;

    Rectangle b = upgrade_rect(menu);
    bool can = inventory_has_all(&g->inventory, def->upgrade_cost, def->upgrade_cost_count);
    bool hot = can && CheckCollisionPointRec(GetMousePosition(), b);
    DrawRectangleRec(b, !can ? (Color){ 54, 57, 70, 255 } : hot ? (Color){ 90, 160, 100, 255 } : button_color);
    /* One that takes time fills up as it goes. */
    float going = game_upgrade_progress(g);
    if (going >= 0.0f) DrawRectangleRec((Rectangle){ b.x, b.y, b.width * going, b.height }, (Color){ 120, 200, 130, 255 });
    DrawText(going >= 0.0f ? "Upgrading" : "Upgrade", (int)b.x + 6, (int)b.y + 6, 14, can ? RAYWHITE : GRAY);
    /* What it costs, right-aligned: a count beside each item. */
    float x = b.x + b.width - 4;
    for (int i = def->upgrade_cost_count - 1; i >= 0; i--) {
        const char *n = TextFormat("%d", def->upgrade_cost[i].count);
        x -= 22;
        hud_draw_item_icon(def->upgrade_cost[i].id, (Rectangle){ x, b.y + 2, 22, 22 });
        x -= (float)MeasureText(n, 14) + 1;
        DrawText(n, (int)x, (int)b.y + 6, 14, can ? RAYWHITE : (Color){ 235, 90, 90, 255 });
        x -= 4;
    }
}

static void draw_list(const BenchUi *ui, const Game *g, const Recipe **recipes, int count,
                      Rectangle list)
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
        /* What comes out in a colour shows the colour. */
        if (recipes[i]->tint > 0 && recipes[i]->tint <= content.tint_count) {
            const unsigned char *c = content.tints[recipes[i]->tint - 1];
            Rectangle swatch = { cell.x + 8, cell.y + cell.height - 22, cell.width - 16, 14 };
            DrawRectangleRec(swatch, (Color){ c[0], c[1], c[2], 255 });
            DrawRectangleLinesEx(swatch, 1.0f, (Color){ 14, 15, 20, 255 });
        }
        /* Where more than one recipe here makes the same thing, each shows
         * what it is made from, small, in its top right corner. */
        bool shared = false;
        for (int k = 0; k < count && !shared; k++) shared = k != i && recipes[k]->output.id == recipes[i]->output.id;
        if (shared && recipes[i]->tint == 0 && recipes[i]->input_count > 0) {
            /* (The first input the others do not all share: what makes this one this one.) */
            ItemId mark = recipes[i]->inputs[0].id;
            for (int in = 0; in < recipes[i]->input_count; in++) {
                bool common = true;
                for (int k = 0; k < count && common; k++) {
                    if (recipes[k]->output.id != recipes[i]->output.id || k == i) continue;
                    bool has = false;
                    for (int j = 0; j < recipes[k]->input_count; j++) has = has || recipes[k]->inputs[j].id == recipes[i]->inputs[in].id;
                    common = has;
                }
                if (!common) {
                    mark = recipes[i]->inputs[in].id;
                    break;
                }
            }
            Rectangle corner = { cell.x + cell.width - 24, cell.y + 2, 22, 22 };
            DrawRectangleRec(corner, (Color){ 20, 21, 27, 230 });
            DrawRectangleLinesEx(corner, 1.0f, edge_color);
            hud_draw_item_icon(mark, corner);
        }
        if (recipe_locked(open_bench(g), recipes[i])) {
            DrawRectangleRec(cell, Fade((Color){ 20, 21, 27, 255 }, 0.72f));
            draw_lock(cell);
            const char *need = TextFormat("Tier %d", recipes[i]->level);
            DrawText(need, (int)(cell.x + (cell.width - MeasureText(need, 10)) / 2),
                     (int)(cell.y + cell.height - 13), 10, LIGHTGRAY);
        }
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
    int cap = recipe_max_crafts(&g->inventory, r, g->crystals);
    int amount = ui->amount;
    float left = menu.x + MENU_PAD, room = menu.width - 2 * MENU_PAD;
    float y = menu.y + MENU_HEADER + 70;

    /* What is being made, on a line of its own so a long name has room. */
    DrawText(TextFormat("%s x%d", item_def(r->output.id)->label, r->output.count * amount),
             (int)left, (int)y, 16, RAYWHITE);
    /* How long each takes, at the end of the line. */
    float secs = game_craft_seconds(g, r);
    const char *takes = secs >= 60.0f ? ((int)secs % 60 ? TextFormat("%dm %ds each", (int)secs / 60, (int)secs % 60)
                                                         : TextFormat("%dm each", (int)secs / 60))
                        : secs >= 1.0f ? TextFormat("%ds each", (int)secs) : "";
    DrawText(takes, (int)(left + room) - MeasureText(takes, 12), (int)y + 2, 12, LIGHTGRAY);
    y += 24;

    /* Inputs with how many are held against how many are needed, then the
     * output. If the row would run off the menu it is drawn smaller. */
    static const struct {
        float icon;
        int font;
    } sizes[] = { { 36, 16 }, { 30, 14 }, { 24, 12 }, { 20, 10 } };
    float icon = sizes[0].icon;
    int font = sizes[0].font;
    for (int k = 0; k < 4; k++) {
        icon = sizes[k].icon;
        font = sizes[k].font;
        float width = 24 + icon;
        if (r->crystals > 0)
            width += icon + 8 + MeasureText(TextFormat("%d/%d", g->crystals, r->crystals * amount), font);
        for (int i = 0; i < r->input_count; i++)
            width += icon + 8 + MeasureText(TextFormat("%d/%d", recipe_input_held(&g->inventory, r->inputs[i]),
                                                       r->inputs[i].count * amount), font);
        if (width <= room) break;
    }
    float x = left, text_y = y + (36 - font) / 2.0f, icon_y = y + (36 - icon) / 2.0f;
    for (int i = 0; i < r->input_count; i++) {
        int have = recipe_input_held(&g->inventory, r->inputs[i]);
        int need = r->inputs[i].count * amount;
        hud_draw_item_icon(r->inputs[i].id, (Rectangle){ x, icon_y, icon, icon });
        const char *text = TextFormat("%d/%d", have, need);
        DrawText(text, (int)(x + icon + 2), (int)text_y, font,
                 have >= need ? RAYWHITE : (Color){ 235, 90, 90, 255 });
        x += icon + 8 + MeasureText(text, font);
    }
    if (r->crystals > 0) {
        /* Crystals are paid like an input, but come from the player, not the inventory. */
        int need = r->crystals * amount;
        assets_draw_image(content.trade.crystal_icon, (Rectangle){ x, icon_y, icon, icon });
        const char *text = TextFormat("%d/%d", g->crystals, need);
        DrawText(text, (int)(x + icon + 2), (int)text_y, font,
                 g->crystals >= need ? RAYWHITE : (Color){ 235, 90, 90, 255 });
        x += icon + 8 + MeasureText(text, font);
    }
    DrawText(">", (int)x + 4, (int)(y + 8), 20, LIGHTGRAY);
    hud_draw_item_icon(r->output.id, (Rectangle){ x + 24, icon_y, icon, icon });

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
    if (game_bench_fuel(g, NULL) == 0) {
        draw_centered("Needs fuel", b, 18, (Color){ 240, 150, 60, 255 });
    } else if (game_craft_power(g, NULL) > 0) {
        /* An electric bench, waiting for what it needs to come down the wire. */
        int arrived, needs = game_craft_power(g, &arrived);
        DrawRectangleRec(b, (Color){ 44, 47, 58, 255 });
        DrawRectangleRec((Rectangle){ b.x, b.y, b.width * (float)arrived / (float)needs, b.height }, (Color){ 150, 120, 30, 255 });
        Rectangle stop = stop_rect(menu);
        draw_centered("Waiting for power", (Rectangle){ b.x + stop.width, b.y, b.width - stop.width, b.height }, 18, (Color){ 250, 224, 110, 255 });
        DrawRectangleRec(stop, CheckCollisionPointRec(GetMousePosition(), stop) ? (Color){ 190, 80, 80, 255 } : (Color){ 150, 60, 60, 255 });
        draw_centered("Stop", stop, 14, RAYWHITE);
    } else if (g->craft_remaining > 0) {
        int done = g->craft_total - g->craft_remaining;
        float part = game_craft_progress(g);
        DrawRectangleRec(b, (Color){ 44, 47, 58, 255 });
        /* (Filling as the one being made comes along, not only as each is finished.) */
        DrawRectangleRec((Rectangle){ b.x, b.y, b.width * ((float)done + (part > 0.0f ? part : 0.0f)) / (float)g->craft_total, b.height },
                         button_color);
        int rush = game_craft_rush_cost(g);
        Rectangle hurry = rush_rect(menu);
        Rectangle stop = stop_rect(menu);
        Rectangle words = { b.x + stop.width, b.y, b.width - stop.width - (rush >= 0 ? hurry.width : 0), b.height };
        draw_centered(TextFormat("Crafting %d/%d", done, g->craft_total), words, 18, RAYWHITE);
        DrawRectangleRec(stop, CheckCollisionPointRec(GetMousePosition(), stop) ? (Color){ 190, 80, 80, 255 } : (Color){ 150, 60, 60, 255 });
        draw_centered("Stop", stop, 14, RAYWHITE);
        if (rush >= 0) {
            /* Finish it now, for a crystal for every ten seconds it still has to go. */
            bool can = g->crystals >= rush, hot = can && CheckCollisionPointRec(GetMousePosition(), hurry);
            DrawRectangleRec(hurry, !can ? (Color){ 54, 57, 70, 255 } : hot ? (Color){ 70, 150, 190, 255 } : (Color){ 50, 120, 160, 255 });
            const char *cost = TextFormat("Now: %d", rush);
            int wide = MeasureText(cost, 14);
            DrawText(cost, (int)(hurry.x + (hurry.width - (float)wide - 18) / 2), (int)hurry.y + 10, 14, can ? RAYWHITE : GRAY);
            assets_draw_image(content.trade.crystal_icon, (Rectangle){ hurry.x + (hurry.width - (float)wide - 18) / 2 + (float)wide + 2, hurry.y + 9, 16, 16 });
        }
    } else if ((item_def(r->output.id)->becomes_player && g->character_count >= content.rules.max_players) ||
               (r->characters > 0 && g->character_count > r->characters)) {
        draw_centered("Already made", b, 18, (Color){ 170, 172, 184, 255 });
    } else if (r->characters > 0 && g->character_count < r->characters) {
        draw_centered("Not yet", b, 18, (Color){ 170, 172, 184, 255 });
    } else if (cap < 1) {
        draw_centered("Not enough items", b, 18, (Color){ 235, 90, 90, 255 });
    } else {
        bool hot = CheckCollisionPointRec(GetMousePosition(), b);
        DrawRectangleRec(b, hot ? (Color){ 90, 160, 100, 255 } : button_color);
        /* From a distance the player has to go there first. */
        draw_centered(game_at_bench(g) ? "Craft" : "Go and craft", b, 18, RAYWHITE);
    }
}

/* The gauge beside a bench that burns fuel, and the list it opens. */
static void draw_fuel(const BenchUi *ui, const Game *g, Rectangle menu)
{
    int capacity = 1, fuel = game_bench_fuel(g, &capacity);
    if (fuel < 0) return;
    Color flame = { 240, 150, 60, 255 };
    Vector2 m = GetMousePosition();

    Rectangle tab = fuel_tab_rect(menu);
    DrawRectangleRec(tab, CheckCollisionPointRec(m, tab) || ui->fuel_open ? (Color){ 44, 47, 58, 245 }
                                                                          : panel_color);
    DrawRectangleLinesEx(tab, 2.0f, fuel > 0 ? edge_color : flame);
    draw_centered("Fuel", (Rectangle){ tab.x, tab.y + 4, tab.width, 18 }, 14, RAYWHITE);
    Rectangle gauge = { tab.x + 16, tab.y + 26, tab.width - 32, tab.height - 36 };
    float level = gauge.height * (float)fuel / (float)capacity;
    DrawRectangleRec(gauge, (Color){ 18, 19, 24, 255 });
    DrawRectangleRec((Rectangle){ gauge.x, gauge.y + gauge.height - level, gauge.width, level }, flame);
    DrawRectangleLinesEx(gauge, 1.0f, edge_color);
    if (!ui->fuel_open) return;

    ItemId items[MAX_FUEL_ROWS];
    int rows = fuel_items(g, items);
    Rectangle panel = fuel_panel_rect(g, menu);
    DrawRectangleRec(panel, panel_color);
    DrawRectangleLinesEx(panel, 2.0f, edge_color);
    DrawText(fuel >= capacity ? "Fuel is full" : "Add fuel", (int)(panel.x + MENU_PAD), (int)panel.y + 10,
             16, RAYWHITE);
    if (rows == 0)
        DrawText("Nothing to burn", (int)(panel.x + MENU_PAD), (int)(panel.y + MENU_HEADER + 10), 14,
                 LIGHTGRAY);
    for (int i = 0; i < rows; i++) {
        Rectangle row = fuel_row_rect(panel, i);
        bool usable = fuel < capacity;
        bool hot = usable && CheckCollisionPointRec(m, row);
        DrawRectangleRec(row, hot ? (Color){ 80, 84, 100, 255 } : (Color){ 44, 47, 58, 255 });
        hud_draw_item_icon(items[i], (Rectangle){ row.x + 2, row.y + 2, row.height - 4, row.height - 4 });
        DrawText(TextFormat("%s x%d", item_def(items[i])->label, inventory_count(&g->inventory, items[i])),
                 (int)(row.x + row.height + 4), (int)row.y + 5, 14, usable ? RAYWHITE : GRAY);
        /* How much of the gauge one of these fills. */
        DrawText(TextFormat("+%d%%", item_def(items[i])->fuel * 100 / capacity),
                 (int)(row.x + row.height + 4), (int)row.y + 22, 12, flame);
    }
}

void benchui_draw(const BenchUi *ui, const Game *g, const View *v)
{
    if (ui->popup && !g->bench_open) draw_popup(ui, g, v);
    if (!g->bench_open || is_trade(g)) return;

    const Recipe *recipes[MAX_BENCH_RECIPES];
    int count = open_recipes(g, recipes);
    Rectangle menu = menu_rect(g, v);
    float bench_y = view_tile_pos(v, 0.0f, (float)g->bench_pos.y + 0.5f).y;

    /* A portal offers to bring the player to it, for crystals. */
    int fare = game_teleport_cost(g);
    if (fare >= 0) {
        Rectangle b = teleport_rect(menu);
        bool can = g->crystals >= fare && !game_at_bench(g);
        bool hot = can && CheckCollisionPointRec(GetMousePosition(), b);
        DrawRectangleRec(b, !can ? (Color){ 44, 47, 60, 245 } : hot ? (Color){ 96, 120, 200, 255 } : (Color){ 66, 88, 160, 250 });
        DrawRectangleLinesEx(b, 1.0f, edge_color);
        DrawText(game_at_bench(g) ? "You are here" : "Teleport", (int)b.x + 8, (int)b.y + 8, 14, can ? RAYWHITE : GRAY);
        if (!game_at_bench(g)) {
            const char *n = TextFormat("%d", fare);
            float x = b.x + b.width - 28;
            assets_draw_image(content.trade.crystal_icon, (Rectangle){ x, b.y + 4, 22, 22 });
            DrawText(n, (int)x - MeasureText(n, 14) - 4, (int)b.y + 8, 14,
                     g->crystals >= fare ? RAYWHITE : (Color){ 235, 90, 90, 255 });
        }
    }

    /* Pointer joining the menu to its bench. */
    DrawTriangle((Vector2){ menu.x, bench_y + 9 }, (Vector2){ menu.x, bench_y - 9 },
                 (Vector2){ menu.x - 12, bench_y }, edge_color);
    DrawRectangleRec(menu, panel_color);
    DrawRectangleLinesEx(menu, 2.0f, edge_color);

    draw_header(g, menu);
    draw_fuel(ui, g, menu);
    /* A storage block is just its name: the inventory draws the slots, so
     * that items can be dragged between them and the bar. */
    if (tile_def(open_bench(g))->storage_columns > 0) return;
    draw_list(ui, g, recipes, count, list_rect(menu));
    /* Until input has caught up with a newly opened bench, show no selection. */
    bool current = ui->was_open && ui->shown.x == g->bench_pos.x && ui->shown.y == g->bench_pos.y;
    if (current && ui->selected >= 0 && ui->selected < count) {
        draw_recipe(ui, g, recipes[ui->selected], menu);
    } else {
        float top = MENU_HEADER + 72;
        Rectangle rest = { menu.x, menu.y + top, menu.width, menu.height - top };
        draw_centered("Select an item to craft", rest, 16, LIGHTGRAY);
    }
}
