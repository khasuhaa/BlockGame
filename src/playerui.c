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

#define CHARACTER_PITCH (BUTTON_SIZE + 3 * (SMALL_BAR_HEIGHT + SMALL_BAR_GAP) + 10.0f)

/* The button of character `c`: they stand one below another. */
static Rectangle button_rect(int c)
{
    return (Rectangle){ BUTTON_X, BUTTON_Y + (float)c * CHARACTER_PITCH, BUTTON_SIZE, BUTTON_SIZE };
}

static Rectangle small_bar(int c, int row)
{
    Rectangle b = button_rect(c);
    return (Rectangle){ b.x, b.y + BUTTON_SIZE + SMALL_BAR_GAP + row * (SMALL_BAR_HEIGHT + SMALL_BAR_GAP), BUTTON_SIZE,
                        SMALL_BAR_HEIGHT };
}

/* The panel comes out beside the button of the character being played. */
static Rectangle panel_rect_for(int c)
{
    float h = PANEL_PAD * 2 + ROW_HEIGHT * 4 + 8.0f + ACTION_HEIGHT;
    return (Rectangle){ BUTTON_X + BUTTON_SIZE + 10.0f, button_rect(c).y, PANEL_WIDTH, h };
}

static int shown_for; /* whose panel it is: the character being played, as of the last frame handled */

static Rectangle panel_rect(void)
{
    return panel_rect_for(shown_for);
}

static int shown_count = 1;

/* The two buttons along the bottom of the panel: 0 sleeps, 1 meditates. */
static Rectangle action_rect(int which)
{
    Rectangle p = panel_rect();
    float w = (p.width - PANEL_PAD * 2 - 8.0f) / 2.0f;
    return (Rectangle){ p.x + PANEL_PAD + which * (w + 8.0f), p.y + p.height - PANEL_PAD - ACTION_HEIGHT, w,
                        ACTION_HEIGHT };
}

/* The character whose button, or the bars under it, a point is on; -1 for none. */
static int button_at(Vector2 m)
{
    for (int c = 0; c < shown_count; c++) {
        Rectangle whole = button_rect(c);
        whole.height += 3 * (SMALL_BAR_HEIGHT + SMALL_BAR_GAP);
        if (CheckCollisionPointRec(m, whole)) return c;
    }
    return -1;
}

static bool can_sleep(const Game *g)
{
    return g->vitals.rest == REST_SLEEP || !game_bar_full(g->vitals.energy);
}

bool playerui_mouse_over(const PlayerUi *ui)
{
    Vector2 m = GetMousePosition();
    return ui->pressed || button_at(m) >= 0 || (ui->open && CheckCollisionPointRec(m, panel_rect()));
}

bool playerui_handle_input(PlayerUi *ui, Game *g)
{
    Vector2 m = GetMousePosition();
    shown_for = g->controlled;
    shown_count = g->character_count;
    ui->switched = false;
    bool over = playerui_mouse_over(ui);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ui->pressed = over;
    if (!ui->pressed) return over;
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return true;
    ui->pressed = false;

    int who = button_at(m);
    if (ui->open && CheckCollisionPointRec(m, action_rect(0))) {
        /* The button for what the player is already doing stops it. */
        if (g->vitals.rest == REST_SLEEP) game_wake(g);
        else if (can_sleep(g)) game_rest(g, REST_SLEEP);
    } else if (ui->open && CheckCollisionPointRec(m, action_rect(1))) {
        if (g->vitals.rest == REST_MEDITATE) game_wake(g);
        else game_rest(g, REST_MEDITATE);
    } else if (who >= 0 && who != g->controlled) {
        /* Somebody else's button: they are the one played from now on. */
        game_control(g, who);
        shown_for = who;
        ui->open = false;
        ui->switched = true;
    } else if (who == g->controlled) {
        ui->open = !ui->open;
    }
    return true;
}

void playerui_draw_bar(Rectangle r, float value, BarColor color)
{
    value = value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
    DrawRectangleRec(r, (Color){ 96, 22, 28, 255 });
    Color fill = color == BAR_GOLD ? (Color){ 244, 196, 48, 255 } : color == BAR_BLUE ? (Color){ 70, 160, 236, 255 }
                                                                                      : (Color){ 76, 190, 84, 255 };
    Color shine = color == BAR_GOLD ? (Color){ 255, 232, 130, 255 } : color == BAR_BLUE ? (Color){ 160, 214, 255, 255 }
                                                                                        : (Color){ 140, 226, 140, 255 };
    Rectangle full = { r.x, r.y, r.width * value, r.height };
    DrawRectangleRec(full, fill);
    if (full.width >= 1.0f && r.height >= 6.0f)
        DrawRectangleRec((Rectangle){ full.x, full.y, full.width, r.height >= 12.0f ? 3.0f : 2.0f }, shine);
    DrawRectangleLinesEx((Rectangle){ r.x - 1, r.y - 1, r.width + 2, r.height + 2 }, 1.0f, (Color){ 14, 15, 20, 255 });
}

static void draw_row(Rectangle panel, int row, const char *label, float value, BarColor color)
{
    bool golden = color == BAR_GOLD;
    float y = panel.y + PANEL_PAD + row * ROW_HEIGHT;
    DrawText(label, (int)(panel.x + PANEL_PAD), (int)y + 1, 16, RAYWHITE);
    Rectangle bar = { panel.x + PANEL_PAD + 70.0f, y + 2.0f, panel.width - PANEL_PAD * 2 - 70.0f - 44.0f, 14.0f };
    playerui_draw_bar(bar, value, color);
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
    shown_for = g->controlled;
    shown_count = g->character_count;
    for (int c = 0; c < g->character_count; c++) {
        const Vitals *v = game_character_vitals(g, c);
        bool played = c == g->controlled;
        Rectangle b = button_rect(c);
        bool hover = !played && CheckCollisionPointRec(GetMousePosition(), b);
        DrawRectangleRec(b, hover ? (Color){ 52, 56, 70, 235 } : (Color){ 34, 36, 44, 225 });
        /* The one being played is picked out; with company, in gold. */
        Color edge = !played ? (Color){ 120, 122, 134, 255 } : g->character_count > 1 ? (Color){ 244, 196, 48, 255 } : RAYWHITE;
        DrawRectangleLinesEx(b, played && (ui->open || g->character_count > 1) ? 3.0f : 2.0f,
                             played && !ui->open && g->character_count == 1 ? (Color){ 120, 122, 134, 255 } : edge);
        hud_draw_head_icon(b);
        if (!played) DrawRectangleRec(b, (Color){ 20, 22, 30, 70 });
        if (g->character_count > 1) DrawText(TextFormat("%d", c + 1), (int)b.x + 4, (int)b.y + 3, 10, played ? edge : LIGHTGRAY);
        /* Someone at rest says so on their button. */
        if (v->rest != REST_NONE)
            DrawText(v->rest == REST_SLEEP ? "zz" : "om", (int)(b.x + b.width) - 16, (int)b.y + 3, 10, (Color){ 170, 220, 255, 255 });
        playerui_draw_bar(small_bar(c, 0), v->health, BAR_GREEN);
        playerui_draw_bar(small_bar(c, 1), v->hunger, v->saturation > 0.0f ? BAR_GOLD : BAR_GREEN);
        /* Breath only shows while it is short. */
        if (v->oxygen < 1.0f) playerui_draw_bar(small_bar(c, 2), v->oxygen, BAR_BLUE);
    }
    if (!ui->open) return;

    const Vitals *v = &g->vitals;
    Rectangle p = panel_rect();
    DrawRectangleRec(p, (Color){ 30, 32, 42, 240 });
    DrawRectangleLinesEx(p, 2.0f, (Color){ 120, 122, 134, 255 });
    /* The panel points back at the button it came out of. */
    DrawTriangle((Vector2){ p.x, p.y + 16 }, (Vector2){ p.x - 8, p.y + 26 }, (Vector2){ p.x, p.y + 36 },
                 (Color){ 120, 122, 134, 255 });
    draw_row(p, 0, "Health", v->health, BAR_GREEN);
    draw_row(p, 1, "Hunger", v->hunger, game_saturated(g) ? BAR_GOLD : BAR_GREEN);
    draw_row(p, 2, "Energy", v->energy, BAR_GREEN);
    draw_row(p, 3, "Oxygen", v->oxygen, BAR_BLUE);
    draw_action(action_rect(0), v->rest == REST_SLEEP ? "Wake up" : "Sleep", can_sleep(g), v->rest == REST_SLEEP);
    draw_action(action_rect(1), v->rest == REST_MEDITATE ? "Stop" : "Meditate", true, v->rest == REST_MEDITATE);
}
