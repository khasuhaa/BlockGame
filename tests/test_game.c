/* Headless tests for the game rules: no window, just the simulation. */
#include "../src/game.h"
#include "../src/save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    CHECK(game_init(&g, 64, 48, 1234, WORLD_FLAT));
    int s = g.world.surface_y;
    for (int x = 0; x < g.world.width; x++) {
        CHECK(world_get(&g.world, x, s - 1) == (x == g.world.spawn_x ? TILE_SPAWN_PORTAL : TILE_AIR));
        CHECK(world_get(&g.world, x, s) == (x == g.world.spawn_x ? TILE_PORTAL_BASE : TILE_DIRT));
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
    game_init(&g, 64, 48, 1, WORLD_FLAT);
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
    game_init(&g, 64, 48, 2, WORLD_FLAT);
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
    CHECK(!game_click_tile(&g, start.x, -1));
    CHECK(g.queue_count == 0);
    run(&g, 1.0f);
    CHECK(g.player.feet.x == start.x - 2);
    game_free(&g);
}

static void test_click_mine(void)
{
    Game g;
    game_init(&g, 64, 48, 3, WORLD_FLAT);
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
    game_init(&g, 64, 48, 4, WORLD_FLAT);
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
    game_init(&g, 64, 48, 5, WORLD_FLAT);
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
    game_init(&g, 64, 48, 6, WORLD_FLAT);
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
        game_init(&g, 64, 48, 7, WORLD_FLAT);
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
    game_init(&g, 64, 48, 8, WORLD_FLAT);
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
    return inventory_count(&g->inventory, id);
}

static void test_inventory(void)
{
    Game g;
    game_init(&g, 64, 48, 9, WORLD_FLAT);
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
    CHECK(inventory_move(inv, bar2, bar1, MAX_STACK));
    CHECK(inv->slots[1].count == 99 && inv->slots[2].count == 31);
    CHECK(!inventory_move(inv, bar1, (SlotRef){ SLOT_ARMOR, 0, 0 }, MAX_STACK));

    /* Part of a stack goes to an empty slot or an equal stack, never a swap. */
    SlotRef in0 = { SLOT_BASKET, 0, 2 };
    CHECK(inventory_move(inv, bar2, in0, 10));
    CHECK(inv->slots[2].count == 21 && inventory_slot(inv, in0)->count == 10);
    CHECK(inventory_move(inv, bar2, in0, 5));
    CHECK(inv->slots[2].count == 16 && inventory_slot(inv, in0)->count == 15);
    CHECK(!inventory_move(inv, bar2, bar0, 5));
    CHECK(inventory_move(inv, in0, bar2, MAX_STACK));
    CHECK(inv->slots[2].count == 31 && inventory_slot(inv, in0)->count == 0);
    CHECK(inventory_move(inv, bar0, bar2, MAX_STACK));
    CHECK(inv->slots[2].id == ITEM_BASKET && inv->slots[0].count == 31);
    CHECK(inventory_slot(inv, (SlotRef){ SLOT_BASKET, 2, 0 })->count == 99);
    CHECK(inventory_slot(inv, (SlotRef){ SLOT_BASKET, 0, 0 }) == NULL);

    /* A basket cannot go into itself, but can go into another basket, where
     * its contents come along but are out of reach. */
    CHECK(!inventory_move(inv, bar2, (SlotRef){ SLOT_BASKET, 2, 2 }, MAX_STACK));
    inv->slots[0] = (ItemStack){ 0 };
    inv->slots[1] = (ItemStack){ 0 };
    CHECK(inventory_add(inv, ITEM_BASKET, 1) == 0);
    CHECK(inv->slots[0].id == ITEM_BASKET && inv->slots[0].container != inv->slots[2].container);
    CHECK(inventory_move(inv, bar2, (SlotRef){ SLOT_BASKET, 0, 1 }, MAX_STACK));
    CHECK(inv->slots[2].count == 0);
    CHECK(inventory_slot(inv, (SlotRef){ SLOT_BASKET, 2, 0 }) == NULL);
    CHECK(!inventory_move(inv, (SlotRef){ SLOT_BASKET, 0, 1 }, bar0, MAX_STACK));
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BASKET, 0, 1 }, bar1, MAX_STACK));
    CHECK(inventory_slot(inv, (SlotRef){ SLOT_BASKET, 1, 0 })->count == 99);
    game_free(&g);
}

static void test_drops(void)
{
    Game g;
    game_init(&g, 64, 48, 10, WORLD_FLAT);
    TilePos start = g.player.feet;
    int s = g.world.surface_y;

    /* A mined block lies in the world first, then is collected. */
    CHECK(game_click_tile(&g, start.x + 1, s));
    run(&g, tile_def(TILE_DIRT)->mine_time + 0.05f);
    CHECK(world_get(&g.world, start.x + 1, s) == TILE_AIR);
    CHECK(g.drops[0].active && g.drops[0].item == ITEM_DIRT && count_item(&g, ITEM_DIRT) == 0);
    run(&g, 1.0f);
    CHECK(!g.drops[0].active && count_item(&g, ITEM_DIRT) == 1);

    /* Out of reach it stays put until the player walks up to it. */
    g.drops[0] = (Drop){ true, ITEM_DIRT, start.x + 6, (float)(s - 5), 0.0f, 0.0f, false, 0, 1 };
    run(&g, 2.0f);
    CHECK(g.drops[0].active && g.drops[0].y == (float)(s - 1));
    CHECK(game_click_tile(&g, start.x + 5, start.y));
    run(&g, 2.0f);
    CHECK(!g.drops[0].active && count_item(&g, ITEM_DIRT) == 2);

    /* With no room, it is left on the ground. */
    inventory_add(&g.inventory, ITEM_DIRT, 99 * 12);
    int before = count_item(&g, ITEM_DIRT);
    g.drops[0] = (Drop){ true, ITEM_DIRT, g.player.feet.x, (float)g.player.feet.y, 0.0f, 0.0f, false, 0, 2 };
    run(&g, 2.0f);
    CHECK(g.drops[0].active && count_item(&g, ITEM_DIRT) == before);
    game_free(&g);
}

static void test_place(void)
{
    Game g;
    game_init(&g, 64, 48, 11, WORLD_FLAT);
    TilePos start = g.player.feet;
    int s = g.world.surface_y;

    /* Empty-handed, or holding something that is not a block, air is a move. */
    CHECK(game_click_tile(&g, start.x + 2, s - 1));
    CHECK(g.queue[0].type == ACTION_MOVE);
    run(&g, 2.0f);

    /* Holding dirt: air beside a block queues a place, other air a move. */
    inventory_add(&g.inventory, ITEM_DIRT, 3);
    inventory_select(&g.inventory, 1, -1);
    CHECK(inventory_selected(&g.inventory)->id == ITEM_DIRT);
    CHECK(game_click_tile(&g, start.x + 6, s - 1));
    CHECK(g.queue[0].type == ACTION_PLACE && g.queue[0].item == ITEM_DIRT);
    /* The next one rests on the block queued before it. */
    CHECK(game_click_tile(&g, start.x + 6, s - 2));
    CHECK(g.queue[1].type == ACTION_PLACE);
    CHECK(!game_click_tile(&g, start.x + 6, s - 9));
    run(&g, 4.0f);
    CHECK(is_idle(&g));
    CHECK(world_get(&g.world, start.x + 6, s - 1) == TILE_DIRT);
    CHECK(world_get(&g.world, start.x + 6, s - 2) == TILE_DIRT);
    CHECK(count_item(&g, ITEM_DIRT) == 1);

    /* No more can be queued than are held. */
    CHECK(game_click_tile(&g, start.x + 9, s - 1));
    CHECK(!game_click_tile(&g, start.x + 10, s - 1));
    run(&g, 3.0f);
    CHECK(count_item(&g, ITEM_DIRT) == 0);
    CHECK(world_get(&g.world, start.x + 9, s - 1) == TILE_DIRT);
    CHECK(world_get(&g.world, start.x + 10, s - 1) == TILE_AIR);

    /* The spawn portal cannot be built over or mined. */
    inventory_add(&g.inventory, ITEM_DIRT, 1);
    CHECK(!game_queue_action(&g, ACTION_PLACE, g.world.spawn_x, s - 1));
    CHECK(!game_queue_action(&g, ACTION_MINE, g.world.spawn_x, s - 1));
    game_free(&g);
}

static void test_bench(void)
{
    Game g;
    game_init(&g, 64, 48, 12, WORLD_FLAT);
    int s = g.world.surface_y, px = g.world.spawn_x;
    int n;

    CHECK(world_get(&g.world, px, s - 1) == TILE_SPAWN_PORTAL);
    CHECK(world_get(&g.world, px, s - 2) == TILE_SPAWN_PORTAL);
    CHECK(world_get(&g.world, px, s - 3) == TILE_SPAWN_PORTAL);
    CHECK(world_get(&g.world, px, s - 4) == TILE_AIR);
    CHECK(world_get(&g.world, px, s) == TILE_PORTAL_BASE);
    CHECK(!game_queue_action(&g, ACTION_MINE, px, s));
    CHECK(tile_def(TILE_SPAWN_PORTAL)->bench && !tile_def(TILE_SPAWN_PORTAL)->removable);
    CHECK(tile_def(TILE_WORKBENCH)->bench && tile_def(TILE_WORKBENCH)->removable);

    const Recipe *portal = bench_recipes(TILE_SPAWN_PORTAL, &n);
    CHECK(n == 1 && portal->output.id == ITEM_WORKBENCH && portal->inputs[0].id == ITEM_DIRT &&
          portal->inputs[0].count == 1);
    const Recipe *bench = bench_recipes(TILE_WORKBENCH, &n);
    CHECK(n == 1 && bench->output.id == ITEM_BASKET && bench->inputs[0].id == ITEM_DIRT);

    /* Using the portal from afar walks back to it, then opens it. */
    CHECK(game_click_tile(&g, px + 8, s - 1));
    run(&g, 3.0f);
    CHECK(!game_craft(&g, portal, 1));
    CHECK(game_queue_action(&g, ACTION_USE, px, s - 1));
    CHECK(!g.bench_open);
    run(&g, 3.0f);
    CHECK(is_idle(&g) && g.bench_open && g.bench_pos.x == px);
    CHECK(!game_queue_action(&g, ACTION_USE, px + 3, s));

    /* Crafting is capped by what is held and takes 100 ms each. */
    CHECK(!game_craft(&g, portal, 1));
    inventory_add(&g.inventory, ITEM_DIRT, 5);
    CHECK(recipe_max_crafts(&g.inventory, portal) == 5);
    CHECK(game_craft(&g, portal, 99));
    CHECK(g.craft_total == 5);
    run(&g, 0.25f);
    CHECK(count_item(&g, ITEM_WORKBENCH) == 2 && count_item(&g, ITEM_DIRT) == 3);
    run(&g, 0.5f);
    CHECK(count_item(&g, ITEM_WORKBENCH) == 5 && count_item(&g, ITEM_DIRT) == 0);
    CHECK(g.craft_remaining == 0);

    /* Walking away shuts the bench. */
    CHECK(game_click_tile(&g, px + 5, s - 1));
    run(&g, 2.0f);
    CHECK(!g.bench_open);

    /* A placed workbench can be opened, makes baskets, and can be removed. */
    inventory_select(&g.inventory, 2, -1);
    CHECK(inventory_selected(&g.inventory)->id == ITEM_WORKBENCH);
    CHECK(game_click_tile(&g, px + 7, s - 1));
    CHECK(game_queue_action(&g, ACTION_USE, px + 7, s - 1));
    run(&g, 2.0f);
    CHECK(world_get(&g.world, px + 7, s - 1) == TILE_WORKBENCH && g.bench_open);
    CHECK(count_item(&g, ITEM_WORKBENCH) == 4);
    inventory_add(&g.inventory, ITEM_DIRT, 2);
    CHECK(game_craft(&g, bench, 2));
    run(&g, 0.5f);
    CHECK(count_item(&g, ITEM_BASKET) == 3 && count_item(&g, ITEM_DIRT) == 0);

    CHECK(game_queue_action(&g, ACTION_MINE, px + 7, s - 1));
    run(&g, 2.0f);
    CHECK(world_get(&g.world, px + 7, s - 1) == TILE_AIR && !g.bench_open);
    CHECK(count_item(&g, ITEM_WORKBENCH) == 5);
    game_free(&g);
}

static void test_wrap(void)
{
    Game g;
    game_init(&g, 64, 48, 13, WORLD_FLAT);
    World *w = &g.world;
    int s = w->surface_y;

    CHECK(world_wrap_x(w, -1) == 63 && world_wrap_x(w, 64) == 0 && world_wrap_x(w, 130) == 2);
    CHECK(world_dx(w, 62, 1) == 3 && world_dx(w, 1, 62) == -3 && world_dx(w, 5, 9) == 4);
    CHECK(world_get(w, -1, s) == world_get(w, 63, s));

    /* Walking off one edge arrives at the other, and that is the short way. */
    g.player.feet = g.player.step_to = (TilePos){ 1, s - 1 };
    CHECK(game_click_tile(&g, 62, s - 1));
    run(&g, WALK_TIME * 3 + 0.05f);
    CHECK(is_idle(&g) && g.player.feet.x == 62);
    game_set_move_input(&g, 1, 0);
    run(&g, WALK_TIME * 4 + 0.05f);
    game_set_move_input(&g, 0, 0);
    run(&g, 0.5f);
    CHECK(g.player.feet.x >= 2 && g.player.feet.x <= 4);

    /* Reach works across the seam: mine at 63 while standing at 0. */
    g.player.feet = g.player.step_to = (TilePos){ 0, s - 1 };
    CHECK(game_click_tile(&g, 63, s));
    run(&g, 1.5f);
    CHECK(g.player.feet.x == 0 && world_get(w, 63, s) == TILE_AIR);
    CHECK(count_item(&g, ITEM_DIRT) == 1);
    game_free(&g);
}

static bool is_tree(TileType t)
{
    return tree_kind_of_log(t) != TREE_NONE || tree_kind_of_leaves(t) != TREE_NONE;
}

/* Feet level of a column: the height of its lowest air tile, ignoring trees. */
static int column_level(const World *w, int x)
{
    int row = 0;
    while (row < w->height &&
           (!tile_def(world_get(w, x, row))->solid || is_tree(world_get(w, x, row))))
        row++;
    return world_row_to_y(w, row) + 1;
}

static void test_terrain(void)
{
    bool saw_mountain = false, saw_valley = false, saw_oak = false, saw_cherry = false;
    for (uint32_t seed = 1; seed <= 12; seed++) {
        World w;
        CHECK(world_init(&w, 2048, 401, seed, WORLD_TERRAIN));
        CHECK(world_row_to_y(&w, 0) == 200 && world_row_to_y(&w, 400) == -200);

        /* Spawn at Y 0, standing on the portal's base inside a 3-tall portal. */
        CHECK(world_row_to_y(&w, w.spawn_y) == 0);
        CHECK(world_get(&w, w.spawn_x, w.spawn_y + 1) == TILE_PORTAL_BASE);
        for (int i = 0; i < 3; i++)
            CHECK(world_get(&w, w.spawn_x, w.spawn_y - i) == TILE_SPAWN_PORTAL);
        CHECK(world_get(&w, w.spawn_x, w.spawn_y - 3) == TILE_AIR);

        int lo = 1000, hi = -1000, max_step = 0, bad = 0;
        for (int x = 0; x < w.width; x++) {
            int level = column_level(&w, x);
            int step = abs(level - column_level(&w, x + 1)); /* x + 1 wraps at the seam */
            int top = world_y_to_row(&w, level - 1);
            if (level < lo) lo = level;
            if (level > hi) hi = level;
            if (step > max_step) max_step = step;

            /* Top to bottom: at most one grass, then dirt, then stone all the way. */
            int dirt = 0, y = top;
            if (world_get(&w, x, y) == TILE_GRASS || world_get(&w, x, y) == TILE_PORTAL_BASE) y++, dirt++;
            while (world_get(&w, x, y) == TILE_DIRT) y++, dirt++;
            for (; y < w.height; y++)
                if (world_get(&w, x, y) != TILE_STONE) bad++;
            if (dirt > 6) bad++;
            /* High peaks and deep floors are bare stone; the plains are not. */
            if ((level > 45 || level < -25) && dirt != 0) bad++;
            if (world_get(&w, x, top) == TILE_DIRT) bad++;

            /* Trees stand on grass: a trunk has trunk or grass under it. */
            for (int r = 0; r < top; r++) {
                TileType t = world_get(&w, x, r);
                if (tree_kind_of_log(t) == TREE_NONE) continue;
                TileType under = world_get(&w, x, r + 1);
                if (under != t && under != TILE_GRASS) bad++;
                if (t == TILE_OAK_LOG) saw_oak = true;
                if (t == TILE_CHERRY_LOG) saw_cherry = true;
            }
        }
        CHECK(bad == 0);
        CHECK(lo >= -FEATURE_MAX_SIZE - 2 && hi <= PLAINS_MAX_LEVEL + FEATURE_MAX_SIZE + 2);
        CHECK(max_step <= 6);
        if (hi >= 40) saw_mountain = true;
        if (lo <= -20) saw_valley = true;

        /* Flat ground either side of the spawn. */
        for (int d = -3; d <= 3; d++) CHECK(column_level(&w, w.spawn_x + d) == 0);
        world_free(&w);
    }
    CHECK(saw_mountain && saw_valley && saw_oak && saw_cherry);

    /* The full-size world can be walked and mined like the small ones. */
    Game g;
    CHECK(game_init(&g, 2048, 401, 3, WORLD_TERRAIN));
    TilePos start = g.player.feet;
    CHECK(game_click_tile(&g, start.x + 2, start.y + 1));
    CHECK(!game_click_tile(&g, start.x + 2, start.y - 30));
    run(&g, 2.0f);
    CHECK(is_idle(&g) && world_get(&g.world, start.x + 2, start.y + 1) == TILE_AIR);
    CHECK(count_item(&g, ITEM_DIRT) == 1);
    game_free(&g);
}

/* Width of a tree's row: its trunk and leaves, counted across the trunk. */
static int tree_row_width(const World *w, int x, int row)
{
    int n = 0;
    for (int dx = -4; dx <= 4; dx++)
        if (is_tree(world_get(w, x + dx, row))) n++;
    return n;
}

static int trunk_height(const World *w, int x, int base)
{
    int h = 0;
    while (tree_kind_of_log(world_get(w, x, base - h)) != TREE_NONE) h++;
    return h;
}

static void test_trees(void)
{
    Game g;
    game_init(&g, 64, 48, 14, WORLD_FLAT);
    World *w = &g.world;
    int s = w->surface_y, base = s - 1;
    uint32_t rng = 99;

    /* An oak grows one block at a time into a stick with a ball on top. */
    int ox = 10, blocks = 1, steps = 0;
    tree_plant(w, ox, base, TREE_OAK);
    for (;;) {
        TreeGrowth growth = tree_grow(w, ox, base, &rng);
        if (!growth.grew) break;
        int now = 0;
        for (int y = 0; y < s; y++) now += tree_row_width(w, ox, y);
        /* Extending the trunk adds one block; it may also shed leaves that hit something. */
        CHECK(now == blocks + 1);
        blocks = now;
        steps++;
    }
    CHECK(trunk_height(w, ox, base) == OAK_MAX_HEIGHT);
    int top = base - OAK_MAX_HEIGHT + 1;
    static const int oak_rows[] = { 3, 5, 5, 3, 1, 1, 1, 1 };
    for (int i = 0; i < 8; i++) CHECK(tree_row_width(w, ox, top - 1 + i) == oak_rows[i]);
    CHECK(world_get(w, ox, top - 1) == TILE_OAK_LEAVES && world_get(w, ox, top) == TILE_OAK_LOG);
    CHECK(steps == 6 + 13);

    /* A cherry ends up 3, 3, 4, 4, 5, 5 wide over two blocks of bare trunk. */
    int cx = 30;
    tree_plant(w, cx, base, TREE_CHERRY);
    while (tree_grow(w, cx, base, &rng).grew) {}
    CHECK(trunk_height(w, cx, base) == CHERRY_MAX_HEIGHT);
    static const int cherry_rows[] = { 3, 3, 4, 4, 5, 5, 1, 1 };
    for (int i = 0; i < 8; i++) {
        int row = base - CHERRY_MAX_HEIGHT + 1 + i;
        CHECK(tree_row_width(w, cx, row) == cherry_rows[i]);
        CHECK(world_get(w, cx, row) == TILE_CHERRY_LOG);
    }
    CHECK(world_get(w, cx, base - CHERRY_MAX_HEIGHT) == TILE_AIR);

    /* A one-block tree can put a leaf out sideways before it grows up. When
     * the trunk does extend, the leaves ride up with it, except one with a
     * block in its way, which is lost. */
    int tx = 45;
    tree_plant(w, tx, base, TREE_CHERRY);
    bool leaf_first = false;
    for (uint32_t seed = 1; seed < 20 && !leaf_first; seed++) {
        uint32_t r = seed;
        TreeGrowth growth = tree_grow(w, tx, base, &r);
        leaf_first = growth.grew && !growth.extended;
    }
    CHECK(leaf_first && trunk_height(w, tx, base) == 1);
    world_set(w, tx - 1, base, TILE_CHERRY_LEAVES);
    world_set(w, tx + 1, base, TILE_CHERRY_LEAVES);
    world_set(w, tx + 1, base - 1, TILE_STONE);
    TreeGrowth growth = tree_grow(w, tx, base, &rng);
    CHECK(growth.extended && trunk_height(w, tx, base) == 2);
    CHECK(world_get(w, tx - 1, base - 1) == TILE_CHERRY_LEAVES);
    CHECK(world_get(w, tx - 1, base) == TILE_AIR && world_get(w, tx + 1, base) == TILE_AIR);
    CHECK(world_get(w, tx + 1, base - 1) == TILE_STONE);

    /* Breaking a leaf gives a stick, a trunk gives wood, and wood placed
     * again is an ordinary block, not a tree. */
    CHECK(item_for_tile(TILE_OAK_LEAVES) == ITEM_STICK && item_for_tile(TILE_CHERRY_LEAVES) == ITEM_STICK);
    CHECK(item_for_tile(TILE_OAK_LOG) == ITEM_WOOD && item_for_tile(TILE_CHERRY_LOG) == ITEM_WOOD);
    CHECK(item_def(ITEM_WOOD)->places == TILE_WOOD && tree_kind_of_log(TILE_WOOD) == TREE_NONE);
    game_free(&g);
}

static void test_ticks(void)
{
    Game g;
    game_init(&g, 64, 48, 15, WORLD_FLAT);
    World *w = &g.world;
    int s = w->surface_y, base = s - 1;
    TilePos start = g.player.feet;

    /* Ten ticks a second, or whatever the world is set to. */
    CHECK(g.tick_rate == 10);
    run(&g, 1.0f);
    CHECK(g.tick >= 9 && g.tick <= 11);
    g.tick_rate = 40;
    uint64_t before = g.tick;
    run(&g, 1.0f);
    CHECK(g.tick - before >= 39 && g.tick - before <= 41);

    /* 1000 ticks of day, 800 of night, with a fade between them. */
    bool day;
    g.tick = 500;
    CHECK(game_daylight(&g) == 1.0f && game_day_phase(&g, &day) == 0.5f && day);
    g.tick = 1400;
    CHECK(game_daylight(&g) == 0.0f && game_day_phase(&g, &day) == 0.5f && !day);
    g.tick = 1000;
    CHECK(game_daylight(&g) > 0.4f && game_daylight(&g) < 0.6f);
    g.tick = 1800;
    CHECK(game_daylight(&g) > 0.4f && game_daylight(&g) < 0.6f && game_day_phase(&g, &day) == 0.0f && day);
    g.tick = 1799;
    float late = game_daylight(&g);
    g.tick = 1801;
    CHECK(late < game_daylight(&g) && late > 0.3f);

    /* Random events alone grow a sapling into a full tree and hang fruit on it. */
    int tx = world_wrap_x(w, start.x + 4);
    tree_plant(w, tx, base, TREE_OAK);
    for (int i = 0; i < 150000; i++) game_tick(&g);
    CHECK(trunk_height(w, tx, base) == OAK_MAX_HEIGHT);
    int fruit = -1, fruits = 0;
    for (int i = 0; i < MAX_DROPS; i++) {
        const Drop *d = &g.drops[i];
        if (!d->active) continue;
        CHECK(d->attached && d->item == ITEM_ACORN && d->ttl > 0 && d->ttl <= FRUIT_TICKS);
        CHECK(world_get(w, d->x, (int)d->y) == TILE_OAK_LEAVES);
        fruits++;
        fruit = i;
    }
    CHECK(fruits > 0 && fruits <= 13);

    /* Fruit stays on the tree however long the player stands under it... */
    Drop kept = g.drops[fruit];
    g.player.feet = g.player.step_to = (TilePos){ tx - 1, base };
    g.tick_rate = 1;
    run(&g, 1.0f);
    CHECK(g.drops[fruit].active && g.drops[fruit].y == kept.y && count_item(&g, ITEM_ACORN) == 0);

    /* ...until it is tapped. One on the underside of the crown can be reached
     * by climbing the trunk; one on top of the crown cannot, and is refused. */
    int top = base - OAK_MAX_HEIGHT + 1, low = -1, high = -1;
    for (int i = 0; i < MAX_DROPS && (low < 0 || high < 0); i++) {
        if (g.drops[i].active) continue;
        TilePos at = low < 0 ? (TilePos){ tx - 1, top + 2 } : (TilePos){ tx, top - 1 };
        g.drops[i] = (Drop){ true, ITEM_ACORN, at.x, (float)at.y, 0.0f, 0.0f, true, FRUIT_TICKS,
                             ++g.next_drop_id };
        if (low < 0) low = i;
        else high = i;
    }
    float cx, cy;
    game_drop_center(&g, low, &cx, &cy);
    /* (Another fruit may already hang on the same leaf; either will do.) */
    int hit = game_drop_at(&g, cx + 0.2f, cy - 0.2f);
    CHECK(hit >= 0 && g.drops[hit].x == g.drops[low].x && g.drops[hit].y == g.drops[low].y);
    CHECK(game_drop_at(&g, cx + 9.0f, cy) == -1);
    CHECK(!game_queue_collect(&g, high));
    CHECK(game_queue_collect(&g, low));
    CHECK(g.queue[0].type == ACTION_COLLECT);
    run(&g, 6.0f);
    CHECK(is_idle(&g) && count_item(&g, ITEM_ACORN) == 1 && !g.drops[low].active);
    CHECK(g.player.climbing && g.drops[high].active);
    kept = g.drops[high];

    /* Fruit that is left alone disappears after 1800 ticks. */
    for (int i = 0; i < FRUIT_TICKS; i++) game_tick(&g);
    int old = 0;
    for (int i = 0; i < MAX_DROPS; i++)
        if (g.drops[i].active && g.drops[i].id <= kept.id) old++;
    CHECK(old == 0);

    /* Breaking the leaf under a fruit lets it fall. */
    for (int i = 0; i < 20000; i++) game_tick(&g);
    fruit = -1;
    for (int i = 0; i < MAX_DROPS && fruit < 0; i++)
        if (g.drops[i].active && g.drops[i].attached) fruit = i;
    CHECK(fruit >= 0);
    if (fruit >= 0) {
        world_set(w, g.drops[fruit].x, (int)g.drops[fruit].y, TILE_AIR);
        game_tick(&g);
        CHECK(!g.drops[fruit].attached);
    }
    game_free(&g);
}

static void test_save(void)
{
    const char *path = "build/test_save.world";
    Game g;
    game_init(&g, 64, 48, 16, WORLD_FLAT);
    TilePos start = g.player.feet;
    int s = g.world.surface_y;

    CHECK(game_click_tile(&g, start.x + 3, s));
    run(&g, 2.0f);
    inventory_add(&g.inventory, ITEM_STONE, 7);
    tree_plant(&g.world, start.x - 5, s - 1, TREE_CHERRY);
    g.world.background[5] = TILE_STONE;
    g.tick = 12345;
    g.tick_rate = 25;
    CHECK(save_game(&g, path));

    Game loaded;
    CHECK(load_game(&loaded, path));
    size_t cells = (size_t)g.world.width * g.world.height;
    CHECK(loaded.world.width == 64 && loaded.world.height == 48 && loaded.world.seed == 16);
    CHECK(memcmp(loaded.world.tiles, g.world.tiles, cells) == 0);
    CHECK(memcmp(loaded.world.background, g.world.background, cells) == 0);
    CHECK(memcmp(&loaded.inventory, &g.inventory, sizeof g.inventory) == 0);
    CHECK(memcmp(loaded.drops, g.drops, sizeof g.drops) == 0);
    CHECK(loaded.player.feet.x == g.player.feet.x && loaded.player.feet.y == g.player.feet.y);
    CHECK(loaded.tick == 12345 && loaded.tick_rate == 25 && loaded.rng == g.rng);
    CHECK(loaded.world.y_top == g.world.y_top && loaded.world.spawn_x == g.world.spawn_x);
    CHECK(count_item(&loaded, ITEM_DIRT) == 1 && count_item(&loaded, ITEM_STONE) == 7);

    /* A loaded game plays on. */
    CHECK(game_click_tile(&loaded, start.x + 5, s - 1));
    run(&loaded, 2.0f);
    CHECK(is_idle(&loaded) && loaded.player.feet.x == world_wrap_x(&loaded.world, start.x + 5));
    game_free(&loaded);

    CHECK(!load_game(&loaded, "build/no_such_file.world"));
    CHECK(!load_game(&loaded, "Makefile"));
    game_free(&g);
    remove(path);
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
    test_place();
    test_bench();
    test_wrap();
    test_terrain();
    test_trees();
    test_ticks();
    test_save();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
