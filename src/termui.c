#include "termui.h"

#include "assets.h"
#include "digital.h"
#include "guide.h"
#include "raylib.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define CELL 52.0f
#define GAP 4.0f
#define COLUMNS 11
#define ROWS 5
#define PAD 16.0f
#define REACH 8 /* blocks off a terminal can be used from */

static const Color panel_color = { 28, 30, 38, 250 };
static const Color edge_color = { 120, 122, 134, 255 };
static const Color slot_color = { 44, 47, 58, 255 };
static const Color slot_hot = { 80, 84, 100, 255 };
static const Color accent = { 70, 130, 190, 255 };
static const Color dim_text = { 170, 172, 184, 255 };

static bool over(Rectangle r)
{
    return CheckCollisionPointRec(GetMousePosition(), r);
}

static bool button(Rectangle r, const char *text, bool enabled, bool lit)
{
    bool hot = enabled && over(r);
    DrawRectangleRec(r, lit ? accent : hot ? slot_hot : slot_color);
    DrawRectangleLinesEx(r, 2.0f, edge_color);
    int size = 18;
    while (size > 10 && MeasureText(text, size) > r.width - 8) size--;
    DrawText(text, (int)(r.x + (r.width - (float)MeasureText(text, size)) / 2), (int)(r.y + (r.height - (float)size) / 2), size, enabled ? RAYWHITE : GRAY);
    return hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

/* "1234" as it is; "12.3k", "4.5M" beyond that. */
static const char *short_count(long n)
{
    if (n < 10000) return TextFormat("%ld", n);
    if (n < 1000000) return TextFormat("%ld.%ldk", n / 1000, n % 1000 / 100);
    return TextFormat("%ld.%ldM", n / 1000000, n % 1000000 / 100000);
}

static void draw_count(Rectangle r, const char *text, Color color)
{
    int size = 14;
    DrawText(text, (int)(r.x + r.width - (float)MeasureText(text, size) - 3) + 1, (int)(r.y + r.height - (float)size - 1) + 1, size, BLACK);
    DrawText(text, (int)(r.x + r.width - (float)MeasureText(text, size) - 3), (int)(r.y + r.height - (float)size - 1), size, color);
}

void termui_open(TermUi *ui, TilePos at, bool remote)
{
    *ui = (TermUi){ .open = true, .at = at, .remote = remote, .amount = 1 };
    while (GetCharPressed() > 0) {}
}

void termui_frame(TermUi *ui, Game *g)
{
    if (!ui->open) return;
    NetInfo info;
    digital_info(g, ui->at.x, ui->at.y, &info);
    /* It shuts when its network is gone, or the character has gone from it. */
    TilePos remote_at;
    long dx = world_dx(&g->world, g->player.feet.x, ui->at.x), dy = ui->at.y - g->player.feet.y;
    bool near = ui->remote ? digital_remote(g, &remote_at) : dx * dx + dy * dy <= REACH * REACH;
    if (ui->remote && near) ui->at = remote_at;
    if (!info.found || !near || !game_alive(g) || IsKeyPressed(KEY_ESCAPE)) {
        ui->open = false;
        return;
    }

    float wide = 2 * PAD + COLUMNS * (CELL + GAP) - GAP, high = 520.0f;
    Rectangle panel = { ((float)GetScreenWidth() - wide) / 2, ((float)GetScreenHeight() - high) / 2, wide, high };
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 0, 0, 0, 110 });
    DrawRectangleRec(panel, panel_color);
    DrawRectangleLinesEx(panel, 2.0f, edge_color);
    DrawText("Storage network", (int)(panel.x + PAD), (int)(panel.y + 14), 24, RAYWHITE);
    if (button((Rectangle){ panel.x + panel.width - PAD - 84, panel.y + 10, 84, 32 }, "Close", true, false)) {
        ui->open = false;
        return;
    }
    /* How full it is. */
    const char *room = info.online ? TextFormat("%s / %s bytes   %d kinds", short_count(info.bytes_used), short_count(info.bytes_total), info.types) : "No power";
    DrawText(room, (int)(panel.x + panel.width - PAD - 96 - (float)MeasureText(room, 16)), (int)(panel.y + 18), 16, info.online ? dim_text : (Color){ 235, 90, 90, 255 });

    static const char *const tabs[3] = { "Stored", "Make", "Jobs" };
    float ty = panel.y + 52;
    for (int i = 0; i < 3; i++) {
        const char *label = i == 2 && info.cpus_busy > 0 ? TextFormat("Jobs (%d)", info.cpus_busy) : tabs[i];
        if (button((Rectangle){ panel.x + PAD + (float)i * 124, ty, 118, 32 }, label, true, ui->tab == i)) ui->tab = i, ui->scroll = 0.0f;
    }
    /* A search, for the first two. */
    Rectangle box = { panel.x + PAD + 3 * 124 + 8, ty, panel.width - 2 * PAD - 3 * 124 - 8, 32 };
    if (ui->tab < 2) {
        size_t len = strlen(ui->search);
        for (int c = GetCharPressed(); c > 0; c = GetCharPressed())
            if (c >= 32 && c < 127 && len < TERMUI_SEARCH_MAX) ui->search[len++] = (char)c, ui->search[len] = '\0';
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && len > 0) ui->search[len - 1] = '\0';
        DrawRectangleRec(box, (Color){ 18, 19, 24, 255 });
        DrawRectangleLinesEx(box, 2.0f, edge_color);
        bool blink = (int)(GetTime() * 2.0) % 2 == 0;
        DrawText(ui->search[0] || blink ? TextFormat("%s%s", ui->search, blink ? "_" : "") : "", (int)box.x + 8, (int)box.y + 8, 16, RAYWHITE);
        if (!ui->search[0]) DrawText("Search", (int)box.x + 20, (int)box.y + 8, 16, GRAY);
    }

    Rectangle grid = { panel.x + PAD, ty + 44, panel.width - 2 * PAD, ROWS * (CELL + GAP) - GAP };
    const char *hover = NULL;
    static NetItem all[MAX_NET_KINDS];
    int kinds = info.online ? digital_list(g, ui->at.x, ui->at.y, all, MAX_NET_KINDS) : 0;

    if (ui->tab < 2) {
        /* The grid: what is stored, or what can be made. */
        int shown = 0;
        ui->scroll -= GetMouseWheelMove() * (CELL + GAP);
        BeginScissorMode((int)grid.x, (int)grid.y, (int)grid.width, (int)grid.height);
        for (int i = 0; i < kinds; i++) {
            const NetItem *it = &all[i];
            if (ui->tab == 0 ? it->count <= 0 : !it->craftable) continue;
            if (!guide_matches(it->item, ui->search)) continue;
            Rectangle r = { grid.x + (float)(shown % COLUMNS) * (CELL + GAP), grid.y + (float)(shown / COLUMNS) * (CELL + GAP) - ui->scroll, CELL, CELL };
            shown++;
            if (r.y + r.height < grid.y || r.y > grid.y + grid.height) continue;
            bool hot = over(r) && over(grid);
            DrawRectangleRec(r, hot ? slot_hot : slot_color);
            if (ui->tab == 1 && it->item == ui->chosen) DrawRectangleLinesEx(r, 2.0f, accent);
            assets_draw_item_tones(it->item, (Rectangle){ r.x + 2, r.y + 2, CELL - 4, CELL - 4 }, 0);
            if (it->count > 0) draw_count(r, short_count(it->count), RAYWHITE);
            else draw_count(r, "make", (Color){ 150, 200, 255, 255 });
            if (!hot) continue;
            hover = item_def(it->item)->label;
            if (ui->tab == 0) {
                /* A click takes a stack of it; the other button, one. */
                int stack = item_def(it->item)->max_stack;
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) digital_withdraw(g, ui->at.x, ui->at.y, it->item, it->count < stack ? (int)it->count : stack);
                if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) digital_withdraw(g, ui->at.x, ui->at.y, it->item, 1);
            } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                ui->chosen = it->item;
                ui->amount = 1;
                ui->message[0] = '\0';
            }
        }
        EndScissorMode();
        float tall = (float)((shown + COLUMNS - 1) / COLUMNS) * (CELL + GAP), most = tall - grid.height;
        if (ui->scroll > most) ui->scroll = most;
        if (ui->scroll < 0.0f) ui->scroll = 0.0f;
        if (shown == 0) DrawText(!info.online ? "No power" : ui->tab == 0 ? "Nothing stored" : "Nothing to make", (int)grid.x + 4, (int)grid.y + 8, 18, GRAY);
    }

    float low = grid.y + grid.height + 12;
    if (ui->tab == 0) {
        /* The character's bar: a click puts a stack of it in; the other button, one. */
        DrawText("Carried", (int)grid.x, (int)low + 4, 16, dim_text);
        for (int i = 0; i < INVENTORY_SLOTS; i++) {
            Rectangle r = { grid.x + 76 + (float)i * (CELL + GAP), low - 4, CELL, CELL };
            const ItemStack *s = &g->inventory.slots[i];
            bool hot = over(r);
            DrawRectangleRec(r, hot ? slot_hot : slot_color);
            if (s->count == 0) continue;
            assets_draw_item_tones(s->id, (Rectangle){ r.x + 2, r.y + 2, CELL - 4, CELL - 4 }, 0);
            if (s->count > 1) draw_count(r, TextFormat("%d", s->count), RAYWHITE);
            if (!hot) continue;
            hover = item_def(s->id)->label;
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) digital_deposit(g, ui->at.x, ui->at.y, i, 0);
            if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) digital_deposit(g, ui->at.x, ui->at.y, i, 1);
        }
        if (button((Rectangle){ panel.x + panel.width - PAD - 110, low + 2, 110, 36 }, "Store all", info.online, false))
            for (int i = 0; i < INVENTORY_SLOTS; i++) digital_deposit(g, ui->at.x, ui->at.y, i, 0);
    } else if (ui->tab == 1) {
        /* What has been picked, how many, and the asking. */
        if (ui->chosen != ITEM_NONE && digital_recipe_for(g, ui->at.x, ui->at.y, ui->chosen)) {
            const Recipe *r = digital_recipe_for(g, ui->at.x, ui->at.y, ui->chosen);
            assets_draw_item_tones(ui->chosen, (Rectangle){ grid.x, low - 4, CELL, CELL }, 0);
            /* (Its name, no wider than the room there is before the buttons.) */
            float name_room = panel.x + panel.width - PAD - 4 * 50 - 76 - 104 - (grid.x + 60) - 10;
            int name_size = 18;
            while (name_size > 10 && (float)MeasureText(item_def(ui->chosen)->label, name_size) > name_room) name_size--;
            DrawText(item_def(ui->chosen)->label, (int)grid.x + 60, (int)low, name_size, RAYWHITE);
            /* (What one lot of it takes.) */
            float ix = grid.x + 60;
            for (int i = 0; i < r->input_count; i++) {
                Rectangle in = { ix, low + 22, 26, 26 };
                assets_draw_item_tones(r->inputs[i].id, in, 0);
                const char *n = TextFormat("%d", r->inputs[i].count);
                DrawText(n, (int)ix + 28, (int)low + 28, 14, dim_text);
                if (over(in)) hover = item_def(r->inputs[i].id)->label;
                ix += 34 + (float)MeasureText(n, 14);
            }
            static const int steps[4] = { -10, -1, 1, 10 };
            static const char *const names[4] = { "-10", "-1", "+1", "+10" };
            float bx = panel.x + panel.width - PAD - 4 * 50 - 76 - 104;
            for (int i = 0; i < 4; i++) {
                if (button((Rectangle){ bx + (float)(i < 2 ? i : i + 1) * 50 + (i >= 2 ? 26.0f : 0.0f), low + 2, 46, 36 }, names[i], true, false)) ui->amount += steps[i];
            }
            if (ui->amount < 1) ui->amount = 1;
            if (ui->amount > 9999) ui->amount = 9999;
            const char *n = TextFormat("%d", ui->amount);
            DrawText(n, (int)(bx + 100 + (76 - (float)MeasureText(n, 20)) / 2), (int)low + 10, 20, RAYWHITE);
            if (button((Rectangle){ panel.x + panel.width - PAD - 96, low + 2, 96, 36 }, "Make", info.cpus > 0, false)) {
                if (digital_request(g, ui->at.x, ui->at.y, ui->chosen, ui->amount, ui->message, sizeof ui->message)) snprintf(ui->message, sizeof ui->message, "Started");
            }
        } else {
            DrawText(info.cpus > 0 ? "Pick something to make" : "No crafting CPU", (int)grid.x, (int)low + 8, 18, GRAY);
        }
    } else {
        /* The jobs: what each is making, how far along, and a way to stop it. */
        const NetJob *jobs[MAX_NET_JOBS];
        int n = digital_jobs(g, ui->at.x, ui->at.y, jobs, MAX_NET_JOBS);
        DrawText(TextFormat("%d of %d CPUs busy", info.cpus_busy < info.cpus ? info.cpus_busy : info.cpus, info.cpus), (int)grid.x, (int)grid.y, 16, dim_text);
        for (int i = 0; i < n && i < 6; i++) {
            const NetJob *job = jobs[i];
            Rectangle row = { grid.x, grid.y + 26 + (float)i * 58, grid.width, 52 };
            DrawRectangleRec(row, slot_color);
            assets_draw_item_tones(job->wanted, (Rectangle){ row.x + 4, row.y + 4, 44, 44 }, 0);
            DrawText(TextFormat("%s x%d", item_def(job->wanted)->label, job->amount), (int)row.x + 56, (int)row.y + 6, 18, RAYWHITE);
            long total = 0, done = 0;
            for (int k = 0; k < job->step_count; k++) total += job->steps[k].crafts, done += job->steps[k].done;
            Rectangle bar = { row.x + 56, row.y + 32, row.width - 56 - 190, 10 };
            DrawRectangleRec(bar, (Color){ 18, 19, 24, 255 });
            DrawRectangleRec((Rectangle){ bar.x, bar.y, bar.width * (total > 0 ? (float)done / (float)total : 0.0f), bar.height }, accent);
            const char *state = i >= info.cpus ? "Queued" : job->waiting ? "Waiting" : "Working";
            DrawText(state, (int)(row.x + row.width - 180), (int)row.y + 16, 16, job->waiting ? (Color){ 240, 190, 90, 255 } : dim_text);
            if (button((Rectangle){ row.x + row.width - 92, row.y + 8, 84, 36 }, "Cancel", true, false)) digital_cancel(g, job);
        }
        if (n == 0) DrawText("No jobs", (int)grid.x, (int)grid.y + 30, 18, GRAY);
    }
    if (ui->tab == 1 && ui->message[0]) DrawText(ui->message, (int)grid.x, (int)(panel.y + panel.height - 30), 16, (Color){ 240, 190, 90, 255 });
    else if (hover) DrawText(hover, (int)grid.x, (int)(panel.y + panel.height - 30), 16, RAYWHITE);
}
