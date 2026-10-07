#include "playerui.h"

#include "assets.h"
#include "hud.h"

#include <string.h>

#define BUTTON_X 12.0f
#define BUTTON_Y 116.0f /* under the readout in the corner */
#define BUTTON_SIZE 52.0f
#define SMALL_BAR_HEIGHT 7.0f
#define SMALL_BAR_GAP 3.0f
#define PANEL_WIDTH 268.0f
#define PANEL_PAD 12.0f
#define ROW_HEIGHT 26.0f
#define ACTION_HEIGHT 34.0f
#define DEATH_WIDTH 240.0f
#define DEATH_HEIGHT 118.0f

static const View *world_view; /* where the world is drawn, as of this frame */
static bool world_showing;

void playerui_set_view(const View *v, bool showing)
{
    world_view = v;
    world_showing = showing;
}

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

/* The menu of a dead character: `where` 0 is beside their button, 1 over them in the world. */
static Rectangle death_rect(const Game *g, int c, int where)
{
    if (where == 0)
        return (Rectangle){ BUTTON_X + BUTTON_SIZE + 10.0f, button_rect(c).y, DEATH_WIDTH, DEATH_HEIGHT };
    TilePos at = game_character_pos(g, c);
    Vector2 p = view_tile_pos(world_view, (float)at.x + 0.5f, (float)at.y);
    return (Rectangle){ p.x - DEATH_WIDTH / 2, p.y - DEATH_HEIGHT - 18.0f, DEATH_WIDTH, DEATH_HEIGHT };
}

static bool death_showing(const PlayerUi *ui, const Game *g, int c, int where)
{
    if (!game_character_dead(g, c)) return false;
    if (where == 0) return ui->death_open == c + 1;
    if (!world_showing || !world_view) return false;
    Rectangle r = death_rect(g, c, 1);
    return r.x + r.width > 0 && r.x < (float)GetScreenWidth() && r.y + r.height > 0 && r.y < (float)GetScreenHeight();
}

/* Its two buttons: 0 comes back now, 1 removes. */
static Rectangle death_button(Rectangle panel, int which)
{
    return which == 0 ? (Rectangle){ panel.x + 10, panel.y + 52, panel.width - 20, 26 }
                      : (Rectangle){ panel.x + 10, panel.y + 84, panel.width - 20, 24 };
}

/* The dead character whose menu a point is on, and which of its two places; -1 for none. */
static int death_at(const PlayerUi *ui, const Game *g, Vector2 m, int *where)
{
    for (int c = 0; c < g->character_count; c++)
        for (int w = 0; w < 2; w++)
            if (death_showing(ui, g, c, w) && CheckCollisionPointRec(m, death_rect(g, c, w))) {
                if (where) *where = w;
                return c;
            }
    return -1;
}

static const Game *watched; /* the game, as of the last frame handled: for asking what the mouse is over */

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
    bool over = false;
    game_sleep_speed(g, &over);
    return g->vitals.rest == REST_SLEEP || !game_bar_full(g->vitals.energy) || (over && g->vitals.energy < 1.0f + content.rules.energy_over);
}

bool playerui_mouse_over(const PlayerUi *ui)
{
    Vector2 m = GetMousePosition();
    if (ui->pressed || button_at(m) >= 0) return true;
    if (watched && death_at(ui, watched, m, NULL) >= 0) return true;
    return ui->open && CheckCollisionPointRec(m, panel_rect());
}

bool playerui_handle_input(PlayerUi *ui, Game *g)
{
    Vector2 m = GetMousePosition();
    watched = g;
    shown_for = g->controlled;
    shown_count = g->character_count;
    ui->switched = false;
    if (ui->death_open > 0 && !game_character_dead(g, ui->death_open - 1)) ui->death_open = 0;
    if (ui->confirm > 0 && !game_character_dead(g, ui->confirm - 1)) ui->confirm = 0;
    if (game_character_dead(g, g->controlled)) ui->open = false;
    bool over = playerui_mouse_over(ui);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ui->pressed = over;
    if (!ui->pressed) return over;
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return true;
    ui->pressed = false;

    int where = 0, dead = death_at(ui, g, m, &where), who = button_at(m);
    if (dead >= 0) {
        /* The menu of somebody dead: bring them back for crystals, or let them go. */
        Rectangle panel = death_rect(g, dead, where);
        if (CheckCollisionPointRec(m, death_button(panel, 0))) {
            game_respawn(g, dead);
            ui->confirm = 0;
        } else if (CheckCollisionPointRec(m, death_button(panel, 1))) {
            /* Removing is for good: it is asked for twice. */
            if (ui->confirm == dead + 1) {
                game_remove_character(g, dead);
                ui->confirm = ui->death_open = 0;
                ui->switched = true;
            } else {
                ui->confirm = dead + 1;
            }
        } else {
            ui->confirm = 0;
        }
    } else if (ui->open && CheckCollisionPointRec(m, action_rect(0))) {
        /* The button for what the player is already doing stops it. */
        if (g->vitals.rest == REST_SLEEP) game_wake(g);
        else if (can_sleep(g)) game_rest(g, REST_SLEEP);
    } else if (ui->open && CheckCollisionPointRec(m, action_rect(1))) {
        if (g->vitals.rest == REST_MEDITATE) game_wake(g);
        else game_rest(g, REST_MEDITATE);
    } else if (who >= 0 && game_character_dead(g, who)) {
        /* A dead character's button brings their menu out beside it. */
        ui->death_open = ui->death_open == who + 1 ? 0 : who + 1;
        ui->confirm = 0;
        ui->open = false;
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
                 : color == BAR_RED ? (Color){ 232, 52, 52, 255 } : color == BAR_POISON ? (Color){ 150, 96, 200, 255 }
                 : (Color){ 76, 190, 84, 255 };
    Color shine = color == BAR_GOLD ? (Color){ 255, 232, 130, 255 } : color == BAR_BLUE ? (Color){ 160, 214, 255, 255 }
                  : color == BAR_RED ? (Color){ 255, 130, 120, 255 } : color == BAR_POISON ? (Color){ 204, 160, 240, 255 }
                  : (Color){ 140, 226, 140, 255 };
    /* (The death bar is red on black: there is nothing under it.) */
    if (color == BAR_RED) DrawRectangleRec(r, (Color){ 16, 6, 8, 255 });
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

/* The menu of a dead character: how long until they are back, and the two things that can be done about it. */
static void draw_death(const PlayerUi *ui, const Game *g, int c, Rectangle p, bool pointer)
{
    const Vitals *v = game_character_vitals(g, c);
    int total = content.rules.respawn_ticks > 0 ? content.rules.respawn_ticks : 1, cost = game_respawn_cost(g, c);
    float done = 1.0f - (float)v->respawn / (float)total;
    DrawRectangleRec(p, (Color){ 34, 20, 24, 244 });
    DrawRectangleLinesEx(p, 2.0f, (Color){ 200, 70, 70, 255 });
    if (pointer)
        DrawTriangle((Vector2){ p.x + p.width / 2 - 8, p.y + p.height }, (Vector2){ p.x + p.width / 2, p.y + p.height + 10 },
                     (Vector2){ p.x + p.width / 2 + 8, p.y + p.height }, (Color){ 200, 70, 70, 255 });
    DrawText(TextFormat("%s has died", game_character_name(g, c)), (int)p.x + 10, (int)p.y + 8, 16, RAYWHITE);
    Rectangle bar = { p.x + 10, p.y + 30, p.width - 20, 8 };
    DrawRectangleRec(bar, (Color){ 16, 6, 8, 255 });
    DrawRectangleRec((Rectangle){ bar.x, bar.y, bar.width * done, bar.height }, (Color){ 150, 210, 255, 255 });
    const char *left = TextFormat("back in %d ticks", v->respawn);
    DrawText(left, (int)(p.x + p.width - 10) - MeasureText(left, 10), (int)p.y + 40, 10, LIGHTGRAY);

    Vector2 m = GetMousePosition();
    Rectangle now = death_button(p, 0), gone = death_button(p, 1);
    bool can = g->crystals >= cost, hot = can && CheckCollisionPointRec(m, now);
    DrawRectangleRec(now, !can ? (Color){ 44, 47, 60, 255 } : hot ? (Color){ 96, 120, 200, 255 } : (Color){ 66, 88, 160, 255 });
    DrawText("Respawn now", (int)now.x + 8, (int)now.y + 6, 14, can ? RAYWHITE : GRAY);
    const char *n = TextFormat("%d", cost);
    assets_draw_image(content.trade.crystal_icon, (Rectangle){ now.x + now.width - 26, now.y + 2, 22, 22 });
    DrawText(n, (int)(now.x + now.width - 30) - MeasureText(n, 14), (int)now.y + 6, 14,
             can ? RAYWHITE : (Color){ 235, 90, 90, 255 });
    bool sure = ui->confirm == c + 1, warm = CheckCollisionPointRec(m, gone);
    DrawRectangleRec(gone, sure ? (Color){ 190, 50, 50, 255 } : warm ? (Color){ 110, 44, 48, 255 } : (Color){ 74, 34, 38, 255 });
    const char *label = sure ? "Really remove? Their things stay here" : "Remove character";
    DrawText(label, (int)(gone.x + (gone.width - MeasureText(label, sure ? 10 : 14)) / 2), (int)gone.y + (sure ? 7 : 5),
             sure ? 10 : 14, RAYWHITE);
}

void playerui_draw(const PlayerUi *ui, const Game *g)
{
    shown_for = g->controlled;
    shown_count = g->character_count;
    watched = g;
    /* Over each of the dead, where they lie. */
    for (int c = 0; c < g->character_count; c++)
        if (death_showing(ui, g, c, 1)) draw_death(ui, g, c, death_rect(g, c, 1), true);
    for (int c = 0; c < g->character_count; c++) {
        const Vitals *v = game_character_vitals(g, c);
        bool played = c == g->controlled, dead = v->respawn > 0;
        Rectangle b = button_rect(c);
        bool hover = !played && CheckCollisionPointRec(GetMousePosition(), b);
        DrawRectangleRec(b, hover ? (Color){ 52, 56, 70, 235 } : (Color){ 34, 36, 44, 225 });
        /* The one being played is picked out; with company, in gold. */
        Color edge = dead ? (Color){ 200, 70, 70, 255 } : !played ? (Color){ 120, 122, 134, 255 }
                     : g->character_count > 1 ? (Color){ 244, 196, 48, 255 } : RAYWHITE;
        DrawRectangleLinesEx(b, dead || (played && (ui->open || g->character_count > 1)) ? 3.0f : 2.0f,
                             !dead && played && !ui->open && g->character_count == 1 ? (Color){ 120, 122, 134, 255 } : edge);
        hud_draw_head_icon(b);
        if (!played && !dead) DrawRectangleRec(b, (Color){ 20, 22, 30, 70 });
        DrawText(game_character_name(g, c), (int)(b.x + b.width) + 6, (int)b.y + 2, 10, dead ? (Color){ 230, 120, 120, 255 } : RAYWHITE);
        if (dead) {
            /* Dead: crossed out, with how far along the wait is where the bars were. */
            int total = content.rules.respawn_ticks > 0 ? content.rules.respawn_ticks : 1;
            DrawRectangleRec(b, (Color){ 60, 10, 14, 150 });
            DrawLineEx((Vector2){ b.x + 10, b.y + 10 }, (Vector2){ b.x + b.width - 10, b.y + b.height - 10 }, 4.0f, (Color){ 230, 70, 70, 255 });
            DrawLineEx((Vector2){ b.x + b.width - 10, b.y + 10 }, (Vector2){ b.x + 10, b.y + b.height - 10 }, 4.0f, (Color){ 230, 70, 70, 255 });
            playerui_draw_bar(small_bar(c, 0), 1.0f - (float)v->respawn / (float)total, BAR_BLUE);
            continue;
        }
        /* Someone at rest says so on their button. */
        if (v->rest != REST_NONE)
            DrawText(v->rest == REST_SLEEP ? "zz" : "om", (int)(b.x + b.width) - 16, (int)b.y + 3, 10, (Color){ 170, 220, 255, 255 });
        /* With no health left, what shows in its place is the death bar. */
        if (v->health <= 0.0f || v->death < 1.0f) playerui_draw_bar(small_bar(c, 0), v->death, BAR_RED);
        else playerui_draw_bar(small_bar(c, 0), v->health, v->poison > 0 ? BAR_POISON : BAR_GREEN); /* (poisoned, it turns sickly) */
        playerui_draw_bar(small_bar(c, 1), v->hunger, v->saturation > 0.0f ? BAR_GOLD : BAR_GREEN);
        /* Breath only shows while it is short. */
        if (v->oxygen < 1.0f) playerui_draw_bar(small_bar(c, 2), v->oxygen, BAR_BLUE);
    }
    for (int c = 0; c < g->character_count; c++)
        if (death_showing(ui, g, c, 0)) draw_death(ui, g, c, death_rect(g, c, 0), false);
    if (!ui->open || !game_alive(g)) return;

    const Vitals *v = &g->vitals;
    Rectangle p = panel_rect();
    DrawRectangleRec(p, (Color){ 30, 32, 42, 240 });
    DrawRectangleLinesEx(p, 2.0f, (Color){ 120, 122, 134, 255 });
    /* The panel points back at the button it came out of. */
    DrawTriangle((Vector2){ p.x, p.y + 16 }, (Vector2){ p.x - 8, p.y + 26 }, (Vector2){ p.x, p.y + 36 },
                 (Color){ 120, 122, 134, 255 });
    /* Health; or, once that is gone, what is left before dying. */
    if (v->health <= 0.0f || v->death < 1.0f) draw_row(p, 0, "Dying", v->death, BAR_RED);
    else draw_row(p, 0, v->poison > 0 ? "Poisoned" : "Health", v->health, v->poison > 0 ? BAR_POISON : BAR_GREEN);
    draw_row(p, 1, "Hunger", v->hunger, game_saturated(g) ? BAR_GOLD : BAR_GREEN);
    /* (Past full - from a soft bed, or coffee - it shows gold until it has run back down to full.) */
    draw_row(p, 2, "Energy", v->energy > 1.0f ? 1.0f : v->energy, v->energy > 1.0f ? BAR_GOLD : BAR_GREEN);
    draw_row(p, 3, "Oxygen", v->oxygen, BAR_BLUE);
    draw_action(action_rect(0), v->rest == REST_SLEEP ? "Wake up" : "Sleep", can_sleep(g), v->rest == REST_SLEEP);
    draw_action(action_rect(1), v->rest == REST_MEDITATE ? "Stop" : "Meditate", true, v->rest == REST_MEDITATE);
}
