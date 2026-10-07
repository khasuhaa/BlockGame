/* Headless tests for the game rules: no window, just the simulation. */
#include "../src/game.h"
#include "../src/light.h"
#include "../src/mob.h"
#include "../src/save.h"
#include "../src/trade.h"

#include <math.h>
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
    g.drops[0] = (Drop){ true, (uint16_t)ITEM_STONE, at.x + 9, (float)(s - 1), 0.0f, 0.0f, false, 0, 500, 0, false };
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
    g.drops[0] = (Drop){ true, (uint16_t)ITEM_DIRT, start.x + 6, (float)(s - 5), 0.0f, 0.0f, false, 0, 1, 0, false };
    run(&g, 2.0f);
    CHECK(g.drops[0].active && g.drops[0].y == (float)(s - 1));
    CHECK(game_click_tile(&g, start.x + 5, start.y));
    run(&g, 2.0f);
    CHECK(!g.drops[0].active && count_item(&g, ITEM_DIRT) == 2);

    /* With no room, it is left on the ground. */
    inventory_add(&g.inventory, ITEM_DIRT, 99 * 12);
    int before = count_item(&g, ITEM_DIRT);
    g.drops[0] = (Drop){ true, (uint16_t)ITEM_DIRT, g.player.feet.x, (float)g.player.feet.y, 0.0f, 0.0f, false, 0, 2, 0, false };
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
    memset(g->inventory.choice, -1, sizeof g->inventory.choice);
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
    /* One base block, set in the ground, and two portal blocks on it. */
    CHECK(world_get(&g.world, px, s - 3) == TILE_AIR);
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

    /* Walking away leaves its menu up - it can be read from anywhere - but
     * the player is no longer at it. */
    CHECK(game_click_tile(&g, px + 5, s - 1));
    run(&g, 2.0f);
    CHECK(g.bench_open && !game_at_bench(&g));
    game_close_bench(&g);

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
    CHECK(g.player.feet.x >= x + 2 && !game_at_bench(&g));
    game_close_bench(&g);
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
    const Recipe *list[32];
    CHECK(bench_recipes(tool1, list, 32) == 18);
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
    /* Tools stack. */
    int pick_slots = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (inv->slots[i].id == item_id("wooden_pickaxe") && inv->slots[i].count == 2) pick_slots++;
    CHECK(pick_slots == 1);

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
    /* (A golden chest lies buried at the bottom of the sand, here and there.) */
    return t == TILE_DIRT || t == tile_id("sand") || t == tile_id("golden_chest") || vein_in(t, TILE_DIRT);
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
        for (int i = 0; i < 2; i++)
            CHECK(world_get(&w, w.spawn_x, w.spawn_y - i) == TILE_SPAWN_PORTAL);
        CHECK(world_get(&w, w.spawn_x, w.spawn_y - 2) == TILE_AIR);

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
            if (level > 45 && dirt != 0) bad++;
            if (world_get(&w, x, top) == TILE_DIRT) bad++;

            /* Trees stand on grass: a trunk has trunk or grass under it. */
            for (int r = 0; r < top; r++) {
                TileType t = world_get_at(&w, BG, x, r);
                if (is_tree(world_get(&w, x, r))) bad++; /* trees keep to the background */
                if (tile_def(t)->log_of < 0) continue;
                TileType ground = world_get(&w, x, r + 1);
                if (world_get_at(&w, BG, x, r + 1) != t && ground != TILE_GRASS && ground != tile_id("sand")) bad++;
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
        g.drops[i] = (Drop){ true, (uint16_t)ITEM_ACORN, at.x, (float)at.y, 0.0f, 0.0f, true, FRUIT_TICKS,
                             ++g.next_drop_id, 0, false };
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

    /* The menu's preview reads just the spawn area out of the file: every
     * layer, with what has been built and dug there. */
    WorldPreview pv;
    world_set(&g.world, g.world.spawn_x + 2, g.world.spawn_y, TILE_STONE);
    world_set_at(&g.world, BG, g.world.spawn_x - 3, g.world.spawn_y, tile_id("torch"));
    world_set(&g.world, g.world.spawn_x + 6, g.world.spawn_y + 2, TILE_AIR);
    CHECK(save_game(&g, path) && save_read_preview(path, &pv) && pv.tick == g.tick);
    int wrong = 0;
    for (int dy = 0; dy < PREVIEW_SIZE; dy++) {
        for (int dx = 0; dx < PREVIEW_SIZE; dx++) {
            int wx = g.world.spawn_x - PREVIEW_RADIUS + dx, wy = g.world.spawn_y - 1 - PREVIEW_RADIUS + dy;
            for (int layer = 0; layer < LAYER_COUNT; layer++)
                if (pv.tiles[layer][dy * PREVIEW_SIZE + dx] != world_get_at(&g.world, layer, wx, wy)) wrong++;
        }
    }
    CHECK(wrong == 0);
    CHECK(pv.tiles[0][(PREVIEW_RADIUS + 1) * PREVIEW_SIZE + PREVIEW_RADIUS + 2] == TILE_STONE);
    CHECK(pv.tiles[BG][(PREVIEW_RADIUS + 1) * PREVIEW_SIZE + PREVIEW_RADIUS - 3] == tile_id("torch"));
    CHECK(pv.tiles[LAYER_WALL][(PREVIEW_RADIUS + 3) * PREVIEW_SIZE + PREVIEW_RADIUS + 6] == TILE_DIRT);
    /* A spawn beside the seam shows the far side of the world next to it. */
    int old_x = g.world.spawn_x;
    g.world.spawn_x = 2;
    world_set(&g.world, 62, g.world.spawn_y, TILE_STONE);
    CHECK(save_game(&g, path) && save_read_preview(path, &pv));
    CHECK(pv.tiles[0][(PREVIEW_RADIUS + 1) * PREVIEW_SIZE + PREVIEW_RADIUS - 4] == TILE_STONE);
    g.world.spawn_x = old_x;
    CHECK(!save_read_preview("build/no_such_file.world", &pv) && !save_read_preview("Makefile", &pv));

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
        /* (Nor what is never held: the time crystal, which turns into crystals, and a new player.) */
        bool coin = i == copper || i == gold || i == plat || content.items[i].crystals > 0 || content.items[i].becomes_player;
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
        CHECK(p->tool_speed > last && a->tool_speed == p->tool_speed && p->max_stack > 1);
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
    for (int i = 0; i < INVENTORY_SLOTS; i++) inv->slots[i] = (ItemStack){ (uint16_t)ITEM_BASKET, 1, 0 };
    world_set_at(w, BG, x + 1, s - 1, tile_id("time_crystal"));
    CHECK(game_click_tile(&g, x + 1, s - 1));
    run(&g, 4.0f);
    CHECK(g.crystals == 45);

    /* The portal chest: 1 golden chest and 100 crystals at a tier 6 portal. */
    const Recipe *pc = find_recipe(portal, "portal_chest");
    CHECK(costs(pc, "golden_chest", 1, NULL, 0) && pc->crystals == 100 && pc->level == 6);
    CHECK(place_and_open(&g, "portal", x + 3));
    inventory_add(inv, item_id("golden_chest"), 1);
    g.crystals = 150;
    CHECK(!game_craft(&g, pc, 1));
    for (int i = 0; i < 5; i++) {
        inventory_add(inv, item_id(gems[i]), 1);
        CHECK(game_upgrade_bench(&g));
    }
    CHECK(world_get_at(w, BG, x + 3, s - 1) == tile_id("portal_6") && !game_upgrade_bench(&g));
    CHECK(game_craft(&g, pc, 1));
    run(&g, 0.3f);
    CHECK(count_item(&g, item_id("portal_chest")) == 1 && g.crystals == 50 && count_item(&g, item_id("golden_chest")) == 0);
    game_free(&g);
}

/* Which part of a column a tile is: 0 body, 1 top, 2 base. (The same rule
 * the drawing code uses, which cannot be linked into a headless test.) */
static int assets_part(TileType t, TileType above, TileType below)
{
    if (above != t) return 1;
    return below != t ? 2 : 0;
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

    /* A made portal is three blocks: a base and two of portal. The spawn
     * portal is two of portal on a base block of its own, set in the ground. */
    CHECK(tile_def(portal)->height == 3 && tile_def(trade)->height == 3 && tile_def(TILE_SPAWN_PORTAL)->height == 2);
    CHECK(tile_def(portal)->texture_base[0] && tile_def(trade)->texture_base[0] && tile_def(portal)->texture_top[0]);
    CHECK(assets_part(portal, portal, TILE_AIR) == 2 && assets_part(portal, portal, portal) == 0 &&
          assets_part(portal, TILE_AIR, portal) == 1);
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
    CHECK(game_queue_action(&g, ACTION_USE, px, s - 2));
    run(&g, 1.0f);
    CHECK(g.bench_open);
    for (int i = 0; i < 5; i++) {
        const TileDef *def = tile_def(tier);
        CHECK(def->bench_level == i + 1 && !def->removable && def->upgrade_cost[0].id == item_id(gems[i]));
        CHECK(!game_upgrade_bench(&g));
        inventory_add(inv, item_id(gems[i]), 1);
        CHECK(game_upgrade_bench(&g));
        tier = def->upgrade_to;
        for (int k = 0; k < 2; k++) CHECK(world_get(w, px, s - 1 - k) == tier);
        CHECK(world_get(w, px, s - 3) == TILE_AIR);
    }
    CHECK(tile_def(tier)->bench_level == 6 && tile_def(tier)->upgrade_to == TILE_AIR && !tile_def(tier)->removable);
    CHECK(world_get(w, px, s) == TILE_PORTAL_BASE && g.bench_open);
    /* It still cannot be taken down or built over. */
    CHECK(!game_queue_action(&g, ACTION_MINE, px, s - 2));

    /* An old save's spawn portal top, a block that no longer exists by that
     * name, loads as part of the portal. */
    CHECK(tile_id("spawn_portal_top") == -1 && tile_id_or_alias("spawn_portal_top") == TILE_SPAWN_PORTAL);
    CHECK(tile_id_or_alias("no_such_block") == -1);

    /* A world saved when the spawn portal stood three blocks tall is trimmed
     * to two when it is loaded. */
    world_set(w, px, s - 3, tier);
    CHECK(save_game(&g, "build/test_tall.world"));
    Game old;
    CHECK(load_game(&old, "build/test_tall.world"));
    CHECK(world_get(&old.world, px, s - 3) == TILE_AIR && world_get(&old.world, px, s - 2) == tier);
    CHECK(world_get(&old.world, px, s - 1) == tier && world_get(&old.world, px, s) == TILE_PORTAL_BASE);
    game_free(&old);
    remove("build/test_tall.world");
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

/* Climate, the sea, and what grows where. */
static void test_biomes(void)
{
    TileType sand = tile_id("sand"), kelp = tile_id("kelp"), cactus = tile_id("cactus");
    int deserts = 0, cacti = 0, kelps = 0, corals = 0, carrots = 0, flax = 0, apples = 0, oranges = 0;
    int shore_sand = 0, shore_all = 0, inland_sand_runs = 0, stripes = 0, thin_edges = 0, loose_arms = 0, arms = 0;
    TileType arm = tile_id("cactus_arm");

    CHECK(content.biome_count == 7 && content.worldgen.has_sea && content.worldgen.sea_level == 0);
    CHECK(!content.biomes[0].has_climate && content.biomes[1].has_climate && content.biomes[6].below_sea);
    int seen[MAX_TREES] = { 0 }, strays = 0, biomes_seen[MAX_BIOMES] = { 0 };

    for (uint32_t seed = 1; seed <= 10; seed++) {
        World w;
        CHECK(world_init(&w, 2048, 401, seed, WORLD_TERRAIN));
        CHECK(world_row_to_y(&w, w.sea_row) == 0);
        int bad = 0;
        for (int x = 0; x < w.width; x++) {
            int level = column_level(&w, x), top = world_y_to_row(&w, level - 1), feet = top - 1;
            TileType surface = world_get(&w, x, top);
            if (level < 0) {
                /* Below Y 0 is ocean: a sand bed, never grass, under water up to sea level. */
                if (surface != sand && !vein_in(surface, TILE_STONE) && surface != TILE_STONE) bad++;
                if (surface == TILE_GRASS || surface == TILE_DIRT) bad++;
                if (surface == sand && world_get(&w, x, top + 1) != sand) bad++;
                for (int y = w.sea_row + 1; y <= feet; y++)
                    if (!world_is_water(&w, x, y)) bad++;
                if (world_is_water(&w, x, w.sea_row)) bad++;
            } else {
                for (int y = 0; y <= feet; y++)
                    if (world_is_water(&w, x, y)) bad++;
                /* The shore: land beside the sea is mostly sand, and less so further in. */
                bool by_sea = column_level(&w, x - 1) < 0 || column_level(&w, x + 1) < 0;
                bool far = true;
                for (int d = 1; d <= 30; d++)
                    if (column_level(&w, x - d) < 0 || column_level(&w, x + d) < 0) far = false;
                if (by_sea) shore_all++, shore_sand += surface == sand;
                if (far && surface == sand) deserts++;
                TileType left = world_get(&w, x - 1, world_y_to_row(&w, column_level(&w, x - 1) - 1));
                TileType right = world_get(&w, x + 1, world_y_to_row(&w, column_level(&w, x + 1) - 1));
                if (far && surface == sand && left != sand) {
                    inland_sand_runs++;
                    /* Where a desert begins its sand is a skin over the dirt. */
                    if (world_get(&w, x, top + 1) != sand) thin_edges++;
                }
                /* Sand and grass do not alternate column by column. */
                if (far && ((surface == sand && left == TILE_GRASS && right == TILE_GRASS) ||
                            (surface == TILE_GRASS && left == sand && right == sand)))
                    stripes++;
            }
            /* What grows: each thing in its own country, on its own ground. */
            TileType growing = world_get_at(&w, BG, x, feet);
            /* Every tree stands in a biome that grows its kind, so like stands with like. */
            const BiomeDef *here = &content.biomes[world_biome(&w, x)];
            biomes_seen[world_biome(&w, x)]++;
            int tree = tree_trunk_at(&w, x, feet);
            if (tree >= 0) {
                bool belongs = false;
                for (int k = 0; k < here->tree_count; k++) belongs = belongs || here->trees[k].tree == tree;
                seen[tree]++;
                if (!belongs) strays++;
            }
            if ((level < 0) != here->below_sea) bad++;
            const char *name = tile_def(growing)->name;
            if (growing == kelp) {
                kelps++;
                if (level >= 0 || surface != sand) bad++;
                /* Kelp is one straight stem, all of it under water. */
                int h = 0;
                while (world_get_at(&w, BG, x, feet - h) == kelp) h++;
                if (h > content.trees[tree_id("kelp")].max_height || !world_is_water(&w, x, feet - h + 1)) bad++;
                if (world_get_at(&w, BG, x - 1, feet) == kelp && world_get_at(&w, BG, x + 1, feet) == kelp) bad++;
            }
            if (growing == cactus) {
                cacti++;
                if (surface != sand || level < 0) bad++;
            }
            if (strncmp(name, "coral_", 6) == 0) corals++, bad += !world_is_water(&w, x, feet) || surface != sand;
            if (strncmp(name, "carrot_", 7) == 0) carrots++, bad += surface != TILE_GRASS;
            if (strncmp(name, "flax_", 5) == 0) flax++, bad += surface != TILE_GRASS;
            if (growing == tile_id("apple_log")) apples++, bad += surface != TILE_GRASS;
            if (growing == tile_id("orange_log")) oranges++, bad += surface != TILE_GRASS;
        }
        CHECK(bad == 0);
        /* Every arm of a cactus is joined on to the rest of it: none hangs in the air. */
        for (int y = 1; y < w.height - 1; y++) {
            for (int x = 0; x < w.width; x++) {
                if (world_get_at(&w, BG, x, y) != arm) continue;
                arms++;
                TileType round[4] = { world_get_at(&w, BG, x - 1, y), world_get_at(&w, BG, x + 1, y),
                                      world_get_at(&w, BG, x, y - 1), world_get_at(&w, BG, x, y + 1) };
                bool held = false;
                for (int k = 0; k < 4; k++) held = held || round[k] == arm || round[k] == cactus;
                if (!held) loose_arms++;
            }
        }
        /* The spawn stands on ordinary ground, with dirt to hand. */
        for (int d = -20; d <= 20; d++) {
            int top = world_y_to_row(&w, column_level(&w, w.spawn_x + d) - 1);
            CHECK(world_get(&w, w.spawn_x + d, top) == (d == 0 ? TILE_PORTAL_BASE : TILE_GRASS));
        }
        world_free(&w);
    }
    CHECK(kelps > 20 && corals > 20 && carrots > 5 && flax > 5 && apples > 0 && oranges > 0);
    CHECK(strays == 0);
    for (int k = 0; k < content.tree_count; k++) CHECK(seen[k] > 0);
    for (int k = 0; k < content.biome_count; k++) CHECK(biomes_seen[k] > 0);
    CHECK(strcmp(content.biomes[0].label, "Plains") == 0 && tree_id("coffee") >= 0);
    /* Deserts turn up, with cacti in them, but most worlds are not all desert. */
    CHECK(deserts > 100 && deserts < 10 * 2048 / 2 && cacti > 3);
    /* Sand does not start at a line, nor in stripes: it thins out over the dirt. */
    CHECK(inland_sand_runs > 0 && stripes == 0 && thin_edges * 10 >= inland_sand_runs * 8);
    CHECK(arms > 5 && loose_arms == 0);

    /* A cactus grown from nothing keeps its arms attached at every step of
     * the way, and ends up with all six blocks of them. */
    Game g;
    game_init(&g, 64, 48, 77, WORLD_FLAT);
    World *fw = &g.world;
    int cx = 20, ground = fw->surface_y;
    world_set(fw, cx, ground, sand);
    tree_plant(fw, cx, ground - 1, tree_id("cactus"));
    uint32_t rng = 5;
    int grown = 0;
    for (int step = 0; step < 400; step++) {
        tree_grow(fw, cx, ground - 1, &rng);
        grown = 0;
        for (int y = ground - 8; y < ground; y++) {
            for (int x = cx - 3; x <= cx + 3; x++) {
                if (world_get_at(fw, BG, x, y) != arm) continue;
                grown++;
                TileType round[4] = { world_get_at(fw, BG, x - 1, y), world_get_at(fw, BG, x + 1, y),
                                      world_get_at(fw, BG, x, y - 1), world_get_at(fw, BG, x, y + 1) };
                bool held = false;
                for (int k = 0; k < 4; k++) held = held || round[k] == arm || round[k] == cactus;
                CHECK(held);
            }
        }
    }
    CHECK(grown == 6 && world_get_at(fw, BG, cx, ground - 5) == cactus && world_get_at(fw, BG, cx, ground - 6) != cactus);
    game_free(&g);
    CHECK(shore_all > 10 && shore_sand * 10 >= shore_all * 7);
}

static void test_plants(void)
{
    Game g;
    game_init(&g, 64, 48, 36, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    TilePos start = g.player.feet;
    int s = w->surface_y, x = start.x + 2;
    TileType sand = tile_id("sand"), kelp = tile_id("kelp");

    /* A plant is a chain of blocks, each stage giving more than the last. */
    static const char *kinds[] = { "carrot", "flax", "coral" };
    for (int k = 0; k < 3; k++) {
        int last = 0;
        for (int i = 0; i < 4; i++) {
            const TileDef *def = tile_def(tile_id(TextName("%s_", kinds[k]) ) );
            (void)def;
            char name[32];
            snprintf(name, sizeof name, "%s_%d", kinds[k], i);
            const TileDef *stage = tile_def(tile_id(name));
            CHECK(tile_id(name) > 0 && stage->removable && !stage->solid && stage->layer == LAYER_BACKGROUND);
            snprintf(name, sizeof name, "%s_%d", kinds[k], i + 1);
            CHECK(stage->grows_into == (i < 3 ? tile_id(name) : TILE_AIR));
            int total = stage->drop_count;
            for (int e = 0; e < stage->extra_drop_count; e++) total += stage->extra_drops[e].count;
            CHECK(total >= last && total >= 1);
            last = total;
        }
        CHECK(last >= 3);
    }
    CHECK(tile_def(tile_id("flax_3"))->drop == item_id("flax_seeds") &&
          tile_def(tile_id("flax_3"))->extra_drops[0].id == item_id("flax"));
    CHECK(tile_def(tile_id("coral_0"))->needs_water && !tile_def(tile_id("carrot_0"))->needs_water);

    /* A carrot is sown on dirt, and grows a stage at a time by random events. */
    clear_inventory(&g);
    inventory_add(inv, item_id("carrot"), 2);
    inventory_add(inv, item_id("coral"), 1);
    inventory_select(inv, 0, -1);
    CHECK(game_click_tile(&g, x, s) && g.queue[0].type == ACTION_PLANT);
    run(&g, 2.0f);
    CHECK(world_get_at(w, BG, x, s - 1) == tile_id("carrot_0") && count_item(&g, item_id("carrot")) == 1);
    /* Not on stone, and coral not on dry land. */
    world_set(w, x + 2, s, TILE_STONE);
    CHECK(game_click_tile(&g, x + 2, s) && g.queue[0].type == ACTION_MINE);
    game_cancel_actions(&g);
    world_set(w, x + 3, s, sand);
    inventory_select(inv, 1, -1);
    CHECK(game_click_tile(&g, x + 3, s) && g.queue[0].type == ACTION_MINE);
    game_cancel_actions(&g);
    for (int i = 0; i < 300000 && world_get_at(w, BG, x, s - 1) != tile_id("carrot_3"); i++) game_tick(&g);
    CHECK(world_get_at(w, BG, x, s - 1) == tile_id("carrot_3"));
    for (int i = 0; i < 5000; i++) game_tick(&g);
    CHECK(world_get_at(w, BG, x, s - 1) == tile_id("carrot_3")); /* full grown stays so */
    /* Pulled up ripe it gives four; pulled up at once, only what was sown. */
    clear_inventory(&g);
    CHECK(game_click_tile(&g, x, s - 1) && g.queue[0].type == ACTION_MINE);
    run(&g, 3.0f);
    CHECK(world_get_at(w, BG, x, s - 1) == TILE_AIR && count_item(&g, item_id("carrot")) == 4);
    inventory_select(inv, 0, -1);
    CHECK(game_click_tile(&g, x, s));
    run(&g, 2.0f);
    CHECK(count_item(&g, item_id("carrot")) == 3);
    CHECK(game_click_tile(&g, x, s - 1));
    run(&g, 3.0f);
    CHECK(count_item(&g, item_id("carrot")) == 4);
    /* Ripe flax gives seeds and flax both. */
    clear_inventory(&g);
    world_set_at(w, BG, x, s - 1, tile_id("flax_3"));
    CHECK(game_click_tile(&g, x, s - 1));
    run(&g, 3.0f);
    CHECK(count_item(&g, item_id("flax_seeds")) == 2 && count_item(&g, item_id("flax")) == 3);

    /* A little sea: coral is planted on sand under water, and only grows there. */
    int ox = start.x - 8;
    for (int y = s - 5; y <= s - 1; y++) {
        world_set(w, ox - 3, y, TILE_STONE); /* a tank, five deep, with sand on the bottom */
        world_set(w, ox + 3, y, TILE_STONE);
    }
    for (int cx = ox - 2; cx <= ox + 2; cx++) {
        for (int y = s - 5; y <= s - 1; y++) world_set_water(w, cx, y, WATER_FULL);
        world_set(w, cx, s, sand);
    }
    long tank = water_total(w);
    for (int i = 0; i < 50; i++) water_step(w);
    CHECK(world_is_water(w, ox, s - 1) && world_is_water(w, ox, s - 5) && !world_is_water(w, ox, s - 6));
    /* It stays in the tank, all of it; and there is none in the ground or out in the open. */
    CHECK(water_total(w) == tank && tank == 5 * 5 * WATER_FULL);
    CHECK(!world_is_water(w, ox, s) && !world_is_water(w, x + 3, s - 1) && !world_is_water(w, ox - 4, s - 1));
    clear_inventory(&g);
    inventory_add(inv, item_id("coral"), 1);
    inventory_select(inv, 0, -1);
    CHECK(game_click_tile(&g, ox, s) && g.queue[0].type == ACTION_PLANT);
    run(&g, 4.0f);
    CHECK(world_get_at(w, BG, ox, s - 1) == tile_id("coral_0") && count_item(&g, item_id("coral")) == 0);
    world_set_at(w, BG, x + 3, s - 1, tile_id("coral_0")); /* one left high and dry */
    for (int i = 0; i < 300000 && world_get_at(w, BG, ox, s - 1) != tile_id("coral_3"); i++) game_tick(&g);
    CHECK(world_get_at(w, BG, ox, s - 1) == tile_id("coral_3") && world_get_at(w, BG, x + 3, s - 1) == tile_id("coral_0"));
    /* Water dims the light with depth. */
    g.tick = (uint64_t)DAY_TICKS / 2;
    CHECK(light_at(&g, ox, s - 5) <= content.rules.light_sun && light_at(&g, ox, s - 1) < light_at(&g, ox, s - 5));

    /* Kelp grows straight up, to the surface and no further. */
    int kx = ox + 2, kind = tree_id("kelp");
    uint32_t rng = 9;
    CHECK(content.trees[kind].needs_water && content.trees[kind].leaves == TILE_AIR && tile_def(kelp)->climbable);
    tree_plant(w, kx, s - 1, kind);
    while (tree_grow(w, kx, s - 1, &rng).grew) {}
    for (int y = s - 5; y <= s - 1; y++) CHECK(world_get_at(w, BG, kx, y) == kelp);
    CHECK(world_get_at(w, BG, kx, s - 6) == TILE_AIR);
    CHECK(world_get_at(w, BG, kx - 1, s - 3) == TILE_AIR && world_get_at(w, BG, kx + 1, s - 3) == TILE_AIR);
    /* (The player has been standing under water all this while, and would
     * not have lived. They are put right, and the clock stopped, for the rest.) */
    g.vitals = (Vitals){ .health = 1.0f, .hunger = 1.0f, .energy = 1.0f, .oxygen = 1.0f, .death = 1.0f };
    g.tick_rate = 0;
    /* It can be climbed. */
    g.player.feet = g.player.step_to = (TilePos){ kx, s - 1 };
    game_set_move_input(&g, 0, -1);
    run(&g, CLIMB_TIME * 2.5f);
    game_set_move_input(&g, 0, 0);
    run(&g, 0.5f);
    CHECK(g.player.climbing && g.player.feet.y < s - 1);
    game_set_move_input(&g, 0, 1);
    run(&g, 2.0f);
    game_set_move_input(&g, 0, 0);
    /* Cut part-way up, everything above the cut goes too; what is below stays. */
    clear_inventory(&g);
    g.player.feet = g.player.step_to = (TilePos){ kx - 1, s - 1 };
    CHECK(game_queue_action(&g, ACTION_MINE, kx, s - 3));
    run(&g, 4.0f);
    CHECK(world_get_at(w, BG, kx, s - 1) == kelp && world_get_at(w, BG, kx, s - 2) == kelp);
    for (int y = s - 5; y <= s - 3; y++) CHECK(world_get_at(w, BG, kx, y) == TILE_AIR);
    int got = count_item(&g, item_id("kelp"));
    for (int i = 0; i < MAX_DROPS; i++) got += g.drops[i].active && g.drops[i].item == item_id("kelp");
    CHECK(got == 3);
    game_free(&g);
}

static void test_new_trees(void)
{
    Game g;
    game_init(&g, 64, 48, 37, WORLD_FLAT);
    World *w = &g.world;
    int s = w->surface_y, base = s - 1;
    uint32_t rng = 4;

    /* Apple and orange trees: their own blocks, their own fruit, planted from it. */
    static const char *fruit[] = { "apple", "orange" };
    for (int k = 0; k < 2; k++) {
        int kind = tree_id(fruit[k]), x = 10 + k * 12;
        const TreeDef *def = &content.trees[kind];
        CHECK(kind >= 0 && def->fruit == item_id(fruit[k]) && item_def(def->fruit)->plants == kind);
        CHECK(def->log != TILE_OAK_LOG && def->leaves != TILE_OAK_LEAVES && tile_def(def->log)->drop == ITEM_WOOD);
        tree_plant(w, x, base, kind);
        while (tree_grow(w, x, base, &rng).grew) {}
        CHECK(trunk_height(w, x, base) == def->max_height);
        int leaves = 0;
        for (int y = 0; y < s; y++)
            for (int dx = -3; dx <= 3; dx++) leaves += world_get_at(w, BG, x + dx, y) == def->leaves;
        CHECK(leaves >= 8);
    }
    /* Fruit turns up on them in time. */
    for (int i = 0; i < 60000; i++) game_tick(&g);
    int apples = 0, oranges = 0;
    for (int i = 0; i < MAX_DROPS; i++) {
        apples += g.drops[i].active && g.drops[i].item == item_id("apple");
        oranges += g.drops[i].active && g.drops[i].item == item_id("orange");
    }
    CHECK(apples > 0 && oranges > 0);

    /* A cactus: a trunk with arms that branch off and turn up - no leaves
     * round it - on sand, bearing prickly pears. */
    int kind = tree_id("cactus"), cx = 40;
    const TreeDef *def = &content.trees[kind];
    world_set(w, cx, s, tile_id("sand"));
    CHECK(def->fruit == item_id("prickly_pear") && item_def(def->fruit)->plants == kind && def->soil[0] == tile_id("sand"));
    tree_plant(w, cx, base, kind);
    while (tree_grow(w, cx, base, &rng).grew) {}
    CHECK(trunk_height(w, cx, base) == def->max_height);
    int arms = 0, beside_trunk = 0, far = 0;
    for (int y = 0; y < s; y++) {
        for (int dx = -3; dx <= 3; dx++) {
            if (world_get_at(w, BG, cx + dx, y) != def->leaves) continue;
            arms++;
            if (dx == -1 || dx == 1) beside_trunk++;
            if (dx == -2 || dx == 2) far++;
        }
    }
    /* Each arm leaves the trunk by one block and then stands off it: far
     * fewer blocks than a crown of leaves would be. */
    CHECK(arms == 6 && beside_trunk == 2 && far == 4);
    CHECK(world_get_at(w, BG, cx, base - def->max_height) == TILE_AIR);
    clear_inventory(&g);
    inventory_add(&g.inventory, item_id("prickly_pear"), 1);
    inventory_select(&g.inventory, 0, -1);
    world_set(w, cx + 6, s, tile_id("sand"));
    g.player.feet = g.player.step_to = (TilePos){ cx + 5, s - 1 };
    CHECK(game_click_tile(&g, cx + 6, s) && g.queue[0].type == ACTION_PLANT);
    run(&g, 2.0f);
    CHECK(world_get_at(w, BG, cx + 6, s - 1) == tile_id("cactus"));
    /* A prickly pear will not take on dirt. */
    inventory_add(&g.inventory, item_id("prickly_pear"), 1);
    inventory_select(&g.inventory, 0, -1);
    CHECK(game_click_tile(&g, cx + 4, s) && g.queue[0].type == ACTION_MINE);
    game_free(&g);
}

static int count_mobs(const Game *g)
{
    int n = 0;
    for (int i = 0; i < MAX_MOBS; i++) n += g->mobs[i].active;
    return n;
}

static void clear_mobs(Game *g)
{
    for (int i = 0; i < MAX_MOBS; i++) g->mobs[i].active = false;
}

static void test_mobs(void)
{
    Game g;
    game_init(&g, 256, 48, 38, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    TilePos start = g.player.feet;
    int s = w->surface_y, dodo = mob_id("dodo");
    MobDef *def = &content.mobs[dodo];
    MobDef saved = *def;
    TileType egg = tile_id("dodo_egg"), placed = tile_id("dodo_egg_placed");

    CHECK(dodo >= 0 && content.mob_spawn_radius == 50 && content.mob_cap == 10);
    CHECK(def->lays == egg && def->baby_ticks == 600 && def->drop_count == 2);
    CHECK(def->drops[0].item == item_id("dodo_meat") && def->drops[0].min == 1 && def->drops[0].max == 2);
    CHECK(def->drops[1].item == item_id("feather") && def->drops[1].min == 1 && def->drops[1].max == 2);
    CHECK(tile_def(egg)->hatches == dodo && !tile_def(egg)->hatch_owned && tile_def(placed)->hatch_owned);
    CHECK(item_def(item_id("dodo_egg"))->places == placed && tile_def(egg)->drop == item_id("dodo_egg"));
    CHECK(costs(find_recipe(tile_id("campfire"), "cooked_dodo_meat"), "dodo_meat", 1, NULL, 0));

    /* A dodo turns up on open air just above grass, near the player - not
     * above bare dirt, not far away, not in the ground. */
    for (int x = 0; x < w->width; x++) world_set(w, x, s, TILE_GRASS);
    world_set(w, start.x + 5, s, TILE_DIRT);
    def->spawn_chance = 1.0f;
    CHECK(!mobs_try_spawn(&g, start.x + 5, s - 1));
    CHECK(!mobs_try_spawn(&g, start.x + 3, s - 2) && !mobs_try_spawn(&g, start.x + 3, s));
    CHECK(!mobs_try_spawn(&g, start.x + 51, s - 1) && !mobs_try_spawn(&g, start.x - 60, s - 1));
    CHECK(mobs_try_spawn(&g, start.x + 3, s - 1) && count_mobs(&g) == 1);
    CHECK(mobs_try_spawn(&g, start.x + 50, s - 1) && count_mobs(&g) == 2);
    for (int i = 0; i < MAX_MOBS; i++)
        if (g.mobs[i].active) CHECK(!g.mobs[i].owned && !g.mobs[i].baby && g.mobs[i].kind == dodo);
    /* No more than ten near the player. */
    for (int i = 0; i < 20; i++) mobs_try_spawn(&g, start.x - 20 + i, s - 1);
    CHECK(count_mobs(&g) == 10 && mobs_near_player(&g) == 10);
    CHECK(!mobs_try_spawn(&g, start.x + 9, s - 1));
    /* Left to random events alone they also come, and stop at the cap. */
    clear_mobs(&g);
    for (int i = 0; i < 6000; i++) game_tick(&g);
    CHECK(count_mobs(&g) > 0 && mobs_near_player(&g) <= 10);
    /* (They will have left eggs about; clear those away for what follows.) */
    int laid = 0;
    for (int x = 0; x < w->width; x++) {
        laid += world_get_at(w, BG, x, s - 1) == egg;
        world_set_at(w, BG, x, s - 1, TILE_AIR);
    }
    CHECK(laid > 0);

    /* They wander: along the level, up and down a step of one block, never
     * across a gap or off a drop, and never into the ground. */
    clear_mobs(&g);
    def->spawn_chance = 0.0f;
    def->lay_chance = 0.0f;
    int px = start.x + 20;
    for (int x = px - 6; x <= px + 6; x++) world_set(w, x, s, TILE_GRASS);
    world_set(w, px + 2, s - 1, TILE_GRASS);                                   /* a step up */
    world_set(w, px + 3, s - 1, TILE_GRASS);
    world_set(w, px - 3, s, TILE_AIR);                                         /* a step down */
    for (int y = s; y < s + 6; y++) world_set(w, px - 5, y, TILE_AIR);         /* a pit one wide */
    for (int y = s - 3; y <= s - 1; y++) world_set(w, px + 6, y, TILE_STONE);  /* a wall */
    int m = mob_spawn(&g, dodo, px, s - 1, true, false);
    int lo = px, hi = px, top = s - 1, low = s - 1, in_ground = 0, jumps = 0;
    for (int i = 0; i < 40000; i++) {
        int before_x = g.mobs[m].x, before_y = g.mobs[m].y;
        mobs_tick(&g);
        const Mob *d = &g.mobs[m];
        if (d->x < lo) lo = d->x;
        if (d->x > hi) hi = d->x;
        if (d->y < top) top = d->y;
        if (d->y > low) low = d->y;
        if (world_is_solid(w, d->x, d->y) || !world_is_solid(w, d->x, d->y + 1)) in_ground++;
        if (abs(d->x - before_x) > 1 || abs(d->y - before_y) > 1) jumps++;
    }
    CHECK(g.mobs[m].active && in_ground == 0 && jumps == 0);
    CHECK(top == s - 2 && low == s);       /* it has been up the step and down the other */
    CHECK(lo == px - 4 && hi == px + 5);   /* stopped by the pit one way, the wall the other */

    /* A wild one too far from the player is gone; the player's own stays. */
    clear_mobs(&g);
    int wild = mob_spawn(&g, dodo, start.x + 60, s - 1, false, false);
    int pet = mob_spawn(&g, dodo, start.x + 62, s - 1, true, false);
    def->move_chance = 0.0f;
    mobs_tick(&g);
    CHECK(!g.mobs[wild].active && g.mobs[pet].active);

    /* A grown dodo lays an egg where it stands, unless something is there already. */
    clear_mobs(&g);
    def->lay_chance = 1.0f;
    float bred_on = def->lay_next_chance;
    def->lay_next_chance = 0.0f; /* its own egg, for now: breeding on has a test of its own */
    int layer = mob_spawn(&g, dodo, start.x + 2, s - 1, false, false);
    int chick = mob_spawn(&g, dodo, start.x + 4, s - 1, false, true);
    int crowded = mob_spawn(&g, dodo, start.x + 6, s - 1, false, false);
    world_set_at(w, BG, start.x + 6, s - 1, tile_id("torch"));
    mobs_tick(&g);
    CHECK(world_get_at(w, BG, start.x + 2, s - 1) == egg);
    CHECK(world_get_at(w, BG, start.x + 4, s - 1) == TILE_AIR); /* too young */
    CHECK(world_get_at(w, BG, start.x + 6, s - 1) == tile_id("torch"));
    (void)layer; (void)chick; (void)crowded;
    def->lay_chance = 0.0f;
    def->lay_next_chance = bred_on;
    clear_mobs(&g);

    /* A natural egg hatches into a wild chick, which grows up in 600 ticks. */
    /* (Not while there are already ten about: then it waits.) */
    for (int i = 0; i < 10; i++) mob_spawn(&g, dodo, start.x - 10 + i, s - 1, false, false);
    CHECK(!mobs_try_hatch(&g, start.x + 2, s - 1) && world_get_at(w, BG, start.x + 2, s - 1) == egg);
    clear_mobs(&g);
    CHECK(mobs_try_hatch(&g, start.x + 2, s - 1) && world_get_at(w, BG, start.x + 2, s - 1) == TILE_AIR);
    CHECK(count_mobs(&g) == 1);
    int born = -1;
    for (int i = 0; i < MAX_MOBS; i++)
        if (g.mobs[i].active) born = i;
    CHECK(g.mobs[born].baby && !g.mobs[born].owned && g.mobs[born].x == start.x + 2);
    for (int i = 0; i < 599; i++) mobs_tick(&g);
    CHECK(g.mobs[born].baby);
    mobs_tick(&g);
    CHECK(!g.mobs[born].baby);
    CHECK(!mobs_try_hatch(&g, start.x + 2, s - 1));
    clear_mobs(&g);

    /* Picked up and put down again, an egg is the player's, and so is what hatches. */
    world_set_at(w, BG, start.x + 2, s - 1, egg);
    clear_inventory(&g);
    CHECK(game_click_tile(&g, start.x + 2, s - 1) && g.queue[0].type == ACTION_MINE);
    run(&g, 2.0f);
    CHECK(world_get_at(w, BG, start.x + 2, s - 1) == TILE_AIR && count_item(&g, item_id("dodo_egg")) == 1);
    inventory_select(inv, 0, -1);
    CHECK(game_click_tile(&g, start.x + 3, s - 1) && g.queue[0].type == ACTION_PLACE);
    run(&g, 2.0f);
    CHECK(world_get_at(w, BG, start.x + 3, s - 1) == placed && count_item(&g, item_id("dodo_egg")) == 0);
    /* The player's egg hatches even with ten wild ones about. */
    for (int i = 0; i < 10; i++) mob_spawn(&g, dodo, start.x - 10 + i, s - 1, false, false);
    CHECK(mobs_try_hatch(&g, start.x + 3, s - 1));
    born = -1;
    for (int i = 0; i < MAX_MOBS; i++) {
        if (g.mobs[i].active && g.mobs[i].owned) born = i;
        else g.mobs[i].active = false;
    }
    CHECK(g.mobs[born].baby && g.mobs[born].owned);
    /* Its own eggs, though, are natural ones. */
    for (int i = 0; i < 600; i++) mobs_tick(&g);
    def->lay_chance = 1.0f;
    float breeds_on = def->lay_next_chance;
    def->lay_next_chance = 0.0f; /* (its own kind of egg, for this) */
    for (int i = 0; i < 20 && world_get_at(w, BG, g.mobs[born].x, g.mobs[born].y) != egg; i++) mobs_tick(&g);
    def->lay_chance = 0.0f;
    def->lay_next_chance = breeds_on;
    CHECK(!g.mobs[born].baby && world_get_at(w, BG, g.mobs[born].x, g.mobs[born].y) == egg);
    world_set_at(w, BG, g.mobs[born].x, g.mobs[born].y, TILE_AIR);

    /* Clicking a dodo goes after it and hits it, once a click; when it is dead: 1 to 2 meat and 1 to 2 feathers. */
    clear_inventory(&g);
    def->move_chance = saved.move_chance;
    g.mobs[born].x = g.mobs[born].to_x = start.x + 12;
    float mx, my;
    game_mob_pos(&g, born, &mx, &my);
    CHECK(game_mob_at(&g, mx + 0.5f, my + 0.5f) == born && game_mob_at(&g, mx + 3.5f, my + 0.5f) == -1);
    CHECK(game_click_mob(&g, born) && g.queue[0].type == ACTION_ATTACK);
    CHECK(game_click_mob(&g, born) && g.queue_count == 0); /* a second click calls it off */
    for (int hit = 0; hit < 3 && g.mobs[born].active; hit++) {
        CHECK(game_click_mob(&g, born));
        run(&g, 20.0f);
        CHECK(is_idle(&g));
    }
    CHECK(!g.mobs[born].active && is_idle(&g));
    int meat = count_item(&g, item_id("dodo_meat")), feathers = count_item(&g, item_id("feather"));
    for (int i = 0; i < MAX_DROPS; i++) {
        meat += g.drops[i].active && g.drops[i].item == item_id("dodo_meat");
        feathers += g.drops[i].active && g.drops[i].item == item_id("feather");
    }
    CHECK(meat >= 1 && meat <= 2 && feathers >= 1 && feathers <= 2);
    /* A chick gives nothing. */
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    clear_inventory(&g);
    clear_mobs(&g);
    def->move_chance = 0.0f;
    int small = mob_spawn(&g, dodo, g.player.feet.x + 1, s - 1, false, true);
    for (int hit = 0; hit < 3; hit++) {
        if (!g.mobs[small].active) break;
        CHECK(game_click_mob(&g, small));
        run(&g, 3.0f);
    }
    CHECK(!g.mobs[small].active);
    for (int i = 0; i < MAX_DROPS; i++) CHECK(!g.drops[i].active);
    for (int i = 0; i < INVENTORY_SLOTS; i++) CHECK(inv->slots[i].count == 0);

    /* Creatures are saved with the world. */
    int kept = mob_spawn(&g, dodo, g.player.feet.x + 2, s - 1, true, true);
    g.mobs[kept].age = 123;
    CHECK(save_game(&g, "build/test_mobs.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_mobs.world"));
    CHECK(count_mobs(&loaded) == 1 && loaded.mobs[kept].active && loaded.mobs[kept].kind == dodo);
    CHECK(loaded.mobs[kept].owned && loaded.mobs[kept].baby && loaded.mobs[kept].age == 123);
    game_free(&loaded);
    remove("build/test_mobs.world");

    *def = saved;
    game_free(&g);
}

static void test_fog(void)
{
    Game g;
    game_init(&g, 256, 96, 39, WORLD_FLAT);
    World *w = &g.world;
    TilePos start = g.player.feet;
    int s = w->surface_y, radius = content.rules.fog_radius, fade = content.rules.fog_fade;

    CHECK(content.rules.fog_enabled && radius == 20 && fade > 0 && g.fog);

    /* A new world is dark but for what is round the player: plain to see up
     * close, fading out toward 20 blocks, black beyond. */
    CHECK(game_explored(&g, start.x, start.y) == 255 && game_explored(&g, start.x + 5, start.y - 3) == 255);
    CHECK(game_explored(&g, start.x + radius - fade - 1, start.y - 1) == 255);
    int mid = game_explored(&g, start.x + radius - fade / 2, start.y - 1);
    CHECK(mid > 0 && mid < 255);
    CHECK(game_explored(&g, start.x + radius - 1, start.y - 1) < mid);
    CHECK(game_explored(&g, start.x + radius + 1, start.y - 1) == 0 && game_explored(&g, start.x + 60, start.y) == 0);
    CHECK(game_explored(&g, start.x, start.y - 1 - radius - 1) == 0 && game_explored(&g, start.x, s + 8) == 255);
    /* It never steps: neighbouring tiles differ a little at a time. */
    int steepest = 0;
    for (int d = 0; d < radius + 2; d++) {
        int step = abs(game_explored(&g, start.x + d, start.y - 1) - game_explored(&g, start.x + d + 1, start.y - 1));
        if (step > steepest) steepest = step;
    }
    CHECK(steepest > 0 && steepest < 110);

    /* Walking uncovers more, and what was uncovered stays so behind. */
    CHECK(game_click_tile(&g, start.x + 40, start.y));
    run(&g, 8.0f);
    CHECK(g.player.feet.x == start.x + 40);
    CHECK(game_explored(&g, start.x + 40, start.y) == 255 && game_explored(&g, start.x + 50, start.y) == 255);
    CHECK(game_explored(&g, start.x, start.y) == 255 && game_explored(&g, start.x - 10, start.y) == 255);
    CHECK(game_explored(&g, start.x + 20, start.y - 6) == 255); /* the whole way along, not only the ends */
    CHECK(game_explored(&g, start.x + 70, start.y) == 0 && game_explored(&g, start.x - 30, start.y) == 0);
    /* Across the seam of the world, too. */
    game_reveal(&g, 2, s - 1);
    CHECK(game_explored(&g, 250, s - 1) == 255 && game_explored(&g, -6, s - 1) == 255);

    /* Turned off, everything shows; turned on again, what was known is still known. */
    g.fog = false;
    CHECK(game_explored(&g, start.x + 100, start.y) == 255 && game_explored(&g, start.x - 100, s + 20) == 255);
    g.fog = true;
    CHECK(game_explored(&g, start.x + 100, start.y) == 0 && game_explored(&g, start.x + 40, start.y) == 255);

    /* It is saved with the world, setting and all. */
    CHECK(save_game(&g, "build/test_fog.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_fog.world"));
    CHECK(loaded.fog && memcmp(loaded.explored, g.explored, (size_t)w->width * w->height) == 0);
    game_free(&loaded);
    g.fog = false;
    CHECK(save_game(&g, "build/test_fog.world") && load_game(&loaded, "build/test_fog.world") && !loaded.fog);
    CHECK(loaded.explored[(size_t)start.y * w->width + start.x] == 255);
    game_free(&loaded);
    remove("build/test_fog.world");
    game_free(&g);
}

static bool near(float a, float b)
{
    return a - b < 0.002f && b - a < 0.002f;
}

static void test_vitals(void)
{
    Game g;
    game_init(&g, 128, 64, 51, WORLD_FLAT);
    const RulesDef *u = &content.rules;
    Vitals *v = &g.vitals;
    Inventory *inv = &g.inventory;
    ItemId apple = item_id("apple"), meat = item_id("cooked_dodo_meat");

    CHECK(u->hunger_ticks == 3600 && u->energy_ticks == 1800 && u->meditation_ticks == 900 && u->rest_speed == 3);
    CHECK(v->health == 1.0f && v->hunger == 1.0f && v->energy == 1.0f && v->rest == REST_NONE);

    /* A full hunger bar lasts 3600 ticks and a full energy bar 1800. */
    for (int i = 0; i < 360; i++) game_tick(&g);
    CHECK(near(v->hunger, 0.9f) && near(v->energy, 0.8f) && v->health == 1.0f);

    /* Food is eaten from the selected slot, and only food. */
    clear_inventory(&g);
    inventory_add(inv, ITEM_DIRT, 1);
    inventory_add(inv, apple, 5);
    inventory_select(inv, 0, -1);
    CHECK(!game_is_food(ITEM_DIRT) && game_is_food(apple) && !game_eat(&g));
    inventory_select(inv, 1, -1);
    v->hunger = 0.5f;
    v->health = 0.5f;
    CHECK(game_can_eat(&g, apple) && game_eat(&g));
    CHECK(near(v->hunger, 0.5f + item_def(apple)->food) && inventory_count(inv, apple) == 4);
    /* It mends health too, but much less than it fills hunger. */
    CHECK(v->health > 0.5f && v->health - 0.5f < item_def(apple)->food / 4);

    /* Eating up to full just fills the bar; eating again when full turns it
     * gold: nothing more can be eaten, and hunger holds off for a while. */
    v->hunger = 0.95f;
    CHECK(game_eat(&g) && v->hunger == 1.0f && !game_saturated(&g) && game_can_eat(&g, apple));
    CHECK(game_eat(&g) && game_saturated(&g) && !game_can_eat(&g, apple) && !game_eat(&g));
    CHECK(inventory_count(inv, apple) == 2);
    int held_off = 0;
    while (game_saturated(&g) && held_off < 100000) {
        game_tick(&g);
        held_off++;
    }
    CHECK(held_off > 100 && v->hunger == 1.0f);
    game_tick(&g);
    CHECK(v->hunger < 1.0f && game_can_eat(&g, apple));
    /* A bigger meal holds it off for longer. */
    CHECK(item_def(meat)->food > item_def(apple)->food);

    /* Health comes back by itself, but with nothing in the stomach it runs
     * down instead - though never all the way. */
    v->health = 0.5f;
    for (int i = 0; i < 100; i++) game_tick(&g);
    CHECK(v->health > 0.5f);
    v->hunger = 0.0f;
    float before = v->health;
    for (int i = 0; i < 100; i++) game_tick(&g);
    CHECK(v->health < before);
    for (int i = 0; i < u->starve_ticks; i++) game_tick(&g);
    CHECK(near(v->health, u->starve_floor) && v->health > 0.0f);
    v->hunger = v->health = 1.0f;

    /* Sleep: not with energy full. Asleep, ticks go three times as fast and
     * energy comes back; the player wakes by themselves when it is full. */
    v->energy = 1.0f;
    CHECK(!game_rest(&g, REST_SLEEP) && game_tick_speed(&g) == g.tick_rate);
    v->energy = 0.5f;
    CHECK(game_rest(&g, REST_SLEEP) && v->rest == REST_SLEEP && game_tick_speed(&g) == 3 * g.tick_rate);
    uint64_t tick = g.tick;
    run(&g, 1.0f);
    CHECK(g.tick - tick >= 29 && g.tick - tick <= 31 && v->energy > 0.5f && is_idle(&g));
    for (int i = 0; i < u->sleep_ticks && v->rest == REST_SLEEP; i++) game_tick(&g);
    CHECK(v->rest == REST_NONE && game_bar_full(v->energy) && game_tick_speed(&g) == g.tick_rate);

    /* Asleep, the player does not move; told to, they wake first. */
    v->energy = 0.2f;
    TilePos at = g.player.feet;
    CHECK(game_rest(&g, REST_SLEEP));
    run(&g, 0.5f);
    CHECK(g.player.feet.x == at.x && v->rest == REST_SLEEP);
    CHECK(game_click_tile(&g, at.x + 3, at.y) && v->rest == REST_NONE);
    run(&g, 2.0f);
    CHECK(g.player.feet.x == at.x + 3);
    CHECK(game_rest(&g, REST_SLEEP));
    game_set_move_input(&g, -1, 0);
    CHECK(v->rest == REST_NONE);
    game_set_move_input(&g, 0, 0);
    run(&g, 0.5f);

    /* Meditating: every 900 ticks something from the list turns up beside
     * the player. It can be done with energy full, and goes on until stopped. */
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    clear_inventory(&g);
    v->energy = 1.0f;
    CHECK(game_rest(&g, REST_MEDITATE) && v->rest == REST_MEDITATE && game_tick_speed(&g) == 3 * g.tick_rate);
    v->hunger = 0.8f;
    for (int i = 0; i < 450; i++) game_tick(&g);
    CHECK(near(v->meditation, 0.5f));
    /* Neither hunger nor energy runs down while meditating. */
    CHECK(v->hunger == 0.8f && v->energy == 1.0f);
    int found = 0;
    for (int i = 0; i < MAX_DROPS; i++) found += g.drops[i].active;
    CHECK(found == 0);
    for (int i = 0; i < 450 + 900; i++) game_tick(&g);
    CHECK(v->rest == REST_MEDITATE);
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!g.drops[i].active) continue;
        found++;
        bool listed = false;
        for (int k = 0; k < u->loot_count; k++) listed = listed || u->loot[k].item == g.drops[i].item;
        CHECK(listed);
        /* It comes out right on top of the player. */
        CHECK(g.drops[i].x == g.player.feet.x && !g.drops[i].held_off);
    }
    CHECK(found == 2);
    /* And so is picked up like anything else lying at their feet. */
    int had = inv->slots[0].count + inv->slots[1].count;
    run(&g, 1.0f);
    found = 0;
    for (int i = 0; i < MAX_DROPS; i++) found += g.drops[i].active;
    CHECK(found == 0 && inv->slots[0].count + inv->slots[1].count == had + 2);
    game_wake(&g);
    CHECK(v->rest == REST_NONE && v->meditation == 0.0f);

    /* All five gems, with each rarer than the last; the fruits; the six ores. */
    const char *gems[] = { "amethyst", "sapphire", "emerald", "ruby", "diamond" };
    const char *others[] = { "acorn", "cherry", "apple", "orange", "prickly_pear", "copper_ore", "tin_ore",
                             "iron_ore", "gold_ore", "platinum_ore", "titanium_ore" };
    int last = 1 << 30;
    for (int i = 0; i < 5; i++) {
        int weight = 0;
        for (int k = 0; k < u->loot_count; k++)
            if (u->loot[k].item == item_id(gems[i])) weight = u->loot[k].weight;
        CHECK(weight > 0 && weight < last);
        last = weight;
    }
    for (int i = 0; i < 11; i++) {
        bool listed = false;
        for (int k = 0; k < u->loot_count; k++) listed = listed || u->loot[k].item == item_id(others[i]);
        CHECK(listed);
    }
    CHECK(u->loot_count == 17);

    /* Not while hanging on a wall. */
    world_set(&g.world, g.player.feet.x + 1, g.player.feet.y + 1, TILE_AIR);
    world_set(&g.world, g.player.feet.x + 1, g.player.feet.y + 2, TILE_AIR);
    world_set(&g.world, g.player.feet.x + 1, g.player.feet.y + 3, TILE_AIR);
    CHECK(game_click_tile(&g, g.player.feet.x + 1, g.player.feet.y + 2));
    run(&g, 2.0f);
    if (g.player.climbing) CHECK(!game_rest(&g, REST_MEDITATE));

    /* The bars are saved with the world; nobody is saved asleep. */
    v->health = 0.7f;
    v->hunger = 0.6f;
    v->energy = 0.4f;
    v->saturation = 0.0f;
    v->rest = REST_SLEEP;
    Game loaded;
    CHECK(save_game(&g, "build/test_vitals.world") && load_game(&loaded, "build/test_vitals.world"));
    CHECK(near(loaded.vitals.health, 0.7f) && near(loaded.vitals.hunger, 0.6f) && near(loaded.vitals.energy, 0.4f));
    CHECK(loaded.vitals.rest == REST_NONE);
    game_free(&loaded);
    remove("build/test_vitals.world");
    game_free(&g);

    /* A step up or down a block goes round its corner, not through it. */
    for (int i = 0; i <= 20; i++) {
        float ox, oy;
        game_step_offset(1, -1, true, (float)i / 20.0f, &ox, &oy);
        CHECK(ox == 0.0f || oy == -1.0f);
        game_step_offset(-1, 1, false, (float)i / 20.0f, &ox, &oy);
        CHECK(ox == -1.0f || oy == 0.0f);
    }
    float ox, oy;
    game_step_offset(1, -1, true, 1.0f, &ox, &oy);
    CHECK(ox == 1.0f && oy == -1.0f);
    game_step_offset(2, 0, false, 0.5f, &ox, &oy);
    CHECK(ox == 1.0f && oy == 0.0f);

    /* Every bench lists what it makes lowest tier first. */
    for (TileType t = 1; t < content.tile_count; t++) {
        const Recipe *list[MAX_RECIPES];
        int n = bench_recipes(t, list, MAX_RECIPES);
        for (int i = 1; i < n; i++) CHECK(list[i - 1]->level <= list[i]->level);
    }
    CHECK(content.rules.max_tick_rate > 60 && content.rules.max_tick_rate <= MAX_TICK_RATE);
}

static int count_drops(const Game *g, ItemId item)
{
    int n = 0;
    for (int i = 0; i < MAX_DROPS; i++) n += g->drops[i].active && g->drops[i].item == item;
    return n;
}

static void test_discard(void)
{
    Game g;
    game_init(&g, 128, 64, 52, WORLD_FLAT);
    Inventory *inv = &g.inventory;
    World *w = &g.world;
    clear_inventory(&g);
    for (int i = 0; i < MAX_MOBS; i++) g.mobs[i].active = false;
    CHECK(content.rules.drop_lifetime == 600.0f);

    /* Part of a stack dropped out of the bar lands right where the player
     * is, and stays there however long they stand over it. */
    inventory_add(inv, ITEM_STONE, 20);
    int px = g.player.feet.x, ground = g.player.feet.y;
    CHECK(game_discard(&g, (SlotRef){ SLOT_BAR, 0, 0 }, 5) == 5);
    CHECK(inv->slots[0].count == 15 && count_drops(&g, ITEM_STONE) == 5);
    for (int i = 0; i < MAX_DROPS; i++)
        if (g.drops[i].active) CHECK(g.drops[i].x == px && g.drops[i].held_off);
    run(&g, 5.0f);
    CHECK(count_drops(&g, ITEM_STONE) == 5 && inventory_count(inv, ITEM_STONE) == 15);
    /* Nor by stepping to the square beside them: that is still within reach. */
    inventory_select(inv, 5, -1); /* an empty hand, so a click is a walk and not a placing */
    CHECK(game_click_tile(&g, px + 1, ground));
    run(&g, 2.0f);
    CHECK(g.player.feet.x == px + 1 && count_drops(&g, ITEM_STONE) == 5);
    /* Out of reach and back again, and they are picked up as usual. */
    CHECK(game_click_tile(&g, px + 4, ground));
    run(&g, 2.0f);
    CHECK(g.player.feet.x == px + 4 && count_drops(&g, ITEM_STONE) == 5);
    for (int i = 0; i < MAX_DROPS; i++)
        if (g.drops[i].active) CHECK(!g.drops[i].held_off);
    CHECK(game_click_tile(&g, px, ground));
    run(&g, 3.0f);
    CHECK(count_drops(&g, ITEM_STONE) == 0 && inventory_count(inv, ITEM_STONE) == 20);

    /* Or, without going anywhere, by asking for one: a tap on it queues picking it up. */
    CHECK(game_discard(&g, (SlotRef){ SLOT_BAR, 0, 0 }, 3) == 3);
    run(&g, 2.0f);
    CHECK(count_drops(&g, ITEM_STONE) == 3 && inventory_count(inv, ITEM_STONE) == 17);
    int tapped = -1;
    for (int i = 0; i < MAX_DROPS; i++)
        if (g.drops[i].active) tapped = i;
    CHECK(tapped >= 0 && game_click_drop(&g, tapped));
    run(&g, 2.0f);
    CHECK(count_drops(&g, ITEM_STONE) == 2 && inventory_count(inv, ITEM_STONE) == 18);
    /* The flag is kept by a save. */
    Game loaded;
    CHECK(save_game(&g, "build/test_discard.world") && load_game(&loaded, "build/test_discard.world"));
    int held = 0;
    for (int i = 0; i < MAX_DROPS; i++) held += loaded.drops[i].active && loaded.drops[i].held_off;
    CHECK(held == 2);
    game_free(&loaded);
    remove("build/test_discard.world");

    /* Asking for more than there is drops the whole stack; an empty slot drops nothing. */
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    CHECK(game_discard(&g, (SlotRef){ SLOT_BAR, 0, 0 }, 99) == 18 && inv->slots[0].count == 0);
    CHECK(game_discard(&g, (SlotRef){ SLOT_BAR, 0, 0 }, 1) == 0 && count_drops(&g, ITEM_STONE) == 18);

    /* A basket goes with what is in it, and comes back with it. */
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    clear_inventory(&g);
    inventory_add(inv, ITEM_BASKET, 1);
    inventory_add(inv, ITEM_WOOD, 7);
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 1, 0 }, (SlotRef){ SLOT_BASKET, 0, 2 }, MAX_STACK));
    CHECK(game_discard(&g, (SlotRef){ SLOT_BAR, 0, 0 }, 1) == 1 && inventory_count(inv, ITEM_WOOD) == 0);
    CHECK(count_drops(&g, ITEM_BASKET) == 1);
    run(&g, 2.0f);
    CHECK(inventory_count(inv, ITEM_BASKET) == 0);
    for (int i = 0; i < MAX_DROPS; i++)
        if (g.drops[i].active) CHECK(game_click_drop(&g, i));
    run(&g, 3.0f);
    CHECK(inventory_count(inv, ITEM_BASKET) == 1 && inventory_count(inv, ITEM_WOOD) == 7);

    /* Whatever is lying in the world is gone after ten minutes, however it
     * got there: thrown out, left by a block, left by a creature. */
    clear_inventory(&g);
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    int far = g.player.feet.x + 20;
    ground = g.player.feet.y;
    game_drop_item(&g, ITEM_STICK, far, ground);
    int dodo = mob_spawn(&g, mob_id("dodo"), far + 3, ground, true, false);
    mob_kill(&g, dodo);
    inventory_add(inv, ITEM_STONE, 3);
    CHECK(game_discard(&g, (SlotRef){ SLOT_BAR, 0, 0 }, 3) == 3);
    int lying = 0;
    for (int i = 0; i < MAX_DROPS; i++) lying += g.drops[i].active;
    CHECK(lying >= 6 && count_drops(&g, item_id("dodo_meat")) >= 1);
    for (int i = 0; i < 599 * 10; i++) game_update(&g, 0.1f);
    int still = 0;
    for (int i = 0; i < MAX_DROPS; i++) still += g.drops[i].active;
    CHECK(still == lying);
    for (int i = 0; i < 2 * 10; i++) game_update(&g, 0.1f);
    still = 0;
    for (int i = 0; i < MAX_DROPS; i++) still += g.drops[i].active;
    CHECK(still == 0);

    /* Fruit on a tree is not lying anywhere: its ten minutes start when it falls. */
    Drop *fruit = NULL;
    game_drop_item(&g, ITEM_ACORN, far, ground - 6);
    for (int i = 0; i < MAX_DROPS; i++)
        if (g.drops[i].active) fruit = &g.drops[i];
    CHECK(fruit != NULL);
    if (fruit) {
        world_set_at(w, BG, far, ground - 6, TILE_OAK_LEAVES);
        fruit->attached = true;
        fruit->age = 900.0f;
        run(&g, 1.0f);
        CHECK(fruit->active && fruit->attached);
        world_set_at(w, BG, far, ground - 6, TILE_AIR);
        run(&g, 1.0f);
        CHECK(fruit->active && !fruit->attached && fruit->age < 5.0f);
    }
    game_free(&g);
}

/* A full-grown tree of a kind, standing on the flat ground at a column. */
static void grow_tree(World *w, int x, const char *kind)
{
    uint32_t rng = 9;
    tree_plant(w, x, w->surface_y - 1, tree_id(kind));
    while (tree_grow(w, x, w->surface_y - 1, &rng).grew) {}
}

static int tree_top(const World *w, int x)
{
    int y = w->surface_y - 1;
    while (tree_trunk_at(w, x, y - 1) >= 0) y--;
    return y;
}

static void test_coffee_and_dropbears(void)
{
    Game g;
    game_init(&g, 256, 64, 61, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    Vitals *v = &g.vitals;
    ItemId bean = item_id("coffee_bean");
    int ground = w->surface_y - 1, px = g.player.feet.x;
    clear_inventory(&g);
    for (int i = 0; i < MAX_MOBS; i++) g.mobs[i].active = false;

    /* A coffee bean gives energy as well as a little hunger and health. */
    CHECK(bean > 0 && game_is_food(bean) && item_def(bean)->food_energy > item_def(bean)->food);
    inventory_add(inv, bean, 5);
    inventory_select(inv, 0, -1);
    v->energy = 0.3f;
    v->hunger = 0.5f;
    v->health = 0.5f;
    CHECK(game_eat(&g));
    CHECK(near(v->energy, 0.3f + item_def(bean)->food_energy) && v->hunger > 0.5f && v->health > 0.5f);
    /* And planted on dirt it grows a coffee tree, which bears more of them. */
    CHECK(game_queue_action(&g, ACTION_PLANT, px + 4, ground + 1));
    run(&g, 3.0f);
    CHECK(tree_trunk_at(w, px + 4, ground) == tree_id("coffee") && inventory_count(inv, bean) == 3);
    CHECK(content.trees[tree_id("coffee")].fruit == bean);
    world_set_at(w, BG, px + 4, ground, TILE_AIR);

    /* Dropbears. One turns up at the top of a tree near the player. */
    int kind = mob_id("dropbear");
    const MobDef *def = &content.mobs[kind];
    CHECK(kind >= 0 && def->perches && def->bite > 0.0f && def->cap > 0);
    int tx = px + 12;
    grow_tree(w, tx, "oak");
    int top = tree_top(w, tx);
    CHECK(top < ground - 2);
    int bear = -1;
    for (int tries = 0; tries < 400 && bear < 0; tries++) {
        mobs_try_spawn(&g, tx, ground - 1);
        for (int i = 0; i < MAX_MOBS; i++)
            if (g.mobs[i].active && g.mobs[i].kind == kind) bear = i;
    }
    CHECK(bear >= 0);
    if (bear < 0) return;
    Mob *m = &g.mobs[bear];
    CHECK(m->x == tx && m->y == top && m->state == MOB_PERCH);
    /* Only one to a tree, and never on open ground. */
    for (int tries = 0; tries < 400; tries++) mobs_try_spawn(&g, tx, ground - 1), mobs_try_spawn(&g, px + 30, ground);
    int bears = 0;
    for (int i = 0; i < MAX_MOBS; i++) bears += g.mobs[i].active && g.mobs[i].kind == kind;
    CHECK(bears == 1);
    for (int i = 0; i < MAX_MOBS; i++)
        if (i != bear) g.mobs[i].active = false;

    /* With nobody underneath it stays where it is. */
    v->health = 1.0f;
    v->hunger = 1.0f;
    for (int i = 0; i < 200; i++) game_tick(&g);
    CHECK(m->active && m->x == tx && m->y == top && m->state == MOB_PERCH && v->health == 1.0f);

    /* Walk under it and it runs down the tree, bites, and runs back up. */
    g.player.feet = g.player.step_to = (TilePos){ tx - 2, ground };
    bool came_down = false, bitten = false;
    for (int i = 0; i < 300 && !bitten; i++) {
        game_tick(&g);
        if (m->y > top) came_down = true;
        if (v->health < 0.99f) bitten = true;
    }
    CHECK(came_down && bitten && m->state == MOB_RETURN);
    CHECK(near(v->health, 1.0f - def->bite) || v->health > 1.0f - def->bite);
    float after = v->health;
    for (int i = 0; i < 100; i++) game_tick(&g);
    CHECK(m->x == tx && m->y == top && m->state == MOB_PERCH);
    /* It does not come straight back down for another go. */
    CHECK(v->health >= after);
    /* Being attacked brings the player out of meditation... */
    m->timer = 0;
    CHECK(game_rest(&g, REST_MEDITATE));
    float before_bite = v->health;
    for (int i = 0; i < 300 && v->rest == REST_MEDITATE; i++) game_tick(&g);
    CHECK(v->rest == REST_NONE && v->health < before_bite);
    for (int i = 0; i < 100; i++) game_tick(&g);
    /* ...and wakes a sleeper. */
    m->timer = 0;
    v->energy = 0.2f;
    CHECK(game_rest(&g, REST_SLEEP));
    for (int i = 0; i < 300 && v->rest == REST_SLEEP; i++) game_tick(&g);
    CHECK(v->rest == REST_NONE);

    /* The player gone, it gives the chase up and goes home. */
    g.player.feet = g.player.step_to = (TilePos){ px - 60, ground };
    m->owned = true; /* (so that it is not lost for being far from the player) */
    for (int i = 0; i < 400; i++) game_tick(&g);
    CHECK(m->x == tx && m->y == top && m->state == MOB_PERCH);

    /* Cut its tree down and it drops, runs about, finds another and goes up that. */
    int other = tx + 9;
    grow_tree(w, other, "cherry");
    for (int y = top - 3; y <= ground; y++)
        for (int x = tx - 3; x <= tx + 3; x++) world_set_at(w, BG, x, y, TILE_AIR);
    bool on_ground = false;
    for (int i = 0; i < 400 && !(m->x == other && m->state == MOB_PERCH); i++) {
        game_tick(&g);
        if (m->y == ground && m->x != other) on_ground = true;
    }
    CHECK(on_ground && m->x == other && m->y == tree_top(w, other) && m->state == MOB_PERCH && m->home_x == other);

    /* With no tree anywhere it keeps running about on the ground. */
    for (int y = 0; y <= ground; y++)
        for (int x = other - 4; x <= other + 4; x++) world_set_at(w, BG, x, y, TILE_AIR);
    int left = m->x, right = m->x;
    for (int i = 0; i < 300; i++) {
        game_tick(&g);
        if (world_dx(w, other, m->x) < world_dx(w, other, left)) left = m->x;
        if (world_dx(w, other, m->x) > world_dx(w, other, right)) right = m->x;
    }
    CHECK(m->active && m->y == ground && m->state == MOB_SEEK && right != left);

    /* It is saved as it is, state and all; killed, it leaves fur. */
    grow_tree(w, m->x + 3, "oak");
    for (int i = 0; i < 200; i++) game_tick(&g);
    CHECK(m->state == MOB_PERCH);
    Game loaded;
    CHECK(save_game(&g, "build/test_bear.world") && load_game(&loaded, "build/test_bear.world"));
    CHECK(loaded.mobs[bear].active && loaded.mobs[bear].state == MOB_PERCH && loaded.mobs[bear].home_x == m->home_x);
    game_free(&loaded);
    remove("build/test_bear.world");
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    mob_kill(&g, bear);
    CHECK(count_drops(&g, item_id("fur")) >= 1);
    game_free(&g);

    /* A world remembers which biome each column is. */
    Game t;
    CHECK(game_init(&t, 1024, 401, 4, WORLD_TERRAIN));
    CHECK(save_game(&t, "build/test_biome.world") && load_game(&loaded, "build/test_biome.world"));
    int differ = 0, kinds = 0;
    bool had[MAX_BIOMES] = { false };
    for (int x = 0; x < t.world.width; x++) {
        differ += world_biome(&t.world, x) != world_biome(&loaded.world, x);
        if (!had[world_biome(&t.world, x)]) had[world_biome(&t.world, x)] = true, kinds++;
    }
    CHECK(differ == 0 && kinds >= 2 && world_biome(&t.world, t.world.spawn_x) == 0);
    /* And one that never recorded them has them worked out from the land. */
    int guessed = 0;
    world_guess_biomes(&loaded.world);
    for (int x = 0; x < t.world.width; x++) guessed += world_biome(&t.world, x) == world_biome(&loaded.world, x);
    CHECK(guessed * 100 >= t.world.width * 95);
    game_free(&loaded);
    game_free(&t);
    remove("build/test_biome.world");
}

static int count_tiles(const World *w, int layer, TileType t, int x0, int x1)
{
    int n = 0;
    for (int y = 0; y < w->height; y++)
        for (int x = x0; x <= x1; x++) n += world_get_at(w, layer, x, y) == t;
    return n;
}

static void test_decay_and_golden_chests(void)
{
    Game g;
    game_init(&g, 256, 64, 71, WORLD_FLAT);
    World *w = &g.world;
    clear_inventory(&g);
    for (int i = 0; i < MAX_MOBS; i++) g.mobs[i].active = false;
    int ground = w->surface_y - 1, tx = g.player.feet.x + 6;
    g.tick_rate = 0; /* no random events but the ones landed by hand: nothing rots away mid-test */
    TileType dead_log = tile_id("oak_log_decayed"), dead_leaves = tile_id("oak_leaves_decayed");
    CHECK(dead_log > 0 && dead_leaves > 0 && tile_def(TILE_OAK_LOG)->decays_into == dead_log);
    CHECK(tile_def(dead_log)->vanishes && tile_def(dead_log)->drop_chance == 0.75f);
    CHECK(tile_def(dead_leaves)->vanishes && tile_def(dead_leaves)->drop_chance == 0.5f);
    CHECK(tile_def(dead_log)->log_of < 0 && tile_def(dead_leaves)->leaves_of < 0 && tile_def(dead_log)->climbable);

    /* Cut an oak through part-way up: the trunk above the cut decays and so
     * do the leaves from there up; what is below the cut lives on. */
    grow_tree(w, tx, "oak");
    int top = tree_top(w, tx), cut = top + 1;
    int logs = count_tiles(w, BG, TILE_OAK_LOG, tx - 4, tx + 4), leaves = count_tiles(w, BG, TILE_OAK_LEAVES, tx - 4, tx + 4);
    int low_leaves = 0;
    for (int y = cut + 1; y <= ground; y++)
        for (int x = tx - 4; x <= tx + 4; x++) low_leaves += world_get_at(w, BG, x, y) == TILE_OAK_LEAVES;
    CHECK(logs == ground - top + 1 && leaves > 8 && low_leaves > 0 && low_leaves < leaves);
    CHECK(game_queue_action(&g, ACTION_MINE, tx, cut));
    run(&g, 8.0f);
    CHECK(world_get_at(w, BG, tx, cut) == TILE_AIR && count_item(&g, ITEM_WOOD) + count_drops(&g, ITEM_WOOD) == 1);
    for (int y = top; y < cut; y++) CHECK(world_get_at(w, BG, tx, y) == dead_log);
    for (int y = cut + 1; y <= ground; y++) CHECK(world_get_at(w, BG, tx, y) == TILE_OAK_LOG);
    CHECK(count_tiles(w, BG, dead_log, tx - 4, tx + 4) == cut - top);
    CHECK(count_tiles(w, BG, TILE_OAK_LEAVES, tx - 4, tx + 4) == low_leaves);
    CHECK(count_tiles(w, BG, dead_leaves, tx - 4, tx + 4) == leaves - low_leaves);
    /* The stump is still a tree; what is dead is not, and grows no more. */
    CHECK(tree_trunk_at(w, tx, ground) == TREE_OAK && tree_trunk_at(w, tx, top) < 0 && tree_leaves_at(w, tx - 1, top) < 0);

    /* Cut at the bottom and the whole tree goes. */
    int t2 = tx + 12;
    grow_tree(w, t2, "cherry");
    int cherry_leaves = count_tiles(w, BG, TILE_CHERRY_LEAVES, t2 - 4, t2 + 4), cherry_top = tree_top(w, t2);
    g.player.feet = g.player.step_to = (TilePos){ t2 - 1, ground };
    CHECK(game_queue_action(&g, ACTION_MINE, t2, ground));
    run(&g, 8.0f);
    CHECK(count_tiles(w, BG, TILE_CHERRY_LOG, t2 - 4, t2 + 4) == 0 && count_tiles(w, BG, TILE_CHERRY_LEAVES, t2 - 4, t2 + 4) == 0);
    CHECK(count_tiles(w, BG, tile_id("cherry_log_decayed"), t2 - 4, t2 + 4) == ground - cherry_top);
    CHECK(count_tiles(w, BG, tile_id("cherry_leaves_decayed"), t2 - 4, t2 + 4) == cherry_leaves);

    /* A random event on something decayed takes it away. Over many, about
     * three logs in four leave wood and one leaf in two a stick. */
    clear_inventory(&g);
    int wood = 0, sticks = 0, trials = 2000;
    g.player.feet = g.player.step_to = (TilePos){ 10, ground }; /* out of the way of the drops */
    for (int i = 0; i < trials; i++) {
        for (int k = 0; k < MAX_DROPS; k++) g.drops[k].active = false;
        world_set_at(w, BG, 100, ground, dead_log);
        world_set_at(w, BG, 102, ground, dead_leaves);
        /* (game_tick lands its events at random; land one on each by hand.) */
        game_random_event(&g, 100, ground);
        game_random_event(&g, 102, ground);
        CHECK(world_get_at(w, BG, 100, ground) == TILE_AIR && world_get_at(w, BG, 102, ground) == TILE_AIR);
        wood += count_drops(&g, ITEM_WOOD);
        sticks += count_drops(&g, ITEM_STICK);
    }
    CHECK(wood > trials * 70 / 100 && wood < trials * 80 / 100);
    CHECK(sticks > trials * 45 / 100 && sticks < trials * 55 / 100);
    /* Mined by hand they go by the same odds. */
    int by_hand = 0;
    for (int i = 0; i < 40; i++) {
        for (int k = 0; k < MAX_DROPS; k++) g.drops[k].active = false;
        clear_inventory(&g);
        world_set_at(w, BG, 13, ground, dead_leaves);
        CHECK(game_queue_action(&g, ACTION_MINE, 13, ground));
        run(&g, 2.0f);
        CHECK(world_get_at(w, BG, 13, ground) == TILE_AIR);
        by_hand += count_item(&g, ITEM_STICK) + count_drops(&g, ITEM_STICK);
    }
    CHECK(by_hand > 8 && by_hand < 32);

    /* A cactus dies the same way, arms and all. */
    TileType sand = tile_id("sand"), cactus = tile_id("cactus"), arm = tile_id("cactus_arm");
    int cx = 60;
    world_set(w, cx, ground + 1, sand);
    grow_tree(w, cx, "cactus");
    CHECK(count_tiles(w, BG, arm, cx - 3, cx + 3) == 6);
    game_decay_tree(&g, cx, ground);
    CHECK(count_tiles(w, BG, arm, cx - 3, cx + 3) == 0 && count_tiles(w, BG, tile_id("cactus_arm_decayed"), cx - 3, cx + 3) == 6);
    CHECK(world_get_at(w, BG, cx, ground) == cactus && world_get_at(w, BG, cx, ground - 1) == tile_id("cactus_decayed"));
    game_free(&g);

    /* Golden chests: buried under beaches, on the rock with sand over them. */
    TileType chest = tile_id("golden_chest");
    ItemId coin = item_id("gold_coin");
    const TileDef *def = tile_def(chest);
    CHECK(chest > 0 && def->loot_count == 5 && def->storage_columns == 4 && item_id("golden_chest") > 0);
    int found = 0, beach_columns = 0, misplaced = 0;
    Game t;
    TilePos where = { 0, 0 };
    for (uint32_t seed = 1; seed <= 6; seed++) {
        CHECK(game_init(&t, 2048, 401, seed, WORLD_TERRAIN));
        World *tw = &t.world;
        for (int x = 0; x < tw->width; x++) {
            bool beach = strcmp(content.biomes[world_biome(tw, x)].name, "beach") == 0;
            beach_columns += beach;
            for (int y = 1; y < tw->height - 1; y++) {
                if (world_get(tw, x, y) != chest) continue;
                found++;
                where = (TilePos){ x, y };
                if (!beach || world_get(tw, x, y - 1) != sand || !world_is_solid(tw, x, y + 1)) misplaced++;
            }
        }
        if (seed < 6 || found == 0) game_free(&t);
    }
    /* Several to a world - about one beach column in eight - but not one under every column. */
    CHECK(found >= 30 && found * 4 < beach_columns && misplaced == 0);
    /* A golden chest can be melted down for a gold coin, at a tier 2 metalwork bench. */
    const Recipe *melt = find_recipe_from(tile_id("metalwork_bench_2"), "gold_coin", "golden_chest");
    CHECK(melt && melt->level == 2 && melt->output.count == 1 && melt->inputs[0].count == 1 && melt->input_count == 1);
    if (found == 0) return;

    /* Opened for the first time it holds its treasure: 1-5 gold coins, up to
     * 3 gold ore, up to 3 iron ingots, perhaps a titanium ingot, and one gem. */
    World *tw = &t.world;
    for (int i = 0; i < MAX_MOBS; i++) t.mobs[i].active = false;
    clear_inventory(&t);
    for (int y = where.y - 4; y < where.y; y++) world_set(tw, where.x, y, TILE_AIR), world_set(tw, where.x - 1, y, TILE_AIR);
    world_set(tw, where.x - 1, where.y, sand);
    t.player.feet = t.player.step_to = (TilePos){ where.x - 1, where.y - 1 };
    CHECK(game_queue_action(&t, ACTION_USE, where.x, where.y));
    run(&t, 2.0f);
    uint16_t box = game_open_storage(&t);
    CHECK(t.bench_open && box != 0);
    int coins = 0, ore = 0, iron = 0, titanium = 0, gems = 0, other = 0;
    for (int i = 0; box != 0 && i < CONTAINER_MAX_SLOTS; i++) {
        const ItemStack *st = &t.inventory.containers[box].slots[i];
        if (st->count == 0) continue;
        const char *name = item_def(st->id)->name;
        if (st->id == coin) coins += st->count;
        else if (strcmp(name, "gold_ore") == 0) ore += st->count;
        else if (strcmp(name, "iron_ingot") == 0) iron += st->count;
        else if (strcmp(name, "titanium_ingot") == 0) titanium += st->count;
        else if (strcmp(name, "amethyst") == 0 || strcmp(name, "sapphire") == 0 || strcmp(name, "emerald") == 0 ||
                 strcmp(name, "ruby") == 0) gems += st->count;
        else other += st->count;
    }
    CHECK(coins >= 1 && coins <= 5 && ore <= 3 && iron <= 3 && titanium <= 1 && gems == 1 && other == 0);
    /* It is not filled twice. Dug up, the chest comes away with what is left in it. */
    game_close_bench(&t);
    CHECK(game_queue_action(&t, ACTION_USE, where.x, where.y));
    run(&t, 1.0f);
    CHECK(game_open_storage(&t) == box);
    game_close_bench(&t);
    CHECK(game_queue_action(&t, ACTION_MINE, where.x, where.y));
    run(&t, 8.0f);
    CHECK(world_get(tw, where.x, where.y) == TILE_AIR && count_item(&t, item_id("golden_chest")) == 1);
    int kept = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        const ItemStack *st = &t.inventory.slots[i];
        if (st->count == 0 || st->id != item_id("golden_chest") || st->container == 0) continue;
        for (int k = 0; k < CONTAINER_MAX_SLOTS; k++)
            if (t.inventory.containers[st->container].slots[k].id == coin) kept += t.inventory.containers[st->container].slots[k].count;
    }
    CHECK(kept == coins);
    game_free(&t);

    /* The gem is likelier to be a lesser one: each rarer than the last, and never a diamond. */
    const LootDef *gem = &def->loot[4];
    CHECK(gem->item == ITEM_NONE && gem->choice_count == 4);
    for (int k = 1; k < gem->choice_count; k++) CHECK(gem->choices[k].weight < gem->choices[k - 1].weight);
    CHECK(gem->choices[0].item == item_id("amethyst") && gem->choices[3].item == item_id("ruby"));
}

/* The kinds a kind of creature breeds on into, by name, in any order. */
static bool breeds_into(const char *kind, const char *a, const char *b)
{
    const MobDef *def = &content.mobs[mob_id(kind)];
    int want = (a != NULL) + (b != NULL);
    if (def->lay_next_count != want) return false;
    for (int i = 0; i < def->lay_next_count; i++) {
        const char *name = content.mobs[def->lay_next[i]].name;
        if (!(a && strcmp(name, a) == 0) && !(b && strcmp(name, b) == 0)) return false;
    }
    return want == 0 || def->lay_next_chance == 0.25f;
}

static void test_dodo_tree(void)
{
    /* The tree: each kind, what it leaves and how often, and what it breeds on into. */
    static const struct {
        const char *kind, *item;
        float chance;
        const char *next_a, *next_b;
    } tree[] = {
        { "stone_dodo", "stone", 1.0f, "amethyst_dodo", "copper_dodo" },
        { "dirt_dodo", "dirt", 1.0f, "wood_dodo", "flint_dodo" },
        { "wood_dodo", "wood", 0.5f, NULL, NULL },
        { "flint_dodo", "flint", 0.5f, NULL, NULL },
        { "copper_dodo", "copper_ore", 0.5f, "tin_dodo", NULL },
        { "tin_dodo", "tin_ore", 0.5f, "iron_dodo", NULL },
        { "iron_dodo", "iron_ore", 0.33f, "gold_dodo", NULL },
        { "gold_dodo", "gold_ore", 0.33f, "titanium_dodo", NULL },
        { "titanium_dodo", "titanium_ore", 0.2f, "platinum_dodo", NULL },
        { "platinum_dodo", "platinum_ore", 0.1f, NULL, NULL },
        { "amethyst_dodo", "amethyst", 0.5f, "sapphire_dodo", NULL },
        { "sapphire_dodo", "sapphire", 0.33f, "emerald_dodo", NULL },
        { "emerald_dodo", "emerald", 0.25f, "ruby_dodo", NULL },
        { "ruby_dodo", "ruby", 0.2f, "diamond_dodo", NULL },
        { "diamond_dodo", "diamond", 0.1f, NULL, NULL },
    };
    int kinds = (int)(sizeof tree / sizeof tree[0]);
    CHECK(breeds_into("dodo", "stone_dodo", "dirt_dodo") && content.mobs[mob_id("dodo")].drop_count == 2);
    for (int i = 0; i < kinds; i++) {
        int kind = mob_id(tree[i].kind);
        CHECK(kind >= 0);
        if (kind < 0) continue;
        const MobDef *def = &content.mobs[kind];
        char name[64];
        /* Meat and feathers as ever, and its own thing on top, some of the time. */
        CHECK(def->drop_count == 3 && def->drops[0].item == item_id("dodo_meat") && def->drops[0].chance == 1.0f);
        CHECK(def->drops[1].item == item_id("feather") && def->drops[2].item == item_id(tree[i].item));
        CHECK(def->drops[2].chance == tree[i].chance && def->drops[2].min == 1 && def->drops[2].max == 1);
        CHECK(breeds_into(tree[i].kind, tree[i].next_a, tree[i].next_b));
        /* It has an egg of its own, which hatches into it, wild or the player's, and can be traded. */
        snprintf(name, sizeof name, "%s_egg", tree[i].kind);
        TileType egg = tile_id(name);
        ItemId egg_item = item_id(name);
        snprintf(name, sizeof name, "%s_egg_placed", tree[i].kind);
        TileType placed = tile_id(name);
        CHECK(egg > 0 && placed > 0 && egg_item > 0 && def->lays == egg);
        CHECK(tile_def(egg)->hatches == kind && !tile_def(egg)->hatch_owned);
        CHECK(tile_def(placed)->hatches == kind && tile_def(placed)->hatch_owned);
        CHECK(item_def(egg_item)->places == placed && tile_def(egg)->drop == egg_item && item_def(egg_item)->category >= 0);
        /* None of them turns up in the wild: they are bred. */
        CHECK(def->spawn_above_count == 0 && !def->perches);
    }

    Game g;
    game_init(&g, 128, 64, 81, WORLD_FLAT);
    World *w = &g.world;
    int ground = w->surface_y - 1, x = g.player.feet.x + 4;
    g.tick_rate = 0;

    /* What a kind lays, counted over many layings. */
    struct {
        const char *kind, *own, *next_a, *next_b;
    } lays[] = { { "dodo", "dodo_egg", "stone_dodo_egg", "dirt_dodo_egg" },
                 { "stone_dodo", "stone_dodo_egg", "amethyst_dodo_egg", "copper_dodo_egg" },
                 { "copper_dodo", "copper_dodo_egg", "tin_dodo_egg", NULL },
                 { "diamond_dodo", "diamond_dodo_egg", NULL, NULL } };
    for (int k = 0; k < 4; k++) {
        MobDef *def = &content.mobs[mob_id(lays[k].kind)];
        float usual = def->lay_chance, moves = def->move_chance;
        def->lay_chance = 1.0f;
        def->move_chance = 0.0f;
        clear_mobs(&g);
        mob_spawn(&g, mob_id(lays[k].kind), x, ground, true, false);
        int own = 0, a = 0, b = 0, other = 0, trials = 4000;
        for (int i = 0; i < trials; i++) {
            world_set_at(w, BG, x, ground, TILE_AIR);
            mobs_tick(&g);
            TileType laid = world_get_at(w, BG, x, ground);
            if (laid == tile_id(lays[k].own)) own++;
            else if (lays[k].next_a && laid == tile_id(lays[k].next_a)) a++;
            else if (lays[k].next_b && laid == tile_id(lays[k].next_b)) b++;
            else other++;
        }
        world_set_at(w, BG, x, ground, TILE_AIR);
        def->lay_chance = usual;
        def->move_chance = moves;
        CHECK(other == 0 && own + a + b == trials);
        if (!lays[k].next_a) {
            CHECK(own == trials); /* the end of its line: only ever its own */
        } else {
            /* Three in four its own; the rest shared equally between what comes next. */
            CHECK(own > trials * 72 / 100 && own < trials * 78 / 100);
            if (lays[k].next_b) CHECK(a > trials * 10 / 100 && a < trials * 15 / 100 && b > trials * 10 / 100 && b < trials * 15 / 100);
            else CHECK(a > trials * 22 / 100 && a < trials * 28 / 100);
        }
    }

    /* An egg of a kind hatches into that kind. */
    clear_mobs(&g);
    world_set_at(w, BG, x, ground, tile_id("gold_dodo_egg_placed"));
    CHECK(mobs_try_hatch(&g, x, ground));
    int hatched = -1;
    for (int i = 0; i < MAX_MOBS; i++)
        if (g.mobs[i].active) hatched = i;
    CHECK(hatched >= 0 && g.mobs[hatched].kind == mob_id("gold_dodo") && g.mobs[hatched].baby && g.mobs[hatched].owned);

    /* Killed, each leaves its own thing as often as it should, with the meat and feathers every time. */
    for (int i = 0; i < kinds; i++) {
        int got = 0, trials = 1500, meatless = 0;
        ItemId item = item_id(tree[i].item), meat = item_id("dodo_meat"), feather = item_id("feather");
        for (int n = 0; n < trials; n++) {
            for (int d = 0; d < MAX_DROPS; d++) g.drops[d].active = false;
            clear_mobs(&g);
            mob_kill(&g, mob_spawn(&g, mob_id(tree[i].kind), x, ground, true, false));
            int count = count_drops(&g, item);
            if (count > 1) got += 1000000; /* never more than one */
            got += count;
            if (count_drops(&g, meat) < 1 || count_drops(&g, feather) < 1) meatless++;
        }
        float share = (float)got / (float)trials;
        CHECK(meatless == 0 && share > tree[i].chance - 0.05f && share < tree[i].chance + 0.05f);
    }
    /* A chick of any kind leaves nothing. */
    for (int d = 0; d < MAX_DROPS; d++) g.drops[d].active = false;
    mob_kill(&g, mob_spawn(&g, mob_id("stone_dodo"), x, ground, true, true));
    CHECK(count_drops(&g, ITEM_STONE) == 0);
    game_free(&g);
}

/* Lets the water run until it is still; returns how many steps that took. */
static int settle(World *w)
{
    int steps = 0;
    while (w->water_stirring && steps < 5000) water_step(w), steps++;
    return steps;
}

static void test_water(void)
{
    Game g;
    game_init(&g, 256, 96, 91, WORLD_FLAT);
    World *w = &g.world;
    int s = w->surface_y, left = 60, right = 119, depth = 6, top = s - depth; /* a tank 60 wide and 6 deep */
    for (int i = 0; i < MAX_MOBS; i++) g.mobs[i].active = false;
    g.tick_rate = 0;
    CHECK(content.rules.water_flow > 0 && WATER_FULL == 8);

    for (int y = top; y < s; y++) {
        world_set(w, left - 1, y, TILE_STONE);
        world_set(w, right + 1, y, TILE_STONE);
        for (int x = left; x <= right; x++) world_set_water(w, x, y, WATER_FULL);
    }
    long total = water_total(w);
    CHECK(total == 60L * depth * WATER_FULL);
    /* Full to the brim and level, it is at rest: nothing moves, and soon nothing is looked at. */
    int moved = 0;
    for (int i = 0; i < 10; i++) moved += water_step(w);
    CHECK(moved == 0 && !w->water_stirring && water_step(w) == 0);

    /* Break a block out of the bottom. The water above dips into the hole... */
    int hx = 90;
    world_set(w, hx, s, TILE_AIR);
    CHECK(w->water_stirring);
    int lowest = WATER_FULL, steps = 0;
    while (w->water_stirring && steps < 500) {
        water_step(w);
        steps++;
        if (world_water(w, hx, top) < lowest) lowest = world_water(w, hx, top);
        CHECK(water_total(w) == total); /* never any more or any less of it */
    }
    CHECK(lowest < 7 && steps < 100);
    /* ...and then flattens out: the hole is full, and the block's worth that
     * went into it is missing as one eighth from each of eight squares of
     * the surface, side by side over the hole. Everything else is full. */
    CHECK(world_water(w, hx, s) == WATER_FULL);
    int sevens = 0, first = -1, last = -1, wrong = 0;
    for (int x = left; x <= right; x++) {
        int level = world_water(w, x, top);
        if (level == 7) {
            sevens++;
            if (first < 0) first = x;
            last = x;
        } else if (level != WATER_FULL) {
            wrong++;
        }
        for (int y = top + 1; y < s; y++) wrong += world_water(w, x, y) != WATER_FULL;
    }
    CHECK(sevens == 8 && wrong == 0 && last - first == 7 && first <= hx && hx <= last);
    CHECK(water_total(w) == total && !w->water_stirring);

    /* A block put into water pushes it out of the way, not out of the world. */
    world_set(w, hx - 20, s - 2, TILE_STONE);
    CHECK(world_water(w, hx - 20, s - 2) == 0 && water_total(w) == total);
    settle(w);
    CHECK(water_total(w) == total);
    world_set(w, hx - 20, s - 2, TILE_AIR);
    settle(w);
    CHECK(water_total(w) == total && world_water(w, hx - 20, s - 2) == WATER_FULL);

    /* Water falls, and runs over an edge: take a wall away and the tank
     * empties out across the ground, every eighth of it still somewhere. */
    for (int y = top; y < s; y++) world_set(w, right + 1, y, TILE_AIR);
    int ran = settle(w);
    CHECK(ran > 10 && ran < 5000 && !w->water_stirring && water_total(w) == total);
    CHECK(world_water(w, right + 30, s - 1) > 0 && world_water(w, right + 5, top) == 0);
    int highest = 0;
    for (int x = 0; x < w->width; x++)
        for (int y = 0; y < s - 1; y++)
            if (world_water(w, x, y) > 0 && s - y > highest) highest = s - y;
    CHECK(highest < depth); /* lower than it stood in the tank */
    /* Nowhere is it left standing in a step two eighths or more high on level ground. */
    int steep = 0;
    for (int x = right + 2; x < right + 40; x++)
        if (abs(world_water(w, x, s - 1) - world_water(w, x + 1, s - 1)) > 1 && world_water(w, x, s - 2) == 0 &&
            world_water(w, x + 1, s - 2) == 0)
            steep++;
    CHECK(steep == 0);
    game_free(&g);

    /* Swimming. A deep tank: the player stays afloat wherever they are in it,
     * and gets about up, down and sideways. */
    game_init(&g, 128, 96, 92, WORLD_FLAT);
    w = &g.world;
    s = w->surface_y;
    for (int i = 0; i < MAX_MOBS; i++) g.mobs[i].active = false;
    clear_inventory(&g);
    int px = g.player.feet.x + 10;
    for (int y = s - 8; y < s; y++) {
        world_set(w, px - 6, y, TILE_STONE);
        world_set(w, px + 6, y, TILE_STONE);
        for (int x = px - 5; x <= px + 5; x++) world_set_water(w, x, y, WATER_FULL);
    }
    settle(w);
    Vitals *v = &g.vitals;
    g.tick_rate = 0; /* breath is counted out by hand below */
    g.player.feet = g.player.step_to = (TilePos){ px, s - 4 };
    g.player.climbing = true;
    run(&g, 1.0f);
    CHECK(g.player.feet.y == s - 4 && g.player.feet.x == px && game_player_swimming(&g) && game_player_underwater(&g));
    CHECK(game_click_tile(&g, px + 3, s - 6));
    run(&g, 3.0f);
    CHECK(g.player.feet.x == px + 3 && (g.player.feet.y == s - 6 || g.player.feet.y == s - 5) && game_player_swimming(&g));
    CHECK(game_click_tile(&g, px - 3, s - 2));
    run(&g, 3.0f);
    CHECK(g.player.feet.x == px - 3 && game_player_swimming(&g));
    /* Standing on the bottom is standing, not swimming - but still under water. */
    g.player.feet = g.player.step_to = (TilePos){ px, s - 1 };
    run(&g, 0.5f);
    CHECK(!game_player_swimming(&g) && game_player_underwater(&g) && g.player.feet.y == s - 1);

    /* Under water, breath runs out; with none left, health goes. */
    const RulesDef *u = &content.rules;
    CHECK(u->oxygen_ticks > 0 && v->oxygen == 1.0f);
    for (int i = 0; i < u->oxygen_ticks / 2; i++) game_tick(&g);
    CHECK(near(v->oxygen, 0.5f) && v->health == 1.0f);
    /* Kelp gives some back, even on a full stomach. */
    ItemId kelp = item_id("kelp");
    inventory_add(&g.inventory, kelp, 3);
    inventory_select(&g.inventory, 0, -1);
    CHECK(item_def(kelp)->food_oxygen > 0.0f);
    v->saturation = 100.0f;
    float before = v->oxygen;
    CHECK(game_can_eat(&g, kelp) && game_eat(&g) && near(v->oxygen, before + item_def(kelp)->food_oxygen));
    v->oxygen = 1.0f;
    CHECK(!game_can_eat(&g, kelp)); /* with breath to spare it is only food */
    v->saturation = 0.0f;
    for (int i = 0; i < u->oxygen_ticks + 50; i++) game_tick(&g);
    CHECK(v->oxygen == 0.0f && v->health < 1.0f && v->health > 0.5f);
    /* Head out of the water, it comes back. */
    g.player.feet = g.player.step_to = (TilePos){ px, s - 8 };
    g.player.climbing = true;
    CHECK(!game_player_underwater(&g) && game_player_swimming(&g));
    float hurt = v->health;
    for (int i = 0; i < u->oxygen_refill_ticks; i++) game_tick(&g);
    CHECK(game_bar_full(v->oxygen) && v->health >= hurt);
    /* Afloat at the surface the player can get out onto the side. */
    CHECK(game_click_tile(&g, px + 6, s - 9));
    run(&g, 4.0f);
    CHECK(g.player.feet.x == px + 6 && g.player.feet.y == s - 9 && !game_player_swimming(&g));

    /* Things dropped in water float. One let go at the bottom comes up to the
     * top of the water; one dropped from above lands on it and goes no lower. */
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    g.player.feet = g.player.step_to = (TilePos){ px + 6, s - 9 }; /* out on the side, dry */
    g.player.climbing = false;
    CHECK(content.rules.drop_float_speed > 0.0f);
    game_drop_item(&g, ITEM_STICK, px - 2, s - 1);  /* on the bottom */
    game_drop_item(&g, ITEM_STONE, px + 2, s - 14); /* in the air over the water */
    game_drop_item(&g, ITEM_WOOD, px - 9, s - 14);  /* over dry land */
    run(&g, 0.2f);
    int surface = s - 8; /* the top square of the tank's water */
    for (int i = 0; i < MAX_DROPS; i++)
        if (g.drops[i].active && g.drops[i].item == ITEM_STICK) CHECK(g.drops[i].y < (float)(s - 1) && g.drops[i].y > (float)surface);
    run(&g, 5.0f);
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!g.drops[i].active) continue;
        if (g.drops[i].item == ITEM_WOOD) CHECK(g.drops[i].y == (float)(s - 1)); /* on the ground, as ever */
        else CHECK(g.drops[i].y == (float)surface && world_is_water(w, g.drops[i].x, surface));
    }
    CHECK(count_drops(&g, ITEM_STICK) == 1 && count_drops(&g, ITEM_STONE) == 1 && count_drops(&g, ITEM_WOOD) == 1);
    /* Let the water out from under one and it goes down with it. */
    for (int y = s - 8; y < s; y++) world_set_water(w, px - 2, y, 0), world_set_water(w, px - 3, y, 0), world_set_water(w, px - 1, y, 0);
    world_set(w, px - 3, s - 1, TILE_STONE);
    world_set(w, px - 1, s - 1, TILE_STONE);
    run(&g, 3.0f);
    for (int i = 0; i < MAX_DROPS; i++)
        if (g.drops[i].active && g.drops[i].item == ITEM_STICK) CHECK(g.drops[i].y > (float)surface);
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    for (int y = s - 8; y < s; y++)
        for (int x = px - 5; x <= px + 5; x++) world_set_water(w, x, y, world_is_solid(w, x, y) ? 0 : WATER_FULL);
    world_set(w, px - 3, s - 1, TILE_AIR);
    world_set(w, px - 1, s - 1, TILE_AIR);
    for (int y = s - 8; y < s; y++)
        for (int x = px - 5; x <= px + 5; x++) world_set_water(w, x, y, WATER_FULL);
    settle(w);

    /* The water is saved as it stands, moving or not. */
    world_set(w, px, s, TILE_AIR);
    water_step(w);
    Game loaded;
    CHECK(save_game(&g, "build/test_water.world") && load_game(&loaded, "build/test_water.world"));
    CHECK(water_total(&loaded.world) == water_total(w) && loaded.world.water_stirring == w->water_stirring);
    int differ = 0;
    for (int y = 0; y < w->height; y++)
        for (int x = 0; x < w->width; x++) differ += world_water(w, x, y) != world_water(&loaded.world, x, y);
    CHECK(differ == 0 && near(loaded.vitals.oxygen, v->oxygen));
    settle(&loaded.world);
    CHECK(world_water(&loaded.world, px, s) == WATER_FULL);
    game_free(&loaded);
    remove("build/test_water.world");
    game_free(&g);

    /* A generated world's seas are full and at rest, and stay so however long they are left. */
    World t;
    CHECK(world_init(&t, 2048, 401, 3, WORLD_TERRAIN));
    long sea = water_total(&t);
    CHECK(sea > 1000 && !t.water_stirring);
    for (int x = 0; x < t.width; x++) water_wake(&t, x);
    int stirred = 0;
    for (int i = 0; i < 50; i++) stirred += water_step(&t);
    CHECK(stirred == 0 && water_total(&t) == sea && !t.water_stirring);
    world_free(&t);
}

static void test_basket_pickup(void)
{
    Game g;
    game_init(&g, 64, 48, 101, WORLD_FLAT);
    Inventory *inv = &g.inventory;
    clear_inventory(&g);
    for (int i = 0; i < MAX_MOBS; i++) g.mobs[i].active = false;
    g.tick_rate = 0;

    /* A full bar: a basket (itself full) in slot 0, seven different things after it. */
    const char *things[] = { "dirt", "stone", "wood", "stick", "flint", "coal", "sand" };
    inventory_add(inv, ITEM_BASKET, 1);
    const char *inside[] = { "apple", "cherry", "acorn", "kelp" };
    for (int i = 0; i < 4; i++) {
        inventory_add(inv, item_id(inside[i]), 1);
        CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 1, 0 }, (SlotRef){ SLOT_BASKET, 0, i }, MAX_STACK));
    }
    for (int i = 0; i < 7; i++) inventory_add(inv, item_id(things[i]), 10 + i);
    for (int i = 0; i < INVENTORY_SLOTS; i++) CHECK(inv->slots[i].count > 0);
    CHECK(inventory_add(inv, item_id("feather"), 1) == 1); /* full: nothing else goes in */
    inventory_select(inv, 3, -1); /* holding the wood */
    CHECK(inventory_selected(inv)->id == ITEM_WOOD);

    /* A basket picked up now takes the first slot that is not a basket's -
     * slot 1, the dirt - and the dirt goes inside it, chosen. */
    CHECK(inventory_add(inv, ITEM_BASKET, 1) == 0);
    CHECK(inv->slots[1].id == ITEM_BASKET && inv->slots[1].count == 1 && inv->slots[1].container != 0);
    const ItemStack *moved = inventory_slot(inv, (SlotRef){ SLOT_BASKET, 1, 0 });
    CHECK(moved && moved->id == ITEM_DIRT && moved->count == 10);
    CHECK(inventory_count(inv, ITEM_DIRT) == 10 && inventory_basket_choice(inv, 1) == 0);
    /* What was selected is still selected: that slot was not the one taken. */
    CHECK(inv->sel_slot == 3 && inv->sel_inner == -1 && inventory_selected(inv)->id == ITEM_WOOD);
    /* But the basket's own choice is the dirt: choosing its slot chooses that. */
    inventory_select_slot(inv, 1);
    CHECK(inventory_selected(inv)->id == ITEM_DIRT && inventory_selection_inside(inv, 1));
    inventory_select_slot(inv, 3);
    CHECK(inventory_selected(inv)->id == ITEM_WOOD);
    /* The basket that was already there is untouched, and has no choice made. */
    CHECK(inventory_basket_choice(inv, 0) == -1);
    inventory_select_slot(inv, 0);
    CHECK(inventory_selected(inv)->id == ITEM_BASKET);

    /* If the slot taken was the selected one, its item is still what is selected. */
    inventory_select(inv, 2, -1);
    CHECK(inventory_selected(inv)->id == ITEM_STONE);
    /* (The new basket in slot 1 has three free slots, which fill first.) */
    const char *fill[] = { "feather", "carrot", "coral", "flax", "orange", "cactus" };
    for (int i = 0; i < 3; i++) CHECK(inventory_add(inv, item_id(fill[i]), 1) == 0);
    CHECK(inventory_add(inv, ITEM_BASKET, 1) == 0);
    CHECK(inv->slots[2].id == ITEM_BASKET && inv->sel_slot == 2 && inventory_selection_inside(inv, 2));
    CHECK(inventory_selected(inv)->id == ITEM_STONE && inventory_selected(inv)->count == 11);

    /* A basket lying in the world is picked up that way too, by walking onto it. */
    int px = g.player.feet.x, ground = g.player.feet.y;
    for (int i = 3; i < 6; i++) CHECK(inventory_add(inv, item_id(fill[i]), 1) == 0);
    game_drop_item(&g, ITEM_BASKET, px + 3, ground);
    inventory_select(inv, 7, -1);
    CHECK(game_click_tile(&g, px + 3, ground));
    run(&g, 3.0f);
    CHECK(count_drops(&g, ITEM_BASKET) == 0 && inv->slots[3].id == ITEM_BASKET && inventory_count(inv, ITEM_WOOD) == 12);

    /* With nothing in the bar but baskets, and those full, there is no room for another. */
    clear_inventory(&g);
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        inventory_add(inv, ITEM_BASKET, 1);
        for (int j = 0; j < 4; j++) {
            ItemStack *in = inventory_slot(inv, (SlotRef){ SLOT_BASKET, i, j });
            if (in) *in = (ItemStack){ (uint16_t)ITEM_DIRT, 1, 0 };
        }
    }
    CHECK(inventory_add(inv, ITEM_BASKET, 1) == 1);

    /* The choices are kept by a save. */
    clear_inventory(&g);
    inventory_add(inv, ITEM_BASKET, 1);
    inventory_add(inv, ITEM_DIRT, 5);
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 1, 0 }, (SlotRef){ SLOT_BASKET, 0, 2 }, MAX_STACK));
    inventory_select(inv, 0, 2);
    inventory_select(inv, 4, -1);
    Game loaded;
    CHECK(save_game(&g, "build/test_choice.world") && load_game(&loaded, "build/test_choice.world"));
    CHECK(inventory_basket_choice(&loaded.inventory, 0) == 2 && inventory_basket_choice(&loaded.inventory, 1) == -1);
    game_free(&loaded);
    remove("build/test_choice.world");
    game_free(&g);
}

static void test_remote_benches_and_teleport(void)
{
    Game g;
    game_init(&g, 512, 64, 102, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    clear_inventory(&g);
    for (int i = 0; i < MAX_MOBS; i++) g.mobs[i].active = false;
    g.tick_rate = 0;
    int px = g.player.feet.x, ground = g.player.feet.y, bx = px + 20;
    world_set_at(w, BG, bx, ground, TILE_WORKBENCH);
    const Recipe *basket = find_recipe(TILE_WORKBENCH, "woodwork_bench"); /* 1 wood each */
    ItemId made = item_id("woodwork_bench");
    CHECK(basket != NULL);

    /* A bench's menu opens from wherever the player is: no walking. */
    CHECK(game_open_bench(&g, bx, ground) && g.bench_open && g.bench_pos.x == bx && !game_at_bench(&g));
    run(&g, 1.0f);
    CHECK(g.player.feet.x == px && is_idle(&g) && g.bench_open);
    /* Crafting from there is queued: the player walks to the bench, and it is made on arriving. */
    inventory_add(inv, ITEM_WOOD, 30);
    CHECK(game_craft(&g, basket, 2));
    CHECK(count_item(&g, made) == 0 && g.craft_remaining == 0 && !is_idle(&g));
    run(&g, 0.3f);
    CHECK(count_item(&g, made) == 0 && inventory_count(inv, ITEM_WOOD) == 30);
    run(&g, 6.0f);
    CHECK(game_at_bench(&g) && abs(g.player.feet.x - bx) <= 2 && count_item(&g, made) == 2);
    CHECK(inventory_count(inv, ITEM_WOOD) == 28 && g.bench_open);
    /* Standing at it, the next lot starts at once. */
    CHECK(game_craft(&g, basket, 1) && g.craft_remaining == 1 && is_idle(&g));
    run(&g, 1.0f);
    CHECK(count_item(&g, made) == 3);

    /* Upgrading from afar is walked to as well. */
    g.player.feet = g.player.step_to = (TilePos){ px, ground };
    inventory_add(inv, ITEM_STONE, 5);
    CHECK(game_open_bench(&g, bx, ground) && game_upgrade_bench(&g));
    CHECK(world_get_at(w, BG, bx, ground) == TILE_WORKBENCH && !is_idle(&g));
    run(&g, 6.0f);
    CHECK(world_get_at(w, BG, bx, ground) == TILE_WORKBENCH_2 && inventory_count(inv, ITEM_STONE) == 0);
    /* What cannot be reached is refused, not left hanging. */
    game_close_bench(&g);

    /* So is buying at a trade portal. */
    clear_inventory(&g);
    int tx = px - 20;
    world_set_at(w, BG, tx, ground, tile_id("trade_portal"));
    world_set_at(w, BG, tx, ground - 1, tile_id("trade_portal"));
    g.player.feet = g.player.step_to = (TilePos){ px, ground };
    inventory_add(inv, item_id("copper_coin"), 10);
    CHECK(game_open_bench(&g, tx, ground) && game_trade_open(&g));
    CHECK(game_trade_buy(&g, ITEM_DIRT, 3) && inventory_count(inv, ITEM_DIRT) == 0 && !is_idle(&g));
    run(&g, 6.0f);
    CHECK(inventory_count(inv, ITEM_DIRT) == 3 && inventory_count(inv, item_id("copper_coin")) == 7);
    CHECK(game_trade_sell(&g, ITEM_DIRT, 1) && inventory_count(inv, ITEM_DIRT) == 2); /* there now: at once */
    game_close_bench(&g);

    /* A chest is not a bench: it still has to be walked to before it opens. */
    int cx = px + 8;
    world_set(w, cx, ground, tile_id("chest"));
    g.player.feet = g.player.step_to = (TilePos){ px, ground };
    CHECK(!game_open_bench(&g, cx, ground) && !g.bench_open);
    world_set(w, cx, ground, TILE_AIR);

    /* Teleporting. A portal's menu offers it, from Tier 1; a workbench's does not. */
    const RulesDef *u = &content.rules;
    CHECK(u->teleport_cost == 10 && u->teleport_free == 100 && u->teleport_per == 10 && u->teleport_level == 1);
    CHECK(game_open_bench(&g, bx, ground) && game_teleport_cost(&g) == -1 && !game_teleport(&g));
    /* 10 crystals up to 100 blocks, and one more for every 10 blocks beyond. */
    int portal_x = w->spawn_x;
    struct {
        int away, cost;
    } fares[] = { { 5, 10 }, { 100, 10 }, { 109, 10 }, { 110, 11 }, { 150, 15 }, { 200, 20 } };
    for (int i = 0; i < 6; i++) {
        g.player.feet = g.player.step_to = (TilePos){ world_wrap_x(w, portal_x + fares[i].away), ground };
        CHECK(game_open_bench(&g, portal_x, ground) && game_teleport_cost(&g) == fares[i].cost);
    }
    /* Not without the crystals for it. */
    g.player.feet = g.player.step_to = (TilePos){ world_wrap_x(w, portal_x + 150), ground };
    g.crystals = 14;
    CHECK(!game_teleport(&g) && g.player.feet.x == world_wrap_x(w, portal_x + 150) && g.crystals == 14);
    /* With them, the player is at the portal at once, and has paid. */
    g.crystals = 40;
    CHECK(game_click_tile(&g, g.player.feet.x + 5, ground)); /* (whatever they were doing is dropped) */
    CHECK(game_teleport(&g));
    CHECK(g.player.feet.x == portal_x && g.player.feet.y == ground && g.crystals == 25 && is_idle(&g));
    CHECK(game_at_bench(&g) && g.bench_open);
    run(&g, 1.0f);
    CHECK(g.player.feet.x == portal_x && g.player.feet.y == ground);
    /* Already there, there is nowhere to go. */
    CHECK(!game_teleport(&g) && g.crystals == 25);
    /* A portal the player made works the same. */
    int far = world_wrap_x(w, portal_x + 60);
    world_set(w, far, ground + 1, TILE_PORTAL_BASE);
    world_set_at(w, tile_def(tile_id("portal"))->layer, far, ground, tile_id("portal"));
    world_set_at(w, tile_def(tile_id("portal"))->layer, far, ground - 1, tile_id("portal"));
    CHECK(game_open_bench(&g, far, ground - 1) && game_teleport_cost(&g) == 10 && game_teleport(&g));
    CHECK(g.player.feet.x == far && g.player.feet.y == ground && g.crystals == 15);
    game_free(&g);
}

static void test_buckets_and_stacks(void)
{
    Game g;
    game_init(&g, 128, 96, 111, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    clear_inventory(&g);
    for (int i = 0; i < MAX_MOBS; i++) g.mobs[i].active = false;
    g.tick_rate = 0;
    ItemId bucket = item_id("bucket");
    int s = w->surface_y, px = g.player.feet.x;

    /* A bucket: 3 iron ingots at a tier 2 metalwork bench. It holds 8 eighths and starts empty. */
    const Recipe *r = find_recipe(tile_id("metalwork_bench_2"), "bucket");
    CHECK(bucket > 0 && r && r->level == 2 && costs(r, "iron_ingot", 3, NULL, 0));
    CHECK(item_def(bucket)->holds_water && item_def(bucket)->meter_max == 8 && item_def(bucket)->category >= 0);
    CHECK(recipe_locked(tile_id("metalwork_bench"), r) && !recipe_locked(tile_id("metalwork_bench_2"), r));
    inventory_add(inv, bucket, 1);
    inventory_select(inv, 0, -1);
    CHECK(inventory_meter(inv, &inv->slots[0]) == 0 && inv->slots[0].container == 0);
    CHECK(inventory_meter(inv, &inv->slots[1]) == -1);

    /* Two puddles in pits, each 5/8 deep. */
    int a = px + 3, b = px + 5;
    world_set(w, a, s, TILE_AIR);
    world_set(w, b, s, TILE_AIR);
    world_set_water(w, a, s, 5);
    world_set_water(w, b, s, 5);
    long total = water_total(w);
    /* Scooping one up takes just what is there: the bucket is 5/8 full. */
    CHECK(game_click_tile(&g, a, s));
    run(&g, 3.0f);
    CHECK(world_water(w, a, s) == 0 && inventory_meter(inv, &inv->slots[0]) == 5 && inv->slots[0].container != 0);
    /* The second fills it, and leaves behind the 2/8 it had no room for. */
    CHECK(game_click_tile(&g, b, s));
    run(&g, 3.0f);
    CHECK(world_water(w, b, s) == 2 && inventory_meter(inv, &inv->slots[0]) == 8);
    CHECK(water_total(w) == total - 8);
    /* Full, it is emptied where it is used - all of it, whatever is there already. */
    world_set(w, b - 1, s - 1, TILE_STONE); /* (kerbs, so what overflows the pit stays over it) */
    world_set(w, b + 1, s - 1, TILE_STONE);
    CHECK(game_click_tile(&g, b, s));
    run(&g, 3.0f);
    CHECK(inventory_meter(inv, &inv->slots[0]) == 0 && inv->slots[0].container == 0); /* empty again, it is as new */
    CHECK(water_total(w) == total && world_water(w, b, s) == 8 && world_water(w, b, s - 1) == 2);
    world_set(w, b - 1, s - 1, TILE_AIR);
    world_set(w, b + 1, s - 1, TILE_AIR);

    /* Emptied onto level ground, a bucketful makes a puddle an eighth deep
     * round where it landed - eight squares of it - and goes no further. */
    world_set_water(w, b, s - 1, 0);
    world_set_water(w, b, s, 0);
    world_set_water(w, a, s, 0);
    world_set(w, a, s, TILE_DIRT);
    world_set(w, b, s, TILE_DIRT);
    CHECK(water_total(w) == 0);
    inventory_set_meter(inv, &inv->slots[0], 8);
    int drop = g.player.feet.x + 2;
    g.tick_rate = 10;
    CHECK(game_click_tile(&g, drop, s - 1) && g.queue[g.queue_count - 1].type == ACTION_BUCKET);
    run(&g, 20.0f);
    g.tick_rate = 0;
    CHECK(inventory_meter(inv, &inv->slots[0]) == 0 && water_total(w) == 8 && !w->water_stirring);
    int wet = 0, leftmost = -1, rightmost = -1, deep = 0;
    for (int x = 0; x < w->width; x++) {
        int level = world_water(w, x, s - 1);
        if (level == 0) continue;
        wet++;
        if (leftmost < 0) leftmost = x;
        rightmost = x;
        if (level > 1) deep++;
    }
    CHECK(wet == 8 && deep == 0 && rightmost - leftmost == 7 && leftmost <= drop && drop <= rightmost);
    /* A second bucketful on top of it only widens it. */
    inventory_set_meter(inv, &inv->slots[0], 8);
    water_add(w, drop, s - 1, 8);
    inventory_set_meter(inv, &inv->slots[0], 0);
    settle(w);
    wet = 0;
    for (int x = 0; x < w->width; x++) wet += world_water(w, x, s - 1) == 1;
    CHECK(wet == 16 && water_total(w) == 16 && world_water(w, drop + 30, s - 1) == 0 && world_water(w, 0, s - 1) == 0);
    for (int x = 0; x < w->width; x++) world_set_water(w, x, s - 1, 0);
    settle(w);
    /* With nothing in it and no water there, a click is an ordinary click. */
    CHECK(game_click_tile(&g, px + 8, s - 1) && g.queue[g.queue_count - 1].type == ACTION_MOVE);
    run(&g, 3.0f);

    /* Stacks. Buckets stack whatever they hold; the one on top is the one used. */
    clear_inventory(&g);
    inventory_add(inv, bucket, 4);
    CHECK(inv->slots[0].count == 4 && inv->slots[1].count == 0);
    CHECK(inventory_set_meter(inv, &inv->slots[0], 6));
    CHECK(inventory_meter(inv, &inv->slots[0]) == 6 && inv->slots[0].count == 4);
    /* Taking some off takes them from the top down: the top two go, the full one among them. */
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 0, 0 }, (SlotRef){ SLOT_BAR, 1, 0 }, 2));
    CHECK(inv->slots[0].count == 2 && inv->slots[1].count == 2);
    CHECK(inventory_meter(inv, &inv->slots[1]) == 6 && inventory_meter(inv, &inv->slots[0]) == 0);
    /* Put the rest back on top of them and the part-full one is in the
     * middle of the stack, with an empty one on top - and that is what is used. */
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 0, 0 }, (SlotRef){ SLOT_BAR, 1, 0 }, MAX_STACK));
    CHECK(inv->slots[0].count == 0 && inv->slots[1].count == 4 && inventory_meter(inv, &inv->slots[1]) == 0);
    ItemStack off;
    CHECK(inventory_split(inv, &inv->slots[1], 2, &off) && inventory_meter(inv, &off) == 0 && off.container == 0);
    CHECK(inventory_meter(inv, &inv->slots[1]) == 6 && inv->slots[1].count == 2);
    CHECK(inventory_split(inv, &inv->slots[1], 1, &off) && inventory_meter(inv, &off) == 6);
    CHECK(inv->slots[1].count == 1 && inventory_meter(inv, &inv->slots[1]) == 0 && inv->slots[1].container == 0);
    container_free(inv, off.container);

    /* One picked up joins a stack of its kind, on top, whatever it holds. */
    clear_inventory(&g);
    inventory_add(inv, bucket, 2);
    ItemStack full = { (uint16_t)bucket, 1, 0 };
    inventory_set_meter(inv, &full, 8);
    CHECK(inventory_add_stack(inv, full) == 0 && inv->slots[0].count == 3 && inv->slots[1].count == 0);
    CHECK(inventory_meter(inv, &inv->slots[0]) == 8);
    /* Dropped and picked up again, it still holds what it held. */
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    CHECK(game_discard(&g, (SlotRef){ SLOT_BAR, 0, 0 }, 1) == 1 && inventory_meter(inv, &inv->slots[0]) == 0);
    for (int i = 0; i < MAX_DROPS; i++)
        if (g.drops[i].active) CHECK(game_click_drop(&g, i));
    run(&g, 2.0f);
    CHECK(inv->slots[0].count == 3 && inventory_meter(inv, &inv->slots[0]) == 8);
    /* Into a chest and out again, the same. */
    uint16_t box = container_alloc(inv);
    CHECK(inventory_quick_move(inv, (SlotRef){ SLOT_BAR, 0, 0 }, box, 16) == 3 && inv->slots[0].count == 0);
    CHECK(inventory_meter(inv, &inv->containers[box].slots[0]) == 8 && inv->containers[box].slots[0].count == 3);
    CHECK(inventory_quick_move(inv, (SlotRef){ SLOT_CONTAINER, box, 0 }, box, 16) == 3);
    CHECK(inv->slots[0].count == 3 && inventory_meter(inv, &inv->slots[0]) == 8);
    container_free(inv, box);
    /* And through a save. */
    Game loaded;
    CHECK(save_game(&g, "build/test_bucket.world") && load_game(&loaded, "build/test_bucket.world"));
    CHECK(loaded.inventory.slots[0].count == 3 && inventory_meter(&loaded.inventory, &loaded.inventory.slots[0]) == 8);
    ItemStack top;
    CHECK(inventory_split(&loaded.inventory, &loaded.inventory.slots[0], 1, &top));
    CHECK(inventory_meter(&loaded.inventory, &loaded.inventory.slots[0]) == 0);
    game_free(&loaded);
    remove("build/test_bucket.world");

    /* Tools stack too. One that wears is used from the top of its stack, and
     * gone when it is worn out. (None of the tools that come with the game wear.) */
    ItemId pick = item_id("stone_pickaxe");
    CHECK(item_def(pick)->max_stack > 1 && item_def(pick)->meter_max == 0);
    clear_inventory(&g);
    content.items[pick].meter_max = content.items[pick].meter_start = 3; /* as "durability": 3 would make it */
    inventory_add(inv, pick, 2);
    CHECK(inv->slots[0].count == 2 && inventory_meter(inv, &inv->slots[0]) == 3);
    inventory_wear_tool(inv, item_def(pick)->tool_class);
    inventory_wear_tool(inv, item_def(pick)->tool_class);
    CHECK(inv->slots[0].count == 2 && inventory_meter(inv, &inv->slots[0]) == 1);
    inventory_wear_tool(inv, item_def(pick)->tool_class);
    CHECK(inv->slots[0].count == 1 && inventory_meter(inv, &inv->slots[0]) == 3 && inv->slots[0].container == 0);
    /* Mining a block of its kind is what wears it. */
    world_set(w, g.player.feet.x + 1, g.player.feet.y, TILE_STONE);
    CHECK(game_queue_action(&g, ACTION_MINE, g.player.feet.x + 1, g.player.feet.y));
    run(&g, 3.0f);
    CHECK(inventory_meter(inv, &inv->slots[0]) == 2);
    content.items[pick].meter_max = content.items[pick].meter_start = 0;
    game_free(&g);

    /* Swimming goes straight to where it is going, not along and then up. */
    game_init(&g, 128, 96, 112, WORLD_FLAT);
    w = &g.world;
    s = w->surface_y;
    g.tick_rate = 0;
    for (int i = 0; i < MAX_MOBS; i++) g.mobs[i].active = false;
    clear_inventory(&g);
    int cx = g.player.feet.x + 30;
    for (int y = s - 30; y < s; y++) {
        world_set(w, cx - 21, y, TILE_STONE);
        world_set(w, cx + 21, y, TILE_STONE);
        for (int x = cx - 20; x <= cx + 20; x++) world_set_water(w, x, y, WATER_FULL);
    }
    settle(w);
    TilePos from = { cx - 12, s - 5 }, to = { cx + 12, s - 17 }; /* 24 across, 12 up */
    g.player.feet = g.player.step_to = from;
    g.player.climbing = true;
    CHECK(game_click_tile(&g, to.x, to.y));
    int steps = 0, slant = 0;
    float worst = 0.0f;
    TilePos last = from;
    for (int i = 0; i < 60 * 20 && !(is_idle(&g) && g.player.feet.x == to.x); i++) {
        game_update(&g, 1.0f / 60.0f);
        float x, y;
        game_player_visual_pos(&g, &x, &y);
        /* How far the player is from the straight line between the two places. */
        float off = fabsf((x - from.x) * (to.y - from.y) - (y - from.y) * (to.x - from.x)) /
                    sqrtf((float)((to.x - from.x) * (to.x - from.x) + (to.y - from.y) * (to.y - from.y)));
        if (off > worst) worst = off;
        if (g.player.feet.x != last.x || g.player.feet.y != last.y) {
            steps++;
            slant += g.player.feet.x != last.x && g.player.feet.y != last.y;
            last = g.player.feet;
        }
    }
    CHECK(g.player.feet.x == to.x && (g.player.feet.y == to.y || g.player.feet.y == to.y + 1));
    /* As few steps as the longer way is long, the rise spread evenly along
     * it, and never more than about a square off the line. */
    CHECK(steps <= 24 && slant >= 10 && worst < 1.2f);

    /* Round something in the way, it still gets there. */
    for (int y = s - 20; y <= s - 3; y++) world_set(w, cx, y, TILE_STONE);
    settle(w);
    g.player.feet = g.player.step_to = from;
    g.player.climbing = true;
    CHECK(game_click_tile(&g, to.x, to.y));
    run(&g, 20.0f);
    CHECK(g.player.feet.x == to.x && is_idle(&g));
    game_free(&g);
}

static void test_sea_and_combat(void)
{
    Game g;
    game_init(&g, 256, 96, 121, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    Vitals *v = &g.vitals;
    clear_inventory(&g);
    clear_mobs(&g);
    g.tick_rate = 0;
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;
    int dodo = mob_id("dodo"), bear = mob_id("dropbear"), fish = mob_id("fish"), shark = mob_id("shark");

    /* Health: dodos and fish 5, dropbears 10, sharks 25. */
    CHECK(fish >= 0 && shark >= 0);
    CHECK(content.mobs[dodo].health == 5 && content.mobs[fish].health == 5 && content.mobs[bear].health == 10 &&
          content.mobs[shark].health == 25 && content.mobs[mob_id("gold_dodo")].health == 5);
    /* Blows: a bare hand 2, flint spear 3, bronze sword 5, iron sword 10, titanium sword 20. */
    const char *weapons[] = { "flint_spear", "bronze_sword", "iron_sword", "titanium_sword" };
    int blows[] = { 3, 5, 10, 20 }, levels[] = { 1, 3, 4, 5 };
    CHECK(content.rules.hand_damage == 2 && game_attack_damage(&g) == 2);
    TileType bench = tile_id("tool_bench_5");
    for (int i = 0; i < 4; i++) {
        ItemId id = item_id(weapons[i]);
        const Recipe *r = find_recipe(bench, weapons[i]);
        CHECK(id > 0 && item_def(id)->damage == blows[i] && item_def(id)->category >= 0 && r && r->level == levels[i]);
        clear_inventory(&g);
        inventory_add(inv, id, 1);
        inventory_select(inv, 0, -1);
        CHECK(game_attack_damage(&g) == blows[i]);
    }
    /* Something that is not a weapon hits no harder than a hand. */
    clear_inventory(&g);
    inventory_add(inv, ITEM_DIRT, 1);
    inventory_select(inv, 0, -1);
    CHECK(game_attack_damage(&g) == 2);

    /* A creature takes as many blows as its health needs: a dodo three by hand, one with an iron sword. */
    clear_inventory(&g);
    int m = mob_spawn(&g, dodo, px + 1, ground, true, false);
    content.mobs[dodo].move_chance = 0.0f;
    CHECK(g.mobs[m].hp == 5 && !mob_hurt(&g, m, 2) && g.mobs[m].hp == 3 && !mob_hurt(&g, m, 2) && g.mobs[m].active);
    CHECK(mob_hurt(&g, m, 2) && !g.mobs[m].active);
    m = mob_spawn(&g, dodo, px + 1, ground, true, false);
    float blow = content.rules.attack_time;
    /* One click is one blow, and no more: the creature is left hurt, and the player idle. */
    CHECK(game_click_mob(&g, m));
    run(&g, blow * 4.0f);
    CHECK(g.mobs[m].active && g.mobs[m].hp == 3 && is_idle(&g));
    CHECK(game_click_mob(&g, m));
    run(&g, blow + 0.1f);
    CHECK(g.mobs[m].active && g.mobs[m].hp == 1 && is_idle(&g));
    /* A third finishes it. */
    CHECK(game_click_mob(&g, m));
    run(&g, blow + 0.1f);
    CHECK(!g.mobs[m].active && is_idle(&g));
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    clear_inventory(&g);
    inventory_add(inv, item_id("iron_sword"), 1);
    inventory_select(inv, 0, -1);
    m = mob_spawn(&g, dodo, px + 1, ground, true, false);
    CHECK(game_click_mob(&g, m));
    run(&g, blow + 0.1f);
    CHECK(!g.mobs[m].active);
    content.mobs[dodo].move_chance = 0.2f;
    clear_mobs(&g);

    /* A sea: 40 wide, 12 deep, in ocean country. */
    int left = px + 20, right = px + 59, top = s - 12;
    for (int y = top; y < s; y++) {
        world_set(w, left - 1, y, TILE_STONE);
        world_set(w, right + 1, y, TILE_STONE);
        for (int x = left; x <= right; x++) world_set_water(w, x, y, WATER_FULL);
    }
    int ocean = -1;
    for (int b = 0; b < content.biome_count; b++)
        if (content.biomes[b].below_sea) ocean = b;
    for (int x = left; x <= right; x++) w->biome[x] = (uint8_t)ocean;
    settle(w);
    g.player.feet = g.player.step_to = (TilePos){ left - 3, top - 1 }; /* on the bank, dry */
    world_set(w, left - 3, top, TILE_STONE);

    /* The water has a cap of its own, 10 like the land's: a full complement
     * of dodos does not keep the fish out, nor fish the dodos. */
    CHECK(content.mob_cap == 10 && content.mob_water_cap == 10);
    for (int i = 0; i < 10; i++) mob_spawn(&g, dodo, g.player.feet.x - 5 + i, ground, false, false);
    CHECK(mobs_near_player(&g) == 10 && mobs_near_player_in_water(&g) == 0);
    /* Fish turn up in it, and only in it; never more than their cap. */
    CHECK(content.mobs[fish].swims && content.mobs[fish].spawn_water && content.mobs[fish].spawn_biome == ocean);
    for (int i = 0; i < 4000; i++) {
        mobs_try_spawn(&g, left + (i * 7) % 40, top + (i * 5) % 12);
        mobs_try_spawn(&g, left - 10, ground); /* dry land */
    }
    int fishes = 0, sharks = 0, strays = 0;
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!g.mobs[i].active) continue;
        fishes += g.mobs[i].kind == fish;
        sharks += g.mobs[i].kind == shark;
        if ((g.mobs[i].kind == fish || g.mobs[i].kind == shark) && !world_is_water(w, g.mobs[i].x, g.mobs[i].y)) strays++;
    }
    CHECK(fishes == content.mobs[fish].cap && sharks <= content.mobs[shark].cap && strays == 0);
    CHECK(mobs_near_player(&g) == 10 && mobs_near_player_in_water(&g) == fishes + sharks);
    /* With the water full, no more of its own turn up - but that is its own business. */
    int kinds_cap = content.mobs[fish].cap;
    content.mobs[fish].cap = 0; /* (no limit of its own: only the water's) */
    for (int i = 0; i < 8000; i++) mobs_try_spawn(&g, left + (i * 7) % 40, top + (i * 5) % 12);
    CHECK(mobs_near_player_in_water(&g) == 10 && mobs_near_player(&g) == 10);
    content.mobs[fish].cap = kinds_cap;
    for (int i = 0; i < MAX_MOBS; i++)
        if (g.mobs[i].active && g.mobs[i].kind == dodo) g.mobs[i].active = false;
    /* And with the water full, the land still fills. */
    world_set(w, g.player.feet.x - 8, ground + 1, TILE_GRASS);
    for (int i = 0; i < 400 && mobs_near_player(&g) == 0; i++) mobs_try_spawn(&g, g.player.feet.x - 8, ground);
    CHECK(mobs_near_player(&g) == 1);
    for (int i = 0; i < MAX_MOBS; i++)
        if (g.mobs[i].active && g.mobs[i].kind == dodo) g.mobs[i].active = false;
    for (int i = 0, kept = 0; i < MAX_MOBS; i++)
        if (g.mobs[i].active && g.mobs[i].kind == fish && ++kept > kinds_cap) g.mobs[i].active = false;
    CHECK(content.mobs[shark].spawn_chance < content.mobs[fish].spawn_chance); /* sharks are the rarer */
    /* They swim about, and never leave the water. */
    int moved = 0;
    TilePos was[MAX_MOBS];
    for (int i = 0; i < MAX_MOBS; i++) was[i] = (TilePos){ g.mobs[i].x, g.mobs[i].y };
    for (int t = 0; t < 600; t++) {
        mobs_tick(&g);
        for (int i = 0; i < MAX_MOBS; i++)
            if (g.mobs[i].active && content.mobs[g.mobs[i].kind].swims && !world_is_water(w, g.mobs[i].x, g.mobs[i].y)) strays++;
    }
    for (int i = 0; i < MAX_MOBS; i++) moved += g.mobs[i].active && (g.mobs[i].x != was[i].x || g.mobs[i].y != was[i].y);
    CHECK(strays == 0 && moved >= 3);
    /* A fish leaves a fish, which a campfire cooks. */
    clear_mobs(&g);
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    mob_kill(&g, mob_spawn(&g, fish, left + 5, top + 5, true, false));
    CHECK(count_drops(&g, item_id("fish")) == 1);
    CHECK(costs(find_recipe(tile_id("campfire"), "cooked_fish"), "fish", 1, NULL, 0));
    CHECK(game_is_food(item_id("fish")) && item_def(item_id("cooked_fish"))->food > item_def(item_id("fish"))->food);

    /* Sharks are 1 block long or 2, and no longer. */
    const MobDef *sd = &content.mobs[shark];
    CHECK(sd->hunts && sd->size_min == 1 && sd->size_max == 2 && sd->bite > 0.0f);
    bool seen[9] = { false };
    for (int i = 0; i < 400; i++) {
        int k = mob_spawn(&g, shark, left + 20, top + 6, true, false);
        CHECK(g.mobs[k].size >= 1 && g.mobs[k].size <= 2 && g.mobs[k].hp == 25);
        seen[g.mobs[k].size] = true;
        g.mobs[k].active = false;
    }
    CHECK(seen[1] && seen[2]);
    for (int n = 3; n <= 8; n++) CHECK(!seen[n]);

    /* A shark leaves the player alone on the bank... */
    clear_mobs(&g);
    int k = mob_spawn(&g, shark, left + 12, top + 4, true, false);
    g.mobs[k].size = 2;
    v->health = 1.0f;
    for (int t = 0; t < 300; t++) mobs_tick(&g);
    CHECK(v->health == 1.0f && world_is_water(w, g.mobs[k].x, g.mobs[k].y));
    /* ...but goes for a swimmer, bites, makes off, and comes round again. */
    g.player.feet = g.player.step_to = (TilePos){ left + 4, top + 5 };
    g.player.climbing = true;
    g.mobs[k].x = g.mobs[k].to_x = left + 14;
    g.mobs[k].y = g.mobs[k].to_y = top + 5;
    g.mobs[k].step_left = 0;
    CHECK(game_player_in_water(&g));
    int bites = 0, furthest_after = 0;
    float last_health = v->health;
    for (int t = 0; t < 400 && bites < 2; t++) {
        mobs_tick(&g);
        if (v->health < last_health) {
            bites++;
            last_health = v->health;
            CHECK(abs(world_dx(w, g.mobs[k].x, g.player.feet.x)) <= 1);
        } else if (bites == 1) {
            int away = abs(world_dx(w, g.mobs[k].x, g.player.feet.x)) + abs(g.mobs[k].y - g.player.feet.y);
            if (away > furthest_after) furthest_after = away;
        }
    }
    CHECK(bites == 2 && furthest_after >= 4 && near(v->health, 1.0f - 2.0f * sd->bite));
    /* It wakes anyone it bites. (Nobody sleeps afloat, but a bite is a bite.) */
    v->rest = REST_MEDITATE;
    g.mobs[k].timer = 0;
    g.mobs[k].x = g.mobs[k].to_x = g.player.feet.x + 1;
    g.mobs[k].y = g.mobs[k].to_y = g.player.feet.y;
    g.mobs[k].step_left = 0;
    mobs_tick(&g);
    CHECK(v->rest == REST_NONE);

    /* Killed, it leaves 3 or 4 fish and its head - a head the size it was. */
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    clear_inventory(&g);
    ItemId head = item_id("shark_head");
    CHECK(head > 0 && item_def(head)->sized && item_def(head)->meter_max == 8 && item_def(head)->max_stack > 1);
    g.player.feet = g.player.step_to = (TilePos){ left - 3, top - 1 };
    g.player.climbing = false;
    CHECK(!mob_hurt(&g, k, 24) && mob_hurt(&g, k, 1));
    int meat = count_drops(&g, item_id("fish"));
    CHECK((meat == 3 || meat == 4) && count_drops(&g, head) == 1);
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!g.drops[i].active || g.drops[i].item != head) continue;
        ItemStack dropped = { (uint16_t)head, 1, g.drops[i].container };
        CHECK(inventory_meter(inv, &dropped) == 2);
    }
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;

    /* Heads stack whatever their size, and it is the top one that is put down - as a head of that size. */
    clear_inventory(&g);
    for (int size = 2; size <= 7; size += 5) {
        ItemStack one = { (uint16_t)head, 1, 0 };
        inventory_set_meter(inv, &one, size);
        CHECK(inventory_add_stack(inv, one) == 0);
    }
    CHECK(inv->slots[0].count == 2 && inv->slots[1].count == 0 && inventory_meter(inv, &inv->slots[0]) == 7);
    inventory_select(inv, 0, -1);
    int hx = g.player.feet.x - 2, hy = g.player.feet.y;
    world_set(w, hx, hy + 1, TILE_STONE);
    world_set(w, hx + 1, hy + 1, TILE_STONE);
    CHECK(game_click_tile(&g, hx, hy));
    run(&g, 3.0f);
    CHECK(world_get_at(w, BG, hx, hy) == tile_id("shark_head_7") && inv->slots[0].count == 1);
    CHECK(inventory_meter(inv, &inv->slots[0]) == 2);
    CHECK(game_click_tile(&g, hx + 1, hy));
    run(&g, 3.0f);
    CHECK(world_get_at(w, BG, hx + 1, hy) == tile_id("shark_head_2") && inv->slots[0].count == 0);
    /* Taken down again, each comes back the size it was. */
    CHECK(game_queue_action(&g, ACTION_MINE, hx, hy));
    run(&g, 3.0f);
    CHECK(world_get_at(w, BG, hx, hy) == TILE_AIR && count_item(&g, head) == 1);
    CHECK(inventory_meter(inv, inventory_find(inv, head)) == 7);
    for (int n = 1; n <= 8; n++) {
        char name[32];
        snprintf(name, sizeof name, "shark_head_%d", n);
        CHECK(tile_id(name) > 0 && tile_def(tile_id(name))->drop == head && tile_def(tile_id(name))->drop_meter == n);
        CHECK(item_def(head)->places_sized[n - 1] == tile_id(name));
    }

    /* A creature from a save made before creatures had health is as good as new. */
    k = mob_spawn(&g, bear, px, ground, true, false);
    g.mobs[k].hp = 0;
    g.mobs[k].state = MOB_SEEK;
    mobs_tick(&g);
    CHECK(g.mobs[k].hp == 10);
    game_free(&g);
}

static void test_characters(void)
{
    Game g;
    game_init(&g, 256, 64, 131, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    clear_inventory(&g);
    clear_mobs(&g);
    g.tick_rate = 0;
    int ground = g.player.feet.y;
    ItemId apple = item_id("apple"), orange = item_id("orange");

    /* The recipe: at a portal, for 100 crystals and 5 fruit - apples, oranges
     * and cherries in a mix that is this world's own. */
    const Recipe *r = find_recipe(TILE_SPAWN_PORTAL, "new_player");
    CHECK(r && r->crystals == 100 && r->level == 1 && item_def(r->output.id)->becomes_player);
    CHECK(r->random_total == 5 && r->random_item_count == 3 && content.rules.max_players == 2);
    int total = 0;
    for (int i = 0; i < r->input_count; i++) {
        total += r->inputs[i].count;
        CHECK(r->inputs[i].id == apple || r->inputs[i].id == orange || r->inputs[i].id == ITEM_CHERRY);
        CHECK(r->inputs[i].count >= 1);
    }
    CHECK(total == 5 && r->input_count >= 1);
    /* Not the same in every world, but always the same in one. */
    RecipeItem mine[RECIPE_MAX_INPUTS];
    int mine_count = r->input_count;
    memcpy(mine, r->inputs, sizeof mine);
    bool differs = false;
    Game other;
    for (uint32_t seed = 1; seed <= 8 && !differs; seed++) {
        game_init(&other, 64, 48, seed, WORLD_FLAT);
        differs = r->input_count != mine_count || memcmp(mine, r->inputs, sizeof(RecipeItem) * (size_t)mine_count) != 0;
        game_free(&other);
    }
    CHECK(differs);
    game_roll_recipes(&g);
    CHECK(r->input_count == mine_count && memcmp(mine, r->inputs, sizeof(RecipeItem) * (size_t)mine_count) == 0);

    /* Making one. It is not an item: there is simply another character, standing at the portal. */
    CHECK(g.character_count == 1 && g.controlled == 0);
    for (int i = 0; i < r->input_count; i++) inventory_add(inv, r->inputs[i].id, r->inputs[i].count);
    g.crystals = 99;
    CHECK(game_open_bench(&g, w->spawn_x, ground) && !game_craft(&g, r, 1)); /* a crystal short */
    g.crystals = 250;
    CHECK(game_craft(&g, r, 5)); /* (however many are asked for, it is one) */
    run(&g, 1.0f);
    CHECK(g.character_count == 2 && g.crystals == 150 && g.controlled == 0 && g.live == 0);
    CHECK(inventory_count(inv, apple) + inventory_count(inv, orange) + inventory_count(inv, ITEM_CHERRY) == 0);
    CHECK(inventory_count(inv, item_id("new_player")) == 0 && count_drops(&g, item_id("new_player")) == 0);
    /* For now, one is all there can be. */
    for (int i = 0; i < r->input_count; i++) inventory_add(inv, r->inputs[i].id, r->inputs[i].count);
    CHECK(!game_craft(&g, r, 1));
    run(&g, 1.0f);
    CHECK(g.character_count == 2 && g.crystals == 150);
    game_close_bench(&g);
    clear_inventory(&g);

    /* The new one has nothing, wants for nothing, and stands by the portal. */
    inventory_add(inv, ITEM_STONE, 7);
    game_control(&g, 1);
    CHECK(g.controlled == 1 && g.live == 1);
    CHECK(abs(g.player.feet.x - w->spawn_x) <= 2 && g.player.feet.y == ground);
    CHECK(g.vitals.health == 1.0f && g.vitals.hunger == 1.0f && g.vitals.energy == 1.0f && g.vitals.oxygen == 1.0f);
    for (int i = 0; i < INVENTORY_SLOTS; i++) CHECK(inv->slots[i].count == 0);
    /* What each carries is their own. */
    inventory_add(inv, ITEM_WOOD, 3);
    inventory_add(inv, ITEM_BASKET, 1);
    inventory_add(inv, ITEM_DIRT, 4);
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 2, 0 }, (SlotRef){ SLOT_BASKET, 1, 0 }, MAX_STACK));
    game_control(&g, 0);
    CHECK(inventory_count(inv, ITEM_STONE) == 7 && inventory_count(inv, ITEM_WOOD) == 0 && inventory_count(inv, ITEM_DIRT) == 0);
    game_control(&g, 1);
    CHECK(inventory_count(inv, ITEM_STONE) == 0 && inventory_count(inv, ITEM_WOOD) == 3 && inventory_count(inv, ITEM_DIRT) == 4);

    /* Both can be given things to do, and both get on with them at once. */
    int start1 = g.player.feet.x;
    inventory_select(inv, 6, -1); /* (an empty hand each, so that a click is a walk) */
    CHECK(game_click_tile(&g, start1 + 12, ground));
    game_control(&g, 0);
    int start0 = g.player.feet.x;
    inventory_select(inv, 6, -1);
    CHECK(is_idle(&g) && game_click_tile(&g, start0 - 12, ground));
    run(&g, 0.5f);
    /* Part-way there, both of them. */
    CHECK(g.player.feet.x < start0 && g.player.feet.x > start0 - 12);
    game_control(&g, 1);
    CHECK(g.player.feet.x > start1 && g.player.feet.x < start1 + 12 && !is_idle(&g));
    run(&g, 5.0f);
    CHECK(g.player.feet.x == start1 + 12 && is_idle(&g));
    game_control(&g, 0);
    CHECK(g.player.feet.x == start0 - 12 && is_idle(&g));

    /* The one not being played still picks up what they walk over, into their own bar. */
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    game_control(&g, 1);
    int at1 = g.player.feet.x;
    game_drop_item(&g, ITEM_STICK, at1 + 5, ground);
    CHECK(game_click_tile(&g, at1 + 6, ground));
    game_control(&g, 0);
    run(&g, 4.0f);
    CHECK(count_drops(&g, ITEM_STICK) == 0 && inventory_count(inv, ITEM_STICK) == 0);
    game_control(&g, 1);
    CHECK(inventory_count(inv, ITEM_STICK) == 1 && g.player.feet.x == at1 + 6);

    /* Each has needs of their own. Both get hungry; eating feeds the one who eats. */
    game_control(&g, 0);
    for (int i = 0; i < 360; i++) game_tick(&g);
    CHECK(near(g.vitals.hunger, 0.9f) && near(game_character_vitals(&g, 1)->hunger, 0.9f));
    inventory_add(inv, apple, 1);
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (inv->slots[i].id == apple) inventory_select(inv, i, -1);
    g.vitals.hunger = 0.5f;
    CHECK(game_eat(&g) && g.vitals.hunger > 0.6f && near(game_character_vitals(&g, 1)->hunger, 0.9f));
    CHECK(game_character_vitals(&g, 0) == &g.vitals);

    /* Time only runs on when both are at rest. */
    g.vitals.energy = 0.5f;
    CHECK(game_rest(&g, REST_SLEEP) && game_tick_speed(&g) == g.tick_rate);
    game_control(&g, 1);
    CHECK(g.vitals.rest == REST_NONE && game_rest(&g, REST_MEDITATE));
    CHECK(game_tick_speed(&g) == 3 * g.tick_rate);
    game_control(&g, 0);
    CHECK(g.vitals.rest == REST_SLEEP && game_tick_speed(&g) == 3 * g.tick_rate);
    /* Meditating goes on for the one who is not being played, and brings what it brings. */
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    for (int i = 0; i < 950; i++) game_tick(&g);
    int brought = 0;
    for (int i = 0; i < MAX_DROPS; i++) brought += g.drops[i].active;
    CHECK(brought == 1 && game_character_vitals(&g, 1)->rest == REST_MEDITATE);
    game_wake(&g);
    game_control(&g, 1);
    game_wake(&g);
    CHECK(game_tick_speed(&g) == g.tick_rate);
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;

    /* A creature goes for whichever of them is nearest: here, the one not being played. */
    int tx = g.player.feet.x + 6;
    grow_tree(w, tx, "oak");
    int bear = mob_spawn(&g, mob_id("dropbear"), tx, tree_top(w, tx), true, false);
    g.mobs[bear].state = MOB_PERCH;
    g.mobs[bear].home_x = tx;
    g.player.feet = g.player.step_to = (TilePos){ tx - 1, ground }; /* character 1, under the tree */
    g.vitals.health = 1.0f;
    game_control(&g, 0);
    g.vitals.health = 1.0f;
    CHECK(abs(world_dx(w, g.player.feet.x, tx)) > 20);
    for (int i = 0; i < 300 && game_character_vitals(&g, 1)->health == 1.0f; i++) game_tick(&g);
    CHECK(game_character_vitals(&g, 1)->health < 1.0f && g.vitals.health == 1.0f && g.live == 0);
    clear_mobs(&g);

    /* A save keeps both: where they are, what they carry, how they are, and who was being played. */
    game_control(&g, 1);
    TilePos one = g.player.feet;
    game_control(&g, 0);
    TilePos zero = g.player.feet;
    game_control(&g, 1);
    Game loaded;
    CHECK(save_game(&g, "build/test_people.world") && g.controlled == 1 && g.live == 1);
    CHECK(load_game(&loaded, "build/test_people.world"));
    CHECK(loaded.character_count == 2 && loaded.controlled == 1 && loaded.live == 1);
    CHECK(loaded.player.feet.x == one.x && loaded.player.feet.y == one.y);
    CHECK(inventory_count(&loaded.inventory, ITEM_WOOD) == 3 && inventory_count(&loaded.inventory, ITEM_DIRT) == 4);
    CHECK(inventory_count(&loaded.inventory, ITEM_STICK) == 1 && inventory_count(&loaded.inventory, ITEM_STONE) == 0);
    CHECK(near(loaded.vitals.health, g.vitals.health));
    game_control(&loaded, 0);
    CHECK(loaded.player.feet.x == zero.x && inventory_count(&loaded.inventory, ITEM_STONE) == 7);
    CHECK(inventory_count(&loaded.inventory, ITEM_WOOD) == 0);
    /* And they can still both be played. */
    CHECK(game_click_tile(&loaded, zero.x + 3, ground));
    run(&loaded, 3.0f);
    CHECK(loaded.player.feet.x == zero.x + 3);
    game_free(&loaded);
    remove("build/test_people.world");
    game_free(&g);
}

static void test_fluid_creatures(void)
{
    Game g;
    game_init(&g, 256, 96, 141, WORLD_FLAT);
    World *w = &g.world;
    clear_mobs(&g);
    g.tick_rate = 0;
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;

    /* A fish keeps swimming: it is on the move nearly all the time, not a block and then a wait. */
    int left = px + 20, right = px + 59, top = s - 12;
    for (int y = top; y < s; y++) {
        world_set(w, left - 1, y, TILE_STONE);
        world_set(w, right + 1, y, TILE_STONE);
        for (int x = left; x <= right; x++) world_set_water(w, x, y, WATER_FULL);
    }
    settle(w);
    int fish = mob_spawn(&g, mob_id("fish"), left + 20, top + 6, true, false);
    int moving = 0, squares = 0, ticks = 600;
    TilePos last = { g.mobs[fish].x, g.mobs[fish].y };
    for (int t = 0; t < ticks; t++) {
        mobs_tick(&g);
        moving += g.mobs[fish].step_left > 0;
        if (g.mobs[fish].x != last.x || g.mobs[fish].y != last.y) squares++, last = (TilePos){ g.mobs[fish].x, g.mobs[fish].y };
        CHECK(world_is_water(w, g.mobs[fish].x, g.mobs[fish].y));
    }
    CHECK(moving * 100 >= ticks * 85);
    /* One square follows straight on from the last: as many squares as the time allows. */
    CHECK(squares * content.mobs[mob_id("fish")].step_ticks * 100 >= ticks * 80);
    /* Between squares it is drawn part-way, moving on every tick. */
    float x0, y0, x1, y1;
    while (g.mobs[fish].step_left != content.mobs[mob_id("fish")].step_ticks) mobs_tick(&g);
    g.tick_timer = 0.0f;
    game_mob_pos(&g, fish, &x0, &y0);
    g.tick_timer = 0.5f;
    game_mob_pos(&g, fish, &x1, &y1);
    CHECK(x0 != x1 || y0 != y1);
    clear_mobs(&g);

    /* A dropbear lets go and drops the whole height of its tree in one fall, onto the player. */
    int tx = px - 20;
    grow_tree(w, tx, "oak");
    int up = tree_top(w, tx), kind = mob_id("dropbear");
    const MobDef *def = &content.mobs[kind];
    int bear = mob_spawn(&g, kind, tx, up, true, false);
    Mob *m = &g.mobs[bear];
    m->state = MOB_PERCH;
    m->home_x = tx;
    g.player.feet = g.player.step_to = (TilePos){ tx + 2, ground };
    g.vitals.health = 1.0f;
    mobs_tick(&g);
    CHECK(m->state == MOB_HUNT && m->step_left > 0 && m->y == up);
    CHECK(m->to_y == ground && m->to_x == tx + 2 && m->to_y - m->y >= 4); /* one step, top to bottom, landing by the player */
    /* Part-way through it is part-way down: falling, and gathering speed. */
    float fx, quarter, half;
    int total = m->step_total;
    g.tick_timer = 0.0f;
    m->step_left = total - total / 4;
    game_mob_pos(&g, bear, &fx, &quarter);
    m->step_left = total - total / 2;
    game_mob_pos(&g, bear, &fx, &half);
    CHECK(quarter > (float)up && half > quarter && half < (float)ground);
    CHECK(half - quarter > quarter - (float)up);
    m->step_left = total;
    /* It lands and bites at once, with no stopping at each block on the way. */
    int fell = 0;
    while (m->y != ground && fell < 50) mobs_tick(&g), fell++;
    CHECK(fell == total && m->x == tx + 2);
    for (int i = 0; i < 3 && g.vitals.health == 1.0f; i++) mobs_tick(&g);
    CHECK(g.vitals.health < 1.0f && m->state == MOB_RETURN);
    /* Then back to the tree and up it without a pause: on the move every tick until it is at the top. */
    int idle = 0, climbed = 0;
    for (int t = 0; t < 200 && m->state != MOB_PERCH; t++) {
        mobs_tick(&g);
        if (m->state != MOB_PERCH && m->step_left == 0) idle++;
        climbed++;
    }
    CHECK(m->state == MOB_PERCH && m->x == tx && m->y == up && idle <= 2);
    /* And no slower than a block every step: across two, and up the height of the tree. */
    CHECK(climbed <= (2 + (ground - up) + 2) * def->step_ticks + 2);
    game_free(&g);
}

static void test_death(void)
{
    Game g;
    game_init(&g, 256, 64, 151, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    Vitals *v = &g.vitals;
    const RulesDef *u = &content.rules;
    clear_inventory(&g);
    clear_mobs(&g);
    g.tick_rate = 0;
    int ground = g.player.feet.y;
    CHECK(u->respawn_ticks == 1800 && u->respawn_cost == 100 && u->death_bar > 0.0f);
    CHECK(v->death == 1.0f && v->respawn == 0 && game_alive(&g) && strcmp(game_character_name(&g, 0), "Player 1") == 0);

    /* Losing all of the health bar is not dying: what goes past it comes off the death bar. */
    game_hurt(&g, 0.4f);
    CHECK(near(v->health, 0.6f) && v->death == 1.0f);
    game_hurt(&g, 0.9f);
    CHECK(v->health == 0.0f && near(v->death, 1.0f - 0.3f / u->death_bar) && game_alive(&g));
    /* Mending fills the death bar again before it does anything for health. */
    float low = v->death;
    for (int i = 0; i < 60; i++) game_tick(&g);
    CHECK(v->death > low && v->health == 0.0f);
    for (int i = 0; i < u->health_regen_ticks; i++) {
        v->hunger = 1.0f; /* (fed all the while) */
        game_tick(&g);
    }
    CHECK(v->death == 1.0f && v->health > 0.5f);

    /* With the death bar gone too, the character is dead. */
    v->hunger = v->health = 1.0f;
    inventory_add(inv, ITEM_STONE, 9);
    inventory_add(inv, ITEM_BASKET, 1);
    inventory_add(inv, ITEM_WOOD, 4);
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 2, 0 }, (SlotRef){ SLOT_BASKET, 1, 0 }, MAX_STACK));
    g.player.feet = g.player.step_to = (TilePos){ w->spawn_x + 30, ground };
    TilePos fell = g.player.feet;
    inventory_select(inv, 6, -1);
    CHECK(game_click_tile(&g, fell.x + 5, ground));
    game_hurt(&g, 1.0f + u->death_bar - 0.01f);
    CHECK(game_alive(&g) && v->death > 0.0f && v->death < 0.02f);
    game_hurt(&g, 0.05f);
    CHECK(!game_alive(&g) && game_character_dead(&g, 0) && v->respawn == 1800 && g.queue_count == 0 && !g.has_current);
    /* The dead do nothing, and nothing more can be done to them. */
    CHECK(!game_click_tile(&g, fell.x + 3, ground) && !game_rest(&g, REST_MEDITATE) && !game_eat(&g));
    game_hurt(&g, 5.0f);
    run(&g, 2.0f);
    game_set_move_input(&g, 1, 0);
    run(&g, 1.0f);
    game_set_move_input(&g, 0, 0);
    CHECK(g.player.feet.x == fell.x && v->respawn == 1800);

    /* Coming back at once costs up to 100 crystals: less the more of the wait is done. */
    CHECK(game_respawn_cost(&g, 0) == 100);
    for (int i = 0; i < 450; i++) game_tick(&g);
    CHECK(v->respawn == 1350 && game_respawn_cost(&g, 0) == 75);
    for (int i = 0; i < 450; i++) game_tick(&g);
    CHECK(game_respawn_cost(&g, 0) == 50);
    g.crystals = 49;
    CHECK(!game_respawn(&g, 0) && game_character_dead(&g, 0));
    g.crystals = 80;
    CHECK(game_respawn(&g, 0) && g.crystals == 30 && game_alive(&g));
    /* Back at the spawn portal, whole, with everything they had. */
    CHECK(abs(g.player.feet.x - w->spawn_x) <= 2 && v->health == 1.0f && v->death == 1.0f && v->respawn == 0);
    CHECK(inventory_count(inv, ITEM_STONE) == 9 && inventory_count(inv, ITEM_WOOD) == 4 && game_respawn_cost(&g, 0) == -1);
    CHECK(!game_remove_character(&g, 0)); /* the living are not removed */

    /* Left alone, the dead come back by themselves when the 1800 ticks are up. */
    g.player.feet = g.player.step_to = fell;
    game_hurt(&g, 10.0f);
    CHECK(!game_alive(&g));
    for (int i = 0; i < 1799; i++) game_tick(&g);
    CHECK(!game_alive(&g) && v->respawn == 1 && game_respawn_cost(&g, 0) == 1);
    game_tick(&g);
    CHECK(game_alive(&g) && abs(g.player.feet.x - w->spawn_x) <= 2 && g.crystals == 30);

    /* Drowning is a way to die. */
    g.player.feet = g.player.step_to = (TilePos){ w->spawn_x + 40, ground };
    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -3; dy <= 0; dy++) world_set_water(w, w->spawn_x + 40 + dx, ground + dy, WATER_FULL);
    world_set(w, w->spawn_x + 38, ground, TILE_STONE);
    world_set(w, w->spawn_x + 42, ground, TILE_STONE);
    world_set(w, w->spawn_x + 38, ground - 1, TILE_STONE);
    world_set(w, w->spawn_x + 42, ground - 1, TILE_STONE);
    int breaths = 0;
    while (game_alive(&g) && breaths < 5000) {
        for (int dx = -1; dx <= 1; dx++) world_set_water(w, w->spawn_x + 40 + dx, ground - 1, WATER_FULL);
        game_tick(&g);
        breaths++;
    }
    CHECK(!game_alive(&g) && breaths > u->oxygen_ticks + u->drown_ticks && breaths < 5000);
    for (int dx = -2; dx <= 2; dx++)
        for (int dy = -4; dy <= 0; dy++) world_set_water(w, w->spawn_x + 40 + dx, ground + dy, 0);
    TilePos drowned = g.player.feet;

    /* Removing a dead character leaves all they carried where they died,
     * and leaves the world with nobody in it. */
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    CHECK(game_remove_character(&g, 0));
    CHECK(g.character_count == 0 && !game_alive(&g));
    CHECK(count_drops(&g, ITEM_STONE) == 9 && count_drops(&g, ITEM_BASKET) == 1 && count_drops(&g, ITEM_WOOD) == 0);
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!g.drops[i].active) continue;
        CHECK(g.drops[i].x == drowned.x);
        /* (The basket still has the wood in it.) */
        if (g.drops[i].item == ITEM_BASKET) CHECK(g.inventory.containers[g.drops[i].container].slots[0].id == ITEM_WOOD);
    }
    /* With nobody there, nothing can be done and nothing comes for anybody; time still passes. */
    uint64_t tick = g.tick;
    CHECK(!game_click_tile(&g, drowned.x + 2, ground) && !game_open_bench(&g, w->spawn_x, ground));
    g.tick_rate = 10;
    run(&g, 2.0f);
    g.tick_rate = 0;
    CHECK(g.tick > tick && g.character_count == 0);
    for (int i = 0; i < 2000; i++) mobs_try_spawn(&g, w->spawn_x + 5, ground);
    CHECK(count_mobs(&g) == 0);

    /* A new first character is made at a portal the player has uncovered, and named. */
    CHECK(game_is_portal(&g, w->spawn_x, ground) && !game_is_portal(&g, w->spawn_x + 5, ground));
    CHECK(game_create_character(&g, (TilePos){ w->spawn_x + 5, ground }, "Nobody") == -1 && g.character_count == 0);
    CHECK(game_create_character(&g, (TilePos){ w->spawn_x, ground }, "Ada") == 0);
    CHECK(g.character_count == 1 && game_alive(&g) && strcmp(game_character_name(&g, 0), "Ada") == 0);
    CHECK(abs(g.player.feet.x - w->spawn_x) <= 2 && v->health == 1.0f && v->death == 1.0f);
    /* This world has had its first already: this one starts with nothing. */
    for (int i = 0; i < INVENTORY_SLOTS; i++) CHECK(inv->slots[i].count == 0);
    CHECK(game_click_tile(&g, w->spawn_x + 4, ground));
    run(&g, 3.0f);
    CHECK(g.player.feet.x == w->spawn_x + 4);
    game_free(&g);

    /* A brand new world has nobody in it until its first character is made
     * - and that one is given what a new player starts with. */
    game_init(&g, 256, 64, 152, WORLD_FLAT);
    w = &g.world;
    ground = g.player.feet.y;
    game_clear_characters(&g);
    CHECK(g.character_count == 0 && !g.started && !game_alive(&g));
    for (int i = 0; i < INVENTORY_SLOTS; i++) CHECK(g.inventory.slots[i].count == 0);
    Game loaded;
    CHECK(save_game(&g, "build/test_death.world") && load_game(&loaded, "build/test_death.world"));
    CHECK(loaded.character_count == 0 && !loaded.started);
    CHECK(game_create_character(&loaded, (TilePos){ loaded.world.spawn_x, ground }, "Grace") == 0 && loaded.started);
    CHECK(inventory_count(&loaded.inventory, ITEM_BASKET) == 1 && strcmp(game_character_name(&loaded, 0), "Grace") == 0);
    game_free(&loaded);
    CHECK(game_create_character(&g, (TilePos){ w->spawn_x, ground }, "") == 0);
    CHECK(strcmp(game_character_name(&g, 0), "Player 1") == 0 && inventory_count(&g.inventory, ITEM_BASKET) == 1);

    /* Two characters. When the one being played dies, the other is played. */
    g.tick_rate = 0;
    clear_mobs(&g);
    CHECK(game_add_character(&g, (TilePos){ w->spawn_x + 6, ground }) == 1);
    game_control(&g, 1);
    inventory_add(&g.inventory, ITEM_DIRT, 3);
    game_hurt(&g, 10.0f);
    CHECK(game_character_dead(&g, 1) && g.controlled == 0);
    game_control(&g, g.controlled);
    CHECK(g.live == 0 && game_alive(&g) && !game_character_dead(&g, 0));
    /* A creature takes no notice of the dead one, however near. */
    int bear = mob_spawn(&g, mob_id("dropbear"), w->spawn_x + 7, ground, true, false);
    g.mobs[bear].state = MOB_HUNT;
    g.mobs[bear].timer = 50;
    g.player.feet = g.player.step_to = (TilePos){ w->spawn_x - 60, ground };
    for (int i = 0; i < 100; i++) game_tick(&g);
    CHECK(g.vitals.health == 1.0f && game_character_vitals(&g, 1)->respawn == 1700);
    clear_mobs(&g);
    /* Time does not wait on the dead: with the living one at rest it runs on. */
    g.tick_rate = 10;
    CHECK(game_rest(&g, REST_MEDITATE) && game_tick_speed(&g) == 3 * 10);
    game_wake(&g);
    g.tick_rate = 0;
    /* Saved and loaded, the dead are still dead, and still called what they were called. */
    snprintf(g.characters[1].name, CHARACTER_NAME_MAX, "Second");
    CHECK(save_game(&g, "build/test_death.world") && load_game(&loaded, "build/test_death.world"));
    CHECK(loaded.character_count == 2 && game_character_dead(&loaded, 1) && !game_character_dead(&loaded, 0));
    CHECK(game_character_vitals(&loaded, 1)->respawn == 1700 && strcmp(game_character_name(&loaded, 1), "Second") == 0);
    CHECK(strcmp(game_character_name(&loaded, 0), "Player 1") == 0 && loaded.controlled == 0);
    game_free(&loaded);
    remove("build/test_death.world");
    /* Removing the dead one frees its place: another can be made at the portal bench. */
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    const Recipe *r = find_recipe(TILE_SPAWN_PORTAL, "new_player");
    g.player.feet = g.player.step_to = (TilePos){ w->spawn_x, ground };
    for (int i = 0; i < r->input_count; i++) inventory_add(&g.inventory, r->inputs[i].id, r->inputs[i].count);
    g.crystals = 500;
    CHECK(game_open_bench(&g, w->spawn_x, ground) && !game_craft(&g, r, 1));
    CHECK(game_remove_character(&g, 1) && g.character_count == 1 && g.live == 0 && g.controlled == 0);
    CHECK(count_drops(&g, ITEM_DIRT) == 3);
    CHECK(game_open_bench(&g, w->spawn_x, ground) && game_craft(&g, r, 1));
    run(&g, 1.0f);
    CHECK(g.character_count == 2 && !game_character_dead(&g, 1));
    /* Removing the first of two leaves the second as the only one, and played. */
    game_hurt(&g, 10.0f);
    CHECK(game_character_dead(&g, 0) && g.controlled == 1);
    game_control(&g, 1);
    CHECK(game_remove_character(&g, 0) && g.character_count == 1 && g.live == 0 && g.controlled == 0 && game_alive(&g));
    CHECK(strcmp(game_character_name(&g, 0), "Player 2") == 0);
    game_free(&g);
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

    /* A click on a stack with the chest open sends the whole stack across,
     * topping up a matching stack before taking a free slot. */
    uint16_t open = game_open_storage(&g);
    int wood_in_chest = chest_slot(&g, 0)->count;
    CHECK(wood_in_chest == 5 && inv->slots[2].id == ITEM_WOOD && inv->slots[2].count == 15);
    CHECK(inventory_quick_move(inv, bar2, open, 16) == 15);
    CHECK(inv->slots[2].count == 0 && chest_slot(&g, 0)->count == 20 && chest_slot(&g, 1)->count == 0);
    CHECK(inventory_quick_move(inv, bar2, open, 16) == 0); /* nothing there to send */
    /* And back out again, into the first free place in the inventory. */
    CHECK(inventory_quick_move(inv, (SlotRef){ SLOT_CONTAINER, open, 0 }, open, 16) == 20);
    CHECK(chest_slot(&g, 0)->count == 0 && count_item(&g, ITEM_WOOD) == 20);
    /* A basket goes across with what it holds, and comes back the same. */
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 1, 0 }, (SlotRef){ SLOT_BAR, 7, 0 }, MAX_STACK));
    int wood_slot = -1;
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (inv->slots[i].id == ITEM_WOOD) wood_slot = i;
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, wood_slot, 0 }, (SlotRef){ SLOT_BASKET, 0, 2 }, 6));
    CHECK(inventory_quick_move(inv, (SlotRef){ SLOT_BAR, 0, 0 }, open, 16) == 1);
    CHECK(inv->slots[0].count == 0 && chest_slot(&g, 0)->id == ITEM_BASKET);
    CHECK(inv->containers[chest_slot(&g, 0)->container].slots[2].count == 6);
    CHECK(inventory_quick_move(inv, (SlotRef){ SLOT_CONTAINER, open, 0 }, open, 16) == 1);
    CHECK(inv->slots[0].id == ITEM_BASKET && inventory_slot(inv, (SlotRef){ SLOT_BASKET, 0, 2 })->count == 6);
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BASKET, 0, 2 }, (SlotRef){ SLOT_BAR, wood_slot, 0 }, MAX_STACK));
    /* With the chest full, only what tops up a matching stack goes; with the
     * inventory full, nothing comes out. */
    for (int i = 0; i < 16; i++) *chest_slot(&g, i) = (ItemStack){ (uint16_t)ITEM_STONE, 99, 0 };
    *chest_slot(&g, 4) = (ItemStack){ (uint16_t)ITEM_WOOD, 95, 0 };
    CHECK(inventory_quick_move(inv, (SlotRef){ SLOT_BAR, wood_slot, 0 }, open, 16) == 4);
    CHECK(inv->slots[wood_slot].count == 16 && chest_slot(&g, 4)->count == 99);
    ItemStack saved_bar[INVENTORY_SLOTS];
    memcpy(saved_bar, inv->slots, sizeof saved_bar);
    Container saved_basket = inv->containers[inv->slots[0].container];
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (inv->slots[i].count == 0) inv->slots[i] = (ItemStack){ (uint16_t)ITEM_DIRT, 99, 0 };
    for (int j = 0; j < BASKET_SLOTS; j++) /* the basket in the bar counts as room too */
        *inventory_slot(inv, (SlotRef){ SLOT_BASKET, 0, j }) = (ItemStack){ (uint16_t)ITEM_DIRT, 99, 0 };
    CHECK(inventory_quick_move(inv, (SlotRef){ SLOT_CONTAINER, open, 3 }, open, 16) == 0);
    CHECK(chest_slot(&g, 3)->count == 99);
    memcpy(inv->slots, saved_bar, sizeof saved_bar);
    inv->containers[inv->slots[0].container] = saved_basket;
    for (int i = 0; i < 16; i++) *chest_slot(&g, i) = (ItemStack){ 0 };
    *chest_slot(&g, 0) = (ItemStack){ (uint16_t)ITEM_WOOD, 5, 0 };
    inv->slots[wood_slot].count = 15;
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, wood_slot, 0 }, bar2, MAX_STACK) || wood_slot == 2);
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 7, 0 }, (SlotRef){ SLOT_BAR, 1, 0 }, MAX_STACK));

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
    inv->containers[other].slots[0] = (ItemStack){ (uint16_t)ITEM_WOOD, 4, 0 };
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
    test_fog();
    test_vitals();
    test_discard();
    test_coffee_and_dropbears();
    test_dodo_tree();
    test_water();
    test_basket_pickup();
    test_death();
    test_fluid_creatures();
    test_characters();
    test_sea_and_combat();
    test_buckets_and_stacks();
    test_remote_benches_and_teleport();
    test_decay_and_golden_chests();
    test_mobs();
    test_biomes();
    test_plants();
    test_new_trees();
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
