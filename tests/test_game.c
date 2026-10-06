/* Headless tests for the game rules: no window, just the simulation. */
#include "../src/game.h"
#include "../src/light.h"
#include "../src/save.h"
#include "../src/trade.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Content is looked up by name: the engine has no ids of its own. */
#define TILE_DIRT tile_id("dirt")
#define TILE_GRASS tile_id("grass")
#define TILE_STONE tile_id("stone")
#define TILE_WORKBENCH tile_id("workbench")
#define TILE_SPAWN_PORTAL tile_id("spawn_portal")
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
    CHECK(game_queue_action(&g, ACTION_MOVE, start.x + 3, s + 1));
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

/* Clicking a square that already has an action queued takes the action back. */
static void test_unqueue(void)
{
    Game g;
    game_init(&g, 64, 48, 23, WORLD_FLAT);
    TilePos start = g.player.feet;
    int s = g.world.surface_y;

    /* From the middle of the queue: the others stay, in order. */
    CHECK(game_click_tile(&g, start.x + 4, s));
    CHECK(game_click_tile(&g, start.x + 6, s));
    CHECK(game_click_tile(&g, start.x + 8, s));
    CHECK(g.queue_count == 3);
    CHECK(game_click_tile(&g, start.x + 6, s));
    CHECK(g.queue_count == 2 && g.queue[0].target.x == start.x + 4 && g.queue[1].target.x == start.x + 8);
    run(&g, 6.0f);
    CHECK(is_idle(&g));
    CHECK(world_get(&g.world, start.x + 4, s) == TILE_AIR && world_get(&g.world, start.x + 8, s) == TILE_AIR);
    CHECK(world_get(&g.world, start.x + 6, s) == TILE_DIRT);

    /* The action being carried out can be taken back too: the walk stops
     * after the step in hand, and mining stops with the block intact. */
    TilePos at = g.player.feet;
    CHECK(game_click_tile(&g, at.x - 6, s - 1));
    run(&g, WALK_TIME * 1.5f);
    CHECK(g.has_current && g.queue_count == 0);
    CHECK(game_click_tile(&g, at.x - 6, s - 1));
    CHECK(!g.has_current);
    run(&g, 2.0f);
    CHECK(is_idle(&g) && g.player.feet.x == at.x - 2);

    CHECK(game_click_tile(&g, at.x - 1, s));
    run(&g, tile_def(TILE_DIRT)->mine_time * 0.5f);
    CHECK(g.player.activity == ACTIVITY_MINING);
    CHECK(game_click_tile(&g, at.x - 1, s));
    run(&g, 2.0f);
    CHECK(is_idle(&g) && world_get(&g.world, at.x - 1, s) == TILE_DIRT);

    /* A click on a square with nothing queued still queues, and one that
     * could never work is refused as before. */
    CHECK(!game_unqueue_tile(&g, at.x - 1, s));
    CHECK(!game_click_tile(&g, at.x, s - 9) && g.queue_count == 0);

    /* The same goes for an item on the ground. */
    g.drops[0] = (Drop){ true, (uint8_t)ITEM_STONE, at.x + 9, (float)(s - 1), 0.0f, 0.0f, false, 0, 500, 0 };
    CHECK(game_click_drop(&g, 0) && g.queue_count == 1 && g.queue[0].type == ACTION_COLLECT);
    CHECK(game_click_drop(&g, 0) && g.queue_count == 0);
    run(&g, 1.0f);
    CHECK(g.drops[0].active && inventory_count(&g.inventory, ITEM_STONE) == 0);
    game_free(&g);
}

/* Hanging from the underside of a row of blocks, and getting round its end. */
static void test_monkey_bars(void)
{
    Game g;
    game_init(&g, 64, 48, 24, WORLD_FLAT);
    World *w = &g.world;
    TilePos start = g.player.feet;
    int s = w->surface_y, bar = s - 4; /* one clear tile above the standing player's head */

    for (int x = start.x - 2; x <= start.x + 6; x++) world_set(w, x, bar, TILE_STONE);

    /* W jumps up and catches the bar; the player hangs there. */
    game_set_move_input(&g, 0, -1);
    run(&g, CLIMB_TIME + 0.05f);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(g.player.climbing && g.player.feet.y == s - 2 && g.player.feet.x == start.x);
    CHECK(game_player_grip_side(&g) == 0);

    /* A direction goes hand over hand along the underside without dropping... */
    game_set_move_input(&g, 1, 0);
    Trace tr = run_traced(&g, CLIMB_TIME * 5.5f);
    game_set_move_input(&g, 0, 0);
    run(&g, 0.3f);
    CHECK(!tr.fell && !tr.hopped && tr.deepest == s - 2);
    CHECK(g.player.climbing && g.player.feet.x == start.x + 6 && g.player.feet.y == s - 2);

    /* At the end, up goes round the end of the last block onto its side... */
    game_set_move_input(&g, 0, -1);
    run(&g, CLIMB_TIME * 0.5f);
    game_set_move_input(&g, 0, 0);
    run(&g, 0.3f);
    CHECK(g.player.climbing && g.player.feet.x == start.x + 7 && g.player.feet.y == s - 3);
    CHECK(game_player_grip_side(&g) == -1);

    /* ...and from there it is an ordinary climb up and onto the top. */
    game_set_move_input(&g, 0, -1);
    run(&g, CLIMB_TIME * 1.5f);
    game_set_move_input(&g, 0, 0);
    run(&g, 0.5f);
    CHECK(!g.player.climbing && g.player.feet.x == start.x + 6 && g.player.feet.y == bar - 1);

    /* Back the other way: off the top, down the side, under, and let go. */
    game_set_move_input(&g, 1, 0);
    run(&g, WALK_TIME * 0.5f);
    game_set_move_input(&g, 0, 1);
    run(&g, CLIMB_TIME * 1.5f + 0.05f);
    game_set_move_input(&g, 0, 0);
    run(&g, 0.3f);
    CHECK(g.player.climbing && g.player.feet.x == start.x + 6 && g.player.feet.y == s - 2);
    game_set_move_input(&g, 0, 1);
    run(&g, 1.0f);
    game_set_move_input(&g, 0, 0);
    CHECK(!g.player.climbing && g.player.feet.y == s - 1);

    /* Holding a direction past the end of the bars just lets go. */
    g.player.feet = g.player.step_to = (TilePos){ start.x + 5, s - 2 };
    g.player.climbing = true;
    game_set_move_input(&g, 1, 0);
    run(&g, 1.0f);
    game_set_move_input(&g, 0, 0);
    run(&g, 0.3f);
    CHECK(!g.player.climbing && g.player.feet.y == s - 1 && g.player.feet.x > start.x + 6);

    /* A click finds the whole route: up to the bar, along, round the end, on top. */
    g.player.feet = g.player.step_to = start;
    CHECK(game_click_tile(&g, start.x + 3, bar - 1));
    tr = run_traced(&g, 8.0f);
    CHECK(is_idle(&g) && tr.climbed && !tr.fell);
    CHECK(g.player.feet.x == start.x + 3 && g.player.feet.y == bar - 1);

    /* Mining the block being hung from lets the player drop. */
    g.player.feet = g.player.step_to = (TilePos){ start.x + 2, s - 2 };
    g.player.climbing = true;
    run(&g, 0.2f);
    CHECK(g.player.climbing && g.player.feet.y == s - 2);
    world_set(w, start.x + 2, bar, TILE_AIR);
    run(&g, 1.0f);
    CHECK(!g.player.climbing && g.player.feet.y == s - 1);
    game_free(&g);
}

/* Going through a gap one block high. */
static void test_crawl(void)
{
    Game g;
    game_init(&g, 64, 48, 25, WORLD_FLAT);
    World *w = &g.world;
    TilePos start = g.player.feet;
    int s = w->surface_y;

    /* A thick wall with a tunnel one block high along the ground. */
    for (int x = start.x + 3; x <= start.x + 6; x++)
        for (int y = s - 12; y <= s - 2; y++) world_set(w, x, y, TILE_STONE);

    CHECK(!game_player_crawling(&g));
    game_set_move_input(&g, 1, 0);
    bool crawled = false, upright_inside = false;
    int lowest = start.y, highest = start.y;
    for (float t = 0.0f; t < 3.0f; t += 1.0f / 60.0f) {
        game_update(&g, 1.0f / 60.0f);
        int x = world_dx(w, start.x, g.player.feet.x);
        if (x >= 3 && x <= 6) {
            crawled = crawled || game_player_crawling(&g);
            upright_inside = upright_inside || !game_player_crawling(&g);
        }
        if (g.player.feet.y > lowest) lowest = g.player.feet.y;
        if (g.player.feet.y < highest) highest = g.player.feet.y;
    }
    game_set_move_input(&g, 0, 0);
    run(&g, 0.5f);
    /* Straight through at ground level, down the whole way, up again after. */
    CHECK(crawled && !upright_inside && lowest == start.y && highest == start.y);
    CHECK(world_dx(w, start.x, g.player.feet.x) > 7 && !game_player_crawling(&g) && !g.player.climbing);

    /* A click inside the tunnel is a place to go; the player stops there, flat. */
    CHECK(game_click_tile(&g, start.x + 4, s - 1));
    run(&g, 4.0f);
    CHECK(is_idle(&g) && g.player.feet.x == start.x + 4 && g.player.feet.y == s - 1);
    CHECK(game_player_crawling(&g));
    /* No jumping in there, but the roof can be mined from below. */
    game_set_move_input(&g, 0, -1);
    run(&g, 0.5f);
    game_set_move_input(&g, 0, 0);
    CHECK(g.player.feet.y == s - 1 && !g.player.climbing);
    CHECK(game_click_tile(&g, start.x + 4, s - 2));
    run(&g, 2.0f);
    CHECK(world_get(w, start.x + 4, s - 2) == TILE_AIR && !game_player_crawling(&g));
    /* Crawling, only the eight tiles around the player are within reach. */
    CHECK(game_click_tile(&g, start.x + 5, s - 1));
    run(&g, 1.0f);
    CHECK(game_player_crawling(&g) && g.player.feet.x == start.x + 5);
    CHECK(!game_queue_action(&g, ACTION_MINE, start.x + 5, s - 3));
    CHECK(game_queue_action(&g, ACTION_MINE, start.x + 6, s - 2));
    run(&g, 2.0f);
    CHECK(world_get(w, start.x + 6, s - 2) == TILE_AIR && g.player.feet.x == start.x + 5);

    /* A crawl takes its own, slower, time per step. */
    CHECK(CRAWL_TIME > WALK_TIME);

    /* A ledge can be climbed up into a low gap. */
    for (int x = start.x - 6; x <= start.x - 3; x++) {
        world_set(w, x, s - 1, TILE_STONE);
        world_set(w, x, s - 3, TILE_STONE);
    }
    g.player.feet = g.player.step_to = (TilePos){ start.x - 1, s - 1 };
    CHECK(game_click_tile(&g, start.x - 5, s - 2));
    run(&g, 4.0f);
    CHECK(is_idle(&g) && g.player.feet.x == start.x - 5 && g.player.feet.y == s - 2 && game_player_crawling(&g));
    game_free(&g);
}

/* Nothing is reached diagonally past two blocks that meet at the corner. */
static void test_blocked_corner(void)
{
    Game g;
    game_init(&g, 64, 48, 26, WORLD_FLAT);
    World *w = &g.world;
    TilePos start = g.player.feet;
    int s = w->surface_y;

    /* On open ground the block diagonally below is fair game: only the
     * ground underfoot is in the way, not the tile beside the player. */
    CHECK(game_queue_action(&g, ACTION_MINE, start.x + 1, s));
    run(&g, 2.0f);
    CHECK(world_get(w, start.x + 1, s) == TILE_AIR && g.player.feet.x == start.x);

    /* Down a one-wide shaft, the blocks diagonally below are walled off. */
    int hx = start.x + 5;
    world_set(w, hx, s, TILE_AIR);
    world_set(w, hx, s + 1, TILE_AIR);
    g.player.feet = g.player.step_to = (TilePos){ hx, s + 1 };
    CHECK(world_is_solid(w, hx + 1, s + 1) && world_is_solid(w, hx, s + 2));
    CHECK(!game_queue_action(&g, ACTION_MINE, hx + 1, s + 2));
    CHECK(!game_queue_action(&g, ACTION_MINE, hx - 1, s + 2));
    /* Straight down and straight to the side are not corners. */
    CHECK(game_queue_action(&g, ACTION_MINE, hx + 1, s + 1));
    run(&g, 2.0f);
    CHECK(world_get(w, hx + 1, s + 1) == TILE_AIR);
    /* With the side block gone there is a way through to the corner. */
    CHECK(game_queue_action(&g, ACTION_MINE, hx + 1, s + 2));
    run(&g, 2.0f);
    CHECK(world_get(w, hx + 1, s + 2) == TILE_AIR && g.player.feet.x == hx);

    /* The same above the head, for placing as well as mining. */
    int cx = start.x - 6;
    g.player.feet = g.player.step_to = (TilePos){ cx, s - 1 };
    game_cancel_actions(&g);
    world_set(w, cx, s - 3, TILE_STONE);     /* over the head */
    world_set(w, cx + 1, s - 2, TILE_STONE); /* beside the head */
    world_set(w, cx + 1, s - 3, TILE_STONE); /* the corner */
    run(&g, 0.2f);
    /* It can still be mined, but only by going somewhere else to do it. */
    bool queued = game_queue_action(&g, ACTION_MINE, cx + 1, s - 3);
    run(&g, tile_def(TILE_STONE)->mine_time * 0.5f);
    CHECK(!queued || g.player.activity != ACTIVITY_MINING);
    game_cancel_actions(&g);
    run(&g, 1.0f);
    g.player.feet = g.player.step_to = (TilePos){ cx, s - 1 };
    g.player.climbing = false;
    world_set(w, cx, s - 3, TILE_STONE);
    world_set(w, cx + 1, s - 2, TILE_STONE);
    world_set(w, cx + 1, s - 3, TILE_AIR);
    memset(&g.inventory, 0, sizeof g.inventory);
    g.inventory.sel_inner = -1;
    inventory_add(&g.inventory, ITEM_DIRT, 4);
    inventory_select(&g.inventory, 0, -1);
    /* Walled in on all sides so there is nowhere else to place it from. */
    world_set(w, cx - 1, s - 1, TILE_STONE);
    world_set(w, cx - 1, s - 2, TILE_STONE);
    world_set(w, cx + 1, s - 1, TILE_STONE);
    CHECK(!game_queue_action(&g, ACTION_PLACE, cx + 1, s - 3));
    world_set(w, cx, s - 3, TILE_AIR);
    CHECK(game_queue_action(&g, ACTION_PLACE, cx + 1, s - 3));
    run(&g, 2.0f);
    CHECK(world_get(w, cx + 1, s - 3) == TILE_DIRT && g.player.feet.x == cx);
    game_free(&g);
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

/* The recipe a bench has for an item, or NULL. */
static const Recipe *find_recipe(TileType bench, const char *output)
{
    const Recipe *list[MAX_BENCH_RECIPES];
    int n = bench_recipes(bench, list, MAX_BENCH_RECIPES);
    for (int i = 0; i < n; i++)
        if (list[i]->output.id == item_id(output) && list[i]->output.count == 1) return list[i];
    return NULL;
}

/* Whether a recipe takes exactly these one or two inputs. */
static bool costs(const Recipe *r, const char *a, int na, const char *b, int nb)
{
    if (!r || r->input_count != (b ? 2 : 1)) return false;
    if (r->inputs[0].id != item_id(a) || r->inputs[0].count != na) return false;
    return !b || (r->inputs[1].id == item_id(b) && r->inputs[1].count == nb);
}

static const char *TextName(const char *fmt, const char *arg)
{
    static char buf[64];
    snprintf(buf, sizeof buf, fmt, arg);
    return buf;
}

static const char *TextName2(const char *fmt, const char *arg)
{
    static char buf[64];
    snprintf(buf, sizeof buf, fmt, arg);
    return buf;
}

static void clear_inventory(Game *g)
{
    /* Everything the player is carrying; the shared chest is not that. */
    Container shared = g->inventory.containers[SHARED_CONTAINER];
    memset(&g->inventory, 0, sizeof g->inventory);
    g->inventory.sel_inner = -1;
    g->inventory.containers[SHARED_CONTAINER] = shared;
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
    CHECK(tile_def(TILE_SPAWN_PORTAL)->bench && !tile_def(TILE_SPAWN_PORTAL)->removable);
    CHECK(tile_def(TILE_WORKBENCH)->bench && tile_def(TILE_WORKBENCH)->removable);

    const Recipe *portal = NULL, *bench = NULL, *other = NULL;
    n = bench_recipes(TILE_SPAWN_PORTAL, &portal, 1);
    CHECK(bench_recipes(TILE_SPAWN_PORTAL, &other, 1) == 1 && other == portal);
    CHECK(bench_recipes(TILE_DIRT, &other, 1) == 0);
    CHECK(n == 1 && portal->output.id == ITEM_WORKBENCH && portal->inputs[0].id == ITEM_DIRT &&
          portal->inputs[0].count == 1);
    /* The workbench makes the other benches: three at tier 1, two more at
     * tier 2 and one at tier 3, and a portal. */
    const Recipe *list[16];
    CHECK(bench_recipes(TILE_WORKBENCH, list, 16) == 8 && bench_recipes(TILE_WORKBENCH_2, list, 16) == 8);
    const Recipe *woodwork = find_recipe(TILE_WORKBENCH, "woodwork_bench");
    const Recipe *toolbench = find_recipe(TILE_WORKBENCH, "tool_bench");
    const Recipe *kiln = find_recipe(TILE_WORKBENCH, "kiln"), *furnace = find_recipe(TILE_WORKBENCH, "furnace");
    bench = find_recipe(TILE_WORKBENCH, "craft_bench");
    CHECK(costs(woodwork, "wood", 1, NULL, 0) && costs(toolbench, "stone", 1, "flint", 1));
    CHECK(costs(bench, "dirt", 1, NULL, 0));
    CHECK(costs(kiln, "dirt", 10, "stick", 1) && costs(furnace, "stone", 5, "stick", 1));
    CHECK(!recipe_locked(TILE_WORKBENCH, woodwork) && !recipe_locked(TILE_WORKBENCH, toolbench) &&
          !recipe_locked(TILE_WORKBENCH, bench));
    CHECK(recipe_locked(TILE_WORKBENCH, kiln) && recipe_locked(TILE_WORKBENCH, furnace));
    CHECK(!recipe_locked(TILE_WORKBENCH_2, kiln) && !recipe_locked(TILE_WORKBENCH_2, furnace));
    CHECK(find_recipe(TILE_WORKBENCH, "basket") == NULL && find_recipe(TILE_WORKBENCH, "chest") == NULL);

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
    CHECK(recipe_max_crafts(&g.inventory, portal, 0) == 5);
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

    /* A placed workbench can be opened and makes its tier 1 benches. */
    inventory_select(&g.inventory, 2, -1);
    CHECK(inventory_selected(&g.inventory)->id == ITEM_WORKBENCH);
    CHECK(game_click_tile(&g, px + 7, s - 1));
    CHECK(game_queue_action(&g, ACTION_USE, px + 7, s - 1));
    run(&g, 2.0f);
    CHECK(world_get_at(&g.world, BG, px + 7, s - 1) == TILE_WORKBENCH && g.bench_open);
    CHECK(count_item(&g, ITEM_WORKBENCH) == 4);
    CHECK(!game_craft(&g, portal, 1)); /* not this bench's recipe */
    clear_inventory(&g);
    inventory_add(&g.inventory, ITEM_WOOD, 2);
    inventory_add(&g.inventory, ITEM_DIRT, 12);
    inventory_add(&g.inventory, ITEM_STONE, 13);
    inventory_add(&g.inventory, item_id("flint"), 1);
    inventory_add(&g.inventory, ITEM_STICK, 2);
    CHECK(game_craft(&g, woodwork, 1));
    run(&g, 0.3f);
    CHECK(game_craft(&g, bench, 1));
    run(&g, 0.3f);
    CHECK(game_craft(&g, toolbench, 5) && g.craft_total == 1);
    run(&g, 0.3f);
    CHECK(count_item(&g, item_id("woodwork_bench")) == 1 && count_item(&g, item_id("craft_bench")) == 1);
    CHECK(count_item(&g, item_id("tool_bench")) == 1 && count_item(&g, ITEM_WOOD) == 1);
    CHECK(count_item(&g, ITEM_STONE) == 12 && count_item(&g, item_id("flint")) == 0);

    /* The kiln and furnace are on the list but locked until tier 2, which
     * costs 5 stone. */
    CHECK(!game_craft(&g, kiln, 1) && !game_craft(&g, furnace, 1));
    inventory_remove(&g.inventory, ITEM_STONE, 8);
    CHECK(!game_upgrade_bench(&g));
    inventory_add(&g.inventory, ITEM_STONE, 6);
    CHECK(game_upgrade_bench(&g));
    CHECK(world_get_at(&g.world, BG, px + 7, s - 1) == TILE_WORKBENCH_2 && count_item(&g, ITEM_STONE) == 5);
    CHECK(g.bench_open && !game_upgrade_bench(&g));
    CHECK(game_craft(&g, kiln, 1));
    run(&g, 0.3f);
    CHECK(game_craft(&g, furnace, 1));
    run(&g, 0.3f);
    CHECK(count_item(&g, item_id("kiln")) == 1 && count_item(&g, item_id("furnace")) == 1);
    CHECK(count_item(&g, ITEM_DIRT) == 1 && count_item(&g, ITEM_STONE) == 0 && count_item(&g, ITEM_STICK) == 0);
    game_close_bench(&g);
    CHECK(!game_upgrade_bench(&g));

    CHECK(game_queue_action(&g, ACTION_MINE, px + 7, s - 1));
    run(&g, 2.0f);
    CHECK(world_get_at(&g.world, BG, px + 7, s - 1) == TILE_AIR && !g.bench_open);
    /* The upgraded bench comes back as itself, tier and all. */
    CHECK(count_item(&g, item_id("workbench_2")) == 1);
    game_free(&g);
}

/* Puts a bench item in the player's hands, places it at column x and opens it. */
static bool place_and_open(Game *g, const char *item, int x)
{
    clear_inventory(g);
    inventory_add(&g->inventory, item_id(item), 1);
    inventory_select(&g->inventory, 0, -1);
    int y = g->world.surface_y - 1;
    g->player.feet = g->player.step_to = (TilePos){ x - 1, y };
    game_close_bench(g);
    if (!game_click_tile(g, x, y) || !game_queue_action(g, ACTION_USE, x, y)) return false;
    run(g, 2.0f);
    return g->bench_open && g->bench_pos.x == world_wrap_x(&g->world, x);
}

static void test_work_benches(void)
{
    Game g;
    game_init(&g, 64, 48, 21, WORLD_FLAT);
    Inventory *inv = &g.inventory;
    int s = g.world.surface_y, x = g.world.spawn_x + 4;
    TileType tool1 = tile_id("tool_bench"), tool2 = tile_id("tool_bench_2");
    TileType f1 = tile_id("furnace"), f2 = tile_id("furnace_2"), f3 = tile_id("furnace_3");

    /* Woodwork bench: a chest for 1 wood. Craft bench: a basket for 10 sticks. */
    CHECK(place_and_open(&g, "woodwork_bench", x));

    /* A bench stands behind the player: it is walked in front of, not round
     * or over, and a block can be put in front of it. */
    CHECK(world_get(&g.world, x, s - 1) == TILE_AIR);
    CHECK(world_get_at(&g.world, BG, x, s - 1) == tile_id("woodwork_bench"));
    CHECK(game_bench_at(&g, x, s - 1) == tile_id("woodwork_bench") && game_bench_at(&g, x + 1, s - 1) == TILE_AIR);
    game_set_move_input(&g, 1, 0);
    Trace walk = run_traced(&g, WALK_TIME * 4 + 0.05f);
    game_set_move_input(&g, 0, 0);
    run(&g, 0.5f);
    CHECK(!walk.climbed && !walk.hopped && walk.deepest == s - 1 && g.player.feet.y == s - 1);
    CHECK(g.player.feet.x >= x + 2 && !g.bench_open);
    /* Standing right in front of it, it can still be opened. */
    g.player.feet = g.player.step_to = (TilePos){ x, s - 1 };
    CHECK(game_queue_action(&g, ACTION_USE, x, s - 1));
    run(&g, 0.2f);
    CHECK(g.bench_open && g.player.feet.x == x);
    /* Another bench cannot go where one already is, but an ordinary block can. */
    inventory_add(inv, item_id("craft_bench"), 1);
    inventory_add(inv, ITEM_DIRT, 1);
    inventory_select(inv, 0, -1);
    CHECK(inventory_selected(inv)->id == item_id("craft_bench"));
    g.player.feet = g.player.step_to = (TilePos){ x - 1, s - 1 };
    CHECK(!game_queue_action(&g, ACTION_PLACE, x, s - 1));
    inventory_select(inv, 1, -1);
    CHECK(game_queue_action(&g, ACTION_PLACE, x, s - 1));
    run(&g, 1.0f);
    CHECK(world_get(&g.world, x, s - 1) == TILE_DIRT && world_get_at(&g.world, BG, x, s - 1) == tile_id("woodwork_bench"));
    /* The block in front is what gets mined first; then the bench. */
    CHECK(game_queue_action(&g, ACTION_MINE, x, s - 1));
    run(&g, 2.0f);
    CHECK(world_get(&g.world, x, s - 1) == TILE_AIR && world_get_at(&g.world, BG, x, s - 1) == tile_id("woodwork_bench"));
    clear_inventory(&g);
    CHECK(game_queue_action(&g, ACTION_USE, x, s - 1));
    run(&g, 1.0f);
    CHECK(g.bench_open);
    CHECK(costs(find_recipe(tile_id("woodwork_bench"), "chest"), "wood", 1, NULL, 0));
    inventory_add(inv, ITEM_WOOD, 2);
    CHECK(game_craft(&g, find_recipe(tile_id("woodwork_bench"), "chest"), 1));
    run(&g, 0.3f);
    CHECK(count_item(&g, ITEM_CHEST) == 1 && count_item(&g, ITEM_WOOD) == 1);
    CHECK(game_bench_fuel(&g, NULL) == -1 && !game_add_fuel(&g, ITEM_WOOD));

    CHECK(place_and_open(&g, "craft_bench", x + 2));
    CHECK(costs(find_recipe(tile_id("craft_bench"), "basket"), "stick", 10, NULL, 0));
    inventory_add(inv, ITEM_STICK, 10);
    CHECK(game_craft(&g, find_recipe(tile_id("craft_bench"), "basket"), 1));
    run(&g, 0.3f);
    CHECK(count_item(&g, ITEM_BASKET) == 1 && count_item(&g, ITEM_STICK) == 0);

    /* Tool bench: wooden and flint tools at tier 1, stone tools at tier 2 (5 stone). */
    CHECK(place_and_open(&g, "tool_bench", x + 4));
    const Recipe *list[16];
    CHECK(bench_recipes(tool1, list, 16) == 14);
    CHECK(costs(find_recipe(tool1, "wooden_pickaxe"), "wood", 1, "stick", 1));
    CHECK(costs(find_recipe(tool1, "wooden_shovel"), "wood", 1, "stick", 1));
    CHECK(costs(find_recipe(tool1, "flint_axe"), "flint", 2, "stick", 1));
    CHECK(costs(find_recipe(tool1, "flint_shovel"), "flint", 2, "stick", 1));
    CHECK(costs(find_recipe(tool1, "stone_pickaxe"), "stone", 5, "stick", 1));
    CHECK(costs(find_recipe(tool1, "stone_shovel"), "stone", 5, "stick", 1));
    CHECK(recipe_locked(tool1, find_recipe(tool1, "stone_pickaxe")) && recipe_locked(tool1, find_recipe(tool1, "stone_shovel")));
    CHECK(!recipe_locked(tool1, find_recipe(tool1, "flint_axe")) && !recipe_locked(tool2, find_recipe(tool1, "stone_shovel")));
    inventory_add(inv, ITEM_WOOD, 2);
    inventory_add(inv, ITEM_STICK, 4);
    inventory_add(inv, item_id("flint"), 2);
    inventory_add(inv, ITEM_STONE, 10);
    CHECK(game_craft(&g, find_recipe(tool1, "wooden_pickaxe"), 2));
    run(&g, 0.4f);
    CHECK(game_craft(&g, find_recipe(tool1, "flint_axe"), 1));
    run(&g, 0.3f);
    CHECK(!game_craft(&g, find_recipe(tool1, "stone_pickaxe"), 1));
    CHECK(game_upgrade_bench(&g) && world_get_at(&g.world, BG, x + 4, s - 1) == tool2);
    CHECK(count_item(&g, ITEM_STONE) == 5 && !game_upgrade_bench(&g));
    CHECK(game_craft(&g, find_recipe(tool1, "stone_pickaxe"), 1));
    run(&g, 0.3f);
    CHECK(count_item(&g, item_id("wooden_pickaxe")) == 2 && count_item(&g, item_id("flint_axe")) == 1);
    CHECK(count_item(&g, item_id("stone_pickaxe")) == 1 && count_item(&g, ITEM_STONE) == 0);
    /* Tools do not stack. */
    int pick_slots = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (inv->slots[i].id == item_id("wooden_pickaxe") && inv->slots[i].count == 1) pick_slots++;
    CHECK(pick_slots == 2);

    /* Kiln: needs fuel to work, and burns sticks, wood or coal. */
    CHECK(place_and_open(&g, "kiln", x + 6));
    const Recipe *charcoal = find_recipe(tile_id("kiln"), "charcoal");
    CHECK(costs(charcoal, "stick", 1, NULL, 0));
    int capacity = 0;
    CHECK(game_bench_fuel(&g, &capacity) == 0 && capacity > 0);
    inventory_add(inv, ITEM_STICK, 6);
    inventory_add(inv, ITEM_DIRT, 1);
    CHECK(!game_craft(&g, charcoal, 1) && count_item(&g, item_id("charcoal")) == 0);
    CHECK(item_def(ITEM_STICK)->fuel > 0 && item_def(ITEM_WOOD)->fuel > 0 && item_def(item_id("coal"))->fuel > 0);
    CHECK(item_def(ITEM_DIRT)->fuel == 0 && !game_add_fuel(&g, ITEM_DIRT) && !game_add_fuel(&g, ITEM_WOOD));
    CHECK(game_add_fuel(&g, ITEM_STICK) && count_item(&g, ITEM_STICK) == 5);
    CHECK(game_bench_fuel(&g, NULL) == item_def(ITEM_STICK)->fuel);
    CHECK(game_craft(&g, charcoal, 2));
    run(&g, 0.4f);
    CHECK(count_item(&g, item_id("charcoal")) == 2 && count_item(&g, ITEM_STICK) == 3);

    /* The fuel burns down by the tick, to nothing, and then nothing can be made. */
    int before = game_bench_fuel(&g, NULL);
    for (int i = 0; i < 10; i++) game_tick(&g);
    CHECK(game_bench_fuel(&g, NULL) == before - 10);
    for (int i = 0; i < item_def(ITEM_STICK)->fuel + 50; i++) game_tick(&g);
    CHECK(game_bench_fuel(&g, NULL) == 0 && !game_craft(&g, charcoal, 1));
    /* It cannot be filled past the top. */
    inventory_add(inv, item_id("coal"), 5);
    int fed = 0;
    while (game_add_fuel(&g, item_id("coal"))) fed++;
    CHECK(fed > 0 && fed <= 5 && game_bench_fuel(&g, NULL) == capacity);
    CHECK(count_item(&g, item_id("coal")) == 5 - fed);
    /* A run in progress stops when the fire goes out. */
    game_close_bench(&g);

    /* Furnace: ore to ingot, tin and copper at tier 1, iron and gold at 2
     * (5 iron ore), platinum and titanium at 3 (5 titanium ore). */
    CHECK(place_and_open(&g, "furnace", x + 8));
    static const char *metals[] = { "copper", "tin", "iron", "gold", "platinum", "titanium" };
    CHECK(bench_recipes(f1, list, 16) == 6);
    for (int i = 0; i < 6; i++) {
        const char *ore = TextName("%s_ore", metals[i]);
        const Recipe *r = find_recipe(f1, TextName2("%s_ingot", metals[i]));
        CHECK(costs(r, ore, 1, NULL, 0) && r->level == i / 2 + 1);
        CHECK(recipe_locked(f1, r) == (i >= 2) && recipe_locked(f2, r) == (i >= 4) && !recipe_locked(f3, r));
    }
    inventory_add(inv, item_id("tin_ore"), 2);
    inventory_add(inv, item_id("iron_ore"), 7);
    inventory_add(inv, item_id("titanium_ore"), 6);
    inventory_add(inv, ITEM_WOOD, 3);
    CHECK(!game_craft(&g, find_recipe(f1, "tin_ingot"), 1)); /* cold */
    CHECK(game_add_fuel(&g, ITEM_WOOD) && game_add_fuel(&g, ITEM_WOOD));
    CHECK(game_craft(&g, find_recipe(f1, "tin_ingot"), 2));
    run(&g, 0.4f);
    CHECK(count_item(&g, item_id("tin_ingot")) == 2 && count_item(&g, item_id("tin_ore")) == 0);
    CHECK(!game_craft(&g, find_recipe(f1, "iron_ingot"), 1));
    int fuel = game_bench_fuel(&g, NULL);
    CHECK(game_upgrade_bench(&g) && world_get_at(&g.world, BG, x + 8, s - 1) == f2);
    CHECK(count_item(&g, item_id("iron_ore")) == 2);
    /* Upgrading keeps the fire in. */
    CHECK(game_bench_fuel(&g, NULL) == fuel);
    CHECK(game_craft(&g, find_recipe(f1, "iron_ingot"), 2));
    run(&g, 0.4f);
    CHECK(count_item(&g, item_id("iron_ingot")) == 2);
    CHECK(!game_craft(&g, find_recipe(f1, "titanium_ingot"), 1));
    CHECK(game_upgrade_bench(&g) && world_get_at(&g.world, BG, x + 8, s - 1) == f3);
    CHECK(count_item(&g, item_id("titanium_ore")) == 1 && !game_upgrade_bench(&g));
    CHECK(game_craft(&g, find_recipe(f1, "titanium_ingot"), 1));
    run(&g, 0.3f);
    CHECK(count_item(&g, item_id("titanium_ingot")) == 1);

    /* Fuel is saved with the world. */
    fuel = game_bench_fuel(&g, NULL);
    CHECK(fuel > 0 && save_game(&g, "build/test_fuel.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_fuel.world"));
    CHECK(game_queue_action(&loaded, ACTION_USE, x + 8, s - 1));
    run(&loaded, 0.05f);
    CHECK(loaded.bench_open && game_bench_fuel(&loaded, NULL) > fuel - 5 && game_bench_fuel(&loaded, NULL) <= fuel);
    game_free(&loaded);
    remove("build/test_fuel.world");

    /* Taken down, the furnace comes back at its tier. */
    CHECK(game_queue_action(&g, ACTION_MINE, x + 8, s - 1));
    run(&g, 2.0f);
    CHECK(count_item(&g, item_id("furnace_3")) == 1);
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
    /* The backwall records the ground as it was made, before any cave broke
     * through it. */
    int row = 0;
    while (row < w->height && world_get_at(w, LAYER_WALL, x, row) == TILE_AIR) row++;
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
            CHECK(world_get(&w, w.spawn_x, w.spawn_y - i) == TILE_SPAWN_PORTAL);
        CHECK(world_get(&w, w.spawn_x, w.spawn_y - 3) == TILE_AIR);

        int lo = 1000, hi = -1000, max_step = 0, bad = 0, carved = 0;
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
            for (; y < w.height; y++) {
                TileType t = world_get(&w, x, y);
                /* Rock, or a cave: open space carved out of rock and nothing else. */
                if (t == TILE_AIR ? world_get_at(&w, LAYER_WALL, x, y) != TILE_STONE : !in_rock(t)) bad++;
                if (t == TILE_AIR) carved++;
            }
            /* Every block has a backwall of itself behind it; the open sky has none. */
            for (int r = 0; r < w.height; r++) {
                TileType t = world_get(&w, x, r), wall = world_get_at(&w, LAYER_WALL, x, r);
                if (r < top ? wall != TILE_AIR : wall == TILE_AIR) bad++;
                if (t == TILE_DIRT && wall != TILE_DIRT) bad++;
            }
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
        /* There are caves, but the world is still mostly rock. */
        CHECK(carved > 2000 && carved < w.width * w.height / 8);
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
    /* Clicking the soil again, or the square above it where the seed is
     * shown, takes the planting back. */
    CHECK(game_click_tile(&g, start.x + 4, s) && g.queue_count == 0);
    CHECK(game_click_tile(&g, start.x + 4, s) && g.queue_count == 1);
    CHECK(game_click_tile(&g, start.x + 4, s - 1) && g.queue_count == 0);
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
        int exposed = 0;
        for (int y = 1; y < w.height - 1; y++) {
            for (int x = 0; x < w.width; x++) {
                TileType t = world_get(&w, x, y);
                if (t == flint) {
                    if (!world_is_solid(&w, x, y - 1) || !world_is_solid(&w, x + 1, y) ||
                        !world_is_solid(&w, x - 1, y) || !world_is_solid(&w, x, y + 1))
                        exposed++;
                }
            }
        }
        CHECK(exposed == 0);

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
        /* Coal: veins of 5 to 10, about as many of them as copper's or tin's. */
        VeinStats coal = vein_stats(&w, tile_id("coal_ore"), 10);
        VeinStats copper = vein_stats(&w, tile_id("copper_ore"), 6);
        CHECK(coal.smallest >= 5 && coal.over == 0);
        CHECK(coal.groups * 3 > copper.groups * 2 && coal.groups * 2 < copper.groups * 3);
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
    CHECK(tile_def(tile_id("coal_ore"))->drop == item_id("coal"));
    CHECK(tile_def(tile_id("copper_dirt"))->drop == item_id("copper_ore"));
    CHECK(tile_def(tile_id("titanium_ore"))->drop == item_id("titanium_ore"));
    CHECK(tile_def(tile_id("titanium_ore"))->mine_time > tile_def(TILE_STONE)->mine_time);
}

/* Seconds the player takes to mine the block beside them, measured by doing it. */
static float time_to_mine(Game *g, TileType tile)
{
    int x = g->player.feet.x + 1, y = g->player.feet.y;
    world_set(&g->world, x, y, tile);
    if (!game_queue_action(g, ACTION_MINE, x, y)) return -1.0f;
    float t = 0.0f;
    while (world_get(&g->world, x, y) != TILE_AIR && t < 10.0f) {
        game_update(g, 1.0f / 240.0f);
        t += 1.0f / 240.0f;
    }
    run(g, 1.0f);
    return t;
}

static bool about(float a, float b)
{
    return a > b - 0.02f && a < b + 0.02f;
}

/* A recipe with any output count. */
static const Recipe *find_recipe_from(TileType bench, const char *output, const char *input)
{
    const Recipe *list[MAX_BENCH_RECIPES];
    int n = bench_recipes(bench, list, MAX_BENCH_RECIPES);
    for (int i = 0; i < n; i++)
        if (list[i]->output.id == item_id(output) && list[i]->inputs[0].id == item_id(input)) return list[i];
    return NULL;
}

static int coins(Game *g, const char *name)
{
    return inventory_count(&g->inventory, item_id(name));
}

static void test_crystals_and_coins(void)
{
    Game g;
    game_init(&g, 64, 48, 27, WORLD_FLAT);
    Inventory *inv = &g.inventory;
    int s = g.world.surface_y, x = g.world.spawn_x + 4;
    TileType wb = TILE_WORKBENCH, wb3 = tile_id("workbench_3");
    TileType m1 = tile_id("metalwork_bench"), m2 = tile_id("metalwork_bench_2"), m3 = tile_id("metalwork_bench_3");

    /* Crystals start at what the data says, and are not part of a world. */
    CHECK(content.trade.start_crystals == 1000 && g.crystals == 0);
    g.crystals = 5;
    profile_load("build/no_profile.txt", &g);
    CHECK(g.crystals == 1000);
    g.crystals = 742;
    CHECK(profile_save("build/test_profile.txt", &g));
    g.crystals = 0;
    profile_load("build/test_profile.txt", &g);
    CHECK(g.crystals == 742);
    remove("build/test_profile.txt");
    g.crystals = 0;

    /* A portal costs 1 stone and 100 crystals at the workbench. */
    const Recipe *portal = find_recipe(wb, "portal");
    CHECK(costs(portal, "stone", 1, NULL, 0) && portal->crystals == 100 && portal->level == 1);
    CHECK(place_and_open(&g, "workbench", x));
    inventory_add(inv, ITEM_STONE, 5);
    CHECK(recipe_max_crafts(inv, portal, 0) == 0 && !game_craft(&g, portal, 1));
    g.crystals = 250;
    CHECK(recipe_max_crafts(inv, portal, g.crystals) == 2);
    CHECK(game_craft(&g, portal, 5) && g.craft_total == 2);
    run(&g, 0.5f);
    CHECK(coins(&g, "portal") == 2 && g.crystals == 50 && count_item(&g, ITEM_STONE) == 3);
    CHECK(!game_craft(&g, portal, 1));

    /* The workbench goes to tier 3 for 5 iron ingots, and then makes the
     * metalwork bench from 3 iron ingots and 1 stone. */
    const Recipe *metal = find_recipe(wb, "metalwork_bench");
    CHECK(costs(metal, "iron_ingot", 3, "stone", 1) && metal->level == 3);
    CHECK(tile_def(TILE_WORKBENCH_2)->upgrade_to == wb3 && tile_def(wb3)->bench_level == 3);
    CHECK(tile_def(TILE_WORKBENCH_2)->upgrade_cost[0].id == item_id("iron_ingot") &&
          tile_def(TILE_WORKBENCH_2)->upgrade_cost[0].count == 5);
    inventory_add(inv, ITEM_STONE, 3); /* 5 for tier 2, 1 for the bench */
    inventory_add(inv, item_id("iron_ingot"), 8);
    CHECK(game_upgrade_bench(&g) && !game_craft(&g, metal, 1));
    CHECK(game_upgrade_bench(&g) && world_get_at(&g.world, BG, x, s - 1) == wb3);
    CHECK(coins(&g, "iron_ingot") == 3 && !game_upgrade_bench(&g));
    CHECK(game_craft(&g, metal, 1));
    run(&g, 0.3f);
    CHECK(coins(&g, "metalwork_bench") == 1 && coins(&g, "iron_ingot") == 0);

    /* Metalwork bench: coins. */
    CHECK(place_and_open(&g, "metalwork_bench", x + 2));
    const Recipe *mint_copper = find_recipe_from(m1, "copper_coin", "copper_ingot");
    const Recipe *gold_to_copper = find_recipe_from(m1, "copper_coin", "gold_coin");
    const Recipe *mint_gold = find_recipe_from(m1, "gold_coin", "gold_ingot");
    const Recipe *copper_to_gold = find_recipe_from(m1, "gold_coin", "copper_coin");
    const Recipe *plat_to_gold = find_recipe_from(m1, "gold_coin", "platinum_coin");
    const Recipe *mint_plat = find_recipe_from(m1, "platinum_coin", "platinum_ingot");
    const Recipe *gold_to_plat = find_recipe_from(m1, "platinum_coin", "gold_coin");
    CHECK(mint_copper && mint_copper->inputs[0].count == 1 && mint_copper->output.count == 10 && mint_copper->level == 1);
    CHECK(gold_to_copper && gold_to_copper->inputs[0].count == 1 && gold_to_copper->output.count == 100 && gold_to_copper->level == 1);
    CHECK(mint_gold && mint_gold->inputs[0].count == 1 && mint_gold->output.count == 1 && mint_gold->level == 2);
    CHECK(copper_to_gold && copper_to_gold->inputs[0].count == 100 && copper_to_gold->output.count == 1 && copper_to_gold->level == 2);
    CHECK(plat_to_gold && plat_to_gold->inputs[0].count == 1 && plat_to_gold->output.count == 100 && plat_to_gold->level == 2);
    CHECK(mint_plat && mint_plat->inputs[0].count == 5 && mint_plat->output.count == 1 && mint_plat->level == 3);
    CHECK(gold_to_plat && gold_to_plat->inputs[0].count == 100 && gold_to_plat->output.count == 1 && gold_to_plat->level == 3);
    CHECK(tile_def(m1)->upgrade_to == m2 && tile_def(m1)->upgrade_cost[0].id == item_id("copper_ingot") &&
          tile_def(m1)->upgrade_cost[0].count == 5);
    CHECK(tile_def(m2)->upgrade_to == m3 && tile_def(m2)->upgrade_cost[0].id == item_id("gold_ingot") &&
          tile_def(m2)->upgrade_cost[0].count == 5 && tile_def(m3)->upgrade_to == TILE_AIR);

    inventory_add(inv, item_id("copper_ingot"), 15);
    CHECK(game_craft(&g, mint_copper, 10));
    run(&g, 1.5f);
    /* 100 coins is more than one stack holds: 99 and 1. */
    CHECK(coins(&g, "copper_coin") == 100 && coins(&g, "copper_ingot") == 5);
    CHECK(!game_craft(&g, copper_to_gold, 1));
    CHECK(game_upgrade_bench(&g) && coins(&g, "copper_ingot") == 0);
    CHECK(game_craft(&g, copper_to_gold, 1));
    run(&g, 0.3f);
    CHECK(coins(&g, "gold_coin") == 1 && coins(&g, "copper_coin") == 0);
    CHECK(game_craft(&g, gold_to_copper, 1));
    run(&g, 0.3f);
    CHECK(coins(&g, "gold_coin") == 0 && coins(&g, "copper_coin") == 100);
    game_free(&g);
}

static void test_trade(void)
{
    Game g;
    game_init(&g, 64, 48, 28, WORLD_FLAT);
    Inventory *inv = &g.inventory;
    int x = g.world.spawn_x + 4;
    ItemId copper = item_id("copper_coin"), gold = item_id("gold_coin"), plat = item_id("platinum_coin");

    /* The spawn portal turns a portal and a gold coin into a trade portal. */
    const Recipe *tp = find_recipe(TILE_SPAWN_PORTAL, "trade_portal");
    CHECK(costs(tp, "portal", 1, "gold_coin", 1));
    CHECK(tile_def(tile_id("trade_portal"))->trade && !tile_def(TILE_WORKBENCH)->trade);

    /* Every item but the coins is for sale, in a category, for 1 copper. */
    const TradeDef *t = &content.trade;
    CHECK(t->currency_count == 3 && t->currencies[0].item == copper && t->currencies[0].value == 1);
    CHECK(t->currencies[1].item == gold && t->currencies[1].value == 100);
    CHECK(t->currencies[2].item == plat && t->currencies[2].value == 10000);
    int listed[MAX_CATEGORIES] = { 0 };
    for (int i = 1; i < content.item_count; i++) {
        /* (Nor the time crystal, which is never held: it turns into crystals.) */
        bool coin = i == copper || i == gold || i == plat || content.items[i].crystals > 0;
        CHECK(coin ? trade_price(i) == 0 : (trade_price(i) == 1 && content.items[i].category >= 0));
        if (!coin) listed[content.items[i].category]++;
    }
    CHECK(t->category_count >= 4);
    for (int c = 0; c < t->category_count; c++) CHECK(listed[c] > 0);
    CHECK(item_def(ITEM_DIRT)->category != item_def(ITEM_ACORN)->category);

    /* Nothing can be traded away from a trade portal. */
    inventory_add(inv, gold, 1);
    CHECK(!game_trade_open(&g) && !game_trade_buy(&g, ITEM_DIRT, 1));
    CHECK(place_and_open(&g, "trade_portal", x) && game_trade_open(&g));

    /* Buying breaks a bigger coin into change: 1 gold buys a 1 copper item
     * and leaves 99 copper. */
    inventory_add(inv, gold, 1);
    CHECK(trade_wallet(inv) == 100);
    CHECK(game_trade_buy(&g, ITEM_DIRT, 1));
    CHECK(coins(&g, "gold_coin") == 0 && coins(&g, "copper_coin") == 99 && count_item(&g, ITEM_DIRT) == 1);
    CHECK(trade_wallet(inv) == 99);

    /* Selling just adds copper: 99 and 1 more, and no gold coin appears. */
    CHECK(game_trade_sell(&g, ITEM_DIRT, 1));
    CHECK(coins(&g, "copper_coin") == 100 && coins(&g, "gold_coin") == 0 && count_item(&g, ITEM_DIRT) == 0);
    int stacks = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (inv->slots[i].id == copper) stacks++;
    CHECK(stacks == 2);
    CHECK(!game_trade_sell(&g, ITEM_DIRT, 1) && !game_trade_sell(&g, copper, 1));

    /* Converting is its own step: 100 copper to 1 gold, and back. */
    CHECK(game_trade_convert(&g, 0, 1));
    CHECK(coins(&g, "copper_coin") == 0 && coins(&g, "gold_coin") == 1);
    CHECK(!game_trade_convert(&g, 0, 1) && !game_trade_convert(&g, 1, 2) && !game_trade_convert(&g, 0, 2));
    CHECK(game_trade_convert(&g, 1, 0) && coins(&g, "copper_coin") == 100 && coins(&g, "gold_coin") == 0);
    CHECK(game_trade_convert(&g, 0, 1));

    /* Small coins are spent before a big one is broken, and too little money buys nothing. */
    clear_inventory(&g);
    inventory_add(inv, copper, 3);
    inventory_add(inv, gold, 2);
    CHECK(game_trade_buy(&g, ITEM_STONE, 2));
    CHECK(coins(&g, "copper_coin") == 1 && coins(&g, "gold_coin") == 2 && count_item(&g, ITEM_STONE) == 2);
    CHECK(game_trade_buy(&g, ITEM_STONE, 10));
    CHECK(coins(&g, "copper_coin") == 91 && coins(&g, "gold_coin") == 1 && count_item(&g, ITEM_STONE) == 12);
    clear_inventory(&g);
    inventory_add(inv, copper, 4);
    CHECK(!game_trade_buy(&g, ITEM_STONE, 5) && coins(&g, "copper_coin") == 4 && count_item(&g, ITEM_STONE) == 0);
    /* A platinum coin breaks all the way down. */
    clear_inventory(&g);
    inventory_add(inv, plat, 1);
    CHECK(game_trade_buy(&g, ITEM_WOOD, 1));
    CHECK(coins(&g, "platinum_coin") == 0 && coins(&g, "gold_coin") == 99 && coins(&g, "copper_coin") == 99);
    CHECK(trade_wallet(inv) == 9999);

    /* Walking away closes the shop. */
    game_close_bench(&g);
    CHECK(!game_trade_sell(&g, ITEM_WOOD, 1) && count_item(&g, ITEM_WOOD) == 1);
    game_free(&g);
}

static void test_metal_tools(void)
{
    Game g;
    game_init(&g, 64, 48, 30, WORLD_FLAT);
    Inventory *inv = &g.inventory;
    int s = g.world.surface_y, x = g.world.spawn_x + 4;
    TileType tb = tile_id("tool_bench"), mw = tile_id("metalwork_bench");

    /* Bronze: 1 copper and 2 tin ingots at the metalwork bench. */
    const Recipe *bronze = find_recipe(mw, "bronze_ingot");
    CHECK(costs(bronze, "copper_ingot", 1, "tin_ingot", 2) && bronze->level == 1);

    /* The tool bench climbs a tier for 5 ingots of each metal in turn. */
    static const char *tier_cost[] = { "stone", "bronze_ingot", "iron_ingot", "titanium_ingot" };
    TileType tier = tb;
    for (int i = 0; i < 4; i++) {
        const TileDef *def = tile_def(tier);
        CHECK(def->bench_level == i + 1 && def->upgrade_to != TILE_AIR && def->upgrade_cost_count == 1);
        CHECK(def->upgrade_cost[0].id == item_id(tier_cost[i]) && def->upgrade_cost[0].count == 5);
        tier = def->upgrade_to;
    }
    CHECK(tile_def(tier)->bench_level == 5 && tile_def(tier)->upgrade_to == TILE_AIR);

    /* Each metal's pickaxe and axe take 5 ingots and a stick, at its tier. */
    static const char *metals[] = { "bronze", "iron", "titanium" };
    float last = item_def(item_id("stone_pickaxe"))->tool_speed;
    for (int i = 0; i < 3; i++) {
        const char *ingot = TextName("%s_ingot", metals[i]);
        const Recipe *pick = find_recipe(tb, TextName2("%s_pickaxe", metals[i]));
        CHECK(costs(pick, ingot, 5, "stick", 1) && pick->level == i + 3);
        const Recipe *axe = find_recipe(tb, TextName2("%s_axe", metals[i]));
        CHECK(costs(axe, ingot, 5, "stick", 1) && axe->level == i + 3);
        /* And each is faster than the one before. */
        const ItemDef *p = item_def(pick->output.id), *a = item_def(axe->output.id);
        CHECK(p->tool_class == tool_class_id("pickaxe") && a->tool_class == tool_class_id("axe"));
        CHECK(p->tool_speed > last && a->tool_speed == p->tool_speed && p->max_stack == 1);
        last = p->tool_speed;
    }
    /* A tin spade with the bronze tools, a gold shovel with the iron ones. */
    CHECK(costs(find_recipe(tb, "tin_spade"), "tin_ingot", 3, "stick", 1) && find_recipe(tb, "tin_spade")->level == 3);
    CHECK(costs(find_recipe(tb, "gold_shovel"), "gold_ingot", 3, "stick", 1) && find_recipe(tb, "gold_shovel")->level == 4);
    CHECK(item_def(item_id("tin_spade"))->tool_class == tool_class_id("shovel"));
    CHECK(item_def(item_id("gold_shovel"))->tool_class == tool_class_id("shovel"));
    CHECK(item_def(item_id("tin_spade"))->tool_speed > item_def(item_id("stone_shovel"))->tool_speed);
    CHECK(item_def(item_id("gold_shovel"))->tool_speed > item_def(item_id("tin_spade"))->tool_speed);

    /* Played through: smelt bronze, raise the bench to tier 3, make the pickaxe. */
    CHECK(place_and_open(&g, "metalwork_bench", x));
    inventory_add(inv, item_id("copper_ingot"), 10);
    inventory_add(inv, item_id("tin_ingot"), 20);
    CHECK(game_craft(&g, bronze, 10));
    run(&g, 1.5f);
    CHECK(count_item(&g, item_id("bronze_ingot")) == 10 && count_item(&g, item_id("tin_ingot")) == 0);
    int bronze_slot = -1;
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (inv->slots[i].id == item_id("bronze_ingot")) bronze_slot = i;
    ItemStack kept = inv->slots[bronze_slot];

    CHECK(place_and_open(&g, "tool_bench_2", x + 2));
    inv->slots[1] = kept;
    inventory_add(inv, ITEM_STICK, 1);
    const Recipe *pick = find_recipe(tb, "bronze_pickaxe");
    CHECK(!game_craft(&g, pick, 1));
    CHECK(game_upgrade_bench(&g) && world_get_at(&g.world, BG, x + 2, s - 1) == tile_id("tool_bench_3"));
    CHECK(count_item(&g, item_id("bronze_ingot")) == 5);
    CHECK(game_craft(&g, pick, 1) && !game_craft(&g, find_recipe(tb, "iron_pickaxe"), 1));
    run(&g, 0.3f);
    CHECK(count_item(&g, item_id("bronze_pickaxe")) == 1 && count_item(&g, item_id("bronze_ingot")) == 0);
    CHECK(about(game_mine_time(&g, TILE_STONE),
                tile_def(TILE_STONE)->mine_time / item_def(item_id("bronze_pickaxe"))->tool_speed));
    game_free(&g);
}

#define WALL LAYER_WALL

static ItemStack *chest_slot(Game *g, int i);

static int count_tile(const World *w, int layer, TileType t)
{
    int n = 0;
    size_t cells = (size_t)w->width * w->height;
    for (size_t i = 0; i < cells; i++) n += w->layers[layer][i] == t;
    return n;
}

/* What is found in the caves. */
static void test_caves(void)
{
    static const char *gems[] = { "amethyst", "sapphire", "emerald", "ruby", "diamond" };
    int total[5] = { 0 }, crystals = 0;
    for (uint32_t seed = 1; seed <= 8; seed++) {
        World w;
        CHECK(world_init(&w, 2048, 401, seed, WORLD_TERRAIN));
        for (int i = 0; i < 5; i++) total[i] += count_tile(&w, BG, tile_id(gems[i]));
        crystals += count_tile(&w, BG, tile_id("time_crystal"));

        int bad = 0;
        for (int y = 0; y < w.height - 1; y++) {
            for (int x = 0; x < w.width; x++) {
                const TileDef *def = tile_def(world_get_at(&w, BG, x, y));
                if (def->light == 0 || def->log_of >= 0 || def->leaves_of >= 0) continue;
                /* Gems and crystals sit in open cave, against a block; crystals on the floor. */
                if (world_get(&w, x, y) != TILE_AIR || world_get_at(&w, WALL, x, y) != TILE_STONE) bad++;
                bool held = world_is_solid(&w, x, y + 1) || world_is_solid(&w, x + 1, y) ||
                            world_is_solid(&w, x - 1, y) || world_is_solid(&w, x, y - 1);
                if (!held) bad++;
                if (world_get_at(&w, BG, x, y) == tile_id("time_crystal") && !world_is_solid(&w, x, y + 1)) bad++;
            }
        }
        CHECK(bad == 0);
        world_free(&w);
    }
    /* Each gem is rarer than the one before it. */
    for (int i = 0; i < 5; i++) CHECK(total[i] > 0 && (i == 0 || total[i] < total[i - 1]));
    CHECK(crystals > 0);
    for (int i = 0; i < 5; i++) {
        const TileDef *def = tile_def(tile_id(gems[i]));
        CHECK(def->light > 0 && def->light < tile_def(tile_id("torch"))->light && def->layer == LAYER_BACKGROUND);
        CHECK(item_def(item_id(gems[i]))->places == tile_id(gems[i]) && trade_price(item_id(gems[i])) > 0);
    }
}

static void test_backwall(void)
{
    Game g;
    game_init(&g, 64, 48, 31, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    TilePos start = g.player.feet;
    int s = w->surface_y, x = start.x + 2;

    /* Every block has a backwall of itself; the air above the ground has none. */
    CHECK(world_get_at(w, WALL, x, s) == TILE_DIRT && world_get_at(w, WALL, x, s - 1) == TILE_AIR);

    /* Mining a block leaves its backwall, and gives one item, not two. */
    CHECK(game_click_tile(&g, x, s));
    run(&g, 2.0f);
    CHECK(world_get(w, x, s) == TILE_AIR && world_get_at(w, WALL, x, s) == TILE_DIRT);
    CHECK(count_item(&g, ITEM_DIRT) == 1);

    /* Placing a different block in front does not change the backwall... */
    clear_inventory(&g);
    inventory_add(inv, ITEM_STONE, 2);
    inventory_select(inv, 0, -1);
    CHECK(game_click_tile(&g, x, s));
    run(&g, 2.0f);
    CHECK(world_get(w, x, s) == TILE_STONE && world_get_at(w, WALL, x, s) == TILE_DIRT);
    /* ...and a block placed where there was none brings its own. */
    CHECK(game_click_tile(&g, x + 1, s - 1));
    run(&g, 2.0f);
    CHECK(world_get(w, x + 1, s - 1) == TILE_STONE && world_get_at(w, WALL, x + 1, s - 1) == TILE_STONE);

    /* With no tool in hand, a click on bare backwall is just somewhere to go. */
    clear_inventory(&g);
    CHECK(game_queue_action(&g, ACTION_MINE, x, s) && game_queue_action(&g, ACTION_MINE, x + 1, s - 1));
    run(&g, 4.0f);
    clear_inventory(&g);
    CHECK(world_get(w, x, s) == TILE_AIR && world_get_at(w, WALL, x, s) == TILE_DIRT);
    CHECK(game_click_tile(&g, x, s) && g.queue[0].type == ACTION_MOVE);
    game_cancel_actions(&g);

    /* The wrong kind of tool does nothing to it either: dirt wants a shovel. */
    inventory_add(inv, item_id("wooden_pickaxe"), 1);
    inventory_add(inv, item_id("wooden_shovel"), 1);
    inventory_select(inv, 0, -1);
    CHECK(game_click_tile(&g, x, s) && g.queue[0].type == ACTION_MOVE);
    game_cancel_actions(&g);
    /* The right kind, in hand, takes the backwall down, and nothing drops. */
    inventory_select(inv, 1, -1);
    CHECK(game_click_tile(&g, x, s) && g.queue[0].type == ACTION_MINE && g.queue[0].wall);
    run(&g, 3.0f);
    CHECK(is_idle(&g) && world_get_at(w, WALL, x, s) == TILE_AIR && count_item(&g, ITEM_DIRT) == 0);
    for (int i = 0; i < MAX_DROPS; i++) CHECK(!g.drops[i].active);
    /* A stone backwall wants the pickaxe. */
    CHECK(world_get_at(w, WALL, x + 1, s - 1) == TILE_STONE);
    CHECK(game_click_tile(&g, x + 1, s - 1) && g.queue[0].type == ACTION_MOVE);
    game_cancel_actions(&g);
    inventory_select(inv, 0, -1);
    CHECK(game_click_tile(&g, x + 1, s - 1) && g.queue[0].wall);
    run(&g, 3.0f);
    CHECK(world_get_at(w, WALL, x + 1, s - 1) == TILE_AIR);
    /* A block in front is always mined before the wall behind it. */
    CHECK(game_click_tile(&g, x + 3, s) && !g.queue[0].wall);
    run(&g, 3.0f);
    CHECK(world_get(w, x + 3, s) == TILE_AIR && world_get_at(w, WALL, x + 3, s) == TILE_DIRT);

    /* A torch or a gem hangs on a backwall, with no block beside it. */
    clear_inventory(&g);
    for (int y = s - 6; y <= s - 2; y++) world_set_at(w, WALL, x + 6, y, TILE_STONE);
    inventory_add(inv, item_id("torch"), 2);
    inventory_add(inv, item_id("amethyst"), 1);
    inventory_select(inv, 0, -1);
    CHECK(!game_queue_action(&g, ACTION_PLACE, x + 8, s - 3)); /* open air, nothing to hang on */
    CHECK(game_queue_action(&g, ACTION_PLACE, x + 6, s - 2));
    run(&g, 3.0f);
    CHECK(world_get_at(w, BG, x + 6, s - 2) == tile_id("torch") && world_get(w, x + 6, s - 2) == TILE_AIR);
    CHECK(world_get_at(w, WALL, x + 6, s - 2) == TILE_STONE);
    inventory_select(inv, 1, -1);
    CHECK(game_queue_action(&g, ACTION_PLACE, x + 7, s - 1));
    run(&g, 3.0f);
    CHECK(world_get_at(w, BG, x + 7, s - 1) == tile_id("amethyst") && count_item(&g, item_id("amethyst")) == 0);
    /* Tapping the gem again picks it up. */
    CHECK(game_click_tile(&g, x + 7, s - 1) && g.queue[0].type == ACTION_MINE);
    run(&g, 3.0f);
    CHECK(world_get_at(w, BG, x + 7, s - 1) == TILE_AIR && count_item(&g, item_id("amethyst")) == 1);

    /* Backwalls are saved with the world. */
    CHECK(save_game(&g, "build/test_wall.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_wall.world"));
    size_t cells = (size_t)w->width * w->height;
    CHECK(memcmp(loaded.world.layers[WALL], w->layers[WALL], cells) == 0);
    game_free(&loaded);
    remove("build/test_wall.world");
    game_free(&g);
}

static int light_at(const Game *g, int x, int y)
{
    uint8_t v;
    light_compute(g, x, y, 1, 1, &v);
    return v;
}

static void test_light(void)
{
    Game g;
    game_init(&g, 64, 48, 32, WORLD_FLAT);
    World *w = &g.world;
    TilePos start = g.player.feet;
    int s = w->surface_y, x = start.x + 10;
    const RulesDef *u = &content.rules;

    /* Open sky is as bright as the sun by day and the moon by night. */
    g.tick = (uint64_t)DAY_TICKS / 2;
    CHECK(light_sky(&g) == u->light_sun && light_at(&g, x, s - 3) == u->light_sun);
    g.tick = (uint64_t)DAY_TICKS + (uint64_t)NIGHT_TICKS / 2;
    CHECK(light_sky(&g) == u->light_moon && light_at(&g, x, s - 3) == u->light_moon);
    CHECK(u->light_moon < u->light_sun && u->light_moon > u->light_minimum);
    g.tick = (uint64_t)DAY_TICKS / 2;

    /* It fades going down into the ground, to the least there ever is. */
    CHECK(light_at(&g, x, s) == u->light_sun - u->light_solid_falloff);
    CHECK(light_at(&g, x, s + 1) < light_at(&g, x, s));
    CHECK(light_at(&g, x, s + 12) == u->light_minimum);

    /* A sealed room underground is dark... */
    for (int cx = x - 2; cx <= x + 2; cx++)
        for (int cy = s + 10; cy <= s + 12; cy++) world_set(w, cx, cy, TILE_AIR);
    CHECK(light_at(&g, x, s + 11) == u->light_minimum);
    /* ...until there is a torch in it, brightest where it hangs. */
    world_set_at(w, BG, x, s + 11, tile_id("torch"));
    int torch = tile_def(tile_id("torch"))->light;
    CHECK(light_at(&g, x, s + 11) == torch && light_at(&g, x + 1, s + 11) == torch - u->light_air_falloff);
    CHECK(light_at(&g, x + 2, s + 11) == torch - 2 * u->light_air_falloff);
    /* It gets into the rock a little way, and no further. */
    CHECK(light_at(&g, x + 3, s + 11) == torch - 2 * u->light_air_falloff - u->light_solid_falloff);
    CHECK(light_at(&g, x + 12, s + 11) == u->light_minimum);
    /* A gem gives less than a torch. */
    world_set_at(w, BG, x, s + 11, tile_id("ruby"));
    CHECK(light_at(&g, x, s + 11) == tile_def(tile_id("ruby"))->light && light_at(&g, x, s + 11) < torch);
    world_set_at(w, BG, x, s + 11, TILE_AIR);

    /* The sun beams straight down an open shaft, backwall or no. */
    int sx = x + 8;
    for (int cy = s; cy <= s + 12; cy++) world_set(w, sx, cy, TILE_AIR);
    CHECK(world_get_at(w, WALL, sx, s + 12) == TILE_DIRT && light_at(&g, sx, s + 12) == u->light_sun);
    /* A lid on the shaft cuts the beam off. */
    world_set(w, sx, s, TILE_STONE);
    CHECK(light_at(&g, sx, s + 12) == u->light_minimum);
    world_set(w, sx, s, TILE_AIR);
    /* Round a corner at the bottom it falls away a tile at a time. */
    world_set(w, sx + 1, s + 12, TILE_AIR);
    world_set(w, sx + 2, s + 12, TILE_AIR);
    CHECK(light_at(&g, sx + 1, s + 12) == u->light_sun - u->light_air_falloff);
    CHECK(light_at(&g, sx + 2, s + 12) == u->light_sun - 2 * u->light_air_falloff);
    /* Open space with no backwall counts as outdoors even under a roof. */
    world_set_at(w, WALL, sx + 2, s + 12, TILE_AIR);
    CHECK(light_at(&g, sx + 2, s + 12) == u->light_sun);

    /* A campfire only gives light while it is burning. */
    CHECK(place_and_open(&g, "campfire", start.x + 3));
    g.tick = (uint64_t)DAY_TICKS + (uint64_t)NIGHT_TICKS / 2;
    int fire = tile_def(tile_id("campfire"))->light;
    CHECK(fire > u->light_moon && light_at(&g, start.x + 3, s - 1) == u->light_moon);
    inventory_add(&g.inventory, ITEM_STICK, 3);
    CHECK(game_add_fuel(&g, ITEM_STICK) && light_at(&g, start.x + 3, s - 1) == fire);
    /* And it makes torches, a stick each. */
    const Recipe *torch_recipe = find_recipe(tile_id("campfire"), "torch");
    CHECK(costs(torch_recipe, "stick", 1, NULL, 0) && costs(find_recipe(TILE_WORKBENCH, "campfire"), "stick", 10, NULL, 0));
    CHECK(find_recipe(TILE_WORKBENCH, "campfire")->level == 1);
    CHECK(game_craft(&g, torch_recipe, 2));
    run(&g, 0.4f);
    CHECK(count_item(&g, item_id("torch")) == 2 && count_item(&g, ITEM_STICK) == 0);

    /* A lit region is the same whether worked out whole or a tile at a time. */
    uint8_t block[9 * 7];
    light_compute(&g, start.x - 2, s - 4, 9, 7, block);
    int differ = 0;
    for (int cy = 0; cy < 7; cy++)
        for (int cx = 0; cx < 9; cx++)
            if (block[cy * 9 + cx] != light_at(&g, start.x - 2 + cx, s - 4 + cy)) differ++;
    CHECK(differ == 0);
    game_free(&g);
}

static void test_portal_tiers(void)
{
    static const char *gems[] = { "amethyst", "sapphire", "emerald", "ruby", "diamond" };
    Game g;
    game_init(&g, 64, 48, 33, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    int s = w->surface_y, x = g.world.spawn_x + 4;
    TileType portal = tile_id("portal");

    /* A portal climbs a tier for one of each gem in turn, and each tier
     * unlocks that gem's pickaxe, which multiplies ore: 2 times up to 6. */
    TileType tier = portal;
    for (int i = 0; i < 5; i++) {
        const TileDef *def = tile_def(tier);
        CHECK(def->bench && def->bench_level == i + 1 && def->upgrade_cost_count == 1);
        CHECK(def->upgrade_cost[0].id == item_id(gems[i]) && def->upgrade_cost[0].count == 1);
        tier = def->upgrade_to;
        const Recipe *pick = find_recipe(portal, TextName("%s_pickaxe", gems[i]));
        CHECK(pick && pick->level == i + 2);
        const ItemDef *tool = item_def(pick->output.id);
        CHECK(tool->tool_class == tool_class_id("pickaxe") && tool->ore_multiplier == i + 2);
        CHECK(tool->tool_speed > item_def(item_id("titanium_pickaxe"))->tool_speed && trade_price(pick->output.id) > 0);
    }
    CHECK(tile_def(tier)->bench_level == 6 && tile_def(tier)->upgrade_to == TILE_AIR);
    CHECK(costs(find_recipe(portal, "trade_portal"), "portal", 1, "gold_coin", 1));

    /* Ore comes out multiplied; plain rock does not. */
    clear_inventory(&g);
    CHECK(inventory_ore_multiplier(inv, tool_class_id("pickaxe")) == 1);
    inventory_add(inv, item_id("iron_pickaxe"), 1);
    CHECK(inventory_ore_multiplier(inv, tool_class_id("pickaxe")) == 1);
    inventory_add(inv, item_id("amethyst_pickaxe"), 1);
    CHECK(inventory_ore_multiplier(inv, tool_class_id("pickaxe")) == 2);
    g.player.feet = g.player.step_to = (TilePos){ x, s - 1 };
    world_set(w, x + 1, s - 1, tile_id("iron_ore"));
    world_set(w, x - 1, s - 1, TILE_STONE);
    CHECK(game_queue_action(&g, ACTION_MINE, x + 1, s - 1) && game_queue_action(&g, ACTION_MINE, x - 1, s - 1));
    run(&g, 4.0f);
    CHECK(count_item(&g, item_id("iron_ore")) == 2 && count_item(&g, ITEM_STONE) == 1);
    inventory_add(inv, item_id("diamond_pickaxe"), 1);
    world_set(w, x + 1, s - 1, tile_id("coal_ore"));
    CHECK(game_queue_action(&g, ACTION_MINE, x + 1, s - 1));
    run(&g, 3.0f);
    CHECK(count_item(&g, item_id("coal")) == 6);

    /* A time crystal block gives 5 crystals - multiplied like ore - straight
     * into the player's crystals, never the inventory. */
    CHECK(tile_def(tile_id("time_crystal"))->ore && tile_def(tile_id("time_crystal"))->drop_count == 5);
    clear_inventory(&g);
    g.crystals = 10;
    world_set_at(w, BG, x + 1, s - 1, tile_id("time_crystal"));
    CHECK(game_click_tile(&g, x + 1, s - 1));
    run(&g, 4.0f);
    CHECK(g.crystals == 15 && count_item(&g, item_id("time_crystal")) == 0);
    for (int i = 0; i < INVENTORY_SLOTS; i++) CHECK(inv->slots[i].count == 0);
    inventory_add(inv, item_id("ruby_pickaxe"), 1);
    world_set_at(w, BG, x + 1, s - 1, tile_id("time_crystal"));
    CHECK(game_click_tile(&g, x + 1, s - 1));
    run(&g, 4.0f);
    CHECK(g.crystals == 15 + 25);
    /* With no room in the inventory at all, crystals are still collected. */
    clear_inventory(&g);
    for (int i = 0; i < INVENTORY_SLOTS; i++) inv->slots[i] = (ItemStack){ (uint8_t)ITEM_BASKET, 1, 0 };
    world_set_at(w, BG, x + 1, s - 1, tile_id("time_crystal"));
    CHECK(game_click_tile(&g, x + 1, s - 1));
    run(&g, 4.0f);
    CHECK(g.crystals == 45);

    /* The portal chest: 1 chest and 100 crystals at a tier 6 portal. */
    const Recipe *pc = find_recipe(portal, "portal_chest");
    CHECK(costs(pc, "chest", 1, NULL, 0) && pc->crystals == 100 && pc->level == 6);
    CHECK(place_and_open(&g, "portal", x + 3));
    inventory_add(inv, ITEM_CHEST, 1);
    g.crystals = 150;
    CHECK(!game_craft(&g, pc, 1));
    for (int i = 0; i < 5; i++) {
        inventory_add(inv, item_id(gems[i]), 1);
        CHECK(game_upgrade_bench(&g));
    }
    CHECK(world_get_at(w, BG, x + 3, s - 1) == tile_id("portal_6") && !game_upgrade_bench(&g));
    CHECK(game_craft(&g, pc, 1));
    run(&g, 0.3f);
    CHECK(count_item(&g, item_id("portal_chest")) == 1 && g.crystals == 50 && count_item(&g, ITEM_CHEST) == 0);
    game_free(&g);
}

/* Portals stand three blocks tall and are one thing. */
static void test_tall_portals(void)
{
    static const char *gems[] = { "amethyst", "sapphire", "emerald", "ruby", "diamond" };
    Game g;
    game_init(&g, 64, 48, 35, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    int s = w->surface_y, px = w->spawn_x, x = px + 4;
    TileType portal = tile_id("portal"), trade = tile_id("trade_portal");

    CHECK(tile_def(portal)->height == 3 && tile_def(trade)->height == 3 && tile_def(TILE_SPAWN_PORTAL)->height == 3);
    CHECK(tile_def(TILE_DIRT)->height == 1 && tile_def(TILE_WORKBENCH)->height == 1);

    /* Placing one puts up all three blocks; it needs the room for them. */
    CHECK(place_and_open(&g, "portal", x));
    for (int i = 0; i < 3; i++) CHECK(world_get_at(w, BG, x, s - 1 - i) == portal);
    CHECK(world_get_at(w, BG, x, s - 4) == TILE_AIR && count_item(&g, item_id("portal")) == 0);
    int bottom;
    CHECK(world_structure(w, BG, x, s - 3, &bottom) == 3 && bottom == s - 1);
    CHECK(world_structure(w, BG, x, s - 1, &bottom) == 3 && bottom == s - 1);
    CHECK(world_structure(w, 0, x, s, &bottom) == 1 && bottom == s);
    clear_inventory(&g);
    inventory_add(inv, item_id("trade_portal"), 2);
    inventory_select(inv, 0, -1);
    world_set(w, x + 2, s - 3, TILE_STONE);
    CHECK(!game_queue_action(&g, ACTION_PLACE, x + 2, s - 1)); /* a block where its top would be */
    CHECK(!game_queue_action(&g, ACTION_PLACE, x, s - 4));     /* nor floating on top of another */
    world_set(w, x + 2, s - 3, TILE_AIR);
    CHECK(game_queue_action(&g, ACTION_PLACE, x + 2, s - 1));
    run(&g, 2.0f);
    for (int i = 0; i < 3; i++) CHECK(world_get_at(w, BG, x + 2, s - 1 - i) == trade);
    CHECK(count_item(&g, item_id("trade_portal")) == 1);

    /* Any of its blocks opens it, and they are walked in front of. */
    game_close_bench(&g);
    CHECK(game_queue_action(&g, ACTION_USE, x + 2, s - 3));
    run(&g, 2.0f);
    CHECK(g.bench_open && game_trade_open(&g));
    game_close_bench(&g);
    CHECK(game_bench_at(&g, x, s - 2) == portal && game_bench_at(&g, x, s - 4) == TILE_AIR);

    /* Upgrading changes the whole portal, from whichever block was opened. */
    CHECK(game_queue_action(&g, ACTION_USE, x, s - 2));
    run(&g, 2.0f);
    CHECK(g.bench_open && g.bench_pos.y == s - 2);
    inventory_add(inv, item_id("amethyst"), 1);
    CHECK(game_upgrade_bench(&g));
    for (int i = 0; i < 3; i++) CHECK(world_get_at(w, BG, x, s - 1 - i) == tile_id("portal_2"));
    CHECK(g.bench_open && find_recipe(portal, "amethyst_pickaxe") &&
          !recipe_locked(game_bench_at(&g, x, s - 2), find_recipe(portal, "amethyst_pickaxe")));

    /* Removing any block of it takes the whole thing down and gives one item back. */
    clear_inventory(&g);
    game_close_bench(&g);
    CHECK(game_queue_action(&g, ACTION_MINE, x, s - 3));
    run(&g, 3.0f);
    for (int i = 0; i < 4; i++) CHECK(world_get_at(w, BG, x, s - 1 - i) == TILE_AIR);
    CHECK(count_item(&g, item_id("portal_2")) == 1 && count_item(&g, item_id("portal")) == 0);

    /* The spawn portal has the same recipes as a made one, and the same tiers. */
    const Recipe *a[MAX_BENCH_RECIPES], *b[MAX_BENCH_RECIPES];
    int na = bench_recipes(TILE_SPAWN_PORTAL, a, MAX_BENCH_RECIPES), nb = bench_recipes(portal, b, MAX_BENCH_RECIPES);
    CHECK(na == nb && na >= 10);
    for (int i = 0; i < na && i < nb; i++) CHECK(a[i] == b[i]);
    CHECK(find_recipe(portal, "workbench") && find_recipe(TILE_SPAWN_PORTAL, "portal_chest"));
    CHECK(find_recipe(TILE_SPAWN_PORTAL, "diamond_pickaxe")->level == 6);

    TileType tier = TILE_SPAWN_PORTAL;
    g.player.feet = g.player.step_to = (TilePos){ px, s - 1 };
    CHECK(game_queue_action(&g, ACTION_USE, px, s - 3));
    run(&g, 1.0f);
    CHECK(g.bench_open);
    for (int i = 0; i < 5; i++) {
        const TileDef *def = tile_def(tier);
        CHECK(def->bench_level == i + 1 && !def->removable && def->upgrade_cost[0].id == item_id(gems[i]));
        CHECK(!game_upgrade_bench(&g));
        inventory_add(inv, item_id(gems[i]), 1);
        CHECK(game_upgrade_bench(&g));
        tier = def->upgrade_to;
        for (int k = 0; k < 3; k++) CHECK(world_get(w, px, s - 1 - k) == tier);
    }
    CHECK(tile_def(tier)->bench_level == 6 && tile_def(tier)->upgrade_to == TILE_AIR && !tile_def(tier)->removable);
    CHECK(world_get(w, px, s) == TILE_PORTAL_BASE && g.bench_open);
    /* It still cannot be taken down or built over. */
    CHECK(!game_queue_action(&g, ACTION_MINE, px, s - 2));

    /* An old save's spawn portal top, a block that no longer exists by that
     * name, loads as part of the portal. */
    CHECK(tile_id("spawn_portal_top") == -1 && tile_id_or_alias("spawn_portal_top") == TILE_SPAWN_PORTAL);
    CHECK(tile_id_or_alias("no_such_block") == -1);
    game_free(&g);
}

static void test_portal_chest(void)
{
    const char *profile = "build/test_shared.txt";
    Game g;
    game_init(&g, 64, 48, 34, WORLD_FLAT);
    Inventory *inv = &g.inventory;
    int x = g.world.spawn_x + 4;
    remove(profile);
    profile_load(profile, &g);

    /* Two portal chests in one world open onto the same sixteen slots. */
    CHECK(place_and_open(&g, "portal_chest", x));
    uint16_t shared = game_open_storage(&g);
    CHECK(shared == SHARED_CONTAINER && chest_slot(&g, 15) != NULL);
    inventory_add(inv, ITEM_WOOD, 30);
    inventory_add(inv, ITEM_STONE, 8);
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 0, 0 }, (SlotRef){ SLOT_CONTAINER, shared, 3 }, 20));
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 1, 0 }, (SlotRef){ SLOT_CONTAINER, shared, 9 }, 8));
    ItemStack carried = inv->slots[0];
    CHECK(place_and_open(&g, "portal_chest", x + 3));
    CHECK(game_open_storage(&g) == shared && chest_slot(&g, 3)->id == ITEM_WOOD && chest_slot(&g, 3)->count == 20);
    (void)carried;

    /* Only plain items go in: a basket with something in it is turned away,
     * an empty one is not. */
    clear_inventory(&g);
    inv->containers[SHARED_CONTAINER].used = true;
    inventory_add(inv, ITEM_BASKET, 1);
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 0, 0 }, (SlotRef){ SLOT_CONTAINER, shared, 0 }, 1));
    CHECK(inventory_move(inv, (SlotRef){ SLOT_CONTAINER, shared, 0 }, (SlotRef){ SLOT_BAR, 0, 0 }, 1));
    inventory_add(inv, ITEM_DIRT, 4);
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 1, 0 }, (SlotRef){ SLOT_BASKET, 0, 0 }, 4));
    CHECK(!inventory_move(inv, (SlotRef){ SLOT_BAR, 0, 0 }, (SlotRef){ SLOT_CONTAINER, shared, 0 }, 1));

    /* Taking the chest down does not take its contents with it. */
    CHECK(game_queue_action(&g, ACTION_MINE, x + 3, g.world.surface_y - 1));
    run(&g, 3.0f);
    CHECK(count_item(&g, item_id("portal_chest")) == 1 && count_item(&g, ITEM_WOOD) == 0);
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (inv->slots[i].id == item_id("portal_chest")) CHECK(inv->slots[i].container == 0);

    /* What is in it, and the crystals, belong to the player: they are in the
     * profile, and turn up in a different world. */
    g.crystals = 321;
    CHECK(profile_save(profile, &g) && save_game(&g, "build/test_shared.world"));
    game_free(&g);

    Game other;
    game_init(&other, 64, 48, 99, WORLD_FLAT);
    profile_load(profile, &other);
    CHECK(other.crystals == 321);
    CHECK(place_and_open(&other, "portal_chest", other.world.spawn_x + 4));
    CHECK(chest_slot(&other, 3)->id == ITEM_WOOD && chest_slot(&other, 3)->count == 20);
    CHECK(chest_slot(&other, 9)->id == ITEM_STONE && chest_slot(&other, 9)->count == 8 && chest_slot(&other, 0)->count == 0);
    /* Emptied in this world, it is empty when the first one is opened again,
     * whatever that world's own save remembers. */
    CHECK(inventory_move(&other.inventory, (SlotRef){ SLOT_CONTAINER, SHARED_CONTAINER, 3 },
                         (SlotRef){ SLOT_BAR, 3, 0 }, 255));
    CHECK(profile_save(profile, &other));
    game_free(&other);

    Game back;
    CHECK(load_game(&back, "build/test_shared.world"));
    profile_load(profile, &back);
    CHECK(back.inventory.containers[SHARED_CONTAINER].slots[3].count == 0);
    CHECK(back.inventory.containers[SHARED_CONTAINER].slots[9].count == 8);
    game_free(&back);
    remove(profile);
    remove("build/test_shared.world");
}

static void test_tools(void)
{
    Game g;
    game_init(&g, 64, 48, 22, WORLD_FLAT);
    Inventory *inv = &g.inventory;
    int shovel = tool_class_id("shovel"), pickaxe = tool_class_id("pickaxe"), axe = tool_class_id("axe");
    float stone = tile_def(TILE_STONE)->mine_time, dirt = tile_def(TILE_DIRT)->mine_time;
    float log = tile_def(TILE_OAK_LOG)->mine_time;

    /* Three kinds of tool, and every block that can be mined belongs to one. */
    CHECK(shovel >= 0 && pickaxe >= 0 && axe >= 0 && content.tool_class_count == 3);
    for (int t = 1; t < content.tile_count; t++)
        CHECK(!content.tiles[t].removable || content.tiles[t].tool_class >= 0);
    CHECK(tile_def(TILE_DIRT)->tool_class == shovel && tile_def(TILE_GRASS)->tool_class == shovel);
    CHECK(tile_def(tile_id("flint"))->tool_class == shovel && tile_def(tile_id("copper_dirt"))->tool_class == shovel);
    CHECK(tile_def(TILE_STONE)->tool_class == pickaxe && tile_def(tile_id("gold_ore"))->tool_class == pickaxe);
    CHECK(tile_def(tile_id("coal_ore"))->tool_class == pickaxe && tile_def(tile_id("furnace"))->tool_class == pickaxe);
    CHECK(tile_def(TILE_OAK_LOG)->tool_class == axe && tile_def(TILE_CHERRY_LEAVES)->tool_class == axe);
    CHECK(tile_def(TILE_WOOD)->tool_class == axe && tile_def(tile_id("chest"))->tool_class == axe);
    CHECK(tile_def(TILE_PORTAL_BASE)->tool_class == -1);
    CHECK(item_def(item_id("wooden_pickaxe"))->tool_class == pickaxe && item_def(item_id("stone_shovel"))->tool_class == shovel);
    CHECK(item_def(item_id("flint_axe"))->tool_class == axe && item_def(ITEM_DIRT)->tool_class == -1);

    /* Bare hands take a block's own time. */
    clear_inventory(&g);
    CHECK(about(time_to_mine(&g, TILE_STONE), stone) && about(time_to_mine(&g, TILE_DIRT), dirt));
    clear_inventory(&g);

    /* A pickaxe speeds up stone and nothing else; it only has to be carried. */
    float wooden = item_def(item_id("wooden_pickaxe"))->tool_speed;
    float best = item_def(item_id("stone_pickaxe"))->tool_speed;
    CHECK(wooden > 1.0f && best > wooden);
    inventory_add(inv, ITEM_STICK, 1);
    inventory_add(inv, item_id("wooden_pickaxe"), 1);
    inventory_select(inv, 0, -1);
    CHECK(about(game_mine_time(&g, TILE_STONE), stone / wooden));
    CHECK(about(time_to_mine(&g, TILE_STONE), stone / wooden));
    CHECK(about(time_to_mine(&g, TILE_DIRT), dirt));
    CHECK(about(game_mine_time(&g, TILE_OAK_LOG), log));

    /* The best one carried is the one that counts, even from inside a basket. */
    clear_inventory(&g);
    inventory_add(inv, ITEM_BASKET, 1);
    inventory_add(inv, item_id("wooden_pickaxe"), 1);
    *inventory_slot(inv, (SlotRef){ SLOT_BASKET, 0, 2 }) = (ItemStack){ (uint8_t)item_id("stone_pickaxe"), 1, 0 };
    CHECK(about(time_to_mine(&g, TILE_STONE), stone / best));

    /* Shovels and axes do the same for their own blocks. */
    clear_inventory(&g);
    inventory_add(inv, item_id("flint_shovel"), 1);
    inventory_add(inv, item_id("flint_axe"), 1);
    CHECK(about(game_mine_time(&g, TILE_DIRT), dirt / item_def(item_id("flint_shovel"))->tool_speed));
    CHECK(about(game_mine_time(&g, TILE_OAK_LOG), log / item_def(item_id("flint_axe"))->tool_speed));
    CHECK(about(game_mine_time(&g, TILE_STONE), stone));
    CHECK(about(time_to_mine(&g, TILE_GRASS), tile_def(TILE_GRASS)->mine_time / item_def(item_id("flint_shovel"))->tool_speed));
    game_free(&g);
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

/* What a new world hands the player, as shipped. */
static void test_start_items(void)
{
    Game g;
    game_init(&g, 64, 48, 29, WORLD_FLAT);
    CHECK(g.inventory.slots[0].id == ITEM_BASKET && g.inventory.slots[0].count == 1);
    CHECK(count_item(&g, item_id("trade_portal")) == 1 && count_item(&g, item_id("gold_coin")) == 1);

    /* The spawn portal also hands out a trade portal or a gold coin for 1 dirt each. */
    const Recipe *list[MAX_BENCH_RECIPES];
    int n = bench_recipes(TILE_SPAWN_PORTAL, list, MAX_BENCH_RECIPES), cheap_portal = 0, cheap_coin = 0;
    for (int i = 0; i < n; i++) {
        if (!costs(list[i], "dirt", 1, NULL, 0) || list[i]->output.count != 1) continue;
        if (list[i]->output.id == item_id("trade_portal")) cheap_portal++;
        if (list[i]->output.id == item_id("gold_coin")) cheap_coin++;
    }
    CHECK(cheap_portal == 1 && cheap_coin == 1);
    game_free(&g);
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
               " \"places\": \"gem_block\", \"category\": \"materials\", \"price\": 7 } ] }");
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
    /* The new item is on sale at once, at its own price. */
    CHECK(item_def(item_id("gem"))->category == item_def(ITEM_STICK)->category && trade_price(item_id("gem")) == 7);

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
    /* So is an item nobody could buy or sell. */
    write_file("build/mod/recipes.json", "{ }");
    write_file("build/mod/items.json", "{ \"items\": [ { \"name\": \"orphan\" } ] }");
    CHECK(!content_load("build/mod", err, sizeof err));
    CHECK(strstr(err, "orphan") && strstr(err, "category"));
    write_file("build/mod/items.json", "{ }");
    write_file("build/mod/recipes.json", "{ not json");
    CHECK(!content_load("build/mod", err, sizeof err) && strstr(err, "not valid JSON"));

    content_reset();
    CHECK(content_load("data", err, sizeof err) && content.tile_count == tiles);
    CHECK(content.rules.start_item_count == 3);
}

int main(void)
{
    char err[256];
    if (!content_load("data", err, sizeof err)) {
        printf("could not load data: %s\n", err);
        return 1;
    }
    /* The shipped data starts the player with extras for trying things out.
     * Every other test assumes a bare start: one basket. */
    test_start_items();
    content.rules.start_item_count = 1;

    test_world();
    test_mine_range();
    test_keys();
    test_click_move();
    test_click_mine();
    test_queue();
    test_unqueue();
    test_monkey_bars();
    test_crawl();
    test_blocked_corner();
    test_hop_gap();
    test_climb_drop();
    test_wall_hop();
    test_step_down();
    test_inventory();
    test_drops();
    test_place();
    test_bench();
    test_work_benches();
    test_tools();
    test_caves();
    test_backwall();
    test_light();
    test_portal_tiers();
    test_tall_portals();
    test_portal_chest();
    test_metal_tools();
    test_crystals_and_coins();
    test_trade();
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
