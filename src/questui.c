#include "questui.h"

#include "hud.h"
#include "trade.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define PANEL_W 1000.0f
#define PANEL_H 580.0f
#define TABS_W 210.0f
#define LIST_W 360.0f
#define TAB_ROW 26.0f
#define ROW 34.0f
#define PAD 10.0f

static const Color panel_color = { 28, 30, 38, 246 }, edge_color = { 120, 122, 134, 255 }, row_color = { 44, 47, 58, 255 }, row_hot = { 62, 66, 80, 255 }, row_on = { 74, 80, 104, 255 };
static const Color gold = { 236, 200, 96, 255 }, green = { 130, 210, 130, 255 }, dim = { 130, 134, 148, 255 }, pale = { 200, 204, 216, 255 };

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

static Rectangle list_rect(Rectangle p)
{
    return (Rectangle){ p.x + TABS_W + PAD, p.y + PAD, LIST_W, p.height - 2 * PAD - 36.0f };
}

static Rectangle claim_all_rect(Rectangle p)
{
    return (Rectangle){ p.x + TABS_W + PAD, p.y + p.height - PAD - 28.0f, 120.0f, 28.0f };
}

static Rectangle detail_rect(Rectangle p)
{
    float x = p.x + TABS_W + LIST_W + 2 * PAD;
    return (Rectangle){ x, p.y + PAD, p.x + p.width - PAD - x, p.height - 2 * PAD };
}

static Rectangle claim_rect(Rectangle p)
{
    Rectangle d = detail_rect(p);
    return (Rectangle){ d.x, d.y + d.height - 32.0f, 130.0f, 30.0f };
}

/* The quests of a tab, in the order they were written: how many, and which. */
static int tab_quests(int tab, int *out, int max)
{
    int n = 0;
    for (int i = 0; i < content.quest_count && n < max; i++)
        if (content.quests[i].tab == tab) out[n++] = i;
    return n;
}

static float max_scroll(int count, Rectangle list)
{
    float all = (float)count * ROW;
    return all > list.height ? all - list.height : 0.0f;
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
        ui->open = ui->pressed = false;
        return false;
    }
    static int quests[MAX_QUESTS];
    Vector2 m = GetMousePosition();
    Rectangle p = panel_rect();
    int tab = open_tab(ui), count = tab >= 0 ? tab_quests(tab, quests, MAX_QUESTS) : 0;
    ui->tab = tab;
    /* The wheel moves the list (by as much as it is turned, however little). */
    float wheel = GetMouseWheelMove();
    if (ui->open && tab >= 0 && wheel != 0.0f && CheckCollisionPointRec(m, p)) {
        float top = max_scroll(count, list_rect(p)), s = ui->scroll[tab] - wheel * ROW * 1.5f;
        ui->scroll[tab] = s < 0.0f ? 0.0f : s > top ? top : s;
    }
    bool on = CheckCollisionPointRec(m, button_rect()) || (ui->open && CheckCollisionPointRec(m, p));
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ui->pressed = on;
    if (!ui->pressed) return ui->open && CheckCollisionPointRec(m, p);
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return true;
    ui->pressed = false;
    if (CheckCollisionPointRec(m, button_rect())) {
        ui->open = !ui->open;
        return true;
    }
    if (!ui->open) return true;
    int tabs[MAX_QUEST_TABS], tab_count = shown_tabs(tabs);
    for (int i = 0; i < tab_count; i++)
        if (CheckCollisionPointRec(m, tab_rect(p, i))) ui->tab = tabs[i];
    Rectangle list = list_rect(p);
    if (tab >= 0 && CheckCollisionPointRec(m, list)) {
        int row = (int)((m.y - list.y + ui->scroll[tab]) / ROW);
        if (row >= 0 && row < count) ui->chosen[tab] = quests[row] + 1;
    }
    if (tab >= 0 && CheckCollisionPointRec(m, claim_all_rect(p))) game_quest_claim_all(g, tab);
    int chosen = tab >= 0 ? ui->chosen[tab] - 1 : -1;
    if (chosen >= 0 && CheckCollisionPointRec(m, claim_rect(p))) {
        if (content.quests[chosen].kind == QUEST_CHECK && game_quest_state(g, chosen) == QUEST_OPEN) game_quest_check(g, chosen);
        else game_quest_claim(g, chosen);
    }
    return true;
}

static void draw_button(Rectangle r, const char *text, bool enabled)
{
    bool hot = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, !enabled ? (Color){ 40, 42, 50, 255 } : hot ? row_hot : (Color){ 54, 57, 70, 255 });
    DrawText(text, (int)(r.x + (r.width - (float)MeasureText(text, 14)) / 2.0f), (int)(r.y + (r.height - 14.0f) / 2.0f), 14, enabled ? RAYWHITE : GRAY);
}

/* Text broken into lines no wider than `wide`: where the last one ended. */
static float draw_wrapped(const char *text, float x, float y, float wide, int size, Color c)
{
    char line[200] = "";
    const char *at = text;
    while (*at) {
        const char *end = at;
        while (*end && *end != ' ') end++;
        char next[200];
        snprintf(next, sizeof next, "%s%s%.*s", line, line[0] ? " " : "", (int)(end - at), at);
        if (line[0] && (float)MeasureText(next, size) > wide) {
            DrawText(line, (int)x, (int)y, size, c);
            y += (float)size + 4.0f;
            snprintf(line, sizeof line, "%.*s", (int)(end - at), at);
        } else {
            snprintf(line, sizeof line, "%s", next);
        }
        at = *end ? end + 1 : end;
    }
    if (line[0]) {
        DrawText(line, (int)x, (int)y, size, c);
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

void questui_draw(const QuestUi *ui, const Game *g)
{
    if (!game_quests_on() || g->character_count == 0) return;
    static int quests[MAX_QUESTS];
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
    int tabs[MAX_QUEST_TABS], tab_count = shown_tabs(tabs), tab = open_tab(ui);
    for (int i = 0; i < tab_count; i++) {
        Rectangle r = tab_rect(p, i);
        int all = tab_quests(tabs[i], quests, MAX_QUESTS), done = 0, claim = 0;
        for (int k = 0; k < all; k++) done += g->quest_state[quests[k]] != 0, claim += g->quest_state[quests[k]] == QUEST_DONE;
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
    int count = tab_quests(tab, quests, MAX_QUESTS), chosen = ui->chosen[tab] - 1, claimable = 0;
    Rectangle list = list_rect(p);
    BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    for (int i = 0; i < count; i++) {
        Rectangle r = { list.x, list.y + (float)i * ROW - ui->scroll[tab], list.width, ROW - 2.0f };
        if (r.y + r.height < list.y || r.y > list.y + list.height) continue;
        const QuestDef *q = &content.quests[quests[i]];
        int state = game_quest_state(g, quests[i]);
        claimable += state == QUEST_DONE;
        DrawRectangleRec(r, quests[i] == chosen ? row_on : CheckCollisionPointRec(m, r) && CheckCollisionPointRec(m, list) ? row_hot : row_color);
        if (q->icon != ITEM_NONE) hud_draw_item_icon(q->icon, (Rectangle){ r.x + 3, r.y + 2, 28, 28 });
        if (state == QUEST_LOCKED) DrawRectangleRec((Rectangle){ r.x + 3, r.y + 2, 28, 28 }, (Color){ 28, 30, 38, 170 });
        DrawText(q->label, (int)r.x + 38, (int)r.y + 8, 16, state == QUEST_LOCKED ? dim : state == QUEST_DONE ? gold : state == QUEST_CLAIMED ? green : RAYWHITE);
        if (state == QUEST_DONE) DrawText("!", (int)(r.x + r.width - 14.0f), (int)r.y + 8, 16, gold);
    }
    EndScissorMode();
    for (int i = 0; i < count; i++) claimable += g->quest_state[quests[i]] == QUEST_DONE;
    draw_button(claim_all_rect(p), "Claim all", claimable > 0);
    if (chosen < 0 || chosen >= content.quest_count || content.quests[chosen].tab != tab) return;

    const QuestDef *q = &content.quests[chosen];
    Rectangle d = detail_rect(p);
    int state = game_quest_state(g, chosen);
    char line[160];
    float y = d.y;
    DrawText(q->label, (int)d.x, (int)y, 20, state == QUEST_DONE ? gold : state == QUEST_CLAIMED ? green : RAYWHITE);
    y += 30.0f;
    y = draw_wrapped(q->text, d.x, y, d.width, 14, pale) + 8.0f;
    task_text(g, chosen, line, sizeof line);
    DrawText(line, (int)d.x, (int)y, 16, state >= QUEST_DONE ? green : RAYWHITE);
    y += 28.0f;
    for (int k = 0; k < q->need_count; k++) {
        snprintf(line, sizeof line, "After: %s", content.quests[q->needs[k]].label);
        DrawText(line, (int)d.x, (int)y, 14, g->quest_state[q->needs[k]] != 0 ? green : dim);
        y += 18.0f;
    }
    y += 10.0f;
    for (int k = 0; k < q->reward_count; k++) {
        hud_draw_item_icon(q->rewards[k].item, (Rectangle){ d.x, y, 32, 32 });
        snprintf(line, sizeof line, "%d  %s", q->rewards[k].count, item_def(q->rewards[k].item)->label);
        DrawText(line, (int)d.x + 40, (int)y + 9, 14, pale);
        y += 36.0f;
    }
    bool tick = q->kind == QUEST_CHECK && state == QUEST_OPEN;
    draw_button(claim_rect(p), state == QUEST_CLAIMED ? "Claimed" : tick ? "Done" : "Claim", state == QUEST_DONE || tick);
}
