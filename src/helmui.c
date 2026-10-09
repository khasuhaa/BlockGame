#include "helmui.h"

#include <stdio.h>

#include "ship.h"

#define PAD 8.0f
#define ROW 28.0f
#define NAME_WIDTH 150.0f
#define NEED_WIDTH 150.0f
#define GO_WIDTH 96.0f
#define STAT_ROWS 3

static const Color panel_color = { 28, 30, 38, 240 };
static const Color edge_color = { 120, 122, 134, 255 };
static const Color button_color = { 54, 57, 70, 255 }, button_hot = { 80, 84, 100, 255 }, button_off = { 40, 42, 50, 255 };
static const char *const need_names[SEA_NEEDS] = { "Keel", "Way", "Freeboard", "Strength", "Cable", "Depth", "Light", "Guns", "Reach" };

static Rectangle helm_rect(const Game *g, const View *v)
{
    float w = NAME_WIDTH + NEED_WIDTH + GO_WIDTH + 2 * PAD, h = (float)(content.worldgen.sea_count + STAT_ROWS + 1) * ROW + 2 * PAD;
    Vector2 tile = view_tile_pos(v, (float)g->bench_pos.x, (float)g->bench_pos.y);
    float x = tile.x + v->tile_size + 14.0f, y = tile.y + v->tile_size - h;
    if (x > (float)GetScreenWidth() - w - 4.0f) x = (float)GetScreenWidth() - w - 4.0f;
    if (y < 4.0f) y = 4.0f;
    return (Rectangle){ x, y, w, h };
}

static Rectangle go_rect(Rectangle panel, int row)
{
    return (Rectangle){ panel.x + PAD + NAME_WIDTH + NEED_WIDTH, panel.y + PAD + (float)row * ROW + 2.0f, GO_WIDTH, ROW - 4.0f };
}

bool helmui_mouse_over(const HelmUi *ui, const Game *g, const View *v)
{
    return game_helm_open(g) && (ui->pressed || CheckCollisionPointRec(GetMousePosition(), helm_rect(g, v)));
}

bool helmui_handle_input(HelmUi *ui, Game *g, const View *v)
{
    if (!game_helm_open(g)) {
        ui->pressed = false;
        return false;
    }
    Vector2 m = GetMousePosition();
    Rectangle panel = helm_rect(g, v);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ui->pressed = CheckCollisionPointRec(m, panel);
    if (!ui->pressed) return false;
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return true;
    ui->pressed = false;
    for (int sea = 0; sea < content.worldgen.sea_count; sea++)
        if (CheckCollisionPointRec(m, go_rect(panel, sea))) ship_set_course(g, sea);
    if (CheckCollisionPointRec(m, go_rect(panel, content.worldgen.sea_count))) ship_cast_off(g);
    return true;
}

static void draw_button(Rectangle r, const char *text, bool enabled)
{
    bool hot = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, !enabled ? button_off : hot ? button_hot : button_color);
    DrawText(text, (int)(r.x + (r.width - (float)MeasureText(text, 14)) / 2.0f), (int)(r.y + (r.height - 14.0f) / 2.0f), 14, enabled ? RAYWHITE : GRAY);
}

void helmui_draw(const HelmUi *ui, const Game *g, const View *v)
{
    (void)ui;
    if (!game_helm_open(g)) return;
    Rectangle panel = helm_rect(g, v);
    DrawRectangleRec(panel, panel_color);
    DrawRectangleLinesEx(panel, 1.0f, edge_color);
    char text[96];
    for (int sea = 0; sea < content.worldgen.sea_count; sea++) {
        const SeaDef *def = &content.worldgen.seas[sea];
        float y = panel.y + PAD + (float)sea * ROW;
        int lacking = -1;
        bool can = ship_can_sail(g, sea, &lacking), here = sea == g->sea && g->passage == 0, bound = g->passage > 0 && sea == g->sea_to;
        DrawText(def->label, (int)(panel.x + PAD), (int)(y + 7.0f), 14, here || bound ? (Color){ 240, 210, 120, 255 } : RAYWHITE);
        if (!can && lacking >= 0) {
            snprintf(text, sizeof text, "%s %d/%d", need_names[lacking], ship_has(g, lacking), def->needs[lacking]);
            DrawText(text, (int)(panel.x + PAD + NAME_WIDTH), (int)(y + 7.0f), 14, (Color){ 220, 120, 110, 255 });
        }
        if (bound) {
            /* The passage: how much of it is behind her. */
            Rectangle bar = go_rect(panel, sea);
            float done = def->passage_ticks > 0 ? 1.0f - (float)g->passage / (float)def->passage_ticks : 1.0f;
            DrawRectangleRec(bar, button_off);
            DrawRectangleRec((Rectangle){ bar.x, bar.y, bar.width * (done < 0.0f ? 0.0f : done), bar.height }, (Color){ 90, 140, 200, 255 });
        } else if (!here) {
            draw_button(go_rect(panel, sea), "Set course", can && g->passage == 0);
        }
    }
    /* What is alongside, and letting it go. */
    float y = panel.y + PAD + (float)content.worldgen.sea_count * ROW;
    const char *beside = ship_alongside(g);
    if (beside) {
        DrawText(beside, (int)(panel.x + PAD), (int)(y + 7.0f), 14, (Color){ 240, 210, 120, 255 });
        draw_button(go_rect(panel, content.worldgen.sea_count), "Cast off", true);
    }
    /* What she is counted as. */
    const ShipReport *r = &g->ship;
    const char *names[9] = { "Keel", "Burden", "Way", "Freeboard", "Strength", "Cable", "Reach", "Depth", "Guns" };
    int have[9] = { r->keel, r->stats[SHIP_BURDEN], r->way, r->freeboard, r->stats[SHIP_STRENGTH], r->stats[SHIP_CABLE], r->stats[SHIP_REACH], r->stats[SHIP_DEPTH], r->stats[SHIP_GUNS] };
    float cell = (panel.width - 2 * PAD) / 3.0f;
    for (int i = 0; i < 9; i++) {
        if (i == 1) snprintf(text, sizeof text, "%s %d/%d", names[i], r->stats[SHIP_WEIGHT], have[i]);
        else snprintf(text, sizeof text, "%s %d", names[i], have[i]);
        DrawText(text, (int)(panel.x + PAD + (float)(i % 3) * cell), (int)(y + ROW + (float)(i / 3) * ROW + 7.0f), 14, LIGHTGRAY);
    }
}
