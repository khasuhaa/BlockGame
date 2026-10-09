#include "dungeonui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trade.h"

#define PAD 8.0f
#define ROW 30.0f
#define NAME_WIDTH 190.0f
#define HARD_WIDTH 84.0f
#define FORGE_WIDTH 340.0f
#define GATE_ROWS 15 /* how many dungeons the gate shows at once: the rest are scrolled to */

static const Color panel_color = { 28, 30, 38, 240 };
static const Color edge_color = { 120, 122, 134, 255 };
static const Color button_color = { 54, 57, 70, 255 }, button_hot = { 80, 84, 100, 255 }, button_off = { 40, 42, 50, 255 };

static Color grade_color(int grade)
{
    const unsigned char *c = content.rules.grade_colors[grade > 0 && grade < MAX_GRADES ? grade : 0];
    if (grade <= 0 || grade >= MAX_GRADES || c[0] + c[1] + c[2] == 0) return RAYWHITE;
    return (Color){ c[0], c[1], c[2], 255 };
}

/* A number as it is said: 1,500 as 1.5K, 2,400,000 as 2.4M. */
static void short_number(long long n, char *out, size_t size)
{
    if (n >= 1000000000000000LL) snprintf(out, size, "%.1fQ", (double)n / 1.0e15);
    else if (n >= 1000000000000LL) snprintf(out, size, "%.1fT", (double)n / 1.0e12);
    else
    if (n >= 1000000000L) snprintf(out, size, "%.1fB", (double)n / 1.0e9);
    else if (n >= 1000000L) snprintf(out, size, "%.1fM", (double)n / 1.0e6);
    else if (n >= 10000L) snprintf(out, size, "%.1fK", (double)n / 1.0e3);
    else snprintf(out, size, "%lld", n);
}

static Rectangle anchored(const Game *g, const View *v, float w, float h)
{
    Vector2 tile = view_tile_pos(v, (float)g->bench_pos.x, (float)g->bench_pos.y);
    float x = tile.x + v->tile_size + 14.0f, y = tile.y + v->tile_size - h;
    if (x > (float)GetScreenWidth() - w - 4.0f) x = (float)GetScreenWidth() - w - 4.0f;
    if (y < 4.0f) y = 4.0f;
    return (Rectangle){ x, y, w, h };
}

static Rectangle gate_rect(const Game *g, const View *v)
{
    return anchored(g, v, NAME_WIDTH + (float)content.rules.difficulty_count * (HARD_WIDTH + 4.0f) + 2 * PAD, (float)(content.dungeon_count < GATE_ROWS ? content.dungeon_count : GATE_ROWS) * ROW + 2 * PAD);
}

/* The dungeons in the order of their tiers: the n-th of them. */
static int nth_dungeon(int n)
{
    int order[MAX_DUNGEONS];
    for (int i = 0; i < content.dungeon_count; i++) order[i] = i;
    for (int i = 1; i < content.dungeon_count; i++)
        for (int k = i; k > 0 && content.dungeons[order[k]].tier < content.dungeons[order[k - 1]].tier; k--) {
            int t = order[k];
            order[k] = order[k - 1];
            order[k - 1] = t;
        }
    return order[n];
}

static Rectangle hard_rect(Rectangle panel, int row, int hard, float scroll)
{
    return (Rectangle){ panel.x + PAD + NAME_WIDTH + (float)hard * (HARD_WIDTH + 4.0f), panel.y + PAD + (float)row * ROW + 2.0f - scroll, HARD_WIDTH, ROW - 4.0f };
}

/* The part of the gate's menu its rows are in: one outside it is not seen and cannot be clicked. */
static Rectangle gate_list(Rectangle panel)
{
    return (Rectangle){ panel.x, panel.y + PAD, panel.width, panel.height - 2 * PAD };
}

static Rectangle forge_rect(const Game *g, const View *v)
{
    return anchored(g, v, FORGE_WIDTH, 216.0f);
}

/* The forge's three buttons: one go at upgrading, all of them, and selling. */
static Rectangle forge_button(Rectangle panel, int i)
{
    return (Rectangle){ panel.x + PAD, panel.y + 112.0f + (float)i * 32.0f, 112.0f, 28.0f };
}

bool dungeonui_mouse_over(const DungeonUi *ui, const Game *g, const View *v)
{
    Vector2 m = GetMousePosition();
    if (game_gate_open_here(g)) return ui->pressed || CheckCollisionPointRec(m, gate_rect(g, v));
    if (game_forge_open(g)) return ui->pressed || CheckCollisionPointRec(m, forge_rect(g, v));
    return false;
}

bool dungeonui_handle_input(DungeonUi *ui, Game *g, const View *v)
{
    bool gate = game_gate_open_here(g), forge = game_forge_open(g);
    if (!gate && !forge) {
        ui->pressed = false;
        return false;
    }
    Vector2 m = GetMousePosition();
    Rectangle panel = gate ? gate_rect(g, v) : forge_rect(g, v);
    /* The wheel moves the gate's list (by as much as it is turned, however little). */
    float wheel = GetMouseWheelMove(), most = (float)(content.dungeon_count - GATE_ROWS) * ROW;
    if (gate && wheel != 0.0f && CheckCollisionPointRec(m, panel)) ui->scroll -= wheel * ROW * 2.0f;
    if (ui->scroll > most) ui->scroll = most;
    if (ui->scroll < 0.0f) ui->scroll = 0.0f;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ui->pressed = CheckCollisionPointRec(m, panel);
    if (!ui->pressed) return gate && wheel != 0.0f && CheckCollisionPointRec(m, panel);
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return true;
    ui->pressed = false;
    if (gate) {
        int level = game_gate_level(g);
        for (int row = 0; row < content.dungeon_count; row++)
            for (int hard = 0; hard < content.rules.difficulty_count; hard++)
                if (hard >= content.dungeons[nth_dungeon(row)].first_hard && content.dungeons[nth_dungeon(row)].tier <= level && CheckCollisionPointRec(m, gate_list(panel)) && CheckCollisionPointRec(m, hard_rect(panel, row, hard, ui->scroll))) {
                    game_dungeon_enter(g, nth_dungeon(row), hard);
                    return true;
                }
        return true;
    }
    if (CheckCollisionPointRec(m, forge_button(panel, 0))) game_upgrade(g, 1);
    else if (CheckCollisionPointRec(m, forge_button(panel, 1))) game_upgrade(g, 1000);
    else if (CheckCollisionPointRec(m, forge_button(panel, 2))) game_sell_rolled(g);
    return true;
}

static void draw_button(Rectangle r, const char *text, bool enabled)
{
    bool hot = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, !enabled ? button_off : hot ? button_hot : button_color);
    DrawText(text, (int)(r.x + (r.width - (float)MeasureText(text, 14)) / 2.0f), (int)(r.y + (r.height - 14.0f) / 2.0f), 14, enabled ? RAYWHITE : GRAY);
}

/* What a rolled thing has, a line to each: how many lines. */
static int roll_lines(const Game *g, const ItemStack *s, char lines[][64], int max)
{
    Roll r = game_roll(&g->inventory, s);
    const ItemDef *def = item_def((ItemId)s->id);
    char n[24];
    int count = 0;
    if (!r.rolled) return 0;
    if (def->grade > 0 && content.rules.grade_labels[def->grade][0] && count < max)
        snprintf(lines[count++], 64, "%s%s", content.rules.grade_labels[def->grade], def->loot_class == 1 ? " - Warrior" : def->loot_class == 2 ? " - Mage" : "");
    if (r.power > 0 && count < max) {
        short_number(r.power, n, sizeof n);
        snprintf(lines[count++], 64, "Power %s", n);
    }
    if (r.health > 0 && count < max) {
        short_number(r.health, n, sizeof n);
        snprintf(lines[count++], 64, "Health %s", n);
    }
    if (r.ups > 0 && count < max) {
        char of[24];
        short_number(r.done, n, sizeof n);
        short_number(r.ups, of, sizeof of);
        snprintf(lines[count++], 64, "Upgrades %s / %s", n, of);
    }
    if (r.fallen && count < max) snprintf(lines[count++], 64, "Roll %d%% / %d%%", r.base_percent, r.ups_percent);
    return count;
}

void dungeonui_draw(const DungeonUi *ui, const Game *g, const View *v)
{
    if (game_gate_open_here(g)) {
        Rectangle panel = gate_rect(g, v);
        int level = game_gate_level(g);
        DrawRectangleRec(panel, panel_color);
        DrawRectangleLinesEx(panel, 2.0f, edge_color);
        Rectangle list = gate_list(panel);
        BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
        for (int row = 0; row < content.dungeon_count; row++) {
            const DungeonDef *d = &content.dungeons[nth_dungeon(row)];
            bool open = d->tier <= level;
            float y = panel.y + PAD + (float)row * ROW - ui->scroll;
            if (y + ROW < list.y || y > list.y + list.height) continue;
            DrawText(d->label, (int)(panel.x + PAD), (int)(y + 7.0f), 16, open ? RAYWHITE : GRAY);
            for (int hard = d->first_hard; hard < content.rules.difficulty_count; hard++) draw_button(hard_rect(panel, row, hard, ui->scroll), content.rules.difficulties[hard].label, open);
        }
        EndScissorMode();
        /* (Where there is more of the list than is shown: how far down it is.) */
        if (content.dungeon_count > GATE_ROWS) {
            float whole = (float)content.dungeon_count * ROW, shown = list.height;
            DrawRectangleRec((Rectangle){ panel.x + panel.width - 5.0f, list.y + ui->scroll / whole * shown, 3.0f, shown * shown / whole }, edge_color);
        }
        return;
    }
    if (!game_forge_open(g)) return;
    Rectangle panel = forge_rect(g, v);
    DrawRectangleRec(panel, panel_color);
    DrawRectangleLinesEx(panel, 2.0f, edge_color);
    const ItemStack *s = inventory_selected(&g->inventory);
    Roll r = game_roll(&g->inventory, s);
    if (!r.rolled) {
        DrawText("Forge", (int)(panel.x + PAD), (int)(panel.y + PAD), 18, GRAY);
        for (int i = 0; i < 3; i++) draw_button(forge_button(panel, i), i == 0 ? "Upgrade" : i == 1 ? "Upgrade all" : "Sell", false);
        return;
    }
    const ItemDef *def = item_def((ItemId)s->id);
    char lines[6][64], price[96];
    int count = roll_lines(g, s, lines, 6);
    DrawText(def->label, (int)(panel.x + PAD), (int)(panel.y + PAD), 18, grade_color(def->grade));
    for (int i = 0; i < count && i < 5; i++) DrawText(lines[i], (int)(panel.x + PAD), (int)(panel.y + PAD + 24.0f + (float)i * 16.0f), 14, (Color){ 200, 204, 216, 255 });
    long wallet = trade_wallet(&g->inventory), one = game_upgrade_cost(g, s, 1), all = game_upgrade_cost(g, s, 1000);
    for (int i = 0; i < 3; i++) {
        Rectangle b = forge_button(panel, i);
        long sum = i == 0 ? one : i == 1 ? all : game_rolled_worth(g, s);
        bool can = i == 2 || (r.step < r.steps && wallet >= one);
        draw_button(b, i == 0 ? "Upgrade" : i == 1 ? "Upgrade all" : "Sell", can);
        if (i < 2 && r.step >= r.steps) continue;
        trade_price_text(sum, price, sizeof price);
        DrawText(price, (int)(b.x + b.width + 8.0f), (int)(b.y + 7.0f), 14, i == 2 ? (Color){ 140, 226, 140, 255 } : wallet >= sum ? RAYWHITE : (Color){ 226, 140, 140, 255 });
    }
}

void dungeonui_draw_over(const Game *g, const Hud *hud)
{
    /* What the thing under the cursor has. */
    Vector2 m = GetMousePosition();
    const ItemStack *s = hud_stack_at(hud, &g->inventory, m);
    if (s && (item_def((ItemId)s->id)->rolled || item_def((ItemId)s->id)->cast_cooldown > 0.0f)) {
        const ItemDef *def = item_def((ItemId)s->id);
        char lines[6][64];
        int count = roll_lines(g, s, lines, 6);
        if (def->cast_cooldown > 0.0f) {
            /* A spell: whose it is, how long between castings, and what it does with what its caster carries now. */
            char n[24];
            short_number((long long)((double)game_best_power(g, def->loot_class) * (double)def->cast_scales + 0.5), n, sizeof n);
            snprintf(lines[count++], 64, "Spell%s", def->loot_class == 1 ? " - Warrior" : def->loot_class == 2 ? " - Mage" : "");
            snprintf(lines[count++], 64, "Cooldown %gs", (double)def->cast_cooldown);
            if (def->cast_scales > 0.0f) snprintf(lines[count++], 64, "Power %s", n);
        }
        float wide = (float)MeasureText(def->label, 18), tall = 30.0f + (float)count * 16.0f;
        for (int i = 0; i < count; i++)
            if ((float)MeasureText(lines[i], 14) > wide) wide = (float)MeasureText(lines[i], 14);
        Rectangle box = { m.x + 14.0f, m.y - tall - 10.0f, wide + 16.0f, tall + 6.0f };
        if (box.x + box.width > (float)GetScreenWidth() - 4.0f) box.x = (float)GetScreenWidth() - 4.0f - box.width;
        if (box.y < 4.0f) box.y = 4.0f;
        DrawRectangleRec(box, panel_color);
        DrawRectangleLinesEx(box, 2.0f, grade_color(def->grade));
        DrawText(def->label, (int)(box.x + 8.0f), (int)(box.y + 6.0f), 18, grade_color(def->grade));
        for (int i = 0; i < count; i++) DrawText(lines[i], (int)(box.x + 8.0f), (int)(box.y + 28.0f + (float)i * 16.0f), 14, (Color){ 200, 204, 216, 255 });
    }
    /* A spell that is waiting: its slot is shaded as far as it has to go, with the seconds left. */
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        const ItemStack *st = &g->inventory.slots[i];
        int wait = st->count > 0 ? game_spell_wait(g, (ItemId)st->id) : 0, whole = st->count > 0 ? game_spell_cooldown((ItemId)st->id) : 0;
        if (wait <= 0 || whole <= 0) continue;
        Rectangle r = hud_bar_rect(i);
        float share = (float)wait / (float)whole;
        char left[16];
        DrawRectangleRec((Rectangle){ r.x, r.y + r.height * (1.0f - share), r.width, r.height * share }, (Color){ 10, 12, 20, 170 });
        snprintf(left, sizeof left, "%.1f", (double)wait / (double)(content.rules.tick_rate > 0 ? content.rules.tick_rate : 10));
        DrawText(left, (int)(r.x + (r.width - (float)MeasureText(left, 16)) / 2.0f), (int)(r.y + r.height / 2.0f - 8.0f), 16, RAYWHITE);
    }
    /* Where blows are numbers: what the character can take, and the health of whoever the place belongs to. */
    int hard = 0;
    if (!game_alive(g) || world_dungeon_run(&g->world, g->player.feet.y, &hard) < 0) return;
    float times = 1.0f;
    game_dungeon_hard(world_dungeon_run(&g->world, g->player.feet.y, NULL), hard, &times, NULL, NULL, NULL);
    char a[24], b[24], text[96];
    short_number((long long)((double)g->vitals.health * (double)game_max_health(g) + 0.5), a, sizeof a);
    short_number(game_max_health(g), b, sizeof b);
    snprintf(text, sizeof text, "%s / %s", a, b);
    DrawText(text, 70, 176, 16, RAYWHITE);
    for (int i = 0; i < MAX_MOBS; i++) {
        const Mob *mob = &g->mobs[i];
        if (!mob->active || !content.mobs[mob->kind].boss || abs(world_dx(&g->world, g->player.feet.x, mob->x)) > 40 || abs(mob->y - g->player.feet.y) > 20) continue;
        float full = (float)content.mobs[mob->kind].health * times, share = full > 0.0f ? (float)mob->hp / full : 0.0f;
        Rectangle bar = { (float)GetScreenWidth() / 2.0f - 260.0f, 54.0f, 520.0f, 18.0f };
        DrawRectangleRec((Rectangle){ bar.x - 3, bar.y - 3, bar.width + 6, bar.height + 6 }, panel_color);
        DrawRectangleRec((Rectangle){ bar.x, bar.y, bar.width * (share > 1.0f ? 1.0f : share < 0.0f ? 0.0f : share), bar.height }, (Color){ 196, 60, 60, 255 });
        DrawRectangleLinesEx((Rectangle){ bar.x - 3, bar.y - 3, bar.width + 6, bar.height + 6 }, 2.0f, edge_color);
        short_number((long long)mob->hp * (content.mobs[mob->kind].health_unit > 1 ? content.mobs[mob->kind].health_unit : 1), a, sizeof a);
        snprintf(text, sizeof text, "%s  %s", content.mobs[mob->kind].label, a);
        DrawText(text, (int)((float)GetScreenWidth() / 2.0f - (float)MeasureText(text, 16) / 2.0f), (int)bar.y + 1, 16, RAYWHITE);
        break;
    }
}
