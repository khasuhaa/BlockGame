#include "daoui.h"

#include "dao.h"
#include "hud.h"

#include <raylib.h>
#include <stdio.h>

static const Color panel_color = { 24, 22, 34, 240 }, edge_color = { 150, 130, 90, 255 }, jade = { 110, 200, 170, 255 }, dim = { 170, 172, 184, 255 };

static Rectangle button_rect(void)
{
    return (Rectangle){ 16.0f, (float)GetScreenHeight() - 62.0f, 196.0f, 46.0f };
}

static Rectangle panel_rect(void)
{
    Rectangle b = button_rect();
    return (Rectangle){ b.x, b.y - 348.0f, 372.0f, 340.0f };
}

static Rectangle break_rect(void)
{
    Rectangle p = panel_rect();
    return (Rectangle){ p.x + 12, p.y + p.height - 44, p.width - 24, 32 };
}

bool daoui_mouse_over(const DaoUi *ui)
{
    if (!dao_on()) return false;
    Vector2 m = GetMousePosition();
    return CheckCollisionPointRec(m, button_rect()) || (ui->open && CheckCollisionPointRec(m, panel_rect()));
}

bool daoui_handle_input(DaoUi *ui, Game *g)
{
    if (!dao_on()) return false;
    bool over = daoui_mouse_over(ui);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ui->pressed = over;
    if (!ui->pressed) return over;
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return true;
    ui->pressed = false;
    Vector2 m = GetMousePosition();
    if (CheckCollisionPointRec(m, button_rect())) ui->open = !ui->open;
    else if (ui->open && CheckCollisionPointRec(m, break_rect())) game_breakthrough(g);
    return true;
}

static void bar(Rectangle r, float value, Color fill)
{
    value = value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
    DrawRectangleRec(r, (Color){ 40, 38, 52, 255 });
    DrawRectangleRec((Rectangle){ r.x, r.y, r.width * value, r.height }, fill);
    DrawRectangleLinesEx(r, 1.0f, (Color){ 12, 12, 18, 255 });
}

void daoui_draw(const DaoUi *ui, const Game *g)
{
    if (!dao_on() || g->character_count == 0) return;
    const Dao *me = &g->dao;
    char label[96];
    game_dao_label(g, label, sizeof label);
    float room = game_dao_capacity(g);
    Rectangle b = button_rect();
    bool hot = CheckCollisionPointRec(GetMousePosition(), b);
    DrawRectangleRec(b, hot ? (Color){ 44, 40, 60, 245 } : panel_color);
    DrawRectangleLinesEx(b, 2.0f, edge_color);
    int size = 16;
    while (size > 10 && MeasureText(label, size) > (int)b.width - 16) size--;
    DrawText(label, (int)b.x + 8, (int)b.y + 6, size, RAYWHITE);
    bar((Rectangle){ b.x + 8, b.y + 28, b.width - 16, 10 }, room > 0.0f ? me->qi / room : me->stage > 0 ? 1.0f : 0.0f, me->bolts > 0 ? (Color){ 240, 230, 120, 255 } : jade);
    if (!ui->open) return;

    Rectangle p = panel_rect();
    DrawRectangleRec(p, panel_color);
    DrawRectangleLinesEx(p, 2.0f, edge_color);
    float x = p.x + 12, y = p.y + 10, wide = p.width - 24;
    DrawText(label, (int)x, (int)y, 20, RAYWHITE);
    y += 28;
    DrawText("Qi", (int)x, (int)y, 14, dim);
    bar((Rectangle){ x + 76, y, wide - 76, 14 }, room > 0.0f ? me->qi / room : 0.0f, jade);
    const char *held = room > 0.0f ? TextFormat("%d / %d", (int)me->qi, (int)room) : "";
    DrawText(held, (int)(x + 76 + (wide - 76 - (float)MeasureText(held, 10)) / 2), (int)y + 2, 10, RAYWHITE);
    y += 20;
    int rate = g->tick_rate > 0 ? g->tick_rate : 20;
    DrawText("Here", (int)x, (int)y, 14, dim);
    DrawText(TextFormat("%.1f qi    +%.2f a second", game_qi_at(g, g->player.feet.x, g->player.feet.y), game_dao_rate(g) * (float)rate), (int)x + 76, (int)y, 14, RAYWHITE);
    y += 20;
    DrawText("Manual", (int)x, (int)y, 14, dim);
    DrawText(me->manual > 0 && me->manual < content.item_count ? item_def((ItemId)me->manual)->label : "None", (int)x + 76, (int)y, 14, RAYWHITE);
    y += 20;
    DrawText("Roots", (int)x, (int)y, 14, dim);
    float rx = x + 76;
    static const Color element[DAO_ELEMENTS] = { { 220, 220, 230, 255 }, { 110, 200, 110, 255 }, { 100, 160, 240, 255 }, { 240, 120, 80, 255 }, { 200, 160, 100, 255 } };
    for (int i = 0; i < DAO_ELEMENTS; i++) {
        if (me->roots[i] == 0) continue;
        const char *root = TextFormat("%s %d", content.dao.elements[i], me->roots[i]);
        DrawText(root, (int)rx, (int)y, 14, element[i]);
        rx += (float)MeasureText(root, 14) + 10;
    }
    if (rx == x + 76) DrawText("None yet", (int)rx, (int)y, 14, RAYWHITE);
    y += 20;
    DrawText("Meridians", (int)x, (int)y, 14, dim);
    for (int i = 0; i < DAO_MERIDIANS; i++)
        DrawRectangleRec((Rectangle){ x + 76 + (float)i * 16, y + 2, 12, 10 }, i < me->meridians ? jade : (Color){ 40, 38, 52, 255 });
    y += 20;
    static const char *built[5] = { "Flawed", "Weak", "Sound", "Firm", "Perfect" };
    DrawText("Base", (int)x, (int)y, 14, dim);
    DrawText(me->stage > 0 ? built[me->foundation > 4 ? 4 : me->foundation] : "", (int)x + 76, (int)y, 14, RAYWHITE);
    DrawText("Insight", (int)x + 190, (int)y, 14, dim);
    DrawText(TextFormat("%d", (int)me->insight), (int)x + 250, (int)y, 14, RAYWHITE);
    y += 20;
    DrawText("Toxin", (int)x, (int)y, 14, dim);
    bar((Rectangle){ x + 76, y + 1, wide - 76, 12 }, me->toxin, (Color){ 150, 96, 200, 255 });
    y += 26;

    /* The step they are on: what it asks for, and the chance it has. */
    RecipeItem needs[4];
    int count = game_dao_needs(g, needs);
    if (me->bolts > 0) {
        DrawText(TextFormat("Tribulation: %d", me->bolts), (int)x, (int)y, 16, (Color){ 240, 230, 120, 255 });
    } else if (me->stage >= 1 && me->stage <= dao_stages()) {
        DrawText("Next", (int)x, (int)y + 6, 14, dim);
        float nx = x + 76;
        for (int i = 0; i < count; i++) {
            int have = inventory_count(&g->inventory, needs[i].id);
            hud_draw_item_icon(needs[i].id, (Rectangle){ nx, y, 26, 26 });
            const char *n = TextFormat("%d/%d", have, needs[i].count);
            DrawText(n, (int)nx + 28, (int)y + 7, 12, have >= needs[i].count ? RAYWHITE : (Color){ 235, 90, 90, 255 });
            if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){ nx, y, 60, 26 }))
                DrawText(item_def(needs[i].id)->label, (int)x + 76, (int)y + 28, 12, RAYWHITE);
            nx += 66;
        }
        if (content.dao.need_insight[me->stage] > 0) {
            const char *n = TextFormat("Insight %d/%d", (int)me->insight, content.dao.need_insight[me->stage]);
            DrawText(n, (int)nx, (int)y + 7, 12, (int)me->insight >= content.dao.need_insight[me->stage] ? RAYWHITE : (Color){ 235, 90, 90, 255 });
        }
        const char *chance = TextFormat("%d%%", (int)(game_dao_chance(g) * 100.0f + 0.5f));
        DrawText(chance, (int)(x + wide) - MeasureText(chance, 16), (int)y + 5, 16, RAYWHITE);
    }
    Rectangle go = break_rect();
    bool can = game_dao_ready(g), over = can && CheckCollisionPointRec(GetMousePosition(), go);
    DrawRectangleRec(go, !can ? (Color){ 54, 57, 70, 255 } : over ? (Color){ 190, 160, 90, 255 } : (Color){ 150, 124, 60, 255 });
    const char *word = me->stage < 1 ? "No manual" : me->stage > dao_stages() ? content.dao.immortal : "Break through";
    DrawText(word, (int)(go.x + (go.width - (float)MeasureText(word, 18)) / 2), (int)go.y + 7, 18, can ? RAYWHITE : GRAY);
}
