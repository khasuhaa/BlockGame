#include "recipeui.h"

#include "assets.h"
#include "guide.h"
#include "raylib.h"
#include "trade.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CELL 48.0f
#define CELL_GAP 4.0f
#define GRID_COLUMNS 8
#define PAD 16.0f
#define MAX_SHOWN_SOURCES 48
#define MAX_SHOWN_RECIPES 256

static const Color panel_color = { 28, 30, 38, 250 };
static const Color edge_color = { 120, 122, 134, 255 };
static const Color slot_color = { 44, 47, 58, 255 };
static const Color slot_hot = { 80, 84, 100, 255 };
static const Color row_color = { 36, 38, 48, 255 };
static const Color accent_color = { 70, 130, 80, 255 };
static const Color dim_text = { 170, 172, 184, 255 };

/* What the mouse is over this frame, and what was clicked. */
static const char *hover_label;
static ItemId clicked;

static bool mouse_in(Rectangle r)
{
    return CheckCollisionPointRec(GetMousePosition(), r);
}

/* An item in a slot; `count` above 1 is written in its corner. Within `clip`
 * it can be pointed at and picked. */
static void slot(ItemId id, Rectangle r, Rectangle clip, int count, bool marked)
{
    bool hot = id != ITEM_NONE && mouse_in(r) && mouse_in(clip);
    DrawRectangleRec(r, hot ? slot_hot : slot_color);
    if (marked) DrawRectangleLinesEx(r, 2.0f, accent_color);
    if (id == ITEM_NONE) return;
    assets_draw_item_tones(id, r, 0);
    if (count > 1) {
        const char *n = TextFormat("%d", count);
        int size = r.width >= 40 ? 16 : 10;
        DrawText(n, (int)(r.x + r.width - MeasureText(n, size) - 2) + 1, (int)(r.y + r.height - size) + 1, size, BLACK);
        DrawText(n, (int)(r.x + r.width - MeasureText(n, size) - 2), (int)(r.y + r.height - size), size, RAYWHITE);
    }
    if (hot) {
        hover_label = item_def(id)->label;
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) clicked = id;
    }
}

static bool button(Rectangle r, const char *text, bool enabled)
{
    bool hot = enabled && mouse_in(r);
    DrawRectangleRec(r, hot ? slot_hot : slot_color);
    DrawRectangleLinesEx(r, 2.0f, edge_color);
    DrawText(text, (int)(r.x + (r.width - MeasureText(text, 20)) / 2), (int)(r.y + (r.height - 20) / 2), 20,
             enabled ? RAYWHITE : GRAY);
    return hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

/* Writes text, broken at spaces to fit a width. Returns how far down it went. */
static float wrapped(const char *text, float x, float y, float width, int size, Color color)
{
    char line[256] = "";
    float used = 0.0f;
    const char *at = text;
    while (*at) {
        const char *end = at;
        while (*end && *end != ' ') end++;
        char next[256];
        snprintf(next, sizeof next, "%s%s%.*s", line, line[0] ? " " : "", (int)(end - at), at);
        if (line[0] && MeasureText(next, size) > width) {
            DrawText(line, (int)x, (int)(y + used), size, color);
            used += size + 3;
            snprintf(line, sizeof line, "%.*s", (int)(end - at), at);
        } else {
            snprintf(line, sizeof line, "%s", next);
        }
        at = end;
        while (*at == ' ') at++;
    }
    if (line[0]) {
        DrawText(line, (int)x, (int)(y + used), size, color);
        used += size + 3;
    }
    return used;
}

static float heading(const char *text, float x, float y)
{
    DrawText(text, (int)x, (int)y, 20, RAYWHITE);
    return 28.0f;
}

/* "3s", "1m 30s". */
static const char *duration(float seconds)
{
    int s = (int)(seconds + 0.5f);
    if (s >= 60 && s % 60 == 0) return TextFormat("%dm", s / 60);
    if (s >= 60) return TextFormat("%dm %ds", s / 60, s % 60);
    return seconds < 10.0f && seconds != floorf(seconds) ? TextFormat("%.1fs", seconds) : TextFormat("%ds", s);
}

/* One recipe: the bench, what goes in, what comes out. */
static float draw_recipe(const Recipe *r, float x, float y, float width, Rectangle clip)
{
    float height = CELL + 12.0f;
    DrawRectangleRec((Rectangle){ x, y, width, height }, row_color);

    /* The bench: picked, it is looked up like anything else. */
    TileType bench = guide_bench_tile(r->bench, r->level);
    ItemId bench_item = guide_placer(bench);
    Rectangle b = { x + 6, y + 6, CELL, CELL };
    if (bench_item != ITEM_NONE) {
        slot(bench_item, b, clip, 1, false);
    } else {
        DrawRectangleRec(b, slot_color);
        if (bench != TILE_AIR) assets_draw_tile(bench, 0, 0, b.x + 8, b.y + 8, CELL - 16, 0.0f, TILE_PART_BODY);
    }
    const TileDef *bd = tile_def(bench);
    float tx = b.x + CELL + 8;
    DrawText(bench != TILE_AIR ? bd->label : r->bench, (int)tx, (int)(y + 8), 16, RAYWHITE);
    DrawText(r->level > 1 ? TextFormat("Tier %d or better", r->level) : "Any tier", (int)tx, (int)(y + 27), 10, dim_text);
    if (r->time > 0.0f) DrawText(TextFormat("Takes %s", duration(r->time)), (int)tx, (int)(y + 40), 10, dim_text);

    /* What goes in. */
    float ix = x + 6 + CELL + 150;
    for (int i = 0; i < r->input_count; i++) {
        slot(r->inputs[i].id, (Rectangle){ ix, y + 6, CELL, CELL }, clip, r->inputs[i].count, false);
        if (r->inputs[i].full) DrawText("full", (int)ix + 3, (int)y + 8, 10, SKYBLUE);
        ix += CELL + CELL_GAP;
    }
    if (r->input_count == 0 && r->random_item_count > 0) {
        /* (Worked out for each world; before one is open, any of these.) */
        for (int i = 0; i < r->random_item_count; i++) {
            slot(r->random_items[i], (Rectangle){ ix, y + 6, CELL, CELL }, clip, 1, false);
            ix += CELL + CELL_GAP;
        }
        DrawText(TextFormat("%d in all", r->random_total), (int)ix, (int)(y + 22), 10, dim_text);
        ix += 60;
    }
    if (r->crystals > 0) {
        Rectangle c = { ix, y + 6, CELL, CELL };
        DrawRectangleRec(c, slot_color);
        assets_draw_image(content.trade.crystal_icon, (Rectangle){ c.x + 8, c.y + 4, CELL - 16, CELL - 16 });
        const char *n = TextFormat("%d", r->crystals);
        DrawText(n, (int)(c.x + c.width - MeasureText(n, 16) - 2), (int)(c.y + c.height - 16), 16, RAYWHITE);
        if (mouse_in(c) && mouse_in(clip)) hover_label = "Time crystals";
        ix += CELL + CELL_GAP;
    }
    /* What comes out. */
    DrawText(">", (int)ix + 6, (int)(y + 20), 20, dim_text);
    Rectangle out = { ix + 28, y + 6, CELL, CELL };
    slot(r->output.id, out, clip, r->output.count, true);
    if (r->tint > 0 && r->tint <= content.tint_count) {
        const unsigned char *c = content.tints[r->tint - 1];
        DrawRectangle((int)(out.x + CELL + 6), (int)(y + 18), 20, 20, (Color){ c[0], c[1], c[2], 255 });
        DrawRectangleLines((int)(out.x + CELL + 6), (int)(y + 18), 20, 20, edge_color);
    }
    return height + 6.0f;
}

/* One way of getting a thing that is not making it. */
static float draw_source(const ItemSource *s, float x, float y, float width, Rectangle clip)
{
    static const char *verbs[] = { "Mine", "Kill", "Shear", "Milk", "Pick from", "Open", "", "" };
    float text_x = x + 6 + CELL + 10, text_width = width - CELL - 28 - (s->placed_by != ITEM_NONE ? CELL + 10 : 0);
    float line = y + 8;
    /* (Measured first, so the row can be drawn behind its words.) */
    int lines = 1;
    {
        char scratch[256] = "";
        const char *at = s->where;
        while (*at) {
            const char *end = at;
            while (*end && *end != ' ') end++;
            char next[256];
            snprintf(next, sizeof next, "%s%s%.*s", scratch, scratch[0] ? " " : "", (int)(end - at), at);
            if (scratch[0] && MeasureText(next, 10) > text_width) {
                lines++;
                snprintf(scratch, sizeof scratch, "%.*s", (int)(end - at), at);
            } else {
                snprintf(scratch, sizeof scratch, "%s", next);
            }
            at = end;
            while (*at == ' ') at++;
        }
    }
    float height = fmaxf(CELL + 12.0f, 8 + 20 + 14 + lines * 13.0f + 6);
    DrawRectangleRec((Rectangle){ x, y, width, height }, row_color);

    Rectangle pic = { x + 6, y + 6, CELL, CELL };
    DrawRectangleRec(pic, slot_color);
    if (s->kind == SOURCE_BLOCK || s->kind == SOURCE_CHEST) {
        assets_draw_tile((TileType)s->from, 0, 0, pic.x + 8, pic.y + 8, CELL - 16, 0.0f, TILE_PART_BODY);
        assets_draw_tile_tones((TileType)s->from, pic.x + 8, pic.y + 8, CELL - 16, 0.0f, 0);
    } else if (s->kind == SOURCE_CREATURE || s->kind == SOURCE_SHEAR || s->kind == SOURCE_MILK)
        assets_draw_sprite(content.mobs[s->from].texture, (Rectangle){ pic.x + 4, pic.y + 4, CELL - 8, CELL - 8 }, false);
    else if (s->kind == SOURCE_FRUIT)
        assets_draw_tile(content.trees[s->from].leaves, 0, 0, pic.x + 8, pic.y + 8, CELL - 16, 0.0f, TILE_PART_BODY);
    else
        DrawText(s->kind == SOURCE_MEDITATION ? "~" : "+", (int)pic.x + 17, (int)pic.y + 12, 30, dim_text);

    DrawText(verbs[s->kind][0] ? TextFormat("%s: %s", verbs[s->kind], s->label) : s->label, (int)text_x, (int)line, 16, RAYWHITE);
    line += 20;
    char facts[160] = "";
    if (s->min != s->max) snprintf(facts, sizeof facts, "%d to %d at a time", s->min, s->max);
    else if (s->max > 1) snprintf(facts, sizeof facts, "%d at a time", s->max);
    if (s->chance < 0.9999f) {
        size_t n = strlen(facts);
        float pc = s->chance * 100.0f;
        snprintf(facts + n, sizeof facts - n, pc < 10.0f ? "%s%.1f%% of the time" : "%s%.0f%% of the time", n ? ", " : "", pc);
    }
    if (s->kind == SOURCE_BLOCK && s->tool_class >= 0) {
        size_t n = strlen(facts);
        snprintf(facts + n, sizeof facts - n, "%squicker with a %s", n ? "; " : "", content.tool_classes[s->tool_class]);
    }
    if (facts[0]) facts[0] = (char)(facts[0] >= 'a' && facts[0] <= 'z' ? facts[0] - 32 : facts[0]);
    DrawText(facts, (int)text_x, (int)line, 10, (Color){ 200, 190, 130, 255 });
    line += 14;
    wrapped(s->where, text_x, line, text_width, 10, dim_text);
    if (s->placed_by != ITEM_NONE)
        slot(s->placed_by, (Rectangle){ x + width - CELL - 6, y + 6, CELL, CELL }, clip, 1, false);
    return height + 6.0f;
}

/* Everything about the item being looked at. Returns how tall it came to. */
static float draw_detail(const RecipeUi *ui, Rectangle area)
{
    static ItemSource sources[MAX_SHOWN_SOURCES];
    static int recipes[MAX_SHOWN_RECIPES], uses[MAX_SHOWN_RECIPES];
    const ItemDef *def = item_def(ui->shown);
    float x = area.x, y = area.y - ui->detail_scroll, width = area.width - 12;

    Rectangle big = { x, y, 72, 72 };
    DrawRectangleRec(big, slot_color);
    assets_draw_item(ui->shown, big);
    DrawText(def->label, (int)x + 84, (int)y + 6, 30, RAYWHITE);
    char about[200] = "";
    if (def->category >= 0)
        snprintf(about, sizeof about, "%s. Sold at the trade portal for %d.", content.trade.categories[def->category].label,
                 trade_price(ui->shown));
    else if (def->crystals > 0)
        snprintf(about, sizeof about, "Turns into %d time crystal%s when picked up.", def->crystals, def->crystals == 1 ? "" : "s");
    DrawText(about, (int)x + 84, (int)y + 42, 10, dim_text);
    char facts[200] = "";
    if (def->tool_class >= 0)
        snprintf(facts, sizeof facts, "A %s: %.3gx as quick.", content.tool_classes[def->tool_class], def->tool_speed);
    if (def->damage > 0) {
        size_t n = strlen(facts);
        snprintf(facts + n, sizeof facts - n, "%sDoes %d damage", n ? " " : "", def->damage);
        n = strlen(facts);
        if (def->range > 0) snprintf(facts + n, sizeof facts - n, " from up to %d blocks off", def->range);
        n = strlen(facts);
        if (def->poison > 0)
            snprintf(facts + n, sizeof facts - n, ", and %d more by poison (1 every %d ticks)", def->poison, def->poison_every);
        n = strlen(facts);
        snprintf(facts + n, sizeof facts - n, ".");
    }
    if (def->armor > 0.0f) {
        size_t n = strlen(facts);
        snprintf(facts + n, sizeof facts - n, "%sWorn, it stops %.0f%% of an attack.", n ? " " : "", def->armor * 100.0f);
    }
    DrawText(facts, (int)x + 84, (int)y + 56, 10, (Color){ 200, 190, 130, 255 });
    y += 88;

    int made = guide_recipes(ui->shown, recipes, MAX_SHOWN_RECIPES);
    int found = guide_sources(ui->shown, sources, MAX_SHOWN_SOURCES);
    int used = guide_uses(ui->shown, uses, MAX_SHOWN_RECIPES);

    if (made > 0) {
        y += heading(made == 1 ? "Made by" : TextFormat("Made by any of %d recipes", made), x, y);
        for (int i = 0; i < made; i++) {
            /* (Rows out of sight are measured but not drawn.) */
            if (y > area.y + area.height || y + CELL + 18 < area.y) y += CELL + 18;
            else y += draw_recipe(&content.recipes[recipes[i]], x, y, width, area);
        }
        y += 8;
    }
    if (found > 0) {
        y += heading(made > 0 ? "Also got by" : "Got by", x, y);
        for (int i = 0; i < found; i++) y += draw_source(&sources[i], x, y, width, area);
        y += 8;
    }
    if (made == 0 && found == 0) {
        y += heading("Nothing makes or gives this yet", x, y);
        y += wrapped(def->category >= 0 ? "It can only be bought at a trade portal." : "It cannot be got at all.", x, y, width, 10,
                     dim_text);
        y += 12;
    }
    if (used > 0) {
        y += heading("Used to make", x, y);
        int column = 0, per_row = (int)(width / (CELL + CELL_GAP));
        if (per_row < 1) per_row = 1;
        for (int i = 0; i < used; i++) {
            ItemId out = content.recipes[uses[i]].output.id;
            bool repeat = false;
            for (int k = 0; k < i; k++) repeat = repeat || content.recipes[uses[k]].output.id == out;
            if (repeat) continue;
            slot(out, (Rectangle){ x + column * (CELL + CELL_GAP), y, CELL, CELL }, area, 1, false);
            if (++column == per_row) {
                column = 0;
                y += CELL + CELL_GAP;
            }
        }
        if (column > 0) y += CELL + CELL_GAP;
    }
    return y + ui->detail_scroll - area.y + 8;
}

void recipeui_open(RecipeUi *ui)
{
    (void)ui;
    while (GetCharPressed() > 0) {
    }
}

static void look_at(RecipeUi *ui, ItemId item)
{
    if (item == ui->shown) return;
    if (ui->shown != ITEM_NONE) {
        if (ui->depth == RECIPEUI_HISTORY) {
            memmove(ui->history, ui->history + 1, sizeof ui->history - sizeof ui->history[0]);
            ui->depth--;
        }
        ui->history[ui->depth++] = ui->shown;
    }
    ui->shown = item;
    ui->detail_scroll = 0.0f;
}

bool recipeui_frame(RecipeUi *ui, bool escape)
{
    bool open = true;
    hover_label = NULL;
    clicked = ITEM_NONE;
    /* (A save from another set of content may have left an id that is no longer one.) */
    if (ui->shown >= content.item_count) ui->shown = ITEM_NONE, ui->depth = 0;

    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    Rectangle panel = { 30, 30, sw - 60, sh - 60 };
    DrawRectangleRec(panel, panel_color);
    DrawRectangleLinesEx(panel, 2.0f, edge_color);

    /* The search. Whatever is typed goes into it. */
    size_t len = strlen(ui->search);
    for (int c = GetCharPressed(); c > 0; c = GetCharPressed()) {
        if (c >= 32 && c < 127 && len < RECIPEUI_SEARCH_MAX) {
            ui->search[len++] = (char)c;
            ui->search[len] = '\0';
            ui->grid_scroll = 0.0f;
        }
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && len > 0) {
        ui->search[--len] = '\0';
        ui->grid_scroll = 0.0f;
    }
    float left_width = GRID_COLUMNS * (CELL + CELL_GAP) - CELL_GAP;
    DrawText("Recipes", (int)(panel.x + PAD), (int)(panel.y + PAD), 30, RAYWHITE);
    Rectangle box = { panel.x + PAD, panel.y + PAD + 40, left_width, 34 };
    DrawRectangleRec(box, (Color){ 18, 19, 24, 255 });
    DrawRectangleLinesEx(box, 2.0f, RAYWHITE);
    bool blink = (int)(GetTime() * 2.0) % 2 == 0;
    if (len == 0 && !blink) DrawText("Type to search", (int)box.x + 10, (int)box.y + 8, 20, GRAY);
    else DrawText(TextFormat("%s%s", ui->search, blink ? "_" : ""), (int)box.x + 10, (int)box.y + 8, 20, RAYWHITE);

    /* The grid of everything that matches. */
    Rectangle grid = { box.x, box.y + box.height + 10, left_width + 10, panel.y + panel.height - PAD - (box.y + box.height + 10) };
    if (mouse_in(grid)) ui->grid_scroll -= GetMouseWheelMove() * (CELL + CELL_GAP);
    float most = ui->grid_height - grid.height;
    if (ui->grid_scroll > most) ui->grid_scroll = most;
    if (ui->grid_scroll < 0.0f) ui->grid_scroll = 0.0f;
    BeginScissorMode((int)grid.x, (int)grid.y, (int)grid.width, (int)grid.height);
    int n = 0;
    for (ItemId i = 1; i < content.item_count; i++) {
        if (!guide_matches(i, ui->search)) continue;
        Rectangle r = { grid.x + (n % GRID_COLUMNS) * (CELL + CELL_GAP),
                        grid.y + (float)(n / GRID_COLUMNS) * (CELL + CELL_GAP) - ui->grid_scroll, CELL, CELL };
        n++;
        if (r.y + r.height < grid.y || r.y > grid.y + grid.height) continue;
        slot(i, r, grid, 1, i == ui->shown);
    }
    EndScissorMode();
    ui->grid_height = (float)((n + GRID_COLUMNS - 1) / GRID_COLUMNS) * (CELL + CELL_GAP);
    if (n == 0) DrawText("Nothing by that name.", (int)grid.x, (int)grid.y + 8, 20, GRAY);
    if (ui->grid_height > grid.height) {
        float bar = grid.height * grid.height / ui->grid_height;
        DrawRectangle((int)(grid.x + grid.width - 4), (int)(grid.y + (grid.height - bar) * ui->grid_scroll / most), 4, (int)bar,
                      edge_color);
    }

    /* What is known about the one picked. */
    float right_x = grid.x + grid.width + PAD;
    Rectangle top = { right_x, panel.y + PAD, panel.x + panel.width - PAD - right_x, 40 };
    if (button((Rectangle){ top.x + top.width - 100, top.y, 100, 36 }, "Close", true)) open = false;
    bool back = button((Rectangle){ top.x + top.width - 210, top.y, 100, 36 }, "Back", ui->depth > 0);
    if (escape) {
        if (ui->depth > 0) back = true;
        else open = false;
    }
    if (back && ui->depth > 0) {
        ui->shown = ui->history[--ui->depth];
        ui->detail_scroll = 0.0f;
    }
    /* The way here, as a trail. */
    {
        float tx = top.x;
        int first = ui->depth > 6 ? ui->depth - 6 : 0;
        for (int i = first; i < ui->depth && tx < top.x + top.width - 260; i++) {
            Rectangle r = { tx, top.y + 2, 32, 32 };
            bool hot = mouse_in(r);
            DrawRectangleRec(r, hot ? slot_hot : slot_color);
            assets_draw_item(ui->history[i], r);
            DrawText(">", (int)tx + 36, (int)top.y + 10, 16, dim_text);
            if (hot) {
                hover_label = item_def(ui->history[i])->label;
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    ui->shown = ui->history[i];
                    ui->depth = i;
                    ui->detail_scroll = 0.0f;
                }
            }
            tx += 50;
        }
    }
    Rectangle detail = { right_x, top.y + top.height + 10, top.width, panel.y + panel.height - PAD - (top.y + top.height + 10) };
    if (ui->shown == ITEM_NONE) {
        wrapped("Pick an item to see what makes it. Pick anything in a recipe to see what makes that, down to the things "
                "that are dug, grown, hunted or found - and where.",
                detail.x, detail.y + 10, detail.width, 20, GRAY);
    } else {
        if (mouse_in(detail)) ui->detail_scroll -= GetMouseWheelMove() * 60.0f;
        float far = ui->detail_height - detail.height;
        if (ui->detail_scroll > far) ui->detail_scroll = far;
        if (ui->detail_scroll < 0.0f) ui->detail_scroll = 0.0f;
        BeginScissorMode((int)detail.x, (int)detail.y, (int)detail.width, (int)detail.height);
        ui->detail_height = draw_detail(ui, detail);
        EndScissorMode();
        if (ui->detail_height > detail.height) {
            float bar = detail.height * detail.height / ui->detail_height;
            DrawRectangle((int)(detail.x + detail.width - 4), (int)(detail.y + (detail.height - bar) * ui->detail_scroll / far), 4,
                          (int)bar, edge_color);
        }
    }

    if (clicked != ITEM_NONE) look_at(ui, clicked);

    /* The name of whatever is under the mouse. */
    if (hover_label) {
        Vector2 m = GetMousePosition();
        int w = MeasureText(hover_label, 20) + 12;
        float tx = m.x + 14, ty = m.y + 18;
        if (tx + w > sw) tx = sw - w;
        DrawRectangle((int)tx, (int)ty, w, 28, (Color){ 10, 10, 14, 240 });
        DrawRectangleLines((int)tx, (int)ty, w, 28, edge_color);
        DrawText(hover_label, (int)tx + 6, (int)ty + 4, 20, RAYWHITE);
    }
    return open;
}
