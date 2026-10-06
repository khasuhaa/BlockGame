#include "hud.h"

#include "assets.h"

#include <stddef.h>

#define SLOT_SIZE 52.0f
#define SLOT_GAP 4.0f
#define BAR_MARGIN 14.0f
#define COLUMN_ROWS 4
#define HOLD_DELAY 0.3f /* seconds before a held press picks up the first item */
#define HOLD_TICK 0.12f /* seconds between each increase after that */
#define NAME_TIME 2.2f  /* how long a newly selected item's name stays up */
#define NAME_FADE 0.5f
#define EAT_HEIGHT 28.0f
#define ICON_INSET 3.0f /* room left round an item in its slot */

/* Cell in the inventory grid. Column 0 is the head button and columns 1..8
 * the bar; row 0 is the bar itself and rows 1..4 extend upward from it. */
static Rectangle cell_rect(int col, int row)
{
    float pitch = SLOT_SIZE + SLOT_GAP;
    float width = (INVENTORY_SLOTS + 1) * pitch - SLOT_GAP;
    float x0 = (float)(int)((GetScreenWidth() - width) / 2.0f);
    float y0 = GetScreenHeight() - BAR_MARGIN - SLOT_SIZE;
    return (Rectangle){ x0 + col * pitch, y0 - row * pitch, SLOT_SIZE, SLOT_SIZE };
}

static bool is_basket(const Inventory *inv, int slot)
{
    const ItemStack *s = &inv->slots[slot];
    return s->count > 0 && item_def(s->id)->opens_in_bar;
}

static bool basket_shown(const Hud *hud, const Inventory *inv, int slot)
{
    return hud->basket_open[slot] && is_basket(inv, slot);
}

/* What is under a point: the head button, a slot, or neither. */
static bool hit_test(const Hud *hud, const Inventory *inv, Vector2 p, bool *head, SlotRef *slot)
{
    *head = false;
    *slot = (SlotRef){ SLOT_NONE, 0, 0 };
    for (int col = 0; col <= INVENTORY_SLOTS; col++) {
        for (int row = 0; row <= COLUMN_ROWS; row++) {
            if (!CheckCollisionPointRec(p, cell_rect(col, row))) continue;
            if (row == 0) {
                if (col == 0) *head = true;
                else *slot = (SlotRef){ SLOT_BAR, col - 1, 0 };
                return true;
            }
            if (col == 0 && hud->armor_open) {
                *slot = (SlotRef){ SLOT_ARMOR, COLUMN_ROWS - row, 0 };
                return true;
            }
            if (col > 0 && basket_shown(hud, inv, col - 1)) {
                *slot = (SlotRef){ SLOT_BASKET, col - 1, row - 1 };
                return true;
            }
            return false;
        }
    }
    for (int i = 0; i < hud->extra_count; i++) {
        if (!CheckCollisionPointRec(p, hud->extra[i].rect)) continue;
        *slot = hud->extra[i].ref;
        return true;
    }
    return false;
}

/* The top of whatever stands on the selected slot: the slot, or the basket open above it. */
static Rectangle selected_top(const Hud *hud, const Inventory *inv)
{
    return cell_rect(inv->sel_slot + 1, basket_shown(hud, inv, inv->sel_slot) ? COLUMN_ROWS : 0);
}

/* The "Eat" button sits just above that. */
static Rectangle eat_rect(const Hud *hud, const Inventory *inv)
{
    Rectangle top = selected_top(hud, inv);
    return (Rectangle){ top.x - 4.0f, top.y - EAT_HEIGHT - 6.0f, top.width + 8.0f, EAT_HEIGHT };
}

void hud_close_all(Hud *hud)
{
    hud->armor_open = false;
    for (int i = 0; i < INVENTORY_SLOTS; i++) hud->basket_open[i] = false;
}

bool hud_mouse_over(const Hud *hud, const Inventory *inv)
{
    bool head;
    SlotRef slot;
    if (hud->eat_pressed || (hud->eat != HUD_EAT_NONE && CheckCollisionPointRec(GetMousePosition(), eat_rect(hud, inv))))
        return true;
    return hud->pressed || hit_test(hud, inv, GetMousePosition(), &head, &slot);
}

/* The storage container whose slots are on screen, if one is open. */
static uint16_t open_storage(const Hud *hud)
{
    if (hud->extra_count == 0 || hud->extra[0].ref.kind != SLOT_CONTAINER) return 0;
    return (uint16_t)hud->extra[0].ref.index;
}

static void handle_tap(Hud *hud, Inventory *inv)
{
    SlotRef s = hud->press_slot;
    /* With a chest open, a click on a stack sends the whole stack across:
     * into the chest from the inventory, out of it from the chest. */
    const ItemStack *tapped = hud->press_head ? NULL : inventory_slot(inv, s);
    if (open_storage(hud) != 0 && tapped && tapped->count > 0 && s.kind != SLOT_ARMOR) {
        inventory_quick_move(inv, s, open_storage(hud), hud->extra_count);
        return;
    }
    if (hud->press_head) {
        /* The head button opens the armor column and every basket at once. */
        bool open = !hud->armor_open;
        hud_close_all(hud);
        hud->armor_open = open;
        for (int i = 0; i < INVENTORY_SLOTS; i++) hud->basket_open[i] = open;
    } else if (s.kind == SLOT_BAR) {
        inventory_select_slot(inv, s.index);
        if (is_basket(inv, s.index)) hud->basket_open[s.index] = !hud->basket_open[s.index];
    } else if (s.kind == SLOT_BASKET) {
        /* An empty slot in a basket selects the basket itself. */
        const ItemStack *inner = inventory_slot(inv, s);
        inventory_select(inv, s.index, inner && inner->count > 0 ? s.inner : -1);
    }
}

/* How many items a press held for `t` seconds has picked up from a stack of
 * `count`: 1, 2 ... 10, then 15, 20, 25 ... stopping one short of the stack. */
static int hold_amount(float t, int count)
{
    if (t < HOLD_DELAY) return 0;
    int ticks = (int)((t - HOLD_DELAY) / HOLD_TICK);
    int amount = ticks < 10 ? 1 + ticks : 10 + 5 * (ticks - 9);
    int most = count > 1 ? count - 1 : 1;
    return amount < most ? amount : most;
}

static bool same_slot(SlotRef a, SlotRef b)
{
    return a.kind == b.kind && a.index == b.index && (a.kind != SLOT_BASKET || a.inner == b.inner);
}

bool hud_handle_input(Hud *hud, Inventory *inv)
{
    Vector2 mouse = GetMousePosition();

    /* Whenever a different slot or a different item is selected, its name goes up. */
    int item = inventory_selected(inv)->count > 0 ? inventory_selected(inv)->id : ITEM_NONE;
    if (inv->sel_slot != hud->named_slot || inv->sel_inner != hud->named_inner || item != hud->named_item) {
        hud->named_slot = inv->sel_slot;
        hud->named_inner = inv->sel_inner;
        hud->named_item = item;
        hud->name_time = item != ITEM_NONE ? NAME_TIME : 0.0f;
    }
    if (hud->name_time > 0.0f) hud->name_time -= GetFrameTime();

    /* The "Eat" button takes a press before the slots do. */
    hud->eat_clicked = false;
    hud->discard = false;
    bool on_eat = hud->eat != HUD_EAT_NONE && CheckCollisionPointRec(mouse, eat_rect(hud, inv));
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && on_eat) {
        hud->eat_pressed = true;
        hud->pressed = false;
        return true;
    }
    if (hud->eat_pressed) {
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            hud->eat_pressed = false;
            hud->eat_clicked = on_eat && hud->eat == HUD_EAT_READY;
        }
        return true;
    }

    bool head;
    SlotRef over;
    bool on_hud = hit_test(hud, inv, mouse, &head, &over);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        hud->pressed = on_hud;
        hud->press_head = head;
        hud->press_slot = over;
        hud->dragging = false;
        hud->hold_time = 0.0f;
        hud->picked = 0;
    }
    if (!hud->pressed) return false;

    const ItemStack *held = inventory_slot(inv, hud->press_slot);
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !hud->dragging && held && held->count > 0) {
        if (on_hud && !head && same_slot(over, hud->press_slot)) {
            hud->hold_time += GetFrameTime();
            hud->picked = hold_amount(hud->hold_time, held->count);
        } else {
            /* Leaving the slot carries off what has been picked up so far; a
             * quick drag that picked nothing up takes the whole stack. */
            hud->dragging = true;
            if (hud->picked == 0) hud->picked = held->count;
        }
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (hud->dragging) {
            if (on_hud && !head) {
                inventory_move(inv, hud->press_slot, over, hud->picked);
            } else if (!on_hud) {
                /* Let go clear of the inventory: out into the world it goes. */
                hud->discard = true;
                hud->discard_slot = hud->press_slot;
                hud->discard_count = hud->picked;
            }
        } else if (hud->picked == 0) {
            handle_tap(hud, inv);
        }
        hud->pressed = false;
        hud->dragging = false;
        hud->picked = 0;
    }
    return true;
}

/* ---- drawing ----------------------------------------------------------- */

static Rectangle sub_rect(Rectangle r, float x, float y, float w, float h)
{
    return (Rectangle){ r.x + r.width * x, r.y + r.height * y, r.width * w, r.height * h };
}

void hud_draw_head_icon(Rectangle r)
{
    DrawRectangleRec(sub_rect(r, 0.2f, 0.2f, 0.6f, 0.6f), (Color){ 240, 200, 160, 255 });
    DrawRectangleRec(sub_rect(r, 0.2f, 0.2f, 0.6f, 0.18f), (Color){ 90, 60, 40, 255 });
    DrawRectangleRec(sub_rect(r, 0.32f, 0.47f, 0.1f, 0.1f), (Color){ 40, 40, 60, 255 });
    DrawRectangleRec(sub_rect(r, 0.58f, 0.47f, 0.1f, 0.1f), (Color){ 40, 40, 60, 255 });
}

void hud_draw_item_icon(ItemId id, Rectangle r)
{
    assets_draw_item(id, r);
}

/* Faint outline of what belongs in an empty armor slot. */
static void draw_armor_hint(int equip, Rectangle r)
{
    Color c = Fade(WHITE, 0.16f);
    switch (equip) {
    case EQUIP_HEAD:
        DrawRectangleRec(sub_rect(r, 0.25f, 0.28f, 0.5f, 0.24f), c);
        DrawRectangleRec(sub_rect(r, 0.25f, 0.52f, 0.14f, 0.2f), c);
        DrawRectangleRec(sub_rect(r, 0.61f, 0.52f, 0.14f, 0.2f), c);
        break;
    case EQUIP_CHEST:
        DrawRectangleRec(sub_rect(r, 0.18f, 0.24f, 0.64f, 0.2f), c);
        DrawRectangleRec(sub_rect(r, 0.32f, 0.44f, 0.36f, 0.34f), c);
        break;
    case EQUIP_LEGS:
        DrawRectangleRec(sub_rect(r, 0.28f, 0.22f, 0.44f, 0.16f), c);
        DrawRectangleRec(sub_rect(r, 0.28f, 0.38f, 0.17f, 0.4f), c);
        DrawRectangleRec(sub_rect(r, 0.55f, 0.38f, 0.17f, 0.4f), c);
        break;
    default:
        DrawRectangleRec(sub_rect(r, 0.24f, 0.3f, 0.16f, 0.44f), c);
        DrawRectangleRec(sub_rect(r, 0.6f, 0.3f, 0.16f, 0.44f), c);
        DrawRectangleRec(sub_rect(r, 0.16f, 0.6f, 0.24f, 0.14f), c);
        DrawRectangleRec(sub_rect(r, 0.6f, 0.6f, 0.24f, 0.14f), c);
        break;
    }
}

static void draw_box(Rectangle r, bool selected)
{
    DrawRectangleRec(r, (Color){ 34, 36, 44, 225 });
    if (selected) DrawRectangleLinesEx(r, 3.0f, RAYWHITE);
    else DrawRectangleLinesEx(r, 2.0f, (Color){ 120, 122, 134, 255 });
}

/* Draws a stack, less `taken` items that are currently on the cursor. */
static void draw_stack(const Inventory *inv, const ItemStack *s, Rectangle r, int taken)
{
    if (!s || s->count <= taken) return;
    int count = s->count - taken;
    /* A meter that is not as new shows as a bar under the item: the top one of the stack's. */
    const ItemDef *def = item_def(s->id);
    int reads = inventory_meter(inv, s);
    if (reads >= 0 && reads != def->meter_start) {
        float share = (float)reads / (float)def->meter_max;
        Rectangle bar = { r.x + 6, r.y + r.height - 9, r.width - 12, 5 };
        Color fill = def->holds_water ? (Color){ 70, 160, 236, 255 }
                     : share > 0.5f ? (Color){ 76, 190, 84, 255 } : share > 0.2f ? (Color){ 236, 190, 60, 255 }
                                                                                 : (Color){ 226, 80, 70, 255 };
        DrawRectangleRec((Rectangle){ bar.x - 1, bar.y - 1, bar.width + 2, bar.height + 2 }, (Color){ 12, 13, 18, 255 });
        DrawRectangleRec((Rectangle){ bar.x, bar.y, bar.width * share, bar.height }, fill);
    }
    hud_draw_item_icon((ItemId)s->id, (Rectangle){ r.x + ICON_INSET, r.y + ICON_INSET, r.width - 2 * ICON_INSET,
                                                   r.height - 2 * ICON_INSET });
    if (count > 1) {
        const char *text = TextFormat("%d", count);
        int size = 18;
        int x = (int)(r.x + r.width) - MeasureText(text, size) - 5;
        int y = (int)(r.y + r.height) - size - 3;
        DrawText(text, x + 1, y + 1, size, BLACK);
        DrawText(text, x, y, size, RAYWHITE);
    }
}

/* An item's name on a dark tag, centred on `x`, kept on the screen. */
static void draw_name(const char *text, float x, float y, float alpha)
{
    const int size = 18;
    float w = (float)MeasureText(text, size) + 16.0f;
    float left = x - w / 2;
    if (left < 4.0f) left = 4.0f;
    if (left + w > (float)GetScreenWidth() - 4.0f) left = (float)GetScreenWidth() - 4.0f - w;
    Rectangle tag = { left, y, w, 26.0f };
    DrawRectangleRec(tag, Fade((Color){ 18, 19, 24, 255 }, 0.88f * alpha));
    DrawRectangleLinesEx(tag, 1.0f, Fade((Color){ 120, 122, 134, 255 }, alpha));
    DrawText(text, (int)tag.x + 8, (int)tag.y + 4, size, Fade(RAYWHITE, alpha));
}

void hud_draw(const Hud *hud, const Inventory *inv)
{
    /* Slots are looked up through the same accessor the game logic uses. */
    Inventory *lookup = (Inventory *)inv;
    const ItemStack *dragged = hud->pressed && hud->picked > 0
                                   ? inventory_slot(lookup, hud->press_slot) : NULL;
    int picked = dragged && hud->picked < dragged->count ? hud->picked
                 : dragged ? dragged->count : 0;

    for (int i = 0; i < hud->extra_count; i++) {
        const ItemStack *s = inventory_slot(lookup, hud->extra[i].ref);
        draw_box(hud->extra[i].rect, false);
        draw_stack(inv, s, hud->extra[i].rect, s == dragged ? picked : 0);
    }

    draw_box(cell_rect(0, 0), hud->armor_open);
    hud_draw_head_icon(cell_rect(0, 0));
    if (hud->armor_open) {
        for (int i = 0; i < ARMOR_SLOTS; i++) {
            Rectangle r = cell_rect(0, COLUMN_ROWS - i);
            draw_box(r, false);
            if (inv->armor[i].count == 0) draw_armor_hint(i, r);
            draw_stack(inv, &inv->armor[i], r, &inv->armor[i] == dragged ? picked : 0);
        }
    }

    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        Rectangle r = cell_rect(i + 1, 0);
        const ItemStack *s = &inv->slots[i];
        bool open = basket_shown(hud, inv, i);
        bool inside = inventory_selection_inside(inv, i);
        draw_box(r, inv->sel_slot == i && !(open && inside));

        int chosen = inside ? inv->sel_inner : inventory_basket_choice(inv, i);
        if (!open && chosen >= 0) {
            /* A closed basket shows the item chosen inside it, with a small
             * basket in the corner as a reminder of where it lives. */
            SlotRef ref = { SLOT_BASKET, i, chosen };
            draw_stack(inv, inventory_slot(lookup, ref), r, 0);
            hud_draw_item_icon((ItemId)s->id, sub_rect(r, 0.04f, 0.04f, 0.42f, 0.42f));
        } else {
            draw_stack(inv, s, r, s == dragged ? picked : 0);
        }

        if (!open) continue;
        for (int j = 0; j < BASKET_SLOTS; j++) {
            Rectangle cell = cell_rect(i + 1, j + 1);
            const ItemStack *inner = inventory_slot(lookup, (SlotRef){ SLOT_BASKET, i, j });
            draw_box(cell, inside && inv->sel_inner == j);
            draw_stack(inv, inner, cell, inner == dragged ? picked : 0);
        }
    }

    /* The selected item's name, over its slot (or over the column standing on it). */
    bool eat = hud->eat != HUD_EAT_NONE && !dragged;
    if (hud->name_time > 0.0f && hud->named_item != ITEM_NONE && !dragged) {
        Rectangle top = selected_top(hud, inv);
        float alpha = hud->name_time < NAME_FADE ? hud->name_time / NAME_FADE : 1.0f;
        /* (Above the "Eat" button, when that is there.) */
        draw_name(item_def(hud->named_item)->label, top.x + top.width / 2,
                  top.y - 30 - (eat ? EAT_HEIGHT + 6.0f : 0.0f), alpha);
    }

    /* Food that is selected can be eaten from here. */
    if (eat) {
        Rectangle r = eat_rect(hud, inv);
        bool ready = hud->eat == HUD_EAT_READY;
        bool hover = ready && CheckCollisionPointRec(GetMousePosition(), r);
        Color back = !ready ? (Color){ 38, 40, 48, 235 } : hud->eat_pressed && hover ? (Color){ 40, 110, 52, 255 }
                     : hover ? (Color){ 66, 160, 78, 255 } : (Color){ 52, 132, 64, 245 };
        DrawRectangleRec(r, back);
        DrawRectangleLinesEx(r, 2.0f, ready ? (Color){ 170, 232, 170, 255 } : (Color){ 90, 92, 102, 255 });
        const char *text = ready ? "Eat" : "Full";
        DrawText(text, (int)(r.x + (r.width - MeasureText(text, 18)) / 2), (int)r.y + 5, 18,
                 ready ? RAYWHITE : (Color){ 244, 196, 48, 255 });
    }

    if (dragged && dragged->count > 0) {
        Vector2 m = GetMousePosition();
        Rectangle r = { m.x - SLOT_SIZE / 2, m.y - SLOT_SIZE / 2, SLOT_SIZE, SLOT_SIZE };
        draw_stack(inv, dragged, r, dragged->count - picked);
        /* What is being carried, named under the cursor. */
        draw_name(item_def(dragged->id)->label, m.x, m.y + SLOT_SIZE / 2 + 4, 1.0f);
    }
}
