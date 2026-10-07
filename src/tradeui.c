#include "tradeui.h"

#include "assets.h"
#include "trade.h"

#define PANEL_WIDTH 404.0f
#define PANEL_HEIGHT 356.0f
#define PAD 10.0f
#define HEADER 32.0f
#define TAB_HEIGHT 24.0f
#define CELL 48.0f
#define CELL_GAP 4.0f
#define GRID_COLUMNS 7
#define GRID_ROWS 3
#define ROW_HEIGHT 40.0f
#define MAX_LISTED 256

static const Color panel_color = { 28, 30, 38, 240 };
static const Color edge_color = { 120, 122, 134, 255 };
static const Color button_color = { 70, 130, 80, 255 };
static const Color button_hot = { 90, 160, 100, 255 };
static const Color button_off = { 54, 57, 70, 255 };
static const Color cell_color = { 44, 47, 58, 255 };

/* ---- layout ------------------------------------------------------------ */

static Rectangle panel_rect(const Game *g, const View *v)
{
    Vector2 tile = view_tile_pos(v, (float)g->bench_pos.x, (float)g->bench_pos.y);
    return (Rectangle){ tile.x + v->tile_size + 14.0f, tile.y + v->tile_size - PANEL_HEIGHT,
                        PANEL_WIDTH, PANEL_HEIGHT };
}

static int tab_count(void)
{
    return content.trade.category_count + 1;
}

static const char *tab_label(int tab)
{
    return tab < content.trade.category_count ? content.trade.categories[tab].label : "Convert";
}

/* Tabs share the width of the panel in proportion to their labels. */
static Rectangle tab_rect(Rectangle panel, int tab)
{
    float total = 0.0f, before = 0.0f, room = panel.width - 2 * PAD - (tab_count() - 1) * 3.0f;
    for (int i = 0; i < tab_count(); i++) {
        float w = (float)MeasureText(tab_label(i), 12) + 10.0f;
        if (i < tab) before += w;
        total += w;
    }
    float w = ((float)MeasureText(tab_label(tab), 12) + 10.0f) * room / total;
    return (Rectangle){ panel.x + PAD + before * room / total + tab * 3.0f, panel.y + HEADER, w,
                        TAB_HEIGHT };
}

static Rectangle grid_rect(Rectangle panel)
{
    return (Rectangle){ panel.x + PAD, panel.y + HEADER + TAB_HEIGHT + 8,
                        GRID_COLUMNS * (CELL + CELL_GAP) - CELL_GAP, GRID_ROWS * (CELL + CELL_GAP) };
}

/* The four buttons under the chosen item: buy 1, buy 10, sell 1, sell 10. */
static Rectangle deal_rect(Rectangle panel, int i)
{
    float w = (panel.width - 2 * PAD - 3 * 6.0f) / 4.0f;
    return (Rectangle){ panel.x + PAD + i * (w + 6.0f), panel.y + panel.height - PAD - 30, w, 30 };
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
    return (Rectangle){ panel.x + PAD, panel.y + HEADER + TAB_HEIGHT + 12 + i * (ROW_HEIGHT + 6),
                        panel.width - 2 * PAD, ROW_HEIGHT };
}

/* The items in a category, in the order they are defined. */
static int listed_items(int category, ItemId *out)
{
    int n = 0;
    for (int i = 1; i < content.item_count && n < MAX_LISTED; i++)
        if (content.items[i].category == category && trade_price(i) > 0) out[n++] = i;
    return n;
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
    bool on_items = ui->tab < content.trade.category_count;
    ItemId items[MAX_LISTED];
    int count = on_items ? listed_items(ui->tab, items) : 0;

    if (inside) ui->scroll -= GetMouseWheelMove() * (CELL + CELL_GAP);
    if (ui->scroll > max_scroll(count)) ui->scroll = max_scroll(count);
    if (ui->scroll < 0.0f) ui->scroll = 0.0f;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ui->pressed = inside;
    if (!ui->pressed) return false;
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return true;
    ui->pressed = false;

    for (int i = 0; i < tab_count(); i++) {
        if (!CheckCollisionPointRec(m, tab_rect(panel, i))) continue;
        ui->tab = i;
        ui->selected = ITEM_NONE;
        ui->scroll = 0.0f;
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
            if (CheckCollisionPointRec(m, cell_rect(grid, i, ui->scroll))) ui->selected = items[i];
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
    int count = listed_items(ui->tab, items);
    Rectangle grid = grid_rect(panel);

    BeginScissorMode((int)grid.x, (int)grid.y, (int)grid.width + 1, (int)grid.height);
    for (int i = 0; i < count; i++) {
        Rectangle cell = cell_rect(grid, i, ui->scroll);
        if (cell.y + cell.height < grid.y || cell.y > grid.y + grid.height) continue;
        bool chosen = items[i] == ui->selected;
        DrawRectangleRec(cell, cell_color);
        DrawRectangleLinesEx(cell, chosen ? 3.0f : 2.0f, chosen ? RAYWHITE : edge_color);
        assets_draw_item_tones(items[i], cell, 0);
    }
    EndScissorMode();
    if (max_scroll(count) > 0.0f) {
        /* Where the view is in the list. */
        float h = grid.height * grid.height / (grid.height + max_scroll(count));
        float y = grid.y + (grid.height - h) * ui->scroll / max_scroll(count);
        DrawRectangleRec((Rectangle){ grid.x + grid.width + 4, y, 3, h }, edge_color);
    }

    float y = grid.y + grid.height + 6;
    if (ui->selected == ITEM_NONE) {
        draw_centered("Pick an item to buy or sell",
                      (Rectangle){ panel.x, y, panel.width, panel.y + panel.height - y }, 16, LIGHTGRAY);
        return;
    }
    const TradeDef *t = &content.trade;
    int price = trade_price(ui->selected), held = inventory_count(&g->inventory, ui->selected);
    long wallet = trade_wallet(&g->inventory);
    assets_draw_item_tones(ui->selected, (Rectangle){ panel.x + PAD, y, 40, 40 }, 0);
    DrawText(item_def(ui->selected)->label, (int)(panel.x + PAD + 46), (int)y + 3, 16, RAYWHITE);
    DrawText(TextFormat("You have %d", held), (int)(panel.x + PAD + 46), (int)y + 23, 14, LIGHTGRAY);
    float right = panel.x + panel.width - PAD;
    float x = draw_coins(t->currencies[0].item, price / t->currencies[0].value, right, y + 8, 24, 16, RAYWHITE);
    DrawText("Price", (int)x - MeasureText("Price", 14) - 8, (int)y + 13, 14, LIGHTGRAY);

    draw_button(deal_rect(panel, 0), "Buy 1", wallet >= price);
    draw_button(deal_rect(panel, 1), "Buy 10", wallet >= (long)price * 10);
    draw_button(deal_rect(panel, 2), "Sell 1", held >= 1);
    draw_button(deal_rect(panel, 3), "Sell 10", held >= 10);
}

static void draw_exchanges(const Game *g, Rectangle panel)
{
    const TradeDef *t = &content.trade;
    Exchange ex[2 * MAX_CURRENCIES];
    int n = exchanges(ex);
    if (n == 0) draw_centered("There is only one kind of coin", panel, 16, LIGHTGRAY);
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
        Rectangle tab = tab_rect(panel, i);
        bool hot = CheckCollisionPointRec(GetMousePosition(), tab);
        DrawRectangleRec(tab, i == ui->tab ? edge_color : hot ? (Color){ 80, 84, 100, 255 } : cell_color);
        draw_centered(tab_label(i), tab, 12, i == ui->tab ? BLACK : RAYWHITE);
    }

    if (ui->tab < t->category_count) draw_items(ui, g, panel);
    else draw_exchanges(g, panel);
}
