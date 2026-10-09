#include "questui.h"

#include "hud.h"
#include "trade.h"

#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define PANEL_W 1180.0f
#define PANEL_H 580.0f
#define TABS_W 196.0f
#define DETAIL_W 300.0f
#define TAB_ROW 26.0f
#define PAD 10.0f
#define CELL 48.0f   /* a square of the page */
#define NODE 34.0f   /* a quest on it */
#define HUB 42.0f    /* one that others hang from */

static const Color panel_color = { 28, 30, 38, 246 }, page_color = { 20, 22, 28, 255 }, edge_color = { 120, 122, 134, 255 }, row_color = { 44, 47, 58, 255 }, row_hot = { 62, 66, 80, 255 }, row_on = { 74, 80, 104, 255 };
static const Color gold = { 236, 200, 96, 255 }, green = { 110, 190, 110, 255 }, dim = { 96, 100, 114, 255 }, pale = { 200, 204, 216, 255 };

static Rectangle button_rect(void)
{
    return (Rectangle){ (float)GetScreenWidth() - 106.0f, 58.0f, 92.0f, 28.0f };
}

static Rectangle panel_rect(void)
{
    float w = PANEL_W < (float)GetScreenWidth() - 40.0f ? PANEL_W : (float)GetScreenWidth() - 40.0f, h = PANEL_H < (float)GetScreenHeight() - 190.0f ? PANEL_H : (float)GetScreenHeight() - 190.0f;
    return (Rectangle){ ((float)GetScreenWidth() - w) / 2.0f, 96.0f, w, h };
}

/* The tabs that have anything in them, in their order: how many, and which. */
static int shown_tabs(int *out)
{
    int n = 0;
    for (int t = 0; t < content.quest_tab_count; t++) {
        bool any = false;
        for (int i = 0; i < content.quest_count && !any; i++) any = content.quests[i].tab == t;
        if (any) out[n++] = t;
    }
    for (int i = 1; i < n; i++)
        for (int k = i; k > 0 && content.quest_tabs[out[k]].order < content.quest_tabs[out[k - 1]].order; k--) {
            int t = out[k];
            out[k] = out[k - 1];
            out[k - 1] = t;
        }
    return n;
}

static int open_tab(const QuestUi *ui)
{
    int tabs[MAX_QUEST_TABS], n = shown_tabs(tabs);
    for (int i = 0; i < n; i++)
        if (tabs[i] == ui->tab) return ui->tab;
    return n > 0 ? tabs[0] : -1;
}

static Rectangle tab_rect(Rectangle p, int i)
{
    return (Rectangle){ p.x + PAD, p.y + PAD + (float)i * TAB_ROW, TABS_W - PAD, TAB_ROW - 2.0f };
}

static Rectangle page_rect(Rectangle p)
{
    float x = p.x + TABS_W + PAD;
    return (Rectangle){ x, p.y + PAD, p.x + p.width - DETAIL_W - 2 * PAD - x, p.height - 2 * PAD };
}

static Rectangle claim_all_rect(Rectangle p)
{
    return (Rectangle){ p.x + PAD, p.y + p.height - PAD - 28.0f, TABS_W - PAD, 28.0f };
}

static Rectangle detail_rect(Rectangle p)
{
    return (Rectangle){ p.x + p.width - DETAIL_W - PAD, p.y + PAD, DETAIL_W, p.height - 2 * PAD };
}

static Rectangle claim_rect(Rectangle p)
{
    Rectangle d = detail_rect(p);
    return (Rectangle){ d.x, d.y + d.height - 32.0f, 130.0f, 30.0f };
}

/* Where a quest stands on the screen: the square it fills. */
static Rectangle node_rect(const QuestUi *ui, Rectangle page, int quest)
{
    const QuestDef *q = &content.quests[quest];
    float size = q->hub ? HUB : NODE;
    float cx = page.x + 76.0f + (float)q->x * CELL - ui->pan_x[q->tab], cy = page.y + 44.0f + (float)q->y * CELL - ui->pan_y[q->tab];
    return (Rectangle){ cx - size / 2.0f, cy - size / 2.0f, size, size };
}

/* The quest of a tab the mouse is on, or -1. */
static int node_at(const QuestUi *ui, Rectangle page, int tab, Vector2 m)
{
    if (!CheckCollisionPointRec(m, page)) return -1;
    for (int i = 0; i < content.quest_count; i++)
        if (content.quests[i].tab == tab && CheckCollisionPointRec(m, node_rect(ui, page, i))) return i;
    return -1;
}

/* How far a tab's page may be moved: no further than its last quests. */
static void keep_on_page(QuestUi *ui, Rectangle page, int tab)
{
    int far_x = 0, far_y = 0;
    for (int i = 0; i < content.quest_count; i++) {
        if (content.quests[i].tab != tab) continue;
        if (content.quests[i].x > far_x) far_x = content.quests[i].x;
        if (content.quests[i].y > far_y) far_y = content.quests[i].y;
    }
    float max_x = (float)far_x * CELL + 130.0f - page.width, max_y = (float)far_y * CELL + 90.0f - page.height;
    if (ui->pan_x[tab] > max_x) ui->pan_x[tab] = max_x;
    if (ui->pan_y[tab] > max_y) ui->pan_y[tab] = max_y;
    if (ui->pan_x[tab] < 0.0f) ui->pan_x[tab] = 0.0f;
    if (ui->pan_y[tab] < 0.0f) ui->pan_y[tab] = 0.0f;
}

/* The rewards of the chosen quest, a row to each. */
static Rectangle reward_rect(Rectangle d, float top, int k)
{
    return (Rectangle){ d.x, top + (float)k * 36.0f, d.width, 34.0f };
}

static float draw_wrapped(const char *text, float x, float y, float wide, int size, Color c, bool draw);
static void task_text(const Game *g, int quest, char *out, size_t size);

/* Where the rewards of the chosen quest begin: below its name, its text, its task and what it comes after. */
static float rewards_top(const Game *g, Rectangle d, int quest)
{
    const QuestDef *q = &content.quests[quest];
    (void)g;
    float y = draw_wrapped(q->label, d.x, d.y, d.width, 20, WHITE, false) + 6.0f;
    y = draw_wrapped(q->text, d.x, y, d.width, 14, WHITE, false) + 8.0f;
    y += 28.0f + (float)q->need_count * 18.0f + 10.0f;
    return y;
}

bool questui_mouse_over(const QuestUi *ui)
{
    if (!game_quests_on()) return false;
    Vector2 m = GetMousePosition();
    return ui->pressed || CheckCollisionPointRec(m, button_rect()) || (ui->open && CheckCollisionPointRec(m, panel_rect()));
}

bool questui_handle_input(QuestUi *ui, Game *g)
{
    if (!game_quests_on()) {
        ui->open = ui->pressed = ui->dragging = false;
        return false;
    }
    Vector2 m = GetMousePosition();
    Rectangle p = panel_rect(), page = page_rect(p);
    int tab = open_tab(ui);
    ui->tab = tab;
    /* The wheel moves the page (by as much as it is turned, however little): up and down, and sideways where it can. */
    Vector2 wheel = GetMouseWheelMoveV();
    if (ui->open && tab >= 0 && (wheel.x != 0.0f || wheel.y != 0.0f) && CheckCollisionPointRec(m, page)) {
        ui->pan_y[tab] -= wheel.y * CELL;
        ui->pan_x[tab] -= wheel.x * CELL;
        keep_on_page(ui, page, tab);
    }
    bool on = CheckCollisionPointRec(m, button_rect()) || (ui->open && CheckCollisionPointRec(m, p));
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ui->pressed = on;
        ui->dragging = ui->open && tab >= 0 && CheckCollisionPointRec(m, page);
        ui->moved = false;
        ui->went = 0.0f;
        ui->last_x = m.x, ui->last_y = m.y;
    }
    if (!ui->pressed) return ui->open && CheckCollisionPointRec(m, p);
    /* A press on the page, held and moved, takes the page with it. */
    if (ui->dragging && tab >= 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        ui->went += fabsf(m.x - ui->last_x) + fabsf(m.y - ui->last_y);
        if (ui->went > 5.0f) ui->moved = true;
        if (ui->moved) {
            ui->pan_x[tab] -= m.x - ui->last_x;
            ui->pan_y[tab] -= m.y - ui->last_y;
            keep_on_page(ui, page, tab);
        }
        ui->last_x = m.x, ui->last_y = m.y;
    }
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return true;
    bool was_drag = ui->dragging && ui->moved;
    ui->pressed = ui->dragging = false;
    if (was_drag) return true;
    if (CheckCollisionPointRec(m, button_rect())) {
        ui->open = !ui->open;
        return true;
    }
    if (!ui->open) return true;
    int tabs[MAX_QUEST_TABS], tab_count = shown_tabs(tabs);
    for (int i = 0; i < tab_count; i++)
        if (CheckCollisionPointRec(m, tab_rect(p, i))) ui->tab = tabs[i];
    if (tab < 0) return true;
    int hit = node_at(ui, page, tab, m);
    if (hit >= 0) {
        ui->chosen[tab] = hit + 1;
        ui->choice = 0;
    }
    if (CheckCollisionPointRec(m, claim_all_rect(p))) game_quest_claim_all(g, tab);
    int chosen = ui->chosen[tab] - 1;
    if (chosen < 0 || chosen >= content.quest_count || content.quests[chosen].tab != tab) return true;
    const QuestDef *q = &content.quests[chosen];
    Rectangle d = detail_rect(p);
    float top = rewards_top(g, d, chosen);
    for (int k = 0; q->pick && k < q->reward_count; k++)
        if (CheckCollisionPointRec(m, reward_rect(d, top, k))) ui->choice = k + 1;
    if (CheckCollisionPointRec(m, claim_rect(p))) {
        if (q->kind == QUEST_CHECK && game_quest_state(g, chosen) == QUEST_OPEN) game_quest_check(g, chosen);
        else if (game_quest_claim(g, chosen, ui->choice - 1)) ui->choice = 0;
    }
    return true;
}

static void draw_button(Rectangle r, const char *text, bool enabled)
{
    bool hot = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, !enabled ? (Color){ 40, 42, 50, 255 } : hot ? row_hot : (Color){ 54, 57, 70, 255 });
    DrawText(text, (int)(r.x + (r.width - (float)MeasureText(text, 14)) / 2.0f), (int)(r.y + (r.height - 14.0f) / 2.0f), 14, enabled ? RAYWHITE : GRAY);
}

/* Text broken into lines no wider than `wide`: where the last one ended. (Measured only, if it is not to be drawn.) */
static float draw_wrapped(const char *text, float x, float y, float wide, int size, Color c, bool draw)
{
    char line[200] = "";
    const char *at = text;
    while (*at) {
        const char *end = at;
        while (*end && *end != ' ') end++;
        char next[200];
        snprintf(next, sizeof next, "%s%s%.*s", line, line[0] ? " " : "", (int)(end - at), at);
        if (line[0] && (float)MeasureText(next, size) > wide) {
            if (draw) DrawText(line, (int)x, (int)y, size, c);
            y += (float)size + 4.0f;
            snprintf(line, sizeof line, "%.*s", (int)(end - at), at);
        } else {
            snprintf(line, sizeof line, "%s", next);
        }
        at = *end ? end + 1 : end;
    }
    if (line[0]) {
        if (draw) DrawText(line, (int)x, (int)y, size, c);
        y += (float)size + 4.0f;
    }
    return y;
}

/* What a quest asks, in a line. */
static void task_text(const Game *g, int quest, char *out, size_t size)
{
    const QuestDef *q = &content.quests[quest];
    int of = 1, got = game_quest_progress(g, quest, &of);
    switch (q->kind) {
    case QUEST_HAVE: snprintf(out, size, "Have %s  %d / %d", item_def((ItemId)q->target)->label, got, of); break;
    case QUEST_KILL: snprintf(out, size, "Defeat %s  %d / %d", content.mobs[q->target].label, got, of); break;
    case QUEST_NEAR: snprintf(out, size, "Stand by %s", tile_def((TileType)q->target)->label); break;
    case QUEST_IN: snprintf(out, size, "Reach %s", content.dimensions[q->target].label); break;
    case QUEST_BIOME: snprintf(out, size, "Find %s", content.biomes[q->target].label); break;
    case QUEST_STAGE: snprintf(out, size, "Reach stage %d", q->target); break;
    default: snprintf(out, size, "Done when you say"); break;
    }
}

static Color state_color(int state)
{
    return state == QUEST_LOCKED ? dim : state == QUEST_DONE ? gold : state == QUEST_CLAIMED ? green : RAYWHITE;
}

void questui_draw(const QuestUi *ui, const Game *g)
{
    if (!game_quests_on() || g->character_count == 0) return;
    Vector2 m = GetMousePosition();
    Rectangle b = button_rect();
    int waiting = 0;
    for (int i = 0; i < content.quest_count; i++) waiting += g->quest_state[i] == QUEST_DONE;
    DrawRectangleRec(b, CheckCollisionPointRec(m, b) || ui->open ? row_hot : panel_color);
    DrawRectangleLinesEx(b, 2.0f, waiting > 0 ? gold : edge_color);
    DrawText("Quests", (int)(b.x + (b.width - (float)MeasureText("Quests", 16)) / 2.0f), (int)b.y + 6, 16, waiting > 0 ? gold : RAYWHITE);
    if (!ui->open) return;

    Rectangle p = panel_rect();
    DrawRectangleRec(p, panel_color);
    DrawRectangleLinesEx(p, 2.0f, edge_color);
    int tabs[MAX_QUEST_TABS], tab_count = shown_tabs(tabs), tab = open_tab(ui), claimable = 0;
    for (int i = 0; i < tab_count; i++) {
        Rectangle r = tab_rect(p, i);
        int all = 0, done = 0, claim = 0;
        for (int k = 0; k < content.quest_count; k++) {
            if (content.quests[k].tab != tabs[i]) continue;
            all++;
            done += g->quest_state[k] != 0;
            claim += g->quest_state[k] == QUEST_DONE;
        }
        if (tabs[i] == tab) claimable = claim;
        DrawRectangleRec(r, tabs[i] == tab ? row_on : CheckCollisionPointRec(m, r) ? row_hot : row_color);
        char n[24];
        snprintf(n, sizeof n, "%d/%d", done, all);
        float nw = (float)MeasureText(n, 12);
        BeginScissorMode((int)r.x, (int)r.y, (int)(r.width - nw - 10.0f), (int)r.height);
        DrawText(content.quest_tabs[tabs[i]].label, (int)r.x + 6, (int)r.y + 5, 14, claim > 0 ? gold : RAYWHITE);
        EndScissorMode();
        DrawText(n, (int)(r.x + r.width - nw - 5.0f), (int)r.y + 7, 12, done == all ? green : dim);
    }
    if (tab < 0) return;
    draw_button(claim_all_rect(p), "Claim all", claimable > 0);

    /* The page: every quest of the tab where it stands, joined to what it comes after. */
    Rectangle page = page_rect(p);
    int chosen = ui->chosen[tab] - 1, hover = node_at(ui, page, tab, m);
    DrawRectangleRec(page, page_color);
    BeginScissorMode((int)page.x, (int)page.y, (int)page.width, (int)page.height);
    for (int i = 0; i < content.quest_count; i++) {
        const QuestDef *q = &content.quests[i];
        if (q->tab != tab) continue;
        Rectangle to = node_rect(ui, page, i);
        int state = game_quest_state(g, i);
        for (int k = 0; k < q->need_count; k++) {
            if (content.quests[q->needs[k]].tab != tab) continue;
            Rectangle from = node_rect(ui, page, q->needs[k]);
            Vector2 a = { from.x + from.width / 2.0f, from.y + from.height / 2.0f }, z = { to.x + to.width / 2.0f, to.y + to.height / 2.0f };
            if ((a.x < page.x && z.x < page.x) || (a.x > page.x + page.width && z.x > page.x + page.width) || (a.y < page.y && z.y < page.y) || (a.y > page.y + page.height && z.y > page.y + page.height)) continue;
            Color c = state == QUEST_LOCKED ? (Color){ 54, 57, 68, 255 } : state == QUEST_OPEN ? (Color){ 150, 154, 170, 255 } : green;
            /* (Along, then down: so that the lines out of one quest run together before they part.) */
            if (fabsf(a.y - z.y) < 1.0f || fabsf(a.x - z.x) < 1.0f) DrawLineEx(a, z, q->hub ? 3.0f : 2.0f, c);
            else {
                DrawLineEx(a, (Vector2){ a.x, z.y }, 2.0f, c);
                DrawLineEx((Vector2){ a.x, z.y }, z, 2.0f, c);
            }
        }
    }
    for (int i = 0; i < content.quest_count; i++) {
        const QuestDef *q = &content.quests[i];
        if (q->tab != tab) continue;
        Rectangle r = node_rect(ui, page, i);
        if (r.x + r.width < page.x || r.x > page.x + page.width || r.y + r.height < page.y - 16.0f || r.y > page.y + page.height) continue;
        int state = game_quest_state(g, i);
        DrawRectangleRec(r, i == chosen ? row_on : i == hover ? row_hot : row_color);
        if (q->icon != ITEM_NONE) hud_draw_item_icon(q->icon, (Rectangle){ r.x + 3, r.y + 3, r.width - 6, r.height - 6 });
        if (state == QUEST_LOCKED) DrawRectangleRec(r, (Color){ 20, 22, 28, 190 });
        DrawRectangleLinesEx(r, q->hub ? 3.0f : 2.0f, state_color(state));
        if (state == QUEST_CLAIMED) DrawRectangleRec((Rectangle){ r.x + r.width - 9, r.y + r.height - 9, 7, 7 }, green);
        if (state == QUEST_DONE) DrawText("!", (int)(r.x + r.width - 8.0f), (int)r.y - 2, 16, gold);
        /* (One that others hang from says what it is the head of.) */
        if (q->hub) DrawText(q->label, (int)(r.x + r.width / 2.0f - (float)MeasureText(q->label, 12) / 2.0f), (int)(r.y - 15.0f), 12, state_color(state));
    }
    EndScissorMode();
    DrawRectangleLinesEx(page, 1.0f, (Color){ 60, 63, 76, 255 });
    if (hover >= 0 && !content.quests[hover].hub) {
        const char *name = content.quests[hover].label;
        float w = (float)MeasureText(name, 14) + 12.0f;
        DrawRectangleRec((Rectangle){ m.x + 12, m.y - 26, w, 22 }, panel_color);
        DrawRectangleLinesEx((Rectangle){ m.x + 12, m.y - 26, w, 22 }, 1.0f, edge_color);
        DrawText(name, (int)m.x + 18, (int)m.y - 22, 14, state_color(game_quest_state(g, hover)));
    }
    if (chosen < 0 || chosen >= content.quest_count || content.quests[chosen].tab != tab) return;

    const QuestDef *q = &content.quests[chosen];
    Rectangle d = detail_rect(p);
    int state = game_quest_state(g, chosen);
    char line[160];
    float y = draw_wrapped(q->label, d.x, d.y, d.width, 20, state == QUEST_OPEN || state == QUEST_LOCKED ? RAYWHITE : state_color(state), true) + 6.0f;
    y = draw_wrapped(q->text, d.x, y, d.width, 14, pale, true) + 8.0f;
    task_text(g, chosen, line, sizeof line);
    BeginScissorMode((int)d.x, (int)y, (int)d.width, 24);
    DrawText(line, (int)d.x, (int)y, 16, state >= QUEST_DONE ? green : RAYWHITE);
    EndScissorMode();
    y += 28.0f;
    for (int k = 0; k < q->need_count; k++) {
        snprintf(line, sizeof line, "After: %s", content.quests[q->needs[k]].label);
        DrawText(line, (int)d.x, (int)y, 14, g->quest_state[q->needs[k]] != 0 ? green : dim);
        y += 18.0f;
    }
    y += 10.0f;
    for (int k = 0; k < q->reward_count; k++) {
        Rectangle r = reward_rect(d, y, k);
        if (q->pick) DrawRectangleRec(r, ui->choice == k + 1 ? row_on : CheckCollisionPointRec(m, r) && state == QUEST_DONE ? row_hot : row_color);
        hud_draw_item_icon(q->rewards[k].item, (Rectangle){ r.x + 1, r.y + 1, 32, 32 });
        snprintf(line, sizeof line, "%d  %s", q->rewards[k].count, item_def(q->rewards[k].item)->label);
        BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);
        DrawText(line, (int)r.x + 40, (int)r.y + 10, 14, pale);
        EndScissorMode();
    }
    y += (float)q->reward_count * 36.0f;
    if (q->crystals > 0) {
        snprintf(line, sizeof line, "%d  crystals", q->crystals);
        DrawText(line, (int)d.x + 40, (int)y + 4, 14, (Color){ 120, 220, 240, 255 });
    }
    bool tick = q->kind == QUEST_CHECK && state == QUEST_OPEN;
    const char *label = state == QUEST_CLAIMED ? "Claimed" : tick ? "Done" : q->pick && ui->choice < 1 ? "Pick one" : "Claim";
    draw_button(claim_rect(p), label, tick || (state == QUEST_DONE && (!q->pick || ui->choice >= 1)));
}
