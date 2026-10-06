#include "playerui.h"

#include "hud.h"

#define BUTTON_X 12.0f
#define BUTTON_Y 116.0f /* under the readout in the corner */
#define BUTTON_SIZE 52.0f
#define SMALL_BAR_HEIGHT 7.0f
#define SMALL_BAR_GAP 3.0f
#define PANEL_WIDTH 268.0f
#define PANEL_PAD 12.0f
#define ROW_HEIGHT 26.0f
#define ACTION_HEIGHT 34.0f

static Rectangle button_rect(void)
{
    return (Rectangle){ BUTTON_X, BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE };
}

static Rectangle small_bar(int row)
{
    return (Rectangle){ BUTTON_X, BUTTON_Y + BUTTON_SIZE + SMALL_BAR_GAP + row * (SMALL_BAR_HEIGHT + SMALL_BAR_GAP),
                        BUTTON_SIZE, SMALL_BAR_HEIGHT };
}

static Rectangle panel_rect(void)
{
    float h = PANEL_PAD * 2 + ROW_HEIGHT * 3 + 8.0f + ACTION_HEIGHT;
    return (Rectangle){ BUTTON_X + BUTTON_SIZE + 10.0f, BUTTON_Y, PANEL_WIDTH, h };
}

/* The two buttons along the bottom of the panel: 0 sleeps, 1 meditates. */
static Rectangle action_rect(int which)
{
    Rectangle p = panel_rect();
    float w = (p.width - PANEL_PAD * 2 - 8.0f) / 2.0f;
    return (Rectangle){ p.x + PANEL_PAD + which * (w + 8.0f), p.y + p.height - PANEL_PAD - ACTION_HEIGHT, w,
                        ACTION_HEIGHT };
}

static bool can_sleep(const Game *g)
{
    return g->vitals.rest == REST_SLEEP || !game_bar_full(g->vitals.energy);
}

bool playerui_mouse_over(const PlayerUi *ui)
{
    Vector2 m = GetMousePosition();
    Rectangle whole = button_rect();
    whole.height += 2 * (SMALL_BAR_HEIGHT + SMALL_BAR_GAP);
    return ui->pressed || CheckCollisionPointRec(m, whole) || (ui->open && CheckCollisionPointRec(m, panel_rect()));
}

bool playerui_handle_input(PlayerUi *ui, Game *g)
{
    Vector2 m = GetMousePosition();
    bool over = playerui_mouse_over(ui);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ui->pressed = over;
    if (!ui->pressed) return over;
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return true;
    ui->pressed = false;

    if (ui->open && CheckCollisionPointRec(m, action_rect(0))) {
        /* The button for what the player is already doing stops it. */
        if (g->vitals.rest == REST_SLEEP) game_wake(g);
        else if (can_sleep(g)) game_rest(g, REST_SLEEP);
    } else if (ui->open && CheckCollisionPointRec(m, action_rect(1))) {
        if (g->vitals.rest == REST_MEDITATE) game_wake(g);
        else game_rest(g, REST_MEDITATE);
    } else if (!ui->open || !CheckCollisionPointRec(m, panel_rect())) {
        ui->open = !ui->open;
    }
    return true;
}

void playerui_draw_bar(Rectangle r, float value, bool golden)
{
    value = value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
    DrawRectangleRec(r, (Color){ 96, 22, 28, 255 });
    Color fill = golden ? (Color){ 244, 196, 48, 255 } : (Color){ 76, 190, 84, 255 };
    Color shine = golden ? (Color){ 255, 232, 130, 255 } : (Color){ 140, 226, 140, 255 };
    Rectangle full = { r.x, r.y, r.width * value, r.height };
    DrawRectangleRec(full, fill);
    if (full.width >= 1.0f && r.height >= 6.0f)
        DrawRectangleRec((Rectangle){ full.x, full.y, full.width, r.height >= 12.0f ? 3.0f : 2.0f }, shine);
    DrawRectangleLinesEx((Rectangle){ r.x - 1, r.y - 1, r.width + 2, r.height + 2 }, 1.0f, (Color){ 14, 15, 20, 255 });
}

static void draw_row(Rectangle panel, int row, const char *label, float value, bool golden)
{
    float y = panel.y + PANEL_PAD + row * ROW_HEIGHT;
    DrawText(label, (int)(panel.x + PANEL_PAD), (int)y + 1, 16, RAYWHITE);
    Rectangle bar = { panel.x + PANEL_PAD + 70.0f, y + 2.0f, panel.width - PANEL_PAD * 2 - 70.0f - 44.0f, 14.0f };
    playerui_draw_bar(bar, value, golden);
    const char *text = golden ? "full" : TextFormat("%d%%", (int)(value * 100.0f + 0.5f));
    DrawText(text, (int)(panel.x + panel.width - PANEL_PAD) - MeasureText(text, 14), (int)y + 2, 14,
             golden ? (Color){ 244, 196, 48, 255 } : LIGHTGRAY);
}

static void draw_action(Rectangle r, const char *text, bool enabled, bool active)
{
    bool hover = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    Color back = active ? (Color){ 70, 96, 150, 255 } : hover ? (Color){ 70, 74, 90, 255 } : (Color){ 50, 53, 66, 255 };
    DrawRectangleRec(r, enabled ? back : (Color){ 38, 40, 48, 255 });
    DrawRectangleLinesEx(r, 1.0f, enabled ? (Color){ 150, 154, 170, 255 } : (Color){ 70, 72, 82, 255 });
    int size = 16;
    DrawText(text, (int)(r.x + (r.width - MeasureText(text, size)) / 2), (int)(r.y + (r.height - size) / 2), size,
             enabled ? RAYWHITE : (Color){ 110, 112, 122, 255 });
}

void playerui_draw(const PlayerUi *ui, const Game *g)
{
    const Vitals *v = &g->vitals;
    Rectangle b = button_rect();
    DrawRectangleRec(b, (Color){ 34, 36, 44, 225 });
    DrawRectangleLinesEx(b, ui->open ? 3.0f : 2.0f, ui->open ? RAYWHITE : (Color){ 120, 122, 134, 255 });
    hud_draw_head_icon(b);
    playerui_draw_bar(small_bar(0), v->health, false);
    playerui_draw_bar(small_bar(1), v->hunger, game_saturated(g));
    if (!ui->open) return;

    Rectangle p = panel_rect();
    DrawRectangleRec(p, (Color){ 30, 32, 42, 240 });
    DrawRectangleLinesEx(p, 2.0f, (Color){ 120, 122, 134, 255 });
    /* The panel points back at the button it came out of. */
    DrawTriangle((Vector2){ p.x, p.y + 16 }, (Vector2){ p.x - 8, p.y + 26 }, (Vector2){ p.x, p.y + 36 },
                 (Color){ 120, 122, 134, 255 });
    draw_row(p, 0, "Health", v->health, false);
    draw_row(p, 1, "Hunger", v->hunger, game_saturated(g));
    draw_row(p, 2, "Energy", v->energy, false);
    draw_action(action_rect(0), v->rest == REST_SLEEP ? "Wake up" : "Sleep", can_sleep(g), v->rest == REST_SLEEP);
    draw_action(action_rect(1), v->rest == REST_MEDITATE ? "Stop" : "Meditate", true, v->rest == REST_MEDITATE);
}
