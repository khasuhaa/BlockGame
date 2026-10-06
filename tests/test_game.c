/* Headless tests for the game rules: no window, just the simulation. */
#include "../src/game.h"

#include <stdio.h>

static int failures;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
            failures++;                                                \
        }                                                              \
    } while (0)

static void run(Game *g, float seconds)
{
    for (float t = 0.0f; t < seconds; t += 1.0f / 60.0f) game_update(g, 1.0f / 60.0f);
}

static bool is_idle(const Game *g)
{
    return g->player.activity == ACTIVITY_IDLE && !g->has_current && g->queue_count == 0;
}

static void test_world(void)
{
    Game g;
    CHECK(game_init(&g, 64, 48, 1234));
    int s = g.world.surface_y;
    for (int x = 0; x < g.world.width; x++) {
        CHECK(world_get(&g.world, x, s - 1) == TILE_AIR);
        CHECK(world_get(&g.world, x, s) == TILE_DIRT);
        CHECK(world_get(&g.world, x, g.world.height - 1) == TILE_DIRT);
    }
    CHECK(g.player.feet.x == g.world.spawn_x);
    CHECK(g.player.feet.y == s - 1);
    game_free(&g);
}

static void test_mine_range(void)
{
    TilePos feet = { 10, 10 };
    int count = 0;
    for (int y = 5; y <= 15; y++)
        for (int x = 5; x <= 15; x++)
            if (game_in_mine_range(feet, (TilePos){ x, y })) count++;
    CHECK(count == 10);
    CHECK(!game_in_mine_range(feet, (TilePos){ 10, 10 }));
    CHECK(!game_in_mine_range(feet, (TilePos){ 10, 9 }));
    CHECK(game_in_mine_range(feet, (TilePos){ 11, 8 }));
    CHECK(!game_in_mine_range(feet, (TilePos){ 10, 12 }));
}

static void test_keys(void)
{
    Game g;
    game_init(&g, 64, 48, 1);
    TilePos start = g.player.feet;

    game_set_move_input(&g, 1, 0);
    run(&g, WALK_TIME * 3 + 0.01f);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(g.player.feet.x >= start.x + 3 && g.player.feet.y == start.y);

    /* A jump goes up one tile and gravity brings the player back. */
    int x = g.player.feet.x;
    game_set_move_input(&g, 0, -1);
    run(&g, WALK_TIME * 0.5f);
    game_set_move_input(&g, 0, 0);
    CHECK(g.player.step_to.y == start.y - 1);
    run(&g, 1.0f);
    CHECK(g.player.feet.x == x && g.player.feet.y == start.y);

    /* Down is solid ground. */
    game_set_move_input(&g, 0, 1);
    run(&g, 0.5f);
    CHECK(g.player.feet.y == start.y);
    game_free(&g);
}

static void test_click_move(void)
{
    Game g;
    game_init(&g, 64, 48, 2);
    TilePos start = g.player.feet;

    /* Feet block onto the clicked tile. */
    CHECK(game_click_tile(&g, start.x + 5, start.y));
    run(&g, 3.0f);
    CHECK(is_idle(&g) && g.player.feet.x == start.x + 5 && g.player.feet.y == start.y);

    /* Head block onto the clicked tile. */
    CHECK(game_click_tile(&g, start.x - 2, start.y - 1));
    run(&g, 3.0f);
    CHECK(is_idle(&g) && g.player.feet.x == start.x - 2 && g.player.feet.y == start.y);

    /* Nothing to stand on up there, and out of the world entirely. */
    CHECK(!game_click_tile(&g, start.x, start.y - 2));
    CHECK(!game_click_tile(&g, start.x + 3, start.y - 6));
    CHECK(!game_click_tile(&g, -1, start.y));
    CHECK(g.queue_count == 0);
    run(&g, 1.0f);
    CHECK(g.player.feet.x == start.x - 2);
    game_free(&g);
}

static void test_click_mine(void)
{
    Game g;
    game_init(&g, 64, 48, 3);
    TilePos start = g.player.feet;
    int s = g.world.surface_y;

    /* Buried dirt cannot be stood next to. */
    CHECK(!game_click_tile(&g, start.x, s + 1));
    CHECK(!game_click_tile(&g, start.x + 4, s + 5));

    /* Surface dirt far away: walk over, then mine. */
    CHECK(game_click_tile(&g, start.x + 6, s));
    run(&g, 3.0f);
    CHECK(is_idle(&g));
    CHECK(world_get(&g.world, start.x + 6, s) == TILE_AIR);
    CHECK(g.player.feet.x == start.x + 5 && g.player.feet.y == start.y);

    /* Mining the block underfoot drops the player into the hole. */
    CHECK(game_click_tile(&g, start.x + 5, s));
    run(&g, 3.0f);
    CHECK(world_get(&g.world, start.x + 5, s) == TILE_AIR);
    CHECK(g.player.feet.x == start.x + 5 && g.player.feet.y == s);

    /* From a one-deep pit the player can still climb out. */
    CHECK(game_click_tile(&g, start.x, start.y));
    run(&g, 3.0f);
    CHECK(is_idle(&g) && g.player.feet.x == start.x && g.player.feet.y == start.y);
    game_free(&g);
}

static void test_queue(void)
{
    Game g;
    game_init(&g, 64, 48, 4);
    TilePos start = g.player.feet;
    int s = g.world.surface_y;

    /* Later clicks are judged against the world the earlier ones leave
     * behind: the second tile only becomes minable, and the third only
     * becomes air to stand in, once the ones before have run. */
    CHECK(game_click_tile(&g, start.x + 3, s));
    CHECK(game_click_tile(&g, start.x + 3, s + 1));
    CHECK(game_click_tile(&g, start.x + 3, s + 1));
    CHECK(g.queue_count == 3);
    CHECK(g.queue[2].type == ACTION_MOVE);
    run(&g, 6.0f);
    CHECK(is_idle(&g));
    CHECK(world_get(&g.world, start.x + 3, s) == TILE_AIR);
    CHECK(world_get(&g.world, start.x + 3, s + 1) == TILE_AIR);
    CHECK(g.player.feet.x == start.x + 3 && g.player.feet.y == s + 1);

    /* A two-deep pit is climbed out of by its wall. */
    CHECK(game_click_tile(&g, start.x, start.y));
    run(&g, 4.0f);
    CHECK(is_idle(&g) && g.player.feet.x == start.x && g.player.feet.y == start.y);

    /* Cancelling drops everything that was queued. */
    CHECK(game_click_tile(&g, start.x + 6, s));
    CHECK(game_click_tile(&g, start.x + 8, s));
    game_cancel_actions(&g);
    run(&g, 2.0f);
    CHECK(is_idle(&g));
    CHECK(world_get(&g.world, start.x + 6, s) == TILE_DIRT);
    game_free(&g);
}

/* Digs columns x0..x1 out to `depth` blocks below the surface. */
static void dig_pit(Game *g, int x0, int x1, int depth)
{
    for (int x = x0; x <= x1; x++)
        for (int d = 0; d < depth; d++) world_set(&g->world, x, g->world.surface_y + d, TILE_AIR);
}

/* What a traversal looked like, sampled every frame. */
typedef struct {
    int deepest;  /* largest feet y seen */
    bool climbed; /* was ever at rest hanging on a wall */
    bool fell;    /* ever dropped more than one block in free fall */
    bool hopped;
} Trace;

static Trace run_traced(Game *g, float seconds)
{
    Trace tr = { g->player.feet.y, false, false, false };
    int fall = 0;
    TilePos last = g->player.feet;
    for (float t = 0.0f; t < seconds; t += 1.0f / 60.0f) {
        game_update(g, 1.0f / 60.0f);
        const Player *p = &g->player;
        if (p->feet.y > tr.deepest) tr.deepest = p->feet.y;
        if (p->climbing) tr.climbed = true;
        if (p->activity == ACTIVITY_STEPPING && p->step_hop) tr.hopped = true;
        if (p->feet.y != last.y || p->feet.x != last.x) {
            bool falling = p->feet.y == last.y + 1 && p->feet.x == last.x && !p->climbing &&
                           !world_is_solid(&g->world, p->feet.x, p->feet.y + 1);
            fall = falling ? fall + 1 : 0;
            if (fall >= 1) tr.fell = true;
            last = p->feet;
        }
    }
    return tr;
}

static void test_hop_gap(void)
{
    Game g;
    game_init(&g, 64, 48, 5);
    TilePos start = g.player.feet;
    int s = g.world.surface_y;
    dig_pit(&g, start.x + 2, start.x + 2, 4);

    /* Holding a direction runs straight over a one-block gap. */
    game_set_move_input(&g, 1, 0);
    Trace tr = run_traced(&g, 1.0f);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(tr.hopped && !tr.climbed && tr.deepest == start.y);
    CHECK(g.player.feet.x > start.x + 3 && g.player.feet.y == start.y);

    /* So does a queued move. */
    CHECK(game_click_tile(&g, start.x, start.y));
    tr = run_traced(&g, 3.0f);
    CHECK(tr.hopped && tr.deepest == start.y && g.player.feet.x == start.x);

    /* Down plus a direction goes into the gap instead, by its wall. */
    game_set_move_input(&g, 1, 1);
    tr = run_traced(&g, 3.0f);
    game_set_move_input(&g, 0, 0);
    CHECK(tr.climbed && !tr.fell);
    CHECK(g.player.feet.x == start.x + 2 && g.player.feet.y == s + 3);
    game_free(&g);
}

static void test_climb_drop(void)
{
    Game g;
    game_init(&g, 64, 48, 6);
    TilePos start = g.player.feet;
    int s = g.world.surface_y;
    dig_pit(&g, start.x + 2, start.x + 7, 3);

    /* One step over the edge of a 2+ drop leaves the player on the wall. */
    game_set_move_input(&g, 1, 0);
    run(&g, WALK_TIME + CLIMB_TIME * 0.5f);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(g.player.climbing && g.player.feet.x == start.x + 2 && g.player.feet.y == s);
    CHECK(game_player_grip_side(&g) == -1);

    /* Keep going: down the wall, across the floor, up the far wall. */
    game_set_move_input(&g, 1, 0);
    Trace tr = run_traced(&g, 4.0f);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(tr.climbed && !tr.fell && tr.deepest == s + 2);
    CHECK(g.player.feet.x > start.x + 7 && g.player.feet.y == start.y);

    game_free(&g);
}

static void test_wall_hop(void)
{
    for (int width = 2; width <= 3; width++) {
        Game g;
        game_init(&g, 64, 48, 7);
        TilePos start = g.player.feet;
        int s = g.world.surface_y;
        dig_pit(&g, start.x + 2, start.x + 1 + width, 5);

        /* Down onto the near wall, hop to the far wall, up and over, without
         * ever going deeper than the first hold. */
        game_set_move_input(&g, 1, 0);
        Trace tr = run_traced(&g, 2.0f);
        game_set_move_input(&g, 0, 0);
        run(&g, 1.0f);
        CHECK(tr.climbed && tr.hopped && !tr.fell && tr.deepest == s);
        CHECK(g.player.feet.x > start.x + 1 + width && g.player.feet.y == start.y);

        /* The same route is found for a click on the far side. */
        CHECK(game_click_tile(&g, start.x, start.y));
        tr = run_traced(&g, 4.0f);
        CHECK(is_idle(&g) && tr.deepest == s && g.player.feet.x == start.x);
        game_free(&g);
    }
}

static void test_step_down(void)
{
    Game g;
    game_init(&g, 64, 48, 8);
    TilePos start = g.player.feet;
    dig_pit(&g, start.x + 2, start.x + 6, 1);

    /* A one-block drop is simply stepped down, with no climbing. */
    game_set_move_input(&g, 1, 0);
    Trace tr = run_traced(&g, WALK_TIME * 3);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(!tr.climbed && g.player.feet.y == start.y + 1);
    game_free(&g);
}

static int count_item(Game *g, ItemId id)
{
    int total = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        if (g->inventory.slots[i].id == id) total += g->inventory.slots[i].count;
        for (int j = 0; j < BASKET_SLOTS; j++) {
            ItemStack *s = inventory_slot(&g->inventory, (SlotRef){ SLOT_BASKET, i, j });
            if (s && s->id == id) total += s->count;
        }
    }
    return total;
}

static void test_inventory(void)
{
    Game g;
    game_init(&g, 64, 48, 9);
    Inventory *inv = &g.inventory;
    SlotRef bar0 = { SLOT_BAR, 0, 0 }, bar1 = { SLOT_BAR, 1, 0 }, bar2 = { SLOT_BAR, 2, 0 };

    /* Starts with a basket, item 1, and nothing else. */
    CHECK(ITEM_BASKET == 1);
    CHECK(inv->slots[0].id == ITEM_BASKET && inv->slots[0].count == 1);
    CHECK(count_item(&g, ITEM_DIRT) == 0);
    CHECK(inventory_selected(inv)->id == ITEM_BASKET);

    /* Stacks stop at 99 and spill into the next slot. */
    CHECK(inventory_add(inv, ITEM_DIRT, 100) == 0);
    CHECK(inv->slots[1].count == 99 && inv->slots[2].count == 1);

    /* Once the bar is full, the basket in it takes the overflow. */
    CHECK(inventory_add(inv, ITEM_DIRT, 98 + 99 * 5) == 0);
    CHECK(inv->slots[7].count == 99);
    CHECK(inventory_add(inv, ITEM_DIRT, 99 * 4 + 5) == 5);
    CHECK(count_item(&g, ITEM_DIRT) == 99 * 11);

    /* Selecting inside a basket shows that item; an emptied slot shows the basket. */
    inventory_select(inv, 0, 2);
    CHECK(inventory_selection_inside(inv, 0) && inventory_selected(inv)->id == ITEM_DIRT);
    *inventory_slot(inv, (SlotRef){ SLOT_BASKET, 0, 2 }) = (ItemStack){ 0 };
    CHECK(!inventory_selection_inside(inv, 0) && inventory_selected(inv)->id == ITEM_BASKET);

    /* Moving: swap, merge, and no armor slot for dirt. */
    inv->slots[2].count = 40;
    inv->slots[1].count = 90;
    CHECK(inventory_move(inv, bar2, bar1));
    CHECK(inv->slots[1].count == 99 && inv->slots[2].count == 31);
    CHECK(!inventory_move(inv, bar1, (SlotRef){ SLOT_ARMOR, 0, 0 }));
    CHECK(inventory_move(inv, bar0, bar2));
    CHECK(inv->slots[2].id == ITEM_BASKET && inv->slots[0].count == 31);
    CHECK(inventory_slot(inv, (SlotRef){ SLOT_BASKET, 2, 0 })->count == 99);
    CHECK(inventory_slot(inv, (SlotRef){ SLOT_BASKET, 0, 0 }) == NULL);

    /* A basket cannot go into itself, but can go into another basket, where
     * its contents come along but are out of reach. */
    CHECK(!inventory_move(inv, bar2, (SlotRef){ SLOT_BASKET, 2, 2 }));
    inv->slots[0] = (ItemStack){ 0 };
    inv->slots[1] = (ItemStack){ 0 };
    CHECK(inventory_add(inv, ITEM_BASKET, 1) == 0);
    CHECK(inv->slots[0].id == ITEM_BASKET && inv->slots[0].container != inv->slots[2].container);
    CHECK(inventory_move(inv, bar2, (SlotRef){ SLOT_BASKET, 0, 1 }));
    CHECK(inv->slots[2].count == 0);
    CHECK(inventory_slot(inv, (SlotRef){ SLOT_BASKET, 2, 0 }) == NULL);
    CHECK(!inventory_move(inv, (SlotRef){ SLOT_BASKET, 0, 1 }, bar0));
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BASKET, 0, 1 }, bar1));
    CHECK(inventory_slot(inv, (SlotRef){ SLOT_BASKET, 1, 0 })->count == 99);
    game_free(&g);
}

static void test_drops(void)
{
    Game g;
    game_init(&g, 64, 48, 10);
    TilePos start = g.player.feet;
    int s = g.world.surface_y;

    /* A mined block lies in the world first, then is collected. */
    CHECK(game_click_tile(&g, start.x + 1, s));
    run(&g, MINE_TIME + 0.05f);
    CHECK(world_get(&g.world, start.x + 1, s) == TILE_AIR);
    CHECK(g.drops[0].active && g.drops[0].item == ITEM_DIRT && count_item(&g, ITEM_DIRT) == 0);
    run(&g, 1.0f);
    CHECK(!g.drops[0].active && count_item(&g, ITEM_DIRT) == 1);

    /* Out of reach it stays put until the player walks up to it. */
    g.drops[0] = (Drop){ true, ITEM_DIRT, start.x + 6, (float)(s - 5), 0.0f, 0.0f };
    run(&g, 2.0f);
    CHECK(g.drops[0].active && g.drops[0].y == (float)(s - 1));
    CHECK(game_click_tile(&g, start.x + 5, start.y));
    run(&g, 2.0f);
    CHECK(!g.drops[0].active && count_item(&g, ITEM_DIRT) == 2);

    /* With no room, it is left on the ground. */
    inventory_add(&g.inventory, ITEM_DIRT, 99 * 12);
    int before = count_item(&g, ITEM_DIRT);
    g.drops[0] = (Drop){ true, ITEM_DIRT, g.player.feet.x, (float)g.player.feet.y, 0.0f, 0.0f };
    run(&g, 2.0f);
    CHECK(g.drops[0].active && count_item(&g, ITEM_DIRT) == before);
    game_free(&g);
}

int main(void)
{
    test_world();
    test_mine_range();
    test_keys();
    test_click_move();
    test_click_mine();
    test_queue();
    test_hop_gap();
    test_climb_drop();
    test_wall_hop();
    test_step_down();
    test_inventory();
    test_drops();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
