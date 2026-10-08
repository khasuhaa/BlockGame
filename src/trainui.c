#include "trainui.h"

#include "train.h"

#define BUTTON_HEIGHT 26.0f
#define MIN_WIDTH 150.0f
#define PAD 6.0f
#define SLOT 52.0f
#define SLOT_GAP 4.0f
#define SLOT_COLUMNS 4
#define HEADER 26.0f

static const Color panel_color = { 28, 30, 38, 240 };
static const Color edge_color = { 120, 122, 134, 255 };

/* The top middle of a vehicle on screen. */
static Vector2 over(const Game *g, const View *v, int vehicle)
{
    float x, y;
    train_visual(g, vehicle, &x, &y);
    Vector2 at = view_tile_pos(v, x, y - 1.0f);
    return (Vector2){ at.x + v->tile_size / 2.0f, at.y };
}

static Rectangle menu_rect(const TrainUi *ui, const Game *g, const View *v, const TrainOption *options, int count)
{
    float wide = MIN_WIDTH, h = (float)count * (BUTTON_HEIGHT + PAD) + PAD;
    for (int i = 0; i < count; i++)
        if ((float)MeasureText(options[i].label, 16) + 36.0f > wide) wide = (float)MeasureText(options[i].label, 16) + 36.0f;
    Vector2 at = over(g, v, ui->vehicle);
    return (Rectangle){ at.x - wide / 2.0f, at.y - h - 8.0f, wide, h };
}

static Rectangle button_rect(Rectangle menu, int i)
{
    return (Rectangle){ menu.x + PAD, menu.y + PAD + (float)i * (BUTTON_HEIGHT + PAD), menu.width - 2 * PAD, BUTTON_HEIGHT };
}

/* The box the open vehicle's slots are in. */
static Rectangle slots_rect(const Game *g, const View *v, int slots)
{
    int columns = slots < SLOT_COLUMNS ? slots : SLOT_COLUMNS, rows = (slots + SLOT_COLUMNS - 1) / SLOT_COLUMNS;
    float w = (float)columns * (SLOT + SLOT_GAP) - SLOT_GAP + 2 * PAD, h = HEADER + (float)rows * (SLOT + SLOT_GAP) - SLOT_GAP + 2 * PAD;
    if (w < 110.0f) w = 110.0f;
    Vector2 at = over(g, v, g->open_vehicle - 1);
    return (Rectangle){ at.x - w / 2.0f, at.y - h - 8.0f, w, h };
}

int trainui_storage_slots(const Game *g, const View *v, Rectangle *rects, int max, uint16_t *container)
{
    int slots = 0, n = 0;
    *container = train_open_container(g, &slots);
    if (*container == 0) return 0;
    Rectangle box = slots_rect(g, v, slots);
    for (int i = 0; i < slots && n < max; i++)
        rects[n++] = (Rectangle){ box.x + PAD + (float)(i % SLOT_COLUMNS) * (SLOT + SLOT_GAP),
                                  box.y + HEADER + PAD + (float)(i / SLOT_COLUMNS) * (SLOT + SLOT_GAP), SLOT, SLOT };
    return n;
}

void trainui_show(TrainUi *ui, int vehicle, int x)
{
    *ui = (TrainUi){ .open = true, .vehicle = vehicle, .x = x };
}

bool trainui_mouse_over(const TrainUi *ui, const Game *g, const View *v)
{
    Vector2 m = GetMousePosition();
    TrainOption options[TRAIN_MAX_OPTIONS];
    int slots = 0, count = ui->open ? train_options(g, ui->vehicle, ui->x, options) : 0;
    if (count > 0 && CheckCollisionPointRec(m, menu_rect(ui, g, v, options, count))) return true;
    return train_open_container(g, &slots) != 0 && CheckCollisionPointRec(m, slots_rect(g, v, slots));
}

bool trainui_handle_input(TrainUi *ui, Game *g, const View *v)
{
    TrainOption options[TRAIN_MAX_OPTIONS];
    int count = ui->open ? train_options(g, ui->vehicle, ui->x, options) : 0;
    /* (Gone out of reach, or taken up: there is nothing to offer.) */
    if (count == 0) ui->open = false;
    bool inside = trainui_mouse_over(ui, g, v);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ui->pressed = inside;
        /* A press anywhere else puts the menu away, and shuts what was open. */
        if (!inside) {
            ui->open = false;
            train_close(g);
        }
    }
    if (!ui->pressed) return false;
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return true;
    ui->pressed = false;
    Vector2 m = GetMousePosition();
    Rectangle menu = menu_rect(ui, g, v, options, count);
    for (int i = 0; i < count; i++) {
        if (!CheckCollisionPointRec(m, button_rect(menu, i))) continue;
        train_close(g);
        train_do(g, ui->vehicle, options[i]);
        ui->open = false;
        break;
    }
    return true;
}

void trainui_draw(const TrainUi *ui, const Game *g, const View *v)
{
    Vector2 m = GetMousePosition();
    int slots = 0;
    if (train_open_container(g, &slots) != 0) {
        Rectangle box = slots_rect(g, v, slots);
        DrawRectangleRec(box, panel_color);
        DrawRectangleLinesEx(box, 2.0f, edge_color);
        const char *name = g->open_vehicle_cargo ? item_def((ItemId)g->vehicles[g->open_vehicle - 1].item)->label : "Fuel";
        DrawText(name, (int)(box.x + PAD + 2), (int)box.y + 7, 16, RAYWHITE);
    }
    TrainOption options[TRAIN_MAX_OPTIONS];
    int count = ui->open ? train_options(g, ui->vehicle, ui->x, options) : 0;
    if (count == 0) return;
    Rectangle menu = menu_rect(ui, g, v, options, count);
    DrawRectangleRec(menu, panel_color);
    DrawRectangleLinesEx(menu, 2.0f, edge_color);
    for (int i = 0; i < count; i++) {
        Rectangle b = button_rect(menu, i);
        bool hot = CheckCollisionPointRec(m, b);
        DrawRectangleRec(b, hot ? (Color){ 80, 84, 100, 255 } : (Color){ 54, 57, 70, 255 });
        DrawText(options[i].label, (int)(b.x + (b.width - (float)MeasureText(options[i].label, 16)) / 2.0f), (int)(b.y + 5), 16, RAYWHITE);
    }
}
