#include "tradeui.h"

#include "assets.h"
#include "trade.h"

#define SIDE 150.0f /* the list of categories down the left */
#define PANEL_WIDTH (404.0f + SIDE)
#define PANEL_HEIGHT 364.0f
#define PAD 10.0f
#define HEADER 32.0f
#define TAB_HEIGHT 24.0f
#define CELL 48.0f
#define CELL_GAP 4.0f
#define GRID_COLUMNS 7
#define GRID_ROWS 4
#define ROW_HEIGHT 40.0f
#define MAX_LISTED 1024

static const Color panel_color = { 28, 30, 38, 240 };
static const Color edge_color = { 120, 122, 134, 255 };
static const Color button_color = { 70, 130, 80, 255 };
static const Color button_hot = { 90, 160, 100, 255 };
static const Color button_off = { 54, 57, 70, 255 };
static const Color cell_color = { 44, 47, 58, 255 };
static const Color unknown_color = { 34, 36, 45, 255 };

/* ---- layout ------------------------------------------------------------ */

static Rectangle panel_rect(const Game *g, const View *v)
{
    Vector2 tile = view_tile_pos(v, (float)g->bench_pos.x, (float)g->bench_pos.y);
    float x = tile.x + v->tile_size + 14.0f, edge = (float)GetScreenWidth() - PANEL_WIDTH - 4.0f;
    /* (Kept on the screen: it is wide, and the block may be near the edge.) */
    if (x > edge) x = edge;
    return (Rectangle){ x, tile.y + v->tile_size - PANEL_HEIGHT, PANEL_WIDTH, PANEL_HEIGHT };
}

/* The tabs: one for each category, then converting coins, then raising the block itself. */
static int tab_count(void)
{
    return content.trade.category_count + 2;
}

static const TileDef *open_block(const Game *g)
{
    return tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y));
}

static int listed_items(const Game *g, int category, ItemId *out, bool *found);

/* Whether a tab has anything to it at the block that is open: a category
 * nothing of which is dealt in there - benches, at an ordinary trade portal - is left out. */
static bool tab_shown(const Game *g, int tab)
{
    static ItemId scratch[MAX_LISTED];
    if (tab > content.trade.category_count) return open_block(g)->upgrade_to != TILE_AIR;
    if (tab == content.trade.category_count) return !game_trade_free(g);
    return listed_items(g, tab, scratch, NULL) > 0;
}

/* Which place down the list a tab has, counting only the ones shown; -1 if it is not. */
static int tab_place(const Game *g, int tab)
{
    int place = 0;
    if (!tab_shown(g, tab)) return -1;
    for (int i = 0; i < tab; i++) place += tab_shown(g, i);
    return place;
}

static const char *tab_label(int tab)
{
    return tab < content.trade.category_count ? content.trade.categories[tab].label : tab == content.trade.category_count ? "Convert" : "Upgrade";
}

/* The categories are a list down the left side, one to a row, each with room for its whole name. */
static Rectangle tab_rect(const Game *g, Rectangle panel, int tab)
{
    int place = tab_place(g, tab), shown = 0;
    for (int i = 0; i < tab_count(); i++) shown += tab_shown(g, i);
    float room = panel.height - HEADER - PAD, each = room / (float)(shown > 0 ? shown : 1);
    if (each > TAB_HEIGHT + 4.0f) each = TAB_HEIGHT + 4.0f;
    if (place < 0) return (Rectangle){ 0, 0, 0, 0 };
    return (Rectangle){ panel.x + PAD, panel.y + HEADER + (float)place * each, SIDE - PAD, each - 3.0f };
}

/* Where everything but that list goes: the rest of the panel, to the right of it. */
static float left_edge(Rectangle panel)
{
    return panel.x + PAD + SIDE;
}

static Rectangle grid_rect(Rectangle panel)
{
    return (Rectangle){ left_edge(panel), panel.y + HEADER,
                        GRID_COLUMNS * (CELL + CELL_GAP) - CELL_GAP, GRID_ROWS * (CELL + CELL_GAP) };
}

/* The four buttons under the chosen item: buy 1, buy 10, sell 1, sell 10. */
static Rectangle deal_rect(Rectangle panel, int i)
{
    float w = (panel.x + panel.width - PAD - left_edge(panel) - 3 * 6.0f) / 4.0f;
    return (Rectangle){ left_edge(panel) + i * (w + 6.0f), panel.y + panel.height - PAD - 30, w, 30 };
}

/* One exchange on the convert tab: `from` and `to` are currencies. */
typedef struct {
    int from, to, give, get;
} Exchange;

static int exchanges(Exchange *out)
{
    const TradeDef *t = &content.trade;
    int n = 0;
    for (int i = 0; i + 1 < t->currency_count; i++) {
        int rate = t->currencies[i + 1].value / t->currencies[i].value;
        out[n++] = (Exchange){ i, i + 1, rate, 1 };
        out[n++] = (Exchange){ i + 1, i, 1, rate };
    }
    return n;
}

static Rectangle exchange_rect(Rectangle panel, int i)
{
    return (Rectangle){ left_edge(panel), panel.y + HEADER + 4 + i * (ROW_HEIGHT + 6),
                        panel.x + panel.width - PAD - left_edge(panel), ROW_HEIGHT };
}

static int listed_items(const Game *g, int category, ItemId *out, bool *found)
{
    return game_trade_listing(g, category, out, found, MAX_LISTED);
}

static Rectangle upgrade_rect(Rectangle panel)
{
    return (Rectangle){ left_edge(panel), panel.y + panel.height - PAD - 34, panel.x + panel.width - PAD - left_edge(panel), 34 };
}

static float max_scroll(int count)
{
    int rows = (count + GRID_COLUMNS - 1) / GRID_COLUMNS;
    return rows > GRID_ROWS ? (rows - GRID_ROWS) * (CELL + CELL_GAP) : 0.0f;
}

static Rectangle cell_rect(Rectangle grid, int i, float scroll)
{
    return (Rectangle){ grid.x + (i % GRID_COLUMNS) * (CELL + CELL_GAP),
                        grid.y + (i / GRID_COLUMNS) * (CELL + CELL_GAP) - scroll, CELL, CELL };
}

/* ---- input ------------------------------------------------------------- */

bool tradeui_mouse_over(const TradeUi *ui, const Game *g, const View *v)
{
    if (!game_trade_open(g)) return false;
    return ui->pressed || CheckCollisionPointRec(GetMousePosition(), panel_rect(g, v));
}

bool tradeui_handle_input(TradeUi *ui, Game *g, const View *v)
{
    if (!game_trade_open(g)) {
        ui->was_open = false;
        ui->pressed = false;
        return false;
    }
    if (!ui->was_open || ui->shown.x != g->bench_pos.x || ui->shown.y != g->bench_pos.y) {
        *ui = (TradeUi){ .was_open = true, .shown = g->bench_pos };
    }

    Vector2 m = GetMousePosition();
    Rectangle panel = panel_rect(g, v), grid = grid_rect(panel);
    bool inside = CheckCollisionPointRec(m, panel);
    /* (A tab that this block has nothing under gives way to the first that it has.) */
    for (int i = 0; !tab_shown(g, ui->tab) && i < tab_count(); i++)
        if (tab_shown(g, i)) ui->tab = i;
    bool on_items = ui->tab < content.trade.category_count;
    ItemId items[MAX_LISTED];
    bool found[MAX_LISTED];
    int count = on_items ? listed_items(g, ui->tab, items, found) : 0;

    if (inside) ui->scroll -= GetMouseWheelMove() * (CELL + CELL_GAP);
    if (ui->scroll > max_scroll(count)) ui->scroll = max_scroll(count);
    if (ui->scroll < 0.0f) ui->scroll = 0.0f;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ui->pressed = inside;
    if (!ui->pressed) return false;
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return true;
    ui->pressed = false;

    for (int i = 0; i < tab_count(); i++) {
        if (!tab_shown(g, i) || !CheckCollisionPointRec(m, tab_rect(g, panel, i))) continue;
        ui->tab = i;
        ui->selected = ITEM_NONE;
        ui->scroll = 0.0f;
        return true;
    }
    if (ui->tab > content.trade.category_count) {
        if (CheckCollisionPointRec(m, upgrade_rect(panel))) game_upgrade_bench(g);
        return true;
    }
    if (!on_items) {
        Exchange ex[2 * MAX_CURRENCIES];
        int n = exchanges(ex);
        for (int i = 0; i < n; i++)
            if (CheckCollisionPointRec(m, exchange_rect(panel, i))) game_trade_convert(g, ex[i].from, ex[i].to);
        return true;
    }
    if (CheckCollisionPointRec(m, grid)) {
        for (int i = 0; i < count; i++)
            if (found[i] && CheckCollisionPointRec(m, cell_rect(grid, i, ui->scroll))) ui->selected = items[i];
        return true;
    }
    if (ui->selected != ITEM_NONE) {
        static const int amounts[4] = { 1, 10, 1, 10 };
        for (int i = 0; i < 4; i++) {
            if (!CheckCollisionPointRec(m, deal_rect(panel, i))) continue;
            if (i < 2) game_trade_buy(g, ui->selected, amounts[i]);
            else game_trade_sell(g, ui->selected, amounts[i]);
        }
    }
    return true;
}

/* ---- drawing ----------------------------------------------------------- */

static void draw_centered(const char *text, Rectangle r, int size, Color c)
{
    DrawText(text, (int)(r.x + (r.width - MeasureText(text, size)) / 2),
             (int)(r.y + (r.height - size) / 2), size, c);
}

/* A count and a coin, right-aligned to end at `right`. Returns where it starts. */
static float draw_coins(ItemId coin, int count, float right, float y, float icon, int size, Color c)
{
    const char *n = TextFormat("%d", count);
    float x = right - icon;
    assets_draw_item(coin, (Rectangle){ x, y, icon, icon });
    x -= (float)MeasureText(n, size) + 1;
    DrawText(n, (int)x, (int)(y + (icon - size) / 2), size, c);
    return x;
}

static void draw_button(Rectangle r, const char *text, bool enabled)
{
    bool hot = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, !enabled ? button_off : hot ? button_hot : button_color);
    draw_centered(text, r, 14, enabled ? RAYWHITE : GRAY);
}

static void draw_items(const TradeUi *ui, const Game *g, Rectangle panel)
{
    ItemId items[MAX_LISTED];
    bool found[MAX_LISTED];
    int count = listed_items(g, ui->tab, items, found);
    Rectangle grid = grid_rect(panel);

    BeginScissorMode((int)grid.x, (int)grid.y, (int)grid.width + 1, (int)grid.height);
    for (int i = 0; i < count; i++) {
        Rectangle cell = cell_rect(grid, i, ui->scroll);
        if (cell.y + cell.height < grid.y || cell.y > grid.y + grid.height) continue;
        bool chosen = items[i] == ui->selected;
        /* Every place says what tier its thing is, found or not: gold where this block has yet to be raised to it. */
        static const char *numerals[11] = { "", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X" };
        int tier = item_def(items[i])->trade_tier, level = game_trade_level(g);
        const char *numeral = tier >= 1 && tier <= 10 ? numerals[tier] : "";
        bool beyond = level > 0 && tier > level;
        Color tier_color = beyond ? (Color){ 226, 196, 102, 255 } : (Color){ 150, 154, 170, 255 };
        if (!found[i]) {
            DrawRectangleRec(cell, unknown_color);
            DrawRectangleLinesEx(cell, 2.0f, button_off);
            if (g->discovered[items[i]] && beyond) {
                /* Found, but more than this block deals in until it is raised: the thing itself, dimmed. */
                assets_draw_item_tones(items[i], cell, 0);
                DrawRectangleRec(cell, Fade(unknown_color, 0.72f));
            } else {
                /* Not found in this world yet: its place is kept, and says no more than its tier. */
                draw_centered("?", cell, 30, (Color){ 96, 100, 116, 255 });
            }
        } else {
            DrawRectangleRec(cell, cell_color);
            DrawRectangleLinesEx(cell, chosen ? 3.0f : 2.0f, chosen ? RAYWHITE : edge_color);
            assets_draw_item_tones(items[i], cell, 0);
        }
        /* (On a dark patch of its own, so that it reads over whatever is drawn there.) */
        int wide = MeasureText(numeral, 10);
        DrawRectangle((int)(cell.x + cell.width) - wide - 6, (int)(cell.y + cell.height) - 14, wide + 4, 12, Fade(unknown_color, 0.85f));
        DrawText(numeral, (int)(cell.x + cell.width) - wide - 4, (int)(cell.y + cell.height) - 13, 10, tier_color);
    }
    EndScissorMode();
    if (max_scroll(count) > 0.0f) {
        /* Where the view is in the list. */
        float h = grid.height * grid.height / (grid.height + max_scroll(count));
        float y = grid.y + (grid.height - h) * ui->scroll / max_scroll(count);
        DrawRectangleRec((Rectangle){ grid.x + grid.width + 4, y, 3, h }, edge_color);
    }

    float y = grid.y + grid.height + 6, left = left_edge(panel), right = panel.x + panel.width - PAD;
    bool free = game_trade_free(g);
    if (ui->selected == ITEM_NONE || !game_trade_deals(g, ui->selected)) {
        Rectangle note = { left, y, right - left, panel.y + panel.height - y };
        draw_centered("Pick an item to buy or sell", note, 16, LIGHTGRAY);
        return;
    }
    const TradeDef *t = &content.trade;
    int price = trade_price(ui->selected), back = trade_sell_price(ui->selected), held = inventory_count(&g->inventory, ui->selected);
    long wallet = trade_wallet(&g->inventory);
    assets_draw_item_tones(ui->selected, (Rectangle){ left, y, 40, 40 }, 0);
    DrawText(item_def(ui->selected)->label, (int)(left + 46), (int)y + 3, 16, RAYWHITE);
    DrawText(TextFormat("You have %d", held), (int)(left + 46), (int)y + 23, 14, LIGHTGRAY);
    if (free) {
        DrawText("Free", (int)right - MeasureText("Free", 16), (int)y + 12, 16, (Color){ 140, 226, 140, 255 });
    } else {
        /* What it costs, and under that what it fetches. */
        float x = draw_coins(t->currencies[0].item, price / t->currencies[0].value, right, y, 20, 14, RAYWHITE);
        DrawText("Buy", (int)x - MeasureText("Buy", 12) - 6, (int)y + 4, 12, LIGHTGRAY);
        x = draw_coins(t->currencies[0].item, back / t->currencies[0].value, right, y + 21, 20, 14, RAYWHITE);
        DrawText("Sell", (int)x - MeasureText("Sell", 12) - 6, (int)y + 25, 12, LIGHTGRAY);
    }

    draw_button(deal_rect(panel, 0), "Buy 1", free || wallet >= price);
    draw_button(deal_rect(panel, 1), "Buy 10", free || wallet >= (long)price * 10);
    draw_button(deal_rect(panel, 2), "Sell 1", held >= 1 && (free || back > 0));
    draw_button(deal_rect(panel, 3), "Sell 10", held >= 10 && (free || back > 0));
}

static void draw_exchanges(const Game *g, Rectangle panel)
{
    const TradeDef *t = &content.trade;
    Exchange ex[2 * MAX_CURRENCIES];
    int n = exchanges(ex);
    if (n == 0) draw_centered("There is only one kind of coin", (Rectangle){ left_edge(panel), panel.y, panel.x + panel.width - left_edge(panel), panel.height }, 16, LIGHTGRAY);
    for (int i = 0; i < n; i++) {
        Rectangle row = exchange_rect(panel, i);
        bool can = inventory_count(&g->inventory, t->currencies[ex[i].from].item) >= ex[i].give;
        bool hot = can && CheckCollisionPointRec(GetMousePosition(), row);
        DrawRectangleRec(row, hot ? (Color){ 80, 84, 100, 255 } : cell_color);
        /* "100 copper > 1 gold", with the coins drawn rather than named. */
        float mid = row.x + row.width * 0.36f, icon = 30, y = row.y + (row.height - icon) / 2;
        draw_coins(t->currencies[ex[i].from].item, ex[i].give, mid, y, icon, 16, can ? RAYWHITE : GRAY);
        DrawText(">", (int)mid + 10, (int)row.y + 10, 20, LIGHTGRAY);
        draw_coins(t->currencies[ex[i].to].item, ex[i].get, mid + 100, y, icon, 16, can ? RAYWHITE : GRAY);
        Rectangle b = { row.x + row.width - 96, row.y + 5, 90, row.height - 10 };
        DrawRectangleRec(b, !can ? button_off : hot ? button_hot : button_color);
        draw_centered("Convert", b, 14, can ? RAYWHITE : GRAY);
    }
}

/* What raising the block takes, with how many of each are held, and the button that does it. */
static void draw_upgrade(const Game *g, Rectangle panel)
{
    const TileDef *def = open_block(g);
    float left = left_edge(panel), right = panel.x + panel.width - PAD, y = panel.y + HEADER + 4;
    bool can = g->crystals >= def->upgrade_crystals;
    DrawText(tile_def(def->upgrade_to)->label, (int)left, (int)y, 16, RAYWHITE);
    if (tile_def(def->upgrade_to)->trade_level > 0)
        DrawText(TextFormat("Tier %d", tile_def(def->upgrade_to)->trade_level), (int)right - MeasureText(TextFormat("Tier %d", tile_def(def->upgrade_to)->trade_level), 16), (int)y, 16, LIGHTGRAY);
    y += 30;
    for (int i = 0; i <= def->upgrade_cost_count; i++) {
        bool crystals = i == def->upgrade_cost_count;
        if (crystals && def->upgrade_crystals <= 0) break;
        int need = crystals ? def->upgrade_crystals : def->upgrade_cost[i].count;
        int have = crystals ? g->crystals : inventory_count(&g->inventory, def->upgrade_cost[i].id);
        Rectangle row = { left, y, right - left, ROW_HEIGHT };
        DrawRectangleRec(row, cell_color);
        if (crystals) assets_draw_image(content.trade.crystal_icon, (Rectangle){ row.x + 6, row.y + 5, 30, 30 });
        else assets_draw_item((ItemId)def->upgrade_cost[i].id, (Rectangle){ row.x + 6, row.y + 5, 30, 30 });
        DrawText(crystals ? "Crystals" : item_def((ItemId)def->upgrade_cost[i].id)->label, (int)row.x + 44, (int)row.y + 12, 16, RAYWHITE);
        const char *n = TextFormat("%d / %d", have, need);
        DrawText(n, (int)(row.x + row.width - 8 - MeasureText(n, 16)), (int)row.y + 12, 16, have >= need ? (Color){ 140, 226, 140, 255 } : (Color){ 226, 120, 120, 255 });
        can = can && have >= need;
        y += ROW_HEIGHT + 6;
    }
    draw_button(upgrade_rect(panel), "Upgrade", can);
}

void tradeui_draw(const TradeUi *ui, const Game *g, const View *v)
{
    if (!game_trade_open(g)) return;
    const TradeDef *t = &content.trade;
    Rectangle panel = panel_rect(g, v);
    float tile_y = view_tile_pos(v, 0.0f, (float)g->bench_pos.y + 0.5f).y;
    /* Until input has caught up with a newly opened block, show the first tab. */
    bool current = ui->was_open && ui->shown.x == g->bench_pos.x && ui->shown.y == g->bench_pos.y;
    TradeUi fresh = { 0 };
    if (!current) ui = &fresh;
    if (!tab_shown(g, ui->tab)) {
        fresh = *ui;
        for (int i = tab_count() - 1; i >= 0; i--)
            if (tab_shown(g, i)) fresh.tab = i;
        ui = &fresh;
    }

    DrawTriangle((Vector2){ panel.x, tile_y + 9 }, (Vector2){ panel.x, tile_y - 9 },
                 (Vector2){ panel.x - 12, tile_y }, edge_color);
    DrawRectangleRec(panel, panel_color);
    DrawRectangleLinesEx(panel, 2.0f, edge_color);

    /* The block's name, and the coins held, biggest first from the left. */
    DrawText(tile_def(game_bench_at(g, g->bench_pos.x, g->bench_pos.y))->label, (int)(panel.x + PAD),
             (int)panel.y + 9, 16, RAYWHITE);
    float right = panel.x + panel.width - PAD;
    for (int i = 0; i < t->currency_count; i++)
        right = draw_coins(t->currencies[i].item, inventory_count(&g->inventory, t->currencies[i].item),
                           right, panel.y + 5, 22, 14, RAYWHITE) - 10;

    for (int i = 0; i < tab_count(); i++) {
        if (!tab_shown(g, i)) continue;
        Rectangle tab = tab_rect(g, panel, i);
        bool hot = CheckCollisionPointRec(GetMousePosition(), tab);
        DrawRectangleRec(tab, i == ui->tab ? edge_color : hot ? (Color){ 80, 84, 100, 255 } : cell_color);
        DrawText(tab_label(i), (int)tab.x + 8, (int)(tab.y + (tab.height - 14) / 2), 14, i == ui->tab ? BLACK : RAYWHITE);
    }

    if (ui->tab < t->category_count) draw_items(ui, g, panel);
    else if (ui->tab == t->category_count) draw_exchanges(g, panel);
    else draw_upgrade(g, panel);
}
