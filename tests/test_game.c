/* Headless tests for the game rules: no window, just the simulation. */
#include "../src/game.h"
#include "../src/save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Content is looked up by name: the engine has no ids of its own. */
#define TILE_DIRT tile_id("dirt")
#define TILE_GRASS tile_id("grass")
#define TILE_STONE tile_id("stone")
#define TILE_WORKBENCH tile_id("workbench")
#define TILE_SPAWN_PORTAL tile_id("spawn_portal")
#define TILE_SPAWN_PORTAL_TOP tile_id("spawn_portal_top")
#define TILE_PORTAL_BASE tile_id("portal_base")
#define TILE_OAK_LOG tile_id("oak_log")
#define TILE_OAK_LEAVES tile_id("oak_leaves")
#define TILE_CHERRY_LOG tile_id("cherry_log")
#define TILE_CHERRY_LEAVES tile_id("cherry_leaves")
#define TILE_WOOD tile_id("wood")
#define ITEM_BASKET item_id("basket")
#define ITEM_DIRT item_id("dirt")
#define ITEM_WORKBENCH item_id("workbench")
#define ITEM_STONE item_id("stone")
#define ITEM_STICK item_id("stick")
#define ITEM_WOOD item_id("wood")
#define ITEM_ACORN item_id("acorn")
#define ITEM_CHERRY item_id("cherry")
#define ITEM_CHEST item_id("chest")
#define TILE_WORKBENCH_2 tile_id("workbench_2")
#define TREE_OAK tree_id("oak")
#define TREE_CHERRY tree_id("cherry")
#define OAK_MAX_HEIGHT (content.trees[TREE_OAK].max_height)
#define CHERRY_MAX_HEIGHT (content.trees[TREE_CHERRY].max_height)
#define FRUIT_TICKS (content.trees[TREE_OAK].fruit_ticks)
#define MAX_STACK 255 /* "the whole stack" when moving */
#define FEATURE_MAX_SIZE 60
#define PLAINS_MAX_LEVEL 20
#define BG LAYER_BACKGROUND

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
    g.drops[0] = (Drop){ true, (uint8_t)ITEM_DIRT, start.x + 6, (float)(s - 5), 0.0f, 0.0f, false, 0, 1, 0 };
    run(&g, 2.0f);
    CHECK(g.drops[0].active && g.drops[0].y == (float)(s - 1));
    CHECK(game_click_tile(&g, start.x + 5, start.y));
    run(&g, 2.0f);
    CHECK(!g.drops[0].active && count_item(&g, ITEM_DIRT) == 2);

    /* With no room, it is left on the ground. */
    inventory_add(&g.inventory, ITEM_DIRT, 99 * 12);
    int before = count_item(&g, ITEM_DIRT);
    g.drops[0] = (Drop){ true, (uint8_t)ITEM_DIRT, g.player.feet.x, (float)g.player.feet.y, 0.0f, 0.0f, false, 0, 2, 0 };
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
    CHECK(world_get(&g.world, px, s - 3) == TILE_SPAWN_PORTAL_TOP);
    CHECK(world_get(&g.world, px, s - 4) == TILE_AIR);
    CHECK(world_get(&g.world, px, s) == TILE_PORTAL_BASE);
    CHECK(!game_queue_action(&g, ACTION_MINE, px, s));
    CHECK(tile_def(TILE_SPAWN_PORTAL)->bench && !tile_def(TILE_SPAWN_PORTAL)->removable);
    CHECK(tile_def(TILE_SPAWN_PORTAL_TOP)->bench && !tile_def(TILE_SPAWN_PORTAL_TOP)->removable);
    CHECK(tile_def(TILE_WORKBENCH)->bench && tile_def(TILE_WORKBENCH)->removable);

    const Recipe *portal = NULL, *bench = NULL, *other = NULL;
    n = bench_recipes(TILE_SPAWN_PORTAL, &portal, 1);
    CHECK(bench_recipes(TILE_SPAWN_PORTAL_TOP, &other, 1) == 1 && other == portal);
    CHECK(bench_recipes(TILE_DIRT, &other, 1) == 0);
    CHECK(n == 1 && portal->output.id == ITEM_WORKBENCH && portal->inputs[0].id == ITEM_DIRT &&
          portal->inputs[0].count == 1);
    const Recipe *list[8];
    n = bench_recipes(TILE_WORKBENCH, list, 8);
    bench = list[0];
    const Recipe *chest = list[1];
    CHECK(n == 2 && bench_recipes(TILE_WORKBENCH_2, list, 8) == 2 && list[1] == chest);
    CHECK(bench->output.id == ITEM_BASKET && bench->input_count == 1);
    CHECK(bench->inputs[0].id == ITEM_STICK && bench->inputs[0].count == 10);
    CHECK(chest->output.id == ITEM_CHEST && chest->inputs[0].id == ITEM_WOOD && chest->inputs[0].count == 1);
    CHECK(chest->level == 2 && recipe_locked(TILE_WORKBENCH, chest) && !recipe_locked(TILE_WORKBENCH_2, chest));
    CHECK(!recipe_locked(TILE_WORKBENCH, bench));

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
    CHECK(!game_craft(&g, portal, 1)); /* not this bench's recipe */
    inventory_add(&g.inventory, ITEM_STICK, 25);
    CHECK(recipe_max_crafts(&g.inventory, bench) == 2);
    CHECK(game_craft(&g, bench, 5));
    run(&g, 0.5f);
    CHECK(count_item(&g, ITEM_BASKET) == 3 && count_item(&g, ITEM_STICK) == 5);

    /* The chest is on the list but locked until the bench is level 2, which
     * costs 5 stone. */
    inventory_add(&g.inventory, ITEM_WOOD, 3);
    CHECK(!game_craft(&g, chest, 1) && count_item(&g, ITEM_CHEST) == 0);
    CHECK(!game_upgrade_bench(&g));
    inventory_add(&g.inventory, ITEM_STONE, 7);
    CHECK(game_upgrade_bench(&g));
    CHECK(world_get(&g.world, px + 7, s - 1) == TILE_WORKBENCH_2 && count_item(&g, ITEM_STONE) == 2);
    CHECK(g.bench_open && !game_upgrade_bench(&g));
    CHECK(game_craft(&g, chest, 2));
    run(&g, 0.5f);
    CHECK(count_item(&g, ITEM_CHEST) == 2 && count_item(&g, ITEM_WOOD) == 1);
    CHECK(game_craft(&g, bench, 1) == false && count_item(&g, ITEM_STICK) == 5);
    game_close_bench(&g);
    CHECK(!game_upgrade_bench(&g));

    CHECK(game_queue_action(&g, ACTION_MINE, px + 7, s - 1));
    run(&g, 2.0f);
    CHECK(world_get(&g.world, px + 7, s - 1) == TILE_AIR && !g.bench_open);
    /* The upgraded bench comes back as itself, level and all. */
    CHECK(count_item(&g, ITEM_WORKBENCH) == 4 && count_item(&g, item_id("workbench_2")) == 1);
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
    return tile_def(t)->log_of >= 0 || tile_def(t)->leaves_of >= 0;
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

/* Soil is dirt and whatever veins form in dirt; rock likewise for stone. */
static bool vein_in(TileType t, TileType host)
{
    for (int i = 0; i < content.vein_count; i++) {
        const VeinDef *v = &content.veins[i];
        for (int h = 0; h < v->host_count; h++)
            if (v->tile == t && v->hosts[h] == host) return true;
    }
    return false;
}

static bool in_soil(TileType t)
{
    return t == TILE_DIRT || vein_in(t, TILE_DIRT);
}

static bool in_rock(TileType t)
{
    return t == TILE_STONE || vein_in(t, TILE_STONE);
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
            CHECK(world_get(&w, w.spawn_x, w.spawn_y - i) == (i == 2 ? TILE_SPAWN_PORTAL_TOP : TILE_SPAWN_PORTAL));
        CHECK(world_get(&w, w.spawn_x, w.spawn_y - 3) == TILE_AIR);

        int lo = 1000, hi = -1000, max_step = 0, bad = 0;
        for (int x = 0; x < w.width; x++) {
            int level = column_level(&w, x);
            int step = abs(level - column_level(&w, x + 1)); /* x + 1 wraps at the seam */
            int top = world_y_to_row(&w, level - 1);
            if (level < lo) lo = level;
            if (level > hi) hi = level;
            if (step > max_step) max_step = step;

            /* Top to bottom: at most one grass, then soil, then rock all the
             * way, each with its own veins in it. */
            int dirt = 0, y = top;
            if (world_get(&w, x, y) == TILE_GRASS || world_get(&w, x, y) == TILE_PORTAL_BASE) y++, dirt++;
            while (in_soil(world_get(&w, x, y))) y++, dirt++;
            for (; y < w.height; y++)
                if (!in_rock(world_get(&w, x, y))) bad++;
            if (dirt > 6) bad++;
            /* High peaks and deep floors are bare stone; the plains are not. */
            if ((level > 45 || level < -25) && dirt != 0) bad++;
            if (world_get(&w, x, top) == TILE_DIRT) bad++;

            /* Trees stand on grass: a trunk has trunk or grass under it. */
            for (int r = 0; r < top; r++) {
                TileType t = world_get_at(&w, BG, x, r);
                if (is_tree(world_get(&w, x, r))) bad++; /* trees keep to the background */
                if (tile_def(t)->log_of < 0) continue;
                if (world_get_at(&w, BG, x, r + 1) != t && world_get(&w, x, r + 1) != TILE_GRASS) bad++;
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
        if (is_tree(world_get_at(w, BG, x + dx, row))) n++;
    return n;
}

static int trunk_height(const World *w, int x, int base)
{
    int h = 0;
    while (tile_def(world_get_at(w, BG, x, base - h))->log_of >= 0) h++;
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
    CHECK(world_get_at(w, BG, ox, top - 1) == TILE_OAK_LEAVES && world_get_at(w, BG, ox, top) == TILE_OAK_LOG);
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
        CHECK(world_get_at(w, BG, cx, row) == TILE_CHERRY_LOG && world_get(w, cx, row) == TILE_AIR);
    }
    CHECK(world_get_at(w, BG, cx, base - CHERRY_MAX_HEIGHT) == TILE_AIR);

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
    world_set_at(w, BG, tx - 1, base, TILE_CHERRY_LEAVES);
    world_set_at(w, BG, tx + 1, base, TILE_CHERRY_LEAVES);
    world_set(w, tx + 1, base - 1, TILE_STONE);
    TreeGrowth growth = tree_grow(w, tx, base, &rng);
    CHECK(growth.extended && trunk_height(w, tx, base) == 2);
    CHECK(world_get_at(w, BG, tx - 1, base - 1) == TILE_CHERRY_LEAVES);
    CHECK(world_get_at(w, BG, tx - 1, base) == TILE_AIR && world_get_at(w, BG, tx + 1, base) == TILE_AIR);
    CHECK(world_get_at(w, BG, tx + 1, base - 1) == TILE_AIR);
    CHECK(world_get(w, tx + 1, base - 1) == TILE_STONE);

    /* Breaking a leaf gives a stick, a trunk gives wood, and wood placed
     * again is an ordinary block, not a tree. */
    CHECK(tile_def(TILE_OAK_LEAVES)->drop == ITEM_STICK && tile_def(TILE_CHERRY_LEAVES)->drop == ITEM_STICK);
    CHECK(tile_def(TILE_OAK_LOG)->drop == ITEM_WOOD && tile_def(TILE_CHERRY_LOG)->drop == ITEM_WOOD);
    CHECK(item_def(ITEM_WOOD)->places == TILE_WOOD && tile_def(TILE_WOOD)->log_of < 0);
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
        CHECK(world_get_at(w, BG, d->x, (int)d->y) == TILE_OAK_LEAVES);
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

    /* ...until it is tapped: the player climbs the tree to it. */
    int top = base - OAK_MAX_HEIGHT + 1, low = -1, high = -1;
    for (int i = 0; i < MAX_DROPS && (low < 0 || high < 0); i++) {
        if (g.drops[i].active) continue;
        TilePos at = low < 0 ? (TilePos){ tx - 1, top + 2 } : (TilePos){ tx, top - 1 };
        g.drops[i] = (Drop){ true, (uint8_t)ITEM_ACORN, at.x, (float)at.y, 0.0f, 0.0f, true, FRUIT_TICKS,
                             ++g.next_drop_id, 0 };
        if (low < 0) low = i;
        else high = i;
    }
    float cx, cy;
    game_drop_center(&g, low, &cx, &cy);
    /* (Another fruit may already hang on the same leaf; either will do.) */
    int hit = game_drop_at(&g, cx + 0.2f, cy - 0.2f);
    CHECK(hit >= 0 && g.drops[hit].x == g.drops[low].x && g.drops[hit].y == g.drops[low].y);
    CHECK(game_drop_at(&g, cx + 9.0f, cy) == -1);
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
        world_set_at(w, BG, g.drops[fruit].x, (int)g.drops[fruit].y, TILE_AIR);
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
    g.world.layers[BG][5] = (uint8_t)TILE_STONE;
    g.tick = 12345;
    g.tick_rate = 25;
    CHECK(save_game(&g, path));

    Game loaded;
    CHECK(load_game(&loaded, path));
    size_t cells = (size_t)g.world.width * g.world.height;
    CHECK(loaded.world.width == 64 && loaded.world.height == 48 && loaded.world.seed == 16);
    CHECK(memcmp(loaded.world.layers[0], g.world.layers[0], cells) == 0);
    CHECK(memcmp(loaded.world.layers[BG], g.world.layers[BG], cells) == 0);
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

/* Trees stand behind the player: walked past on the ground, climbed to be mined. */
static void test_background_trees(void)
{
    Game g;
    game_init(&g, 64, 48, 17, WORLD_FLAT);
    World *w = &g.world;
    TilePos start = g.player.feet;
    int base = w->surface_y - 1, tx = start.x + 3;
    uint32_t rng = 5;

    tree_plant(w, tx, base, TREE_OAK);
    while (tree_grow(w, tx, base, &rng).grew) {}
    int top = base - OAK_MAX_HEIGHT + 1;
    CHECK(world_get(w, tx, base) == TILE_AIR && world_get_at(w, BG, tx, base) == TILE_OAK_LOG);

    /* Walking goes straight through it, by key and by click. */
    game_set_move_input(&g, 1, 0);
    Trace tr = run_traced(&g, WALK_TIME * 6 + 0.05f);
    game_set_move_input(&g, 0, 0);
    run(&g, 0.5f);
    CHECK(!tr.climbed && tr.deepest == base && g.player.feet.x >= tx + 3 && g.player.feet.y == base);
    CHECK(game_click_tile(&g, start.x, base - 1));
    CHECK(g.queue[0].type == ACTION_MOVE);
    tr = run_traced(&g, 3.0f);
    CHECK(!tr.climbed && g.player.feet.x == start.x && g.player.feet.y == base);

    /* Up on a tree: W in front of the trunk grabs it, and the player hangs there. */
    g.player.feet = g.player.step_to = (TilePos){ tx, base };
    game_set_move_input(&g, 0, -1);
    run(&g, CLIMB_TIME * 3 + 0.02f);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(g.player.climbing && g.player.feet.x == tx && g.player.feet.y < base - 1);
    CHECK(game_player_grip_side(&g) == 0);
    game_set_move_input(&g, 0, 1);
    run(&g, 2.0f);
    game_set_move_input(&g, 0, 0);
    CHECK(!g.player.climbing && g.player.feet.y == base);

    /* Clicking a leaf at the very top queues mining it: the player climbs
     * the tree to get within reach, and the leaf leaves a stick. */
    g.player.feet = g.player.step_to = (TilePos){ start.x, base };
    CHECK(game_click_tile(&g, tx, top - 1));
    CHECK(g.queue[0].type == ACTION_MINE);
    tr = run_traced(&g, 8.0f);
    CHECK(is_idle(&g) && tr.climbed);
    CHECK(world_get_at(w, BG, tx, top - 1) == TILE_AIR);
    int sticks = count_item(&g, ITEM_STICK);
    for (int i = 0; i < MAX_DROPS; i++)
        if (g.drops[i].active && g.drops[i].item == ITEM_STICK) sticks++;
    CHECK(sticks == 1);

    /* A trunk block gives wood, and a block in front is mined before the tree behind it. */
    CHECK(game_click_tile(&g, tx, base - 1));
    run(&g, 8.0f);
    CHECK(world_get_at(w, BG, tx, base - 1) == TILE_AIR && world_get_at(w, BG, tx, base) == TILE_OAK_LOG);
    world_set(w, tx, base, TILE_STONE);
    g.player.feet = g.player.step_to = (TilePos){ tx - 1, base };
    CHECK(game_click_tile(&g, tx, base));
    run(&g, 3.0f);
    CHECK(world_get(w, tx, base) == TILE_AIR && world_get_at(w, BG, tx, base) == TILE_OAK_LOG);
    game_free(&g);
}

static void test_planting(void)
{
    Game g;
    game_init(&g, 64, 48, 19, WORLD_FLAT);
    World *w = &g.world;
    TilePos start = g.player.feet;
    int s = w->surface_y;

    /* Clicking dirt with nothing special in hand mines it. */
    CHECK(game_click_tile(&g, start.x + 2, s) && g.queue[0].type == ACTION_MINE);
    game_cancel_actions(&g);

    /* With an acorn it plants an oak: one trunk block on top of the dirt. */
    inventory_add(&g.inventory, ITEM_ACORN, 2);
    inventory_add(&g.inventory, ITEM_CHERRY, 1);
    inventory_select(&g.inventory, 1, -1);
    CHECK(inventory_selected(&g.inventory)->id == ITEM_ACORN);
    CHECK(game_click_tile(&g, start.x + 4, s));
    CHECK(g.queue[0].type == ACTION_PLANT && g.queue[0].item == ITEM_ACORN);
    /* The same spot cannot be planted twice, even while the first is only queued. */
    CHECK(game_click_tile(&g, start.x + 4, s) && g.queue[1].type == ACTION_MINE);
    game_cancel_actions(&g);
    CHECK(game_click_tile(&g, start.x + 4, s));
    run(&g, 3.0f);
    CHECK(is_idle(&g) && world_get(w, start.x + 4, s) == TILE_DIRT);
    CHECK(world_get_at(w, BG, start.x + 4, s - 1) == TILE_OAK_LOG);
    CHECK(world_get_at(w, BG, start.x + 4, s - 2) == TILE_AIR && count_item(&g, ITEM_ACORN) == 1);

    /* It also takes on grass, and a cherry plants a cherry tree. */
    world_set(w, start.x + 8, s, TILE_GRASS);
    inventory_select(&g.inventory, 2, -1);
    CHECK(game_click_tile(&g, start.x + 8, s) && g.queue[0].type == ACTION_PLANT);
    run(&g, 3.0f);
    CHECK(world_get_at(w, BG, start.x + 8, s - 1) == TILE_CHERRY_LOG && count_item(&g, ITEM_CHERRY) == 0);

    /* Not on stone, and not where something is in the way: those are mined as usual. */
    inventory_select(&g.inventory, 1, -1);
    world_set(w, start.x + 10, s, TILE_STONE);
    CHECK(game_click_tile(&g, start.x + 10, s) && g.queue[0].type == ACTION_MINE);
    game_cancel_actions(&g);
    CHECK(game_click_tile(&g, start.x + 12, s + 1) == false);
    world_set(w, start.x + 13, s - 1, TILE_STONE);
    CHECK(game_click_tile(&g, start.x + 13, s) == false || g.queue[0].type == ACTION_MINE);
    game_cancel_actions(&g);

    /* From there it grows by random events like any other tree. */
    for (int i = 0; i < 150000; i++) game_tick(&g);
    CHECK(trunk_height(w, start.x + 4, s - 1) == OAK_MAX_HEIGHT);
    CHECK(trunk_height(w, start.x + 8, s - 1) == CHERRY_MAX_HEIGHT);
    game_free(&g);
}

/* Sizes of the connected groups of one tile in a world. */
typedef struct {
    int blocks, groups, smallest, largest, widest;
    int over; /* groups bigger than `limit` */
} VeinStats;

static VeinStats vein_stats(const World *w, TileType tile, int limit)
{
    VeinStats st = { 0, 0, 1 << 30, 0, 0, 0 };
    size_t cells = (size_t)w->width * w->height;
    uint8_t *seen = calloc(cells, 1);
    int *stack = malloc(sizeof(int) * cells);
    for (int start = 0; start < (int)cells; start++) {
        if (seen[start] || w->layers[0][start] != tile) continue;
        int n = 0, top = 0, min_x = w->width, max_x = -1;
        stack[top++] = start;
        seen[start] = 1;
        while (top > 0) {
            int c = stack[--top], x = c % w->width, y = c / w->width;
            n++;
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            /* Blocks touching at a corner count as joined: a scratch runs diagonally. */
            for (int d = 0; d < 9; d++) {
                int nx = world_wrap_x(w, x + d % 3 - 1), ny = y + d / 3 - 1;
                if (ny < 0 || ny >= w->height) continue;
                int next = ny * w->width + nx;
                if (seen[next] || w->layers[0][next] != tile) continue;
                seen[next] = 1;
                stack[top++] = next;
            }
        }
        st.blocks += n;
        st.groups++;
        if (n < st.smallest) st.smallest = n;
        if (n > st.largest) st.largest = n;
        if (max_x - min_x + 1 > st.widest && max_x - min_x < w->width / 2) st.widest = max_x - min_x + 1;
        if (n > limit) st.over++;
    }
    free(seen);
    free(stack);
    return st;
}

static void test_veins(void)
{
    static const char *ores[] = { "copper_ore", "tin_ore", "iron_ore", "gold_ore", "platinum_ore",
                                  "titanium_ore" };
    int total[6] = { 0 }, flint_total = 0, dirt_copper = 0, dirt_tin = 0, mega = 0;

    for (uint32_t seed = 1; seed <= 6; seed++) {
        World w;
        CHECK(world_init(&w, 2048, 401, seed, WORLD_TERRAIN));
        TileType flint = tile_id("flint");

        /* Flint: veins of 4 to 10 in the dirt, never touching the open air. */
        VeinStats fs = vein_stats(&w, flint, 10);
        CHECK(fs.groups > 20 && fs.smallest >= 4 && fs.largest <= 10 && fs.over == 0);
        flint_total += fs.blocks;
        int exposed = 0, misplaced = 0;
        for (int y = 1; y < w.height - 1; y++) {
            for (int x = 0; x < w.width; x++) {
                TileType t = world_get(&w, x, y);
                if (t == flint) {
                    if (!world_is_solid(&w, x, y - 1) || !world_is_solid(&w, x + 1, y) ||
                        !world_is_solid(&w, x - 1, y) || !world_is_solid(&w, x, y + 1))
                        exposed++;
                }
                /* Nothing is left hanging in the air or sitting on the grass. */
                if (vein_in(t, TILE_STONE) && !vein_in(t, TILE_DIRT) && world_get(&w, x, y + 1) == TILE_AIR)
                    misplaced++;
            }
        }
        CHECK(exposed == 0 && misplaced == 0);

        /* Ores: every kind is somewhere in every world, in veins of its size. */
        for (int i = 0; i < 6; i++) {
            int limit = i < 2 ? 30 : i < 4 ? 8 : 3;
            VeinStats st = vein_stats(&w, tile_id(ores[i]), limit);
            CHECK(st.groups > 0 && st.smallest >= 2 && st.over == 0);
            total[i] += st.blocks;
            /* Copper and tin also come in long scratches of 10 to 30 blocks. */
            if (i < 2) {
                CHECK(st.largest >= 10 && st.widest >= 8);
                /* Every vein is either an ordinary one or a scratch, and the
                 * scratches are much the rarer. */
                VeinStats small = vein_stats(&w, tile_id(ores[i]), 6);
                VeinStats gap = vein_stats(&w, tile_id(ores[i]), 9);
                CHECK(small.over == gap.over && small.over > 0 && small.over * 8 < small.groups);
                mega += small.over;
            }
        }
        dirt_copper += vein_stats(&w, tile_id("copper_dirt"), 6).blocks;
        dirt_tin += vein_stats(&w, tile_id("tin_dirt"), 6).blocks;
        world_free(&w);
    }

    /* Copper and tin are the common ones, iron and gold rarer, platinum and
     * titanium rarest. */
    int common = total[0] < total[1] ? total[0] : total[1];
    int middle_hi = total[2] > total[3] ? total[2] : total[3];
    int middle_lo = total[2] < total[3] ? total[2] : total[3];
    int rare = total[4] > total[5] ? total[4] : total[5];
    CHECK(common > middle_hi * 2 && middle_lo > rare * 2 && rare > 0);
    CHECK(flint_total > 0 && dirt_copper > 0 && dirt_tin > 0 && mega > 0);

    /* Mining gives the material, and the dirt kinds give ore too. */
    CHECK(tile_def(tile_id("flint"))->drop == item_id("flint"));
    CHECK(tile_def(tile_id("copper_dirt"))->drop == item_id("copper_ore"));
    CHECK(tile_def(tile_id("titanium_ore"))->drop == item_id("titanium_ore"));
    CHECK(tile_def(tile_id("titanium_ore"))->mine_time > tile_def(TILE_STONE)->mine_time);
}

static ItemStack *chest_slot(Game *g, int i)
{
    return inventory_slot(&g->inventory, (SlotRef){ SLOT_CONTAINER, game_open_storage(g), i });
}

static int used_containers(const Game *g)
{
    int n = 0;
    for (int i = 1; i < MAX_CONTAINERS; i++) n += g->inventory.containers[i].used;
    return n;
}

static void test_chests(void)
{
    Game g;
    game_init(&g, 64, 48, 20, WORLD_FLAT);
    Inventory *inv = &g.inventory;
    TilePos start = g.player.feet;
    int s = g.world.surface_y, cx = start.x + 3;
    SlotRef bar1 = { SLOT_BAR, 1, 0 }, bar2 = { SLOT_BAR, 2, 0 }, bar3 = { SLOT_BAR, 3, 0 };
    TileType chest_tile = tile_id("chest");

    /* Baskets never stack; empty chests do. */
    CHECK(item_def(ITEM_BASKET)->max_stack == 1 && item_def(ITEM_CHEST)->max_stack > 1);
    inventory_add(inv, ITEM_BASKET, 1);
    CHECK(inv->slots[0].count == 1 && inv->slots[1].id == ITEM_BASKET && inv->slots[1].count == 1);
    CHECK(inventory_move(inv, bar1, (SlotRef){ SLOT_BAR, 0, 0 }, MAX_STACK));
    CHECK(inv->slots[0].count == 1 && inv->slots[1].count == 1);
    inv->slots[1] = (ItemStack){ 0 };
    inventory_add(inv, ITEM_CHEST, 2);
    inventory_add(inv, ITEM_WOOD, 20);
    CHECK(inv->slots[1].id == ITEM_CHEST && inv->slots[1].count == 2 && inv->slots[1].container == 0);
    CHECK(inv->slots[2].id == ITEM_WOOD && inv->slots[2].count == 20);

    /* A placed chest opens onto 16 slots, and takes what is dragged into it. */
    inventory_select(inv, 1, -1);
    CHECK(game_click_tile(&g, cx, s - 1) && g.queue[0].type == ACTION_PLACE);
    CHECK(game_queue_action(&g, ACTION_USE, cx, s - 1));
    run(&g, 3.0f);
    CHECK(world_get(&g.world, cx, s - 1) == chest_tile && g.bench_open && inv->slots[1].count == 1);
    CHECK(tile_def(chest_tile)->storage_columns == 4 && tile_def(chest_tile)->storage_rows == 4);
    CHECK(game_open_storage(&g) != 0 && chest_slot(&g, 15) != NULL && chest_slot(&g, 16) == NULL);
    CHECK(inventory_move(inv, bar2, (SlotRef){ SLOT_CONTAINER, game_open_storage(&g), 5 }, 5));
    CHECK(chest_slot(&g, 5)->id == ITEM_WOOD && chest_slot(&g, 5)->count == 5 && inv->slots[2].count == 15);
    CHECK(inventory_move(inv, (SlotRef){ SLOT_CONTAINER, game_open_storage(&g), 5 },
                         (SlotRef){ SLOT_CONTAINER, game_open_storage(&g), 0 }, MAX_STACK));
    /* A basket goes in with what it holds. */
    CHECK(inventory_move(inv, bar2, (SlotRef){ SLOT_BASKET, 0, 1 }, 3));
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 0, 0 }, (SlotRef){ SLOT_CONTAINER, game_open_storage(&g), 9 }, 1));
    CHECK(chest_slot(&g, 9)->id == ITEM_BASKET && inv->slots[0].count == 0);
    CHECK(inv->containers[chest_slot(&g, 9)->container].slots[1].count == 3);
    CHECK(inventory_move(inv, (SlotRef){ SLOT_CONTAINER, game_open_storage(&g), 9 }, (SlotRef){ SLOT_BAR, 0, 0 }, 1));
    CHECK(inventory_slot(inv, (SlotRef){ SLOT_BASKET, 0, 1 })->count == 3);
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BASKET, 0, 1 }, bar2, MAX_STACK) && inv->slots[2].count == 15);

    /* Removing the chest gives a chest item carrying what was inside. */
    CHECK(game_queue_action(&g, ACTION_MINE, cx, s - 1));
    run(&g, 3.0f);
    CHECK(world_get(&g.world, cx, s - 1) == TILE_AIR && !g.bench_open && game_open_storage(&g) == 0);
    CHECK(inv->slots[1].id == ITEM_CHEST && inv->slots[1].count == 1 && inv->slots[1].container == 0);
    CHECK(inv->slots[3].id == ITEM_CHEST && inv->slots[3].count == 1 && inv->slots[3].container != 0);
    CHECK(inv->containers[inv->slots[3].container].slots[0].count == 5);

    /* It does not stack with an empty chest, nor with one holding something else... */
    CHECK(inventory_move(inv, bar3, bar1, MAX_STACK));
    CHECK(inv->slots[1].container != 0 && inv->slots[3].container == 0 && inv->slots[1].count == 1);
    uint16_t other = container_alloc(inv);
    inv->containers[other].slots[0] = (ItemStack){ (uint8_t)ITEM_WOOD, 4, 0 };
    inv->slots[3].container = other;
    CHECK(!containers_equal(inv, inv->slots[1].container, other));
    CHECK(inventory_move(inv, bar3, bar1, MAX_STACK) && inv->slots[1].count == 1 && inv->slots[3].count == 1);
    /* ...but two chests with 5 wood in each are the same, and stack. */
    other = (inv->slots[1].container == other) ? inv->slots[1].container : inv->slots[3].container;
    inv->containers[other].slots[0].count = 5;
    CHECK(containers_equal(inv, inv->slots[1].container, inv->slots[3].container));
    int before = used_containers(&g);
    CHECK(inventory_move(inv, bar3, bar1, MAX_STACK));
    CHECK(inv->slots[1].count == 2 && inv->slots[3].count == 0 && used_containers(&g) == before - 1);

    /* Taking one off the stack gives it its own copy of the contents. */
    CHECK(inventory_move(inv, bar1, bar3, 1));
    CHECK(inv->slots[1].count == 1 && inv->slots[3].count == 1);
    CHECK(inv->slots[3].container != 0 && inv->slots[3].container != inv->slots[1].container);
    CHECK(containers_equal(inv, inv->slots[1].container, inv->slots[3].container));
    CHECK(inventory_move(inv, bar3, bar1, MAX_STACK) && inv->slots[1].count == 2);

    /* Placing one of the two puts 5 wood in the block and leaves 5 in the item. */
    inventory_select(inv, 1, -1);
    CHECK(game_click_tile(&g, cx, s - 1));
    CHECK(game_queue_action(&g, ACTION_USE, cx, s - 1));
    run(&g, 3.0f);
    CHECK(g.bench_open && chest_slot(&g, 0)->id == ITEM_WOOD && chest_slot(&g, 0)->count == 5);
    CHECK(inv->slots[1].count == 1 && inv->containers[inv->slots[1].container].slots[0].count == 5);
    CHECK(game_open_storage(&g) != inv->slots[1].container);

    /* A chest cannot be put inside itself by way of something it holds, and
     * emptied out it is a plain chest again. */
    CHECK(inventory_move(inv, (SlotRef){ SLOT_CONTAINER, game_open_storage(&g), 0 }, bar2, MAX_STACK));
    CHECK(inv->slots[2].count == 20);
    CHECK(game_queue_action(&g, ACTION_MINE, cx, s - 1));
    run(&g, 3.0f);
    int empties = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (inv->slots[i].id == ITEM_CHEST && inv->slots[i].container == 0) empties += inv->slots[i].count;
    CHECK(empties == 1);

    /* Contents survive saving, in the item and in the block. */
    inventory_select(inv, 1, -1);
    CHECK(inv->slots[1].container != 0 && game_click_tile(&g, cx + 2, s - 1));
    run(&g, 3.0f);
    CHECK(world_get(&g.world, cx + 2, s - 1) == chest_tile && save_game(&g, "build/test_chest.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_chest.world"));
    CHECK(game_queue_action(&loaded, ACTION_USE, cx + 2, s - 1));
    run(&loaded, 3.0f);
    CHECK(loaded.bench_open && chest_slot(&loaded, 0)->id == ITEM_WOOD && chest_slot(&loaded, 0)->count == 5);
    game_free(&loaded);
    remove("build/test_chest.world");
    game_free(&g);
}

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

/* Runs last: it changes the loaded content. */
static void test_content(void)
{
    char err[256];

    /* The shipped data, found by name. */
    CHECK(tile_id("air") == TILE_AIR && item_id("none") == ITEM_NONE);
    CHECK(ITEM_BASKET == 1 && tile_id("no_such_tile") == -1 && item_id("no_such_item") == -1);
    CHECK(tile_def(TILE_STONE)->solid && tile_def(TILE_STONE)->mine_time > tile_def(TILE_DIRT)->mine_time);
    CHECK(!tile_def(TILE_OAK_LOG)->solid && tile_def(TILE_OAK_LOG)->climbable);
    CHECK(content.trees[TREE_OAK].layer == LAYER_BACKGROUND && content.feature_count == 3);
    CHECK(content.worldgen.width == 2048 && content.worldgen.height == 401);
    CHECK(content.rules.day_ticks == 1000 && content.rules.night_ticks == 800 && content.rules.tick_rate == 10);

    /* A second directory loads on top of the first, the way a mod would:
     * it can change what exists and add what does not. */
    int tiles = content.tile_count, items = content.item_count, recipes = content.recipe_count;
    write_file("build/mod/tiles.json",
               "{ \"tiles\": [ { \"name\": \"dirt\", \"solid\": true, \"removable\": true,"
               " \"mine_time\": 2.5, \"drop\": \"gem\" },"
               " { \"name\": \"gem_block\", \"solid\": true, \"removable\": true, \"drop\": \"gem\" } ] }");
    write_file("build/mod/items.json",
               "{ \"items\": [ { \"name\": \"gem\", \"label\": \"Gem\", \"max_stack\": 20,"
               " \"places\": \"gem_block\" } ] }");
    write_file("build/mod/recipes.json",
               "{ \"recipes\": [ { \"bench\": \"workbench\", \"output\": { \"item\": \"gem\" },"
               " \"inputs\": [ { \"item\": \"stone\", \"count\": 4 } ] } ] }");
    write_file("build/mod/rules.json", "{ \"time\": { \"day_ticks\": 500 } }");
    CHECK(content_load("build/mod", err, sizeof err));
    CHECK(content.tile_count == tiles + 1 && content.item_count == items + 1);
    CHECK(content.recipe_count == recipes + 1 && tile_id("dirt") == 1);
    CHECK(tile_def(TILE_DIRT)->mine_time == 2.5f && tile_def(TILE_DIRT)->drop == item_id("gem"));
    CHECK(item_def(item_id("gem"))->max_stack == 20 && item_def(item_id("gem"))->places == tile_id("gem_block"));
    CHECK(content.rules.day_ticks == 500 && content.rules.night_ticks == 800);

    /* The new content works in the game with no code written for it. */
    Game g;
    game_init(&g, 64, 48, 18, WORLD_FLAT);
    TilePos start = g.player.feet;
    int s = g.world.surface_y;
    CHECK(game_click_tile(&g, start.x + 1, s));
    run(&g, 4.0f);
    CHECK(count_item(&g, item_id("gem")) == 1);
    inventory_select(&g.inventory, 1, -1);
    CHECK(game_click_tile(&g, start.x + 1, s));
    run(&g, 2.0f);
    CHECK(world_get(&g.world, start.x + 1, s) == tile_id("gem_block"));
    CHECK(inventory_add(&g.inventory, item_id("gem"), 25) == 0 && g.inventory.slots[1].count == 20);
    game_free(&g);

    /* A mistake in the data is reported, not ignored. */
    write_file("build/mod/recipes.json",
               "{ \"recipes\": [ { \"bench\": \"workbench\", \"output\": { \"item\": \"unobtainium\" } } ] }");
    CHECK(!content_load("build/mod", err, sizeof err));
    CHECK(strstr(err, "recipes.json") && strstr(err, "unobtainium"));
    write_file("build/mod/recipes.json", "{ not json");
    CHECK(!content_load("build/mod", err, sizeof err) && strstr(err, "not valid JSON"));

    content_reset();
    CHECK(content_load("data", err, sizeof err) && content.tile_count == tiles);
}

int main(void)
{
    char err[256];
    if (!content_load("data", err, sizeof err)) {
        printf("could not load data: %s\n", err);
        return 1;
    }

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
    test_veins();
    test_trees();
    test_ticks();
    test_save();
    test_background_trees();
    test_planting();
    test_chests();
    test_content();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
