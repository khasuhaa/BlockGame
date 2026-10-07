/* Headless tests for the game rules: no window, just the simulation. */
#include "../src/digital.h"
#include "../src/game.h"
#include "../src/guide.h"
#include "../src/industry.h"
#include "../src/light.h"
#include "../src/mob.h"
#include "../src/mods.h"
#include "../src/power.h"
#include "../src/save.h"
#include "../src/trade.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

/* A gap one block high is got into wherever it is: at head height, or up a wall. */
static void test_crawl_into_gaps(void)
{
    Game g;
    game_init(&g, 256, 48, 25, WORLD_FLAT);
    World *w = &g.world;
    TilePos start = g.player.feet;
    int s = w->surface_y;

    /* A gap at head height, in a passage with no room to rise in: it is got into all the same, by a click or by the keys. */
    int ax = start.x + 20;
    for (int x = ax - 2; x <= ax + 5; x++) world_set(w, x, s - 3, TILE_STONE);          /* the roof, over passage and gap alike */
    for (int x = ax + 1; x <= ax + 5; x++) world_set(w, x, s - 1, TILE_STONE);          /* the floor of the gap */
    g.player.feet = g.player.step_to = (TilePos){ ax, s - 1 };
    CHECK(!game_player_crawling(&g) && world_is_solid(w, ax, s - 3));
    CHECK(game_click_tile(&g, ax + 3, s - 2));
    run(&g, 6.0f);
    CHECK(is_idle(&g) && g.player.feet.x == ax + 3 && g.player.feet.y == s - 2 && game_player_crawling(&g));
    g.player.feet = g.player.step_to = (TilePos){ ax, s - 1 };
    game_set_move_input(&g, 1, 0);
    run(&g, 0.5f);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(g.player.feet.y == s - 2 && g.player.feet.x > ax && game_player_crawling(&g));
    /* (But not from one gap up into another: there is a block in the way overhead.) */
    int cx = start.x + 30;
    for (int x = cx - 1; x <= cx + 3; x++) world_set(w, x, s - 2, TILE_STONE);
    world_set(w, cx, s - 2, TILE_STONE);
    world_set(w, cx + 1, s - 1, TILE_STONE);
    world_set(w, cx + 1, s - 2, TILE_AIR);
    g.player.feet = g.player.step_to = (TilePos){ cx, s - 1 };
    CHECK(game_player_crawling(&g));
    game_set_move_input(&g, 1, 0);
    run(&g, 1.5f);
    game_set_move_input(&g, 0, 0);
    CHECK(g.player.feet.x == cx && g.player.feet.y == s - 1);

    /* A gap up a wall, in a shaft with a lid on it: climbed up to, and gone into from as high as the shaft lets them get. */
    int bx = start.x + 40;
    for (int y = s - 6; y <= s - 1; y++) {
        world_set(w, bx - 1, y, TILE_STONE);
        for (int x = bx + 1; x <= bx + 4; x++) world_set(w, x, y, TILE_STONE);
    }
    world_set(w, bx, s - 5, TILE_STONE);                                        /* the lid */
    for (int x = bx + 1; x <= bx + 4; x++) world_set(w, x, s - 4, TILE_AIR);  /* the gap, level with the top of the shaft */
    g.player.feet = g.player.step_to = (TilePos){ bx, s - 1 };
    CHECK(game_click_tile(&g, bx + 3, s - 4));
    Trace up = run_traced(&g, 10.0f);
    CHECK(is_idle(&g) && g.player.feet.x == bx + 3 && g.player.feet.y == s - 4 && game_player_crawling(&g) && up.climbed);
    /* And back out and down again. */
    CHECK(game_click_tile(&g, bx, s - 1));
    run(&g, 10.0f);
    CHECK(is_idle(&g) && g.player.feet.x == bx && g.player.feet.y == s - 1);

    /* Off a tree straight into a gap beside the feet. */
    int lx = start.x - 20;
    TileType ladder = tile_id("oak_log");
    CHECK(ladder > 0 && tile_def(ladder)->climbable);
    for (int y = s - 6; y <= s - 1; y++) world_set_at(w, tile_def(ladder)->layer, lx, y, ladder);
    for (int x = lx + 1; x <= lx + 3; x++) {
        world_set(w, x, s - 3, TILE_STONE);
        world_set(w, x, s - 5, TILE_STONE);
    }
    g.player.feet = g.player.step_to = (TilePos){ lx, s - 4 };
    g.player.climbing = true;
    game_set_move_input(&g, 1, 0);
    run(&g, 0.6f);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(g.player.feet.y == s - 4 && g.player.feet.x > lx && game_player_crawling(&g) && !g.player.climbing);
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
     * the player is no longer at it. (One that was walked to and opened is theirs to close first.) */
    g.menu_hold = false;
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
    game_close_bench(&g); /* (as a click away from it does) */
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
    game_close_bench(&g); /* (as a click away from it does) */
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
    return t == TILE_DIRT || t == tile_id("sand") || t == tile_id("black_sand") || t == tile_id("golden_chest") || vein_in(t, TILE_DIRT);
}

static bool in_rock(TileType t)
{
    /* (Stone, what runs through it in veins and scars, and the rock some biomes have under them.) */
    for (int b = 0; b < content.biome_count; b++)
        if (t != TILE_AIR && content.biomes[b].under == t) return true;
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
                if (t == TILE_AIR ? !in_rock(world_get_at(&w, LAYER_WALL, x, y)) : !in_rock(t)) bad++;
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
    g.world.layers[BG][5] = (uint16_t)TILE_STONE;
    g.tick = 12345;
    g.tick_rate = 25;
    CHECK(save_game(&g, path));

    Game loaded;
    CHECK(load_game(&loaded, path));
    size_t cells = (size_t)g.world.width * g.world.height;
    CHECK(loaded.world.width == 64 && loaded.world.height == 48 && loaded.world.seed == 16);
    CHECK(memcmp(loaded.world.layers[0], g.world.layers[0], cells * sizeof(uint16_t)) == 0);
    CHECK(memcmp(loaded.world.layers[BG], g.world.layers[BG], cells * sizeof(uint16_t)) == 0);
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
        /* Clay lies in the dirt by the same rules. */
        VeinStats cs = vein_stats(&w, tile_id("clay"), 10);
        CHECK(cs.groups > 20 && cs.smallest >= 4 && cs.largest <= 10 && cs.over == 0);
        CHECK(tile_def(tile_id("clay"))->drop == item_id("clay") && tile_def(tile_id("clay"))->tool_class == tool_class_id("shovel"));

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
                if (def->light == 0 || def->log_of >= 0 || def->leaves_of >= 0 || def->storage_columns > 0) continue;
                /* (What floats over a pole is out in the open air, and nothing to do with the caves.) */
                bool over_pole = false;
                for (int k = 0; k < content.worldgen.pole_count; k++) over_pole = over_pole || content.worldgen.poles[k].top == world_get_at(&w, BG, x, y);
                if (over_pole) continue;
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
    /* (These are of backwall as a block leaves it behind. Walls that are run
     * and taken down by themselves have a test of their own, and are off here.) */
    int span = content.rules.wall_span;
    content.rules.wall_span = 0;
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
    CHECK(memcmp(loaded.world.layers[WALL], w->layers[WALL], cells * sizeof(uint16_t)) == 0);
    game_free(&loaded);
    remove("build/test_wall.world");
    game_free(&g);
    content.rules.wall_span = span;
}

/* Work at a bench is seen through, and a menu that was walked to is closed, before what is queued after starts. */
static void test_queue_waits(void)
{
    Game g;
    game_init(&g, 128, 64, 71, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    g.tick_rate = 0;
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;
    TileType bench = tile_id("workbench"), chest = tile_id("chest");
    /* (Something that takes time to make, at a bench of the first tier, for nothing but items.) */
    const Recipe *r = NULL;
    for (int i = 0; i < content.recipe_count && !r; i++) {
        const Recipe *c = &content.recipes[i];
        if (c->level > 1 || c->crystals != 0 || item_def(c->output.id)->becomes_player || item_def(c->output.id)->crystals) continue;
        for (TileType t = 1; t < content.tile_count && !r; t++) {
            const TileDef *def = tile_def(t);
            if (!def->bench || def->bench_level != 1 || def->fuel_capacity != 0 || def->electric != 0 || def->height != 1 ||
                strcmp(def->bench_kind, c->bench) != 0)
                continue;
            r = c;
            bench = t;
        }
    }
    CHECK(r != NULL);
    if (!r) return;
    /* (Made to take two seconds apiece, for the length of this test.) */
    float took = r->time;
    ((Recipe *)r)->time = 2.0f;
    world_set_at(w, tile_def(bench)->layer, px + 3, ground, bench);
    world_set_at(w, tile_def(chest)->layer, px - 3, ground, chest);
    clear_inventory(&g);
    for (int k = 0; k < r->input_count; k++) inventory_add(inv, r->inputs[k].id, r->inputs[k].count * 3);
    g.crystals = 1000;

    /* Three to make, started from where they stand; then somewhere to walk to, queued behind it. */
    CHECK(game_open_bench(&g, px + 3, ground) && game_craft(&g, r, 3));
    CHECK(game_bench_busy(&g)); /* on the way there to do it */
    CHECK(game_queue_action(&g, ACTION_MOVE, px + 12, ground));
    run(&g, r->time * 1.5f + 1.0f);
    CHECK(g.bench_open && g.craft_remaining > 0 && g.craft_remaining < 3 && game_queue_held(&g) && game_bench_busy(&g));
    CHECK(world_dx(w, px + 3, g.player.feet.x) >= -1 && world_dx(w, px + 3, g.player.feet.x) <= 1 && g.queue_count == 1);
    /* The keys do not take them from it either. */
    game_set_move_input(&g, 1, 0);
    run(&g, 0.5f);
    game_set_move_input(&g, 0, 0);
    CHECK(g.craft_remaining > 0 && world_dx(w, px + 3, g.player.feet.x) <= 1);
    /* Nor does another bench opened meanwhile take its place. */
    world_set_at(w, tile_def(bench)->layer, px + 20, ground, bench);
    CHECK(!game_open_bench(&g, px + 20, ground) && g.bench_pos.x == px + 3);
    /* All three made, they go on to what was queued; the menu is still up. */
    run(&g, r->time * 3.0f + 4.0f);
    CHECK(g.craft_remaining == 0 && count_item(&g, r->output.id) >= 3 * r->output.count);
    CHECK(g.player.feet.x == px + 12 && is_idle(&g) && g.bench_open && !game_queue_held(&g));
    game_close_bench(&g);

    /* Stopped, the work is given up and what waited goes ahead; the bench stays open. */
    for (int k = 0; k < r->input_count; k++) inventory_add(inv, r->inputs[k].id, r->inputs[k].count * 3);
    CHECK(game_open_bench(&g, px + 3, ground) && game_craft(&g, r, 3));
    CHECK(game_queue_action(&g, ACTION_MOVE, px + 12, ground));
    run(&g, r->time * 0.5f + 3.0f);
    CHECK(g.craft_remaining > 0 && game_queue_held(&g));
    game_craft_stop(&g);
    CHECK(g.bench_open && !game_queue_held(&g) && !game_bench_busy(&g));
    run(&g, 4.0f);
    CHECK(g.player.feet.x == px + 12 && is_idle(&g));
    game_close_bench(&g);

    /* A chest gone to and opened: what is queued after it waits until the chest is closed. */
    CHECK(game_queue_action(&g, ACTION_USE, px - 3, ground));
    CHECK(game_queue_action(&g, ACTION_MOVE, px + 6, ground));
    run(&g, 8.0f);
    CHECK(g.bench_open && g.bench_pos.x == world_wrap_x(w, px - 3) && game_queue_held(&g) && !game_bench_busy(&g));
    CHECK(g.queue_count == 1 && world_dx(w, px - 3, g.player.feet.x) >= -1 && world_dx(w, px - 3, g.player.feet.x) <= 1);
    run(&g, 5.0f);
    CHECK(g.bench_open && g.queue_count == 1);
    game_close_bench(&g);
    CHECK(!game_queue_held(&g));
    run(&g, 8.0f);
    CHECK(g.player.feet.x == px + 6 && is_idle(&g) && !g.bench_open);
    /* A bench looked at from where they stand holds nothing up. */
    CHECK(game_open_bench(&g, px + 3, ground) && !game_queue_held(&g));
    CHECK(game_queue_action(&g, ACTION_MOVE, px + 9, ground));
    run(&g, 4.0f);
    CHECK(g.player.feet.x == px + 9 && g.bench_open);
    ((Recipe *)r)->time = took;
    game_free(&g);
}

/* The debug world: one of every block, each every way it can be put up. */
static void test_debug_world(void)
{
    int width = world_debug_width();
    CHECK(width >= 64 && width % 64 == 0 && width <= 65536);
    Game g;
    CHECK(game_init(&g, width, 128, 3, WORLD_DEBUG));
    World *w = &g.world;
    int ground = w->surface_y - 1;
    TileType holder = content.worldgen.soil;
    CHECK(g.random_events == 0 && w->spawn_y == ground && world_is_solid(w, w->spawn_x, ground + 1));
    /* Every block there is, is somewhere in it (the other halves of two-block things with their first halves). */
    static int count[MAX_TILES];
    static uint8_t mounts[MAX_TILES];
    memset(count, 0, sizeof count);
    memset(mounts, 0, sizeof mounts);
    for (int y = 0; y < ground + 1; y++) {
        for (int x = 0; x < w->width; x++) {
            for (int layer = 0; layer < BLOCK_LAYERS; layer++) {
                TileType t = world_get_at(w, layer, x, y);
                if (t == TILE_AIR) continue;
                count[t]++;
                const TileDef *def = tile_def(t);
                if (def->pair_of != TILE_AIR) continue;
                int m = layer == LAYER_BACKGROUND ? world_mount(w, x, y) : world_mount_block(w, x, y);
                if (m == MOUNT_NONE && def->pair != TILE_AIR) m = world_mount_block(w, x + 1, y);
                mounts[t] |= (uint8_t)(1 << m);
            }
        }
    }
    int missing = 0, unshown = 0, bad_holder = 0;
    for (TileType t = 1; t < content.tile_count; t++) {
        const TileDef *def = tile_def(t);
        if (count[t] == 0) missing++;
        if (def->pair_of != TILE_AIR) continue;
        bool mounted = def->hangs || def->texture_mount[0][0] || def->texture_mount[1][0] || def->texture_mount[2][0];
        if (!mounted) continue;
        /* Something that goes up against a block is shown under one; and, unless it only hangs, on one and on each side of one. */
        int want = 1 << MOUNT_CEILING;
        if (!def->hangs) want |= 1 << MOUNT_FLOOR | 1 << MOUNT_LEFT | 1 << MOUNT_RIGHT;
        if (!def->hangs && def->layer == LAYER_BACKGROUND) want |= 1 << MOUNT_WALL;
        if ((mounts[t] & want) != want) unshown++;
    }
    CHECK(missing == 0 && unshown == 0);
    /* What holds them is the soil, and nothing else is put there for the purpose. */
    for (int y = 0; y < ground; y++)
        for (int x = 0; x < w->width; x++)
            if (world_get_at(w, LAYER_WALL, x, y) != TILE_AIR && world_get_at(w, LAYER_WALL, x, y) != holder) bad_holder++;
    CHECK(bad_holder == 0);
    /* A door is shown as itself and between two blocks, with their look; steps in a flight each way. */
    TileType door = tile_id("wooden_door");
    int doors = 0, looked = 0;
    for (int x = 0; x < w->width; x++) {
        if (world_get(w, x, ground) != door) continue;
        doors++;
        if (world_looks_like(w, x, ground) == holder) looked++;
    }
    CHECK(doors == 2 && looked == 1);
    /* It is saved and loaded like any other. */
    CHECK(save_game(&g, "build/test_debug.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_debug.world"));
    CHECK(loaded.world.width == width && memcmp(loaded.world.layers[0], w->layers[0], sizeof(uint16_t) * (size_t)width * 128) == 0);
    game_free(&loaded);
    remove("build/test_debug.world");
    for (int t = 0; t < 50; t++) game_tick(&g);
    game_free(&g);
}

static int light_at(const Game *g, int x, int y)
{
    uint8_t v;
    light_compute(g, x, y, 1, 1, &v);
    return v;
}

/* The lighting the game ships with goes all the way to black, and past true colours for what glows. */
static void test_shipped_light(void)
{
    const RulesDef *u = &content.rules;
    CHECK(u->light_minimum == 0 && u->light_darkness >= 0.95f && u->light_darkness <= 1.0f);
    CHECK(u->light_contrast > 1.0f && u->light_bloom > 0.0f && u->light_glow > 0);
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
    /* (What gets through the lid still falls off down the shaft.) */
    int through = u->light_sun - u->light_solid_falloff - 12 * u->light_air_falloff;
    CHECK(light_at(&g, sx, s + 12) == (through > u->light_minimum ? through : u->light_minimum));
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
    /* Something that gives light is itself shown brighter than the light it gives: by the glow of the rules. */
    {
        int glow = content.rules.light_glow, gx = start.x - 8;
        TileType torch = tile_id("torch"), fire = tile_id("campfire");
        content.rules.light_glow = 2;
        CHECK(torch > 0 && tile_def(torch)->light > 0 && light_glow(&g, gx, s - 1) == 0);
        world_set_at(w, tile_def(torch)->layer, gx, s - 1, torch);
        int want = tile_def(torch)->light + 2 > LIGHT_MAX ? LIGHT_MAX : tile_def(torch)->light + 2;
        CHECK(light_glow(&g, gx, s - 1) == want && light_glow(&g, gx + 1, s - 1) == 0);
        /* Not one out of sight behind a block; nor a fire that is out. */
        if (tile_def(torch)->layer != LAYER_FOREGROUND) {
            world_set(w, gx, s - 1, TILE_STONE);
            CHECK(light_glow(&g, gx, s - 1) == 0);
            world_set(w, gx, s - 1, TILE_AIR);
        }
        if (fire > 0 && tile_def(fire)->light_needs_fuel) {
            world_set_at(w, tile_def(fire)->layer, gx + 3, s - 1, fire);
            CHECK(light_glow(&g, gx + 3, s - 1) == 0);
            world_set_at(w, tile_def(fire)->layer, gx + 3, s - 1, TILE_AIR);
        }
        /* With no glow in the rules, nothing is shown any brighter. */
        content.rules.light_glow = 0;
        CHECK(light_glow(&g, gx, s - 1) == 0);
        content.rules.light_glow = glow;
        world_set_at(w, tile_def(torch)->layer, gx, s - 1, TILE_AIR);
    }
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
    game_close_bench(&g); /* (as a click away from it does) */
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
    game_close_bench(&g); /* (as a click away from it does) */
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

    CHECK(content.biome_count == 9 && content.worldgen.has_sea && content.worldgen.sea_level == 0);
    CHECK(!content.biomes[0].has_climate && content.biomes[1].has_climate && content.biomes[8].below_sea);
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
    for (int k = 0; k < content.tree_count; k++) {
        /* (Every tree that some country grows. The ones that only grow on islands in the sky are seen to elsewhere.) */
        bool grown = false;
        for (int b = 0; b < content.biome_count; b++)
            for (int i = 0; i < content.biomes[b].tree_count; i++) grown = grown || content.biomes[b].trees[i].tree == k;
        CHECK(seen[k] > 0 || !grown);
    }
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

static int shipped_torch_light;
static float shipped_island_rate;
static TradeDef shipped_trade;
static int shipped_price[MAX_ITEMS];
static TileDef *tile_def_mut(TileType t) { return &content.tiles[t]; }
static int count_mobs(const Game *g);
static void clear_mobs(Game *g);
static bool is_idle(const Game *g);

/* How many of an item lie about as drops, and getting rid of them. */
static int drops_of(const Game *g, ItemId id)
{
    int n = 0;
    for (int i = 0; i < MAX_DROPS; i++) n += g->drops[i].active && g->drops[i].item == id;
    return n;
}

static void clear_drops(Game *g)
{
    for (int i = 0; i < MAX_DROPS; i++) g->drops[i].active = false;
}

/* Scars of pale and coloured stone through the rock, and sandstone under the sand. */
static void test_scars_and_sandstone(void)
{
    static const char *stones[] = { "limestone", "marble", "lapis_lazuli", "red_marble" };
    TileType sandstone = tile_id("sandstone");
    int order[4], crossings = 0, total[4] = { 0 }, thick = 0;

    /* They are laid down in that order, each able to cut through the ones before it and never the ones after. */
    for (int k = 0; k < 4; k++) {
        order[k] = -1;
        for (int i = 0; i < content.vein_count; i++)
            if (content.veins[i].tile == tile_id(stones[k])) order[k] = i;
        CHECK(order[k] >= 0 && content.veins[order[k]].shape == VEIN_SCAR && (k == 0 || order[k] > order[k - 1]));
        const VeinDef *v = &content.veins[order[k]];
        CHECK(v->host_count == k + 1 && v->hosts[0] == TILE_STONE && v->max_thick >= 2 && v->min_size >= 30);
        for (int h = 1; h < v->host_count; h++) CHECK(v->hosts[h] == tile_id(stones[h - 1]));
        const TileDef *def = tile_def(tile_id(stones[k]));
        CHECK(def->solid && def->removable && def->drop == item_id(stones[k]) && def->tool_class == tool_class_id("pickaxe"));
    }
    BiomeDef *desert = NULL, *beach = NULL;
    for (int b = 0; b < content.biome_count; b++) {
        if (strcmp(content.biomes[b].name, "desert") == 0) desert = &content.biomes[b];
        if (strcmp(content.biomes[b].name, "beach") == 0) beach = &content.biomes[b];
    }
    CHECK(desert && beach && desert->under == sandstone && beach->under == sandstone && desert->under_depth == 10 &&
          beach->under_depth == 10);
    CHECK(tile_def(sandstone)->drop == item_id("sandstone") && content.biomes[0].under == TILE_AIR);

    int under_sand = 0, sand_columns = 0, stray = 0, too_deep = 0, broken = 0;
    int black_sand = 0, black_astray = 0, black_over = 0, over_sandstone = 0, jump = 0, sea_columns = 0, under_sea = 0;
    const BiomeDef *ocean = NULL;
    for (int b = 0; b < content.biome_count; b++)
        if (content.biomes[b].below_sea) ocean = &content.biomes[b];
    CHECK(ocean && ocean->under == sandstone);
    for (uint32_t seed = 1; seed <= 4; seed++) {
        World w;
        CHECK(world_init(&w, 2048, 401, seed, WORLD_TERRAIN));
        for (int k = 0; k < 4; k++) {
            int n = 0;
            for (int y = 0; y < w.height; y++) {
                for (int x = 0; x < w.width; x++) {
                    if (world_get(&w, x, y) != tile_id(stones[k])) continue;
                    n++;
                    /* (A scar is more than one block through, somewhere.) */
                    if (world_get(&w, x, y + 1) == tile_id(stones[k]) && world_get(&w, x, y + 2) == tile_id(stones[k])) thick++;
                    /* One stone against an earlier one: where a later scar has cut an earlier one. */
                    for (int e = 0; e < k; e++)
                        if (world_get(&w, x + 1, y) == tile_id(stones[e]) || world_get(&w, x, y + 1) == tile_id(stones[e])) crossings++;
                }
            }
            total[k] += n;
        }
        /* Sandstone: only under country that has it, and there only where
         * the sand goes all the way down - under sand lying thin over dirt
         * there is stone. No more than 10 blocks of it, giving way to stone
         * raggedly. The lowest layer of sand over it is half black sand. */
        TileType sand = tile_id("sand"), black = tile_id("black_sand");
        int prev_depth = -1;
        for (int x = 0; x < w.width; x++) {
            const BiomeDef *b = &content.biomes[world_biome(&w, x)];
            int first = -1, last = -1, gaps = 0, depth = 0, lowest = -1;
            /* (By the backwalls, which keep the ground as it was laid before caves were cut in it.) */
            for (int y = 0; y < w.height; y++) {
                TileType t = world_get_at(&w, LAYER_WALL, x, y);
                if (t == sand || t == black || t == TILE_DIRT || t == TILE_GRASS) lowest = y;
                if (t == black) {
                    black_sand++;
                    if (world_get_at(&w, LAYER_WALL, x, y + 1) != sandstone) black_astray++;
                }
                if (t != sandstone) continue;
                depth++;
                if (first < 0) first = y;
                else if (y - last > 1) gaps++;
                last = y;
            }
            TileType bottom = lowest >= 0 ? world_get_at(&w, LAYER_WALL, x, lowest) : TILE_AIR;
            if (first >= 0 && b->under != sandstone) stray++;
            if (first >= 0 && last - first >= 10) too_deep++;
            /* Never under dirt; always right under the last of the sand. */
            if (first >= 0 && (bottom == TILE_DIRT || bottom == TILE_GRASS || first != lowest + 1)) stray++;
            broken += gaps;
            if (first >= 0) over_sandstone++, black_over += bottom == black;
            /* It comes in and goes out by degrees along the ground, not as a cliff. */
            if (prev_depth >= 0 && abs(depth - prev_depth) > jump) jump = abs(depth - prev_depth);
            prev_depth = depth;
            if (b == ocean && lowest >= 0) sea_columns++, under_sea += first >= 0;
            if (b != desert || (bottom != sand && bottom != black)) continue;
            /* (Desert whose sand goes right down: all but the very edge of that has it.) */
            sand_columns++;
            under_sand += first >= 0;
        }
        world_free(&w);
    }
    for (int k = 0; k < 4; k++) CHECK(total[k] > 200);
    CHECK(total[0] > total[1] && total[1] > total[2]);
    CHECK(thick > 500 && crossings > 0);
    CHECK(stray == 0 && too_deep == 0 && broken > 20);
    CHECK(sand_columns > 100 && under_sand * 10 >= sand_columns * 8);
    CHECK(sea_columns > 100 && under_sea * 10 >= sea_columns * 9);
    CHECK(jump <= 6);
    /* Black sand: only in that one layer, and about half of it. */
    CHECK(black_sand > 200 && black_astray == 0 && black_over * 10 > over_sandstone * 4 && black_over * 10 < over_sandstone * 6);
    CHECK(tile_def(tile_id("black_sand"))->drop == item_id("black_sand") && desert->seam == tile_id("black_sand") && desert->seam_share == 0.5f);
}

/* Plants that end up two blocks tall, and chilli, which does not. */
static void test_tall_plants(void)
{
    static const char *kinds[] = { "sunflower", "tomato", "wheat" };
    static const char *seeds[] = { "sunflower_seed", "tomato", "wheat" };
    Game g;
    game_init(&g, 64, 48, 37, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    int s = w->surface_y, x = g.player.feet.x + 2;
    char name[48];
    g.tick_rate = 0;

    for (int k = 0; k < 3; k++) {
        TileType stage[4], top;
        for (int i = 0; i < 4; i++) {
            snprintf(name, sizeof name, "%s_%d", kinds[k], i);
            stage[i] = tile_id(name);
            CHECK(stage[i] > 0 && !tile_def(stage[i])->solid && tile_def(stage[i])->tool_class == tool_class_id("machete"));
        }
        snprintf(name, sizeof name, "%s_top", kinds[k]);
        top = tile_id(name);
        ItemId seed = item_id(seeds[k]);
        /* Three stages of one block, then a foot with a top on it. */
        CHECK(top > 0 && item_def(seed)->sows == stage[0]);
        CHECK(tile_def(stage[0])->grows_into == stage[1] && tile_def(stage[1])->grows_into == stage[2]);
        CHECK(tile_def(stage[2])->grows_into == stage[3] && tile_def(stage[2])->grows_top == top);
        CHECK(tile_def(stage[3])->grows_into == TILE_AIR && tile_def(stage[3])->topped_by == top && tile_def(top)->cut_to == stage[2]);
        CHECK(tile_def(top)->drop == seed && tile_def(top)->drop_count == 3 && tile_def(stage[3])->drop == seed);

        /* Sown by hand, it comes up by random events like any plant. */
        clear_inventory(&g);
        clear_drops(&g);
        inventory_add(inv, seed, 1);
        inventory_select(inv, 0, -1);
        CHECK(game_click_tile(&g, x, s) && g.queue[0].type == ACTION_PLANT);
        run(&g, 2.0f);
        CHECK(world_get_at(w, BG, x, s - 1) == stage[0] && count_item(&g, seed) == 0);
        game_random_event(&g, x, s - 1);
        game_random_event(&g, x, s - 1);
        CHECK(world_get_at(w, BG, x, s - 1) == stage[2] && world_get_at(w, BG, x, s - 2) == TILE_AIR);
        /* With something in the way overhead it waits at the third stage. */
        world_set(w, x, s - 2, TILE_STONE);
        game_random_event(&g, x, s - 1);
        CHECK(world_get_at(w, BG, x, s - 1) == stage[2] && !world_grow_plant(w, BG, x, s - 1));
        world_set(w, x, s - 2, TILE_AIR);
        game_random_event(&g, x, s - 1);
        CHECK(world_get_at(w, BG, x, s - 1) == stage[3] && world_get_at(w, BG, x, s - 2) == top);
        game_random_event(&g, x, s - 1);
        game_random_event(&g, x, s - 2);
        CHECK(world_get_at(w, BG, x, s - 1) == stage[3] && world_get_at(w, BG, x, s - 2) == top && world_get_at(w, BG, x, s - 3) == TILE_AIR);

        /* The top taken: three of what it bears, and the plant is back a stage, to grow it again. */
        CHECK(game_click_tile(&g, x, s - 2) && g.queue[0].type == ACTION_MINE);
        run(&g, 3.0f);
        CHECK(world_get_at(w, BG, x, s - 2) == TILE_AIR && world_get_at(w, BG, x, s - 1) == stage[2]);
        CHECK(count_item(&g, seed) + drops_of(&g, seed) == 3);
        game_random_event(&g, x, s - 1);
        CHECK(world_get_at(w, BG, x, s - 1) == stage[3] && world_get_at(w, BG, x, s - 2) == top);
        /* The foot taken: the whole plant goes, top and all. */
        clear_inventory(&g);
        clear_drops(&g);
        CHECK(game_click_tile(&g, x, s - 1) && g.queue[0].type == ACTION_MINE);
        run(&g, 3.0f);
        CHECK(world_get_at(w, BG, x, s - 1) == TILE_AIR && world_get_at(w, BG, x, s - 2) == TILE_AIR);
        CHECK(count_item(&g, seed) + drops_of(&g, seed) == 4);
    }

    /* Chilli: four stages, one block high throughout, more to pick at each. */
    TileType chilli[4];
    for (int i = 0; i < 4; i++) {
        snprintf(name, sizeof name, "chilli_%d", i);
        chilli[i] = tile_id(name);
        CHECK(chilli[i] > 0 && tile_def(chilli[i])->grows_top == TILE_AIR && tile_def(chilli[i])->drop == item_id("chilli"));
        CHECK(i == 0 || tile_def(chilli[i - 1])->grows_into == chilli[i]);
    }
    CHECK(item_def(item_id("chilli"))->sows == chilli[0] && tile_def(chilli[3])->grows_into == TILE_AIR && tile_def(chilli[3])->drop_count == 4);
    world_set_at(w, BG, x, s - 1, chilli[0]);
    for (int i = 0; i < 6; i++) game_random_event(&g, x, s - 1);
    CHECK(world_get_at(w, BG, x, s - 1) == chilli[3] && world_get_at(w, BG, x, s - 2) == TILE_AIR);
    game_free(&g);

    /* They grow wild, at every stage, tops and all; and so do the three new trees. */
    int wild[4] = { 0 }, tops = 0, loose = 0, trees[3] = { 0 }, palms_off_sand = 0;
    static const char *logs[] = { "pine_log", "coconut_log", "maple_log" };
    static const char *fruit[] = { "pine_cone", "coconut", "maple_seed" };
    for (int k = 0; k < 3; k++) {
        int kind = tree_id(k == 0 ? "pine" : k == 1 ? "coconut" : "maple");
        CHECK(kind >= 0 && content.trees[kind].log == tile_id(logs[k]) && content.trees[kind].fruit == item_id(fruit[k]));
        CHECK(item_def(item_id(fruit[k]))->plants == kind && tile_def(tile_id(logs[k]))->decays_into > 0);
        CHECK(tile_def(content.trees[kind].leaves)->tool_class == tool_class_id("machete"));
    }
    for (uint32_t seed = 1; seed <= 6; seed++) {
        World world;
        CHECK(world_init(&world, 2048, 401, seed, WORLD_TERRAIN));
        for (int y = 1; y < world.height - 1; y++) {
            for (int cx = 0; cx < world.width; cx++) {
                TileType t = world_get_at(&world, BG, cx, y);
                if (t == TILE_AIR) continue;
                const char *n = tile_def(t)->name;
                for (int k = 0; k < 3; k++) {
                    wild[k] += strncmp(n, kinds[k], strlen(kinds[k])) == 0;
                    if (t == tile_id(logs[k])) {
                        trees[k]++;
                        if (k == 1 && world_get_at(&world, BG, cx, y + 1) != t && world_get(&world, cx, y + 1) != tile_id("sand"))
                            palms_off_sand++;
                    }
                }
                wild[3] += strncmp(n, "chilli_", 7) == 0;
                if (tile_def(t)->cut_to != TILE_AIR) {
                    tops++;
                    if (tile_def(world_get_at(&world, BG, cx, y + 1))->topped_by != t) loose++;
                }
                if (tile_def(t)->topped_by != TILE_AIR && world_get_at(&world, BG, cx, y - 1) != tile_def(t)->topped_by) loose++;
            }
        }
        world_free(&world);
    }
    CHECK(wild[0] > 3 && wild[1] > 3 && wild[2] > 3 && wild[3] > 3 && tops > 3 && loose == 0);
    CHECK(trees[0] > 20 && trees[1] > 20 && trees[2] > 20 && palms_off_sand == 0);
}

/* Tulips: two colours each, kept by the flower, its bulb and its seed, and bred by planting one on the other. */
static void test_tulips(void)
{
    Game g;
    game_init(&g, 64, 48, 38, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    int s = w->surface_y, x = g.player.feet.x + 2;
    TileType tulip = tile_id("tulip"), from_bulb = tile_id("tulip_bulb_0"), from_seed = tile_id("tulip_seed_0"),
             crossed = tile_id("tulip_cross_0");
    ItemId bulb = item_id("tulip_bulb"), seed = item_id("tulip_seed");
    g.tick_rate = 0;

    /* A pair of colours is one number, and comes back out of it. */
    int body, tip;
    tone_unpack(tone_pack(17, 200), &body, &tip);
    CHECK(body == 17 && tip == 200 && tone_pack(0, 0) == 1 && tone_pack(TONE_COLORS - 1, TONE_COLORS - 1) == TONE_MAX);
    unsigned char rgb[3];
    tone_rgb(tone_nearest(204, 51, 51), rgb);
    CHECK(rgb[0] == 204 && rgb[1] == 51 && rgb[2] == 51 && tone_nearest(255, 255, 255) == TONE_COLORS - 1 && tone_nearest(0, 0, 0) == 0);
    int red = tone_nearest(204, 51, 51), blue = tone_nearest(51, 102, 204), yellow = tone_nearest(255, 204, 51),
        green = tone_nearest(51, 153, 51);

    /* The flower and its three kinds of shoot are two-tone blocks; bulb and seed are two-tone items. */
    CHECK(tulip > 0 && tile_def(tulip)->two_tone && tile_def(tulip)->tone_color_count == 4 && tile_def(tulip)->tone_textures[1][0]);
    CHECK(tile_def(from_bulb)->two_tone && tile_def(from_seed)->two_tone && tile_def(crossed)->two_tone);
    CHECK(tile_def(from_bulb)->grows_into == tulip && tile_def(from_seed)->grows_into == tulip && tile_def(crossed)->grows_into == tulip);
    CHECK(tile_def(tulip)->drop == bulb && tile_def(tulip)->extra_drops[0].id == seed && tile_def(tulip)->grows_top == TILE_AIR);
    CHECK(item_def(bulb)->two_tone && item_def(seed)->two_tone && item_def(bulb)->meter_max == TONE_MAX);
    CHECK(item_def(bulb)->meter_start == tone_pack(red, red) && item_def(bulb)->sows == from_bulb && item_def(seed)->sows == from_seed);
    CHECK(item_def(bulb)->crosses_with == from_seed && item_def(bulb)->crosses_into == crossed);
    CHECK(item_def(seed)->crosses_with == from_bulb && item_def(seed)->crosses_into == crossed);

    /* A flower gives a bulb and a seed, each of its own two colours. */
    clear_inventory(&g);
    world_set_at(w, BG, x, s - 1, tulip);
    world_set_meta(w, x, s - 1, tone_pack(blue, yellow));
    CHECK(game_click_tile(&g, x, s - 1) && g.queue[0].type == ACTION_MINE);
    run(&g, 3.0f);
    CHECK(world_get_at(w, BG, x, s - 1) == TILE_AIR && world_meta(w, x, s - 1) == 0);
    CHECK(count_item(&g, bulb) == 1 && count_item(&g, seed) == 1);
    CHECK(inventory_meter(inv, inventory_find(inv, bulb)) == tone_pack(blue, yellow));
    CHECK(inventory_meter(inv, inventory_find(inv, seed)) == tone_pack(blue, yellow));

    /* Tulips stack only with ones of exactly the same colours. */
    clear_inventory(&g);
    ItemStack one = { (uint16_t)bulb, 1, 0 };
    CHECK(inventory_set_meter(inv, &one, tone_pack(blue, blue)) && inventory_add_stack(inv, one) == 0);
    one = (ItemStack){ (uint16_t)bulb, 1, 0 };
    CHECK(inventory_set_meter(inv, &one, tone_pack(blue, blue)) && inventory_add_stack(inv, one) == 0);
    CHECK(inv->slots[0].count == 2 && inv->slots[1].count == 0);
    one = (ItemStack){ (uint16_t)bulb, 1, 0 };
    CHECK(inventory_set_meter(inv, &one, tone_pack(blue, green)) && inventory_add_stack(inv, one) == 0);
    CHECK(inv->slots[0].count == 2 && inv->slots[1].count == 1 && inv->slots[1].id == bulb);
    CHECK(inventory_add(inv, bulb, 3) == 0); /* plain red ones: a stack of their own again */
    CHECK(inv->slots[0].count == 2 && inv->slots[1].count == 1 && inv->slots[2].count == 3);
    CHECK(inventory_meter(inv, &inv->slots[2]) == tone_pack(red, red) && inventory_count(inv, bulb) == 6);
    /* Nor will one stack be put on top of another that differs; it changes places with it instead. */
    CHECK(inventory_move(inv, (SlotRef){ SLOT_BAR, 1, 0 }, (SlotRef){ SLOT_BAR, 0, 0 }, 1));
    CHECK(inv->slots[0].count == 1 && inv->slots[1].count == 2 && inventory_meter(inv, &inv->slots[0]) == tone_pack(blue, green));
    CHECK(inventory_meter(inv, &inv->slots[1]) == tone_pack(blue, blue));

    /* A bulb planted comes up in its colours. */
    clear_inventory(&g);
    one = (ItemStack){ (uint16_t)bulb, 1, 0 };
    inventory_set_meter(inv, &one, tone_pack(red, red));
    inventory_add_stack(inv, one);
    one = (ItemStack){ (uint16_t)seed, 1, 0 };
    inventory_set_meter(inv, &one, tone_pack(blue, blue));
    inventory_add_stack(inv, one);
    inventory_select(inv, 0, -1);
    CHECK(game_click_tile(&g, x, s) && g.queue[0].type == ACTION_PLANT);
    run(&g, 2.0f);
    CHECK(world_get_at(w, BG, x, s - 1) == from_bulb && world_meta(w, x, s - 1) == tone_pack(red, red) && count_item(&g, bulb) == 0);
    /* A second bulb will not go in on top of it... */
    inventory_add(inv, bulb, 1);
    inventory_select(inv, 0, -1);
    CHECK(game_click_tile(&g, x, s) && g.queue[0].type == ACTION_MINE);
    game_cancel_actions(&g);
    CHECK(game_click_tile(&g, x, s - 1) && g.queue[0].type == ACTION_MINE);
    game_cancel_actions(&g);
    /* ...but a seed will, clicked on the shoot or on the soil under it, and the two are crossed. */
    inventory_select(inv, 1, -1);
    CHECK(inv->slots[1].id == seed && game_click_tile(&g, x, s - 1) && g.queue[0].type == ACTION_PLANT && g.queue[0].target.y == s);
    run(&g, 2.0f);
    CHECK(world_get_at(w, BG, x, s - 1) == crossed && count_item(&g, seed) == 0);
    game_free(&g);

    /* The cross, many times over: one feature is a mix of the parents' - red
     * and blue make something between them - and the other is one parent's whole. */
    int mixed_body = 0, mixed_tip = 0, from_red = 0, from_blue = 0, wrong = 0;
    unsigned char r[3], b[3], m[3];
    tone_rgb(red, r);
    tone_rgb(blue, b);
    for (uint32_t trial = 1; trial <= 200; trial++) {
        Game t;
        game_init(&t, 64, 48, trial, WORLD_FLAT);
        t.tick_rate = 0;
        t.rng = trial * 2654435761u;
        int ts = t.world.surface_y, tx = t.player.feet.x + 2;
        clear_inventory(&t);
        world_set_at(&t.world, BG, tx, ts - 1, from_seed);
        world_set_meta(&t.world, tx, ts - 1, tone_pack(red, red));
        one = (ItemStack){ (uint16_t)bulb, 1, 0 };
        inventory_set_meter(&t.inventory, &one, tone_pack(blue, blue));
        inventory_add_stack(&t.inventory, one);
        inventory_select(&t.inventory, 0, -1);
        if (!game_click_tile(&t, tx, ts)) wrong++;
        run(&t, 2.0f);
        if (world_get_at(&t.world, BG, tx, ts - 1) != crossed) wrong++;
        tone_unpack(world_meta(&t.world, tx, ts - 1), &body, &tip);
        bool body_pure = body == red || body == blue, tip_pure = tip == red || tip == blue;
        if (body_pure == tip_pure) wrong++; /* exactly one of the two is crossed */
        int mix = body_pure ? tip : body, kept = body_pure ? body : tip;
        mixed_body += !body_pure;
        mixed_tip += !tip_pure;
        from_red += kept == red;
        from_blue += kept == blue;
        tone_rgb(mix, m);
        for (int k = 0; k < 3; k++) {
            int lo = (r[k] + b[k]) / 2 - 26, hi = (r[k] + b[k]) / 2 + 26;
            if (m[k] < lo || m[k] > hi) wrong++;
        }
        /* It grows into a flower of those colours, and that gives a bulb and a seed of them. */
        if (trial == 1) {
            int bred = world_meta(&t.world, tx, ts - 1);
            game_random_event(&t, tx, ts - 1);
            CHECK(world_get_at(&t.world, BG, tx, ts - 1) == tulip && world_meta(&t.world, tx, ts - 1) == bred);
            /* Saved and loaded, it is still that flower. */
            CHECK(save_game(&t, "build/test_tulip.world"));
            Game loaded;
            CHECK(load_game(&loaded, "build/test_tulip.world"));
            CHECK(world_get_at(&loaded.world, BG, tx, ts - 1) == tulip && world_meta(&loaded.world, tx, ts - 1) == bred);
            CHECK(world_meta(&loaded.world, tx + 1, ts - 1) == 0);
            game_free(&loaded);
            CHECK(game_click_tile(&t, tx, ts - 1));
            run(&t, 3.0f);
            CHECK(count_item(&t, bulb) == 1 && count_item(&t, seed) == 1);
            CHECK(inventory_meter(&t.inventory, inventory_find(&t.inventory, bulb)) == bred);
            CHECK(inventory_meter(&t.inventory, inventory_find(&t.inventory, seed)) == bred);
        }
        game_free(&t);
    }
    CHECK(wrong == 0 && mixed_body > 50 && mixed_tip > 50 && from_red > 50 && from_blue > 50);

    /* Wild ones are all of a colour, one of the four. */
    int wild = 0, odd = 0, seen[4] = { 0 };
    for (uint32_t seed_ = 1; seed_ <= 6; seed_++) {
        World world;
        CHECK(world_init(&world, 2048, 401, seed_, WORLD_TERRAIN));
        for (int y = 1; y < world.height; y++) {
            for (int cx = 0; cx < world.width; cx++) {
                if (world_get_at(&world, BG, cx, y) != tulip) {
                    if (world_meta(&world, cx, y) != 0) odd++;
                    continue;
                }
                wild++;
                tone_unpack(world_meta(&world, cx, y), &body, &tip);
                int which = body == red ? 0 : body == blue ? 1 : body == yellow ? 2 : body == green ? 3 : -1;
                if (world_meta(&world, cx, y) == 0 || body != tip || which < 0) odd++;
                else seen[which]++;
            }
        }
        world_free(&world);
    }
    CHECK(wild > 20 && odd == 0 && seen[0] > 0 && seen[1] > 0 && seen[2] > 0 && seen[3] > 0);
}

/* Scorpions, which poison; yaks, which are shorn; and the shards that meditation and golden chests give. */
static void test_scorpions_and_yaks(void)
{
    Game g;
    game_init(&g, 96, 48, 39, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    clear_mobs(&g);
    g.tick_rate = 0;
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;
    int scorpion = mob_id("scorpion"), yak = mob_id("yak"), dodo = mob_id("dodo");
    MobDef *sc = &content.mobs[scorpion], *yk = &content.mobs[yak];
    int desert = -1, tropics = -1;
    for (int b = 0; b < content.biome_count; b++) {
        if (strcmp(content.biomes[b].name, "desert") == 0) desert = b;
        if (strcmp(content.biomes[b].name, "tropics") == 0) tropics = b;
    }

    /* A scorpion: 1 to 3 poison when killed, 40 ticks of poison when it stings. Only in the desert. */
    CHECK(scorpion >= 0 && sc->sting_ticks == 40 && sc->drop_count == 1 && sc->drops[0].item == item_id("poison"));
    CHECK(sc->drops[0].min == 1 && sc->drops[0].max == 3 && sc->spawn_biome_count == 1 && sc->spawn_biomes[0] == desert);
    CHECK(content.rules.poison_damage > 0.0f);
    sc->spawn_chance = 1.0f;
    content.mobs[dodo].spawn_chance = 0.0f;
    for (int x = px + 10; x < px + 20; x++) world_set(w, x, s, tile_id("sand"));
    CHECK(!mobs_try_spawn(&g, px + 12, ground)); /* sand, but not desert */
    for (int x = px + 10; x < px + 20; x++) w->biome[x] = (uint8_t)desert;
    CHECK(mobs_try_spawn(&g, px + 12, ground) && count_mobs(&g) == 1);
    CHECK(!mobs_try_spawn(&g, px + 5, ground)); /* desert it may be, but that is grass */
    clear_mobs(&g);
    sc->spawn_chance = 0.0f;

    /* It makes for a player it can see, and stings when it gets there. */
    float saved_move = sc->move_chance;
    sc->move_chance = 0.0f;
    int m = mob_spawn(&g, scorpion, px + 5, ground, true, false);
    g.vitals.health = 1.0f;
    CHECK(g.vitals.poison == 0);
    for (int t = 0; t < 60 && g.vitals.poison == 0; t++) mobs_tick(&g);
    CHECK(abs(g.mobs[m].x - px) <= 1 && g.vitals.poison == 40 && g.mobs[m].timer > 0);
    /* Forty ticks of it, each taking its share of health, and then it is over. */
    float before = g.vitals.health;
    g.mobs[m].active = false;
    for (int t = 0; t < 40; t++) game_tick(&g);
    CHECK(g.vitals.poison == 0 && g.vitals.health < before - 40.0f * content.rules.poison_damage * 0.9f &&
          g.vitals.health > before - 40.0f * content.rules.poison_damage * 1.1f);
    before = g.vitals.health;
    for (int t = 0; t < 10; t++) game_tick(&g);
    CHECK(g.vitals.health >= before);
    /* A second sting while the first is at work does not stack: it lasts 40 ticks from then. */
    game_poison(&g, 40);
    for (int t = 0; t < 10; t++) game_tick(&g);
    game_poison(&g, 40);
    CHECK(g.vitals.poison == 40);
    /* It wakes a sleeper, and goes with whoever it kills. */
    g.vitals.energy = 0.2f;
    CHECK(game_rest(&g, REST_SLEEP) && g.vitals.rest == REST_SLEEP);
    game_poison(&g, 5);
    CHECK(g.vitals.rest == REST_NONE);
    g.vitals.poison = 0;
    /* Having stung, it leaves off for a while. */
    m = mob_spawn(&g, scorpion, px + 1, ground, true, false);
    mobs_tick(&g);
    CHECK(g.vitals.poison == 40 && g.mobs[m].timer == sc->sting_rest);
    g.vitals.poison = 0;
    for (int t = 0; t < sc->sting_rest - 2; t++) mobs_tick(&g);
    CHECK(g.vitals.poison == 0);
    for (int t = 0; t < 4; t++) mobs_tick(&g);
    CHECK(g.vitals.poison == 40);
    g.vitals.poison = 0;
    g.vitals.health = 1.0f;
    /* Killed, it leaves poison. */
    int least = 99, most = 0;
    for (int k = 0; k < 40; k++) {
        clear_drops(&g);
        mob_kill(&g, mob_spawn(&g, scorpion, px + 6, ground, true, false));
        int n = drops_of(&g, item_id("poison"));
        if (n < least) least = n;
        if (n > most) most = n;
    }
    CHECK(least == 1 && most == 3);
    clear_mobs(&g);
    clear_drops(&g);
    sc->move_chance = saved_move;

    /* A yak: on grass in the forest biomes, not in the tropics. */
    CHECK(yak >= 0 && yk->shear_item == item_id("yak_shavings") && yk->shear_min == 1 && yk->shear_max == 3);
    CHECK(yk->drop_count == 3 && yk->drops[0].item == item_id("yak_horn") && yk->drops[0].min == 1 && yk->drops[0].max == 1);
    CHECK(yk->spawn_biome_count >= 4 && item_def(item_id("razor"))->shears && yk->sting_ticks == 0);
    bool in_plains = false, in_tropics = false, in_desert = false;
    for (int i = 0; i < yk->spawn_biome_count; i++) {
        in_plains = in_plains || yk->spawn_biomes[i] == 0;
        in_tropics = in_tropics || yk->spawn_biomes[i] == tropics;
        in_desert = in_desert || yk->spawn_biomes[i] == desert;
    }
    CHECK(in_plains && !in_tropics && !in_desert);
    yk->spawn_chance = 1.0f;
    for (int x = px + 26; x <= px + 32; x++) world_set(w, x, s, TILE_GRASS);
    w->biome[world_wrap_x(w, px + 30)] = (uint8_t)tropics;
    CHECK(!mobs_try_spawn(&g, px + 30, ground) && count_mobs(&g) == 0);
    CHECK(mobs_try_spawn(&g, px + 28, ground) && count_mobs(&g) == 1);
    /* Where dodos live too, both turn up. */
    content.mobs[dodo].spawn_chance = 0.5f;
    int dodos = 0, yaks = 0;
    for (int k = 0; k < 200; k++) {
        clear_mobs(&g);
        if (!mobs_try_spawn(&g, px + 28, ground)) continue;
        for (int i = 0; i < MAX_MOBS; i++) {
            dodos += g.mobs[i].active && g.mobs[i].kind == dodo;
            yaks += g.mobs[i].active && g.mobs[i].kind == yak;
        }
    }
    CHECK(dodos > 40 && yaks > 40);
    yk->spawn_chance = 0.0f;
    clear_mobs(&g);

    /* A razor on a yak takes 1 to 3 shavings off it and does it no harm. */
    saved_move = yk->move_chance;
    yk->move_chance = 0.0f;
    clear_inventory(&g);
    inventory_add(inv, item_id("razor"), 1);
    inventory_select(inv, 0, -1);
    m = mob_spawn(&g, yak, px + 2, ground, true, false);
    int hp = g.mobs[m].hp;
    CHECK(hp == yk->health && !mob_shorn(&g.mobs[m]));
    CHECK(game_click_mob(&g, m) && g.queue[0].type == ACTION_ATTACK && g.queue[0].arg == HAND_SHEAR);
    run(&g, 3.0f);
    int shavings = count_item(&g, item_id("yak_shavings")) + drops_of(&g, item_id("yak_shavings"));
    CHECK(g.mobs[m].active && g.mobs[m].hp == hp && shavings >= 1 && shavings <= 3 && mob_shorn(&g.mobs[m]) && is_idle(&g));
    CHECK(count_item(&g, item_id("razor")) == 1 && count_item(&g, item_id("yak_horn")) + drops_of(&g, item_id("yak_horn")) == 0);
    /* Shorn, it has nothing more to give until it has grown back. */
    CHECK(game_click_mob(&g, m));
    run(&g, 3.0f);
    CHECK(count_item(&g, item_id("yak_shavings")) + drops_of(&g, item_id("yak_shavings")) == shavings && g.mobs[m].hp == hp);
    CHECK(!mob_shear(&g, m));
    for (int t = 0; t < yk->shear_ticks; t++) mobs_tick(&g);
    CHECK(!mob_shorn(&g.mobs[m]) && mob_shear(&g, m) && mob_shorn(&g.mobs[m]));
    /* Without the razor in hand a click is a blow, as on anything. */
    inventory_select(inv, 3, -1);
    CHECK(game_click_mob(&g, m) && g.queue[0].arg == 0);
    run(&g, 3.0f);
    CHECK(g.mobs[m].hp == hp - content.rules.hand_damage);
    /* A razor does nothing to a creature that is not shorn but hit it. */
    inventory_select(inv, 0, -1);
    content.mobs[dodo].move_chance = 0.0f;
    int bird = mob_spawn(&g, dodo, px + 1, ground, true, false);
    CHECK(game_click_mob(&g, bird) && g.queue[0].arg == 0);
    game_cancel_actions(&g);
    content.mobs[dodo].move_chance = 0.2f;
    /* Killed, a yak leaves one horn. */
    clear_drops(&g);
    mob_kill(&g, m);
    CHECK(drops_of(&g, item_id("yak_horn")) == 1);
    yk->move_chance = saved_move;
    game_free(&g);

    /* Poison lasts through a save. */
    Game p;
    game_init(&p, 64, 48, 40, WORLD_FLAT);
    game_poison(&p, 33);
    CHECK(save_game(&p, "build/test_poison.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_poison.world") && loaded.vitals.poison == 33);
    game_free(&loaded);
    game_free(&p);

    /* Meditating and golden chests give shards of the gems now, not the gems. */
    static const char *gems[] = { "amethyst", "sapphire", "emerald", "ruby", "diamond" };
    for (int k = 0; k < 5; k++) {
        char shard[48];
        snprintf(shard, sizeof shard, "%s_shard", gems[k]);
        bool whole = false, part = false;
        for (int i = 0; i < content.rules.loot_count; i++) {
            whole = whole || content.rules.loot[i].item == item_id(gems[k]);
            part = part || content.rules.loot[i].item == item_id(shard);
        }
        CHECK(!whole && part && item_id(shard) > 0);
    }
}

/* Steps in the way are walked past; a bed is gone to and slept in; the book lists things in order. */
static void test_steps_beds_and_order(void)
{
    Game g;
    game_init(&g, 128, 64, 91, WORLD_FLAT);
    World *w = &g.world;
    g.tick_rate = 0;
    clear_mobs(&g);
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;
    TileType stairs = tile_id("stone_stairs");

    /* One flight of steps standing by itself in the way: the keys walk past it, not up it and down again. */
    world_set_at(w, BG, px + 3, ground, stairs);
    int highest = ground;
    game_set_move_input(&g, 1, 0);
    for (int i = 0; i < 300 && g.player.feet.x < px + 6; i++) {
        game_update(&g, 1.0f / 60.0f);
        if (g.player.feet.y < highest) highest = g.player.feet.y;
    }
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(g.player.feet.x >= px + 6 && highest == ground && g.player.feet.y == ground);
    /* So does a route somewhere beyond it. */
    highest = ground;
    CHECK(game_queue_action(&g, ACTION_MOVE, px, ground));
    for (int i = 0; i < 600 && !(is_idle(&g) && g.player.feet.x == px); i++) {
        game_update(&g, 1.0f / 60.0f);
        if (g.player.feet.y < highest) highest = g.player.feet.y;
    }
    CHECK(g.player.feet.x == px && highest == ground);
    /* With up held as well they are gone up; and steps that lead on to a floor are gone up without it. */
    g.player.feet = g.player.step_to = (TilePos){ px + 2, ground };
    highest = ground;
    game_set_move_input(&g, 1, -1);
    for (int i = 0; i < 40; i++) {
        game_update(&g, 1.0f / 60.0f);
        if (g.player.feet.y < highest) highest = g.player.feet.y;
    }
    game_set_move_input(&g, 0, 0);
    CHECK(highest == ground - 1);
    run(&g, 1.0f);
    for (int x = px + 4; x <= px + 8; x++) world_set(w, x, ground, TILE_STONE);
    g.player.feet = g.player.step_to = (TilePos){ px + 2, ground };
    highest = ground;
    game_set_move_input(&g, 1, 0);
    for (int i = 0; i < 40; i++) {
        game_update(&g, 1.0f / 60.0f);
        if (g.player.feet.y < highest) highest = g.player.feet.y;
    }
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(highest == ground - 1);
    for (int x = px + 3; x <= px + 8; x++) world_set(w, x, ground, TILE_AIR), world_set_at(w, BG, x, ground, TILE_AIR);

    /* A bed: found at either half; told to sleep in it, the character goes there and lies down. */
    TileType bed = tile_id("wooden_bed"), foot = tile_id("wooden_bed_foot");
    world_set_at(w, tile_def(bed)->layer, px - 8, ground, bed);
    world_set_at(w, tile_def(foot)->layer, px - 7, ground, foot);
    CHECK(game_bed_at(&g, px - 8, ground) == bed && game_bed_at(&g, px - 7, ground) == bed && game_bed_at(&g, px - 6, ground) == TILE_AIR);
    CHECK(game_bench_at(&g, px - 8, ground) == TILE_AIR); /* (it is not something that is opened) */
    g.player.feet = g.player.step_to = (TilePos){ px, ground };
    g.vitals.energy = 0.3f;
    CHECK(!game_sleep_in(&g, px - 5, ground) && game_sleep_in(&g, px - 7, ground));
    run(&g, 6.0f);
    CHECK(g.player.feet.x == px - 7 && g.player.feet.y == ground && g.vitals.rest == REST_SLEEP);
    CHECK(game_sleep_speed(&g, NULL) == tile_def(bed)->sleep_speed);
    game_wake(&g);
    /* (Wide awake, they go to it and stand there.) */
    g.player.feet = g.player.step_to = (TilePos){ px, ground };
    g.vitals.energy = 1.0f;
    CHECK(game_sleep_in(&g, px - 8, ground));
    run(&g, 6.0f);
    CHECK(g.player.feet.x == px - 8 && g.vitals.rest == REST_NONE && is_idle(&g));

    /* The book: every item once, by category in the order the categories come, and by name within one. */
    static ItemId order[MAX_ITEMS];
    static uint8_t met[MAX_ITEMS];
    int n = guide_order(order), out_of_order = 0;
    memset(met, 0, sizeof met);
    CHECK(n == content.item_count - 1);
    for (int i = 0; i < n; i++) {
        met[order[i]]++;
        if (i == 0) continue;
        const ItemDef *a = item_def(order[i - 1]), *b = item_def(order[i]);
        int ca = a->category < 0 ? 1000 : a->category, cb = b->category < 0 ? 1000 : b->category;
        if (ca > cb) out_of_order++;
        if (ca == cb) {
            char la[CONTENT_NAME_LEN * 2], lb[CONTENT_NAME_LEN * 2];
            snprintf(la, sizeof la, "%s", a->label);
            snprintf(lb, sizeof lb, "%s", b->label);
            for (char *c = la; *c; c++) *c = (char)tolower((unsigned char)*c);
            for (char *c = lb; *c; c++) *c = (char)tolower((unsigned char)*c);
            if (strcmp(la, lb) > 0) out_of_order++;
        }
    }
    for (ItemId i = 1; i < content.item_count; i++) CHECK(met[i] == 1);
    CHECK(out_of_order == 0);
    game_free(&g);
}

/* Columns and stairs in the background, and stairs to walk up; vines; worms; milk; leather; the mango tree. */
static void test_stairs_and_more(void)
{
    Game g;
    game_init(&g, 96, 48, 41, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    clear_mobs(&g);
    g.tick_rate = 0;
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;
    TileType stairs = tile_id("stone_stairs"), column = tile_id("marble_column");

    /* Every column and every flight of stairs is a block of the background, walked through. */
    int columns = 0, flights = 0;
    for (ItemId i = 1; i < content.item_count; i++) {
        const char *name = content.items[i].name;
        size_t len = strlen(name);
        bool is_column = len > 7 && strcmp(name + len - 7, "_column") == 0, is_stairs = len > 7 && strcmp(name + len - 7, "_stairs") == 0;
        if (!is_column && !is_stairs) continue;
        const TileDef *def = tile_def(content.items[i].places);
        CHECK(content.items[i].places == tile_id(name) && !def->solid && def->layer == LAYER_BACKGROUND && def->removable);
        CHECK(def->drop == i && def->steps == is_stairs && def->attaches && !def->wall);
        columns += is_column;
        flights += is_stairs;
    }
    CHECK(columns == 27 && flights == 27 && content.tile_count > 256);

    /* A flight of three: steps are not solid, but they hold up whoever is on top of them. */
    for (int k = 0; k < 3; k++) world_set_at(w, BG, px + 2 + k, ground - k, stairs);
    CHECK(!world_is_solid(w, px + 2, ground) && world_is_step(w, px + 2, ground) && !world_is_step(w, px + 1, ground));
    /* Up them, a step at a time, to the top. */
    CHECK(game_click_tile(&g, px + 4, ground - 3));
    Trace up = run_traced(&g, 10.0f);
    CHECK(g.player.feet.x == px + 4 && g.player.feet.y == ground - 3 && is_idle(&g) && !g.player.climbing);
    CHECK(!up.climbed && !up.fell); /* walked, not climbed */
    run(&g, 2.0f);
    CHECK(g.player.feet.x == px + 4 && g.player.feet.y == ground - 3); /* and there they stand */
    /* Down again the same way. */
    CHECK(game_click_tile(&g, px, ground));
    run(&g, 10.0f);
    CHECK(g.player.feet.x == px && g.player.feet.y == ground && is_idle(&g));
    /* Or straight through them, along the ground. */
    CHECK(game_click_tile(&g, px + 7, ground));
    Trace through = run_traced(&g, 10.0f);
    CHECK(g.player.feet.x == px + 7 && g.player.feet.y == ground && !through.climbed);
    /* With the keys: walking into steps goes up them; with down held as well, through them. */
    g.player.feet = g.player.step_to = (TilePos){ px + 1, ground };
    game_set_move_input(&g, 1, 0);
    run(&g, content.rules.walk_time * 1.5f);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(g.player.feet.y < ground && g.player.feet.x >= px + 2);
    g.player.feet = g.player.step_to = (TilePos){ px + 1, ground };
    game_set_move_input(&g, 1, 1);
    run(&g, content.rules.walk_time * 2.5f);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(g.player.feet.y == ground && g.player.feet.x >= px + 2);
    /* Something that falls onto steps lands on them. */
    g.player.feet = g.player.step_to = (TilePos){ px + 3, ground - 8 };
    run(&g, 3.0f);
    CHECK(g.player.feet.x == px + 3 && g.player.feet.y == ground - 2);
    for (int k = 0; k < 3; k++) world_set_at(w, BG, px + 2 + k, ground - k, TILE_AIR);
    run(&g, 2.0f);
    CHECK(g.player.feet.y == ground);

    /* Putting them up: steps hold on to the steps a step below them, and columns to the column under them;
     * other things in the background still need a block or a backwall. */
    g.player.feet = g.player.step_to = (TilePos){ px, ground };
    clear_inventory(&g);
    inventory_add(inv, item_id("stone_stairs"), 3);
    inventory_add(inv, item_id("marble_column"), 3);
    inventory_add(inv, item_id("workbench"), 1);
    inventory_select(inv, 0, -1);
    CHECK(!game_queue_action(&g, ACTION_PLACE, px + 3, ground - 1)); /* in mid air */
    CHECK(game_queue_action(&g, ACTION_PLACE, px + 2, ground));
    run(&g, 3.0f);
    CHECK(world_get_at(w, BG, px + 2, ground) == stairs);
    CHECK(game_queue_action(&g, ACTION_PLACE, px + 3, ground - 1));
    run(&g, 3.0f);
    CHECK(world_get_at(w, BG, px + 3, ground - 1) == stairs && count_item(&g, item_id("stone_stairs")) == 1);
    inventory_select(inv, 1, -1);
    CHECK(game_queue_action(&g, ACTION_PLACE, px - 2, ground));
    run(&g, 3.0f);
    CHECK(game_queue_action(&g, ACTION_PLACE, px - 2, ground - 1));
    run(&g, 3.0f);
    CHECK(world_get_at(w, BG, px - 2, ground) == column && world_get_at(w, BG, px - 2, ground - 1) == column);
    inventory_select(inv, 2, -1);
    /* (A bench will stand on a column as on a block, but not hang beside one in the air.) */
    CHECK(inv->slots[2].id == item_id("workbench") && !game_queue_action(&g, ACTION_PLACE, px - 3, ground - 1 - 1));
    CHECK(game_queue_action(&g, ACTION_PLACE, px - 2, ground - 2));
    game_cancel_actions(&g);
    /* Taken down, each gives itself back. */
    clear_inventory(&g);
    CHECK(game_click_tile(&g, px - 2, ground - 1) && g.queue[0].type == ACTION_MINE);
    run(&g, 5.0f);
    CHECK(world_get_at(w, BG, px - 2, ground - 1) == TILE_AIR && count_item(&g, item_id("marble_column")) == 1);
    /* Saved with the world (and the ids of these are past what a byte holds). */
    CHECK(save_game(&g, "build/test_stairs.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_stairs.world"));
    CHECK(world_get_at(&loaded.world, BG, px + 3, ground - 1) == stairs && world_get_at(&loaded.world, BG, px - 2, ground) == column);
    CHECK(world_get(&loaded.world, px, s) == world_get(w, px, s));
    WorldPreview preview;
    CHECK(save_read_preview("build/test_stairs.world", &preview));
    game_free(&loaded);

    /* Worms: one time in ten, digging dirt. */
    TileDef *dirt = tile_def_mut(TILE_DIRT);
    CHECK(dirt->drop == item_id("dirt") && dirt->extra_drop_count == 1 && dirt->extra_drops[0].id == item_id("worms"));
    dirt->extra_chance[0] = 0.1f;
    int worms = 0, earth = 0;
    for (int k = 0; k < 300; k++) {
        clear_inventory(&g);
        clear_drops(&g);
        world_set(w, px + 1, ground, TILE_DIRT);
        if (!game_click_tile(&g, px + 1, ground)) continue;
        run(&g, 1.2f);
        worms += count_item(&g, item_id("worms")) + drops_of(&g, item_id("worms"));
        earth += count_item(&g, item_id("dirt")) + drops_of(&g, item_id("dirt"));
    }
    CHECK(earth == 300 && worms >= 12 && worms <= 55);
    dirt->extra_chance[0] = 0.0f;
    CHECK(tile_def(TILE_GRASS)->extra_drop_count == 0);

    /* Milk: an empty bucket on a yak. */
    int yak = mob_id("yak"), bear = mob_id("dropbear");
    MobDef *yk = &content.mobs[yak];
    float saved_move = yk->move_chance;
    yk->move_chance = 0.0f;
    ItemId bucket = item_id("bucket"), milk = item_id("milk");
    content.items[bucket].milks = true; /* (the bucket these tests run with is the one from before it could) */
    CHECK(yk->milk_item == milk && yk->milk_ticks > 0 && !item_def(item_id("razor"))->milks);
    clear_inventory(&g);
    clear_drops(&g);
    inventory_add(inv, bucket, 1);
    inventory_select(inv, 0, -1);
    int m = mob_spawn(&g, yak, px + 2, ground, true, false);
    int hp = g.mobs[m].hp;
    CHECK(game_click_mob(&g, m) && g.queue[0].type == ACTION_ATTACK && g.queue[0].arg == HAND_MILK);
    run(&g, 3.0f);
    CHECK(count_item(&g, milk) + drops_of(&g, milk) == 1 && count_item(&g, bucket) == 1 && g.mobs[m].hp == hp && is_idle(&g));
    /* It has no more for a while. */
    CHECK(game_click_mob(&g, m));
    run(&g, 3.0f);
    CHECK(count_item(&g, milk) + drops_of(&g, milk) == 1 && !mob_milk(&g, m));
    for (int t = 0; t < yk->milk_ticks; t++) mobs_tick(&g);
    /* A bucket with water in it will not do. */
    inventory_set_meter(inv, inventory_find(inv, bucket), 3);
    inventory_select_slot(inv, 0);
    CHECK(inventory_selected(inv)->id == bucket && game_click_mob(&g, m));
    run(&g, 3.0f);
    CHECK(count_item(&g, milk) + drops_of(&g, milk) == 1);
    inventory_set_meter(inv, inventory_find(inv, bucket), 0);
    CHECK(game_click_mob(&g, m));
    run(&g, 3.0f);
    CHECK(count_item(&g, milk) + drops_of(&g, milk) == 2 && g.mobs[m].hp == hp);
    /* Being shorn and being milked are separate matters. */
    CHECK(!mob_shorn(&g.mobs[m]) && mob_shear(&g, m) && mob_shorn(&g.mobs[m]));
    clear_mobs(&g);
    yk->move_chance = saved_move;

    /* Leather: 1 or 2 from a yak or a dropbear, and a fur half the time. */
    for (int kind = 0; kind < 2; kind++) {
        int which = kind == 0 ? yak : bear, least = 99, most = 0, furs = 0, horns = 0;
        for (int k = 0; k < 100; k++) {
            clear_drops(&g);
            mob_kill(&g, mob_spawn(&g, which, px + 5, ground, true, false));
            int n = drops_of(&g, item_id("leather"));
            if (n < least) least = n;
            if (n > most) most = n;
            CHECK(drops_of(&g, item_id("fur")) <= 1);
            furs += drops_of(&g, item_id("fur"));
            horns += drops_of(&g, item_id("yak_horn"));
        }
        CHECK(least == 1 && most == 2 && furs > 30 && furs < 70 && horns == (kind == 0 ? 100 : 0));
    }
    clear_drops(&g);

    /* A vine that is cut comes away from the cut downward. */
    TileType vine = tile_id("vine");
    CHECK(vine > 0 && !tile_def(vine)->solid && tile_def(vine)->climbable && tile_def(vine)->breaks_below && tile_def(vine)->drop == item_id("vine"));
    world_set(w, px + 6, ground - 6, TILE_STONE);
    for (int k = 1; k <= 5; k++) world_set_at(w, BG, px + 6, ground - 6 + k, vine);
    clear_inventory(&g);
    CHECK(game_click_tile(&g, px + 6, ground - 3) && g.queue[0].type == ACTION_MINE);
    run(&g, 6.0f);
    CHECK(world_get_at(w, BG, px + 6, ground - 5) == vine && world_get_at(w, BG, px + 6, ground - 4) == vine);
    for (int k = 3; k <= 5; k++) CHECK(world_get_at(w, BG, px + 6, ground - 6 + k) == TILE_AIR);
    run(&g, 3.0f);
    CHECK(count_item(&g, item_id("vine")) + drops_of(&g, item_id("vine")) == 3);
    game_free(&g);

    /* The mango tree, and vines, in worlds as they are generated. */
    int mango = tree_id("mango"), tropics = -1;
    for (int b = 0; b < content.biome_count; b++)
        if (strcmp(content.biomes[b].name, "tropics") == 0) tropics = b;
    CHECK(mango >= 0 && content.trees[mango].fruit == item_id("mango") && item_def(item_id("mango"))->plants == mango);
    CHECK(content.trees[mango].log == tile_id("mango_log") && tile_def(tile_id("mango_leaves"))->decays_into == tile_id("mango_leaves_decayed"));
    bool grows = false;
    for (int i = 0; tropics >= 0 && i < content.biomes[tropics].tree_count; i++) grows = grows || content.biomes[tropics].trees[i].tree == mango;
    CHECK(grows);
    int mangoes = 0, hanging = 0, loose = 0, long_ones = 0, longest = 0, outside = 0;
    for (uint32_t seed = 1; seed <= 4; seed++) {
        World world;
        CHECK(world_init(&world, 2048, 401, seed, WORLD_TERRAIN));
        for (int x = 0; x < world.width; x++) {
            for (int y = 1; y < world.height - 1; y++) {
                TileType t = world_get_at(&world, BG, x, y);
                if (t == tile_id("mango_log")) mangoes++;
                if (t != vine || world_get_at(&world, BG, x, y - 1) == vine) continue;
                /* The top of one: it hangs from a block, in a cave, and is no longer than 5. */
                hanging++;
                if (!world_is_solid(&world, x, y - 1)) loose++;
                if (world_get_at(&world, LAYER_WALL, x, y) == TILE_AIR) outside++;
                int len = 0;
                while (world_get_at(&world, BG, x, y + len) == vine) {
                    if (world_get(&world, x, y + len) != TILE_AIR) loose++;
                    len++;
                }
                if (len > 5) long_ones++;
                if (len > longest) longest = len;
            }
        }
        world_free(&world);
    }
    CHECK(mangoes > 20 && hanging > 100 && loose == 0 && outside == 0 && long_ones == 0 && longest >= 4);
}

static uint64_t day_ticks_total(void)
{
    return (uint64_t)(content.rules.day_ticks + content.rules.night_ticks);
}

/* The year and its weather: seasons, rain and snow, water freezing and thawing, and how many random events there are. */
static void test_seasons_and_weather(void)
{
    const RulesDef *u = &content.rules;
    Game g;
    game_init(&g, 64, 200, 42, WORLD_FLAT);
    World *w = &g.world;
    clear_mobs(&g);
    g.tick_rate = 0;
    int s = w->surface_y, px = g.player.feet.x;
    uint64_t day = day_ticks_total(), season = day * (uint64_t)u->season_days;

    /* Four seasons of ten days: a year of forty. */
    CHECK(u->season_count == 4 && u->season_days == 10);
    CHECK(strcmp(u->seasons[0].label, "Spring") == 0 && strcmp(u->seasons[3].label, "Winter") == 0);
    CHECK(u->seasons[0].thaws && !u->seasons[0].freezes && u->seasons[3].freezes && u->seasons[3].snows && !u->seasons[1].snows);
    int year, date, tick;
    g.tick = 0;
    game_date(&g, &year, &date, &tick);
    CHECK(game_season(&g) == 0 && year == 1 && date == 1 && tick == 0);
    g.tick = season - 1;
    CHECK(game_season(&g) == 0);
    g.tick = season;
    game_date(&g, &year, &date, &tick);
    CHECK(game_season(&g) == 1 && year == 1 && date == 11 && tick == 0);
    g.tick = season * 3 + day * 4 + 123;
    game_date(&g, &year, &date, &tick);
    CHECK(game_season(&g) == 3 && year == 1 && date == 35 && tick == 123);
    g.tick = season * 4 + 5;
    game_date(&g, &year, &date, &tick);
    CHECK(game_season(&g) == 0 && year == 2 && date == 1 && tick == 5);

    /* Rain: there or not for a spell at a time, about as often as the rules say, and always the same for the same moment. */
    int wet = 0, spells = 2000;
    for (int k = 0; k < spells; k++) {
        g.tick = (uint64_t)k * (uint64_t)u->weather_ticks + (uint64_t)u->weather_ticks / 2;
        float rain = game_rain(&g);
        CHECK(rain == 0.0f || rain == 1.0f); /* (in the middle of a spell it is one or the other) */
        CHECK(game_rain(&g) == rain);
        wet += rain > 0.0f;
    }
    CHECK(wet > spells * 22 / 100 && wet < spells * 38 / 100);
    /* It comes on and eases off. */
    uint64_t rainy = 0;
    for (uint64_t k = 1; k < 5000 && rainy == 0; k++) {
        g.tick = k * (uint64_t)u->weather_ticks + (uint64_t)u->weather_ticks / 2;
        if (game_rain(&g) > 0.0f) {
            g.tick -= (uint64_t)u->weather_ticks;
            if (game_rain(&g) == 0.0f) rainy = k;
        }
    }
    CHECK(rainy > 0);
    g.tick = rainy * (uint64_t)u->weather_ticks + 2;
    CHECK(game_rain(&g) > 0.0f && game_rain(&g) < 0.2f);

    /* A pool two deep, between walls. */
    int left = px + 5, right = px + 12, top = s - 2;
    for (int y = top; y < s; y++) {
        world_set(w, left - 1, y, TILE_STONE);
        world_set(w, right + 1, y, TILE_STONE);
        for (int x = left; x <= right; x++) world_set_water(w, x, y, WATER_FULL);
    }
    world_set_water(w, left, top, 5); /* (one square of the top not full) */
    TileType ice = tile_id("ice");
    CHECK(u->ice == ice && ice > 0 && tile_def(ice)->solid && tile_def(ice)->drop == item_id("ice"));
    /* Nothing freezes in the summer. */
    g.tick = season + 10;
    for (int k = 0; k < 200; k++)
        for (int x = left; x <= right; x++) game_weather_event(&g, x, top);
    for (int x = left; x <= right; x++) CHECK(world_get(w, x, top) == TILE_AIR && world_water(w, x, top) > 0);
    /* In winter the open top of it does - and only the top. */
    g.tick = season * 3 + 10;
    for (int k = 0; k < 200; k++) {
        for (int x = left; x <= right; x++) {
            game_weather_event(&g, x, top);
            game_weather_event(&g, x, top + 1);
        }
    }
    for (int x = left; x <= right; x++) {
        CHECK(world_get(w, x, top) == ice && world_water(w, x, top) == 0);
        CHECK(world_get(w, x, top + 1) == TILE_AIR && world_water(w, x, top + 1) == WATER_FULL);
    }
    CHECK(world_meta(w, left, top) == 5 && world_meta(w, right, top) == WATER_FULL);
    /* It stays frozen through the winter, and is ice to stand on. */
    CHECK(world_is_solid(w, left + 2, top));
    /* In spring it thaws, to the water it was. */
    g.tick = season * 4 + 10;
    for (int k = 0; k < 200; k++)
        for (int x = left; x <= right; x++) game_weather_event(&g, x, top);
    for (int x = left; x <= right; x++) CHECK(world_get(w, x, top) == TILE_AIR && world_meta(w, x, top) == 0);
    int total = 0;
    for (int y = top; y < s; y++)
        for (int x = left; x <= right; x++) total += world_water(w, x, y);
    CHECK(total == (right - left + 1) * 2 * WATER_FULL - 3);

    /* Snow: in winter, while it is coming down, it lies - up to half a block deep and no deeper. */
    CHECK(u->snow_max == 4 && u->snow_line == 50);
    uint64_t snowing = 0;
    for (uint64_t t = season * 3; t < season * 4 && snowing == 0; t += (uint64_t)u->weather_ticks) {
        g.tick = t + (uint64_t)u->weather_ticks / 2;
        if (game_rain(&g) > 0.0f) snowing = g.tick;
    }
    CHECK(snowing > 0 && game_snow(&g, px) == 0);
    for (int k = 0; k < 20000; k++) game_weather_event(&g, px, 3);
    CHECK(g.snow[px] == 4 && game_snow(&g, px) == 4 && game_snow(&g, px + 1) == 0);
    CHECK(game_sky_floor(&g, px, false) == s);
    /* None on the water. */
    for (int k = 0; k < 20000; k++) game_weather_event(&g, left + 1, 3);
    CHECK(game_snow(&g, left + 1) == 0);
    /* Not while it is fine. */
    uint64_t fine = 0;
    for (uint64_t t = season * 3; t < season * 4 && fine == 0; t += (uint64_t)u->weather_ticks) {
        g.tick = t + (uint64_t)u->weather_ticks / 2;
        if (game_rain(&g) == 0.0f) fine = g.tick;
    }
    for (int k = 0; k < 20000; k++) game_weather_event(&g, px + 1, 3);
    CHECK(fine > 0 && game_snow(&g, px + 1) == 0 && game_snow(&g, px) == 4);
    /* Saved with the world. */
    g.random_events = 7;
    CHECK(save_game(&g, "build/test_snow.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_snow.world"));
    CHECK(loaded.snow[px] == 4 && loaded.snow[px + 1] == 0 && loaded.random_events == 7 && game_snow(&loaded, px) == 4);
    game_free(&loaded);
    /* In spring it melts away. */
    g.tick = season * 4 + 10;
    for (int k = 0; k < 20000; k++) game_weather_event(&g, px, 3);
    CHECK(game_snow(&g, px) == 0);
    /* High ground has snow the year round: above 50, not below. */
    g.tick = season + 10;
    int high = world_y_to_row(w, 51), low = world_y_to_row(w, 49);
    for (int y = high; y <= s; y++) world_set(w, px + 20, y, TILE_STONE);
    for (int y = low; y <= s; y++) world_set(w, px + 22, y, TILE_STONE);
    CHECK(game_snow(&g, px + 20) == 4 && game_snow(&g, px + 22) == 0 && game_snow(&g, px + 21) == 0);
    game_free(&g);

    /* Ten random events to a chunk in a new world, a setting of the world's; what grows keeps the pace of one. */
    CHECK(u->random_event_base == 1 && u->max_random_events >= 10);
    int grown[2] = { 0, 0 }, frozen[2] = { 0, 0 };
    for (int trial = 0; trial < 2; trial++) {
        Game t;
        game_init(&t, 64, 48, 43, WORLD_FLAT);
        clear_mobs(&t);
        CHECK(t.random_events == u->random_events_per_chunk);
        t.random_events = trial == 0 ? 1 : 10;
        int ts = t.world.surface_y;
        for (int y = ts - 6; y < ts - 2; y++)
            for (int x = 0; x < 64; x++) world_set_at(&t.world, BG, x, y, tile_id("carrot_0"));
        /* (A long trough of water along the ground, in winter.) */
        t.tick = season * 3 + 5;
        for (int x = 0; x < 64; x++) {
            world_set(&t.world, x, ts, TILE_STONE);
            world_set(&t.world, x, ts - 1, TILE_AIR);
        }
        for (int x = 0; x < 64; x++) world_set_water(&t.world, x, ts - 1, WATER_FULL);
        t.character_count = 0; /* (nobody about to be hungry, or to bring creatures) */
        for (int k = 0; k < 400; k++) game_tick(&t);
        for (int y = ts - 6; y < ts - 2; y++) {
            for (int x = 0; x < 64; x++) {
                TileType c = world_get_at(&t.world, BG, x, y);
                grown[trial] += c == tile_id("carrot_1") ? 1 : c == tile_id("carrot_2") ? 2 : c == tile_id("carrot_3") ? 3 : 0;
            }
        }
        for (int x = 0; x < 64; x++) frozen[trial] += world_get(&t.world, x, ts - 1) == tile_id("ice");
        game_free(&t);
    }
    CHECK(grown[0] > 60 && grown[1] > grown[0] * 6 / 10 && grown[1] < grown[0] * 16 / 10);
    CHECK(frozen[1] > frozen[0] * 3 && frozen[1] > 20);

    /* Leaves change with the seasons, the ones that do. */
    const TileDef *oak = tile_def(TILE_OAK_LEAVES), *pine = tile_def(tile_id("pine_leaves")), *palm = tile_def(tile_id("coconut_leaves"));
    CHECK(oak->season_texture_count[0] == oak->texture_count && oak->season_texture_count[1] == 0 &&
          oak->season_texture_count[2] == oak->texture_count && oak->season_texture_count[3] == oak->texture_count);
    CHECK(pine->season_texture_count[3] == pine->texture_count && pine->season_texture_count[2] == 0);
    for (int k = 0; k < 4; k++) CHECK(palm->season_texture_count[k] == 0);
}

/* Doors and trapdoors, walls that run themselves, lamps, and armour. */
static void test_doors_walls_lamps_armor(void)
{
    Game g;
    game_init(&g, 96, 64, 44, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    clear_mobs(&g);
    g.tick_rate = 0;
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;
    TileType door = tile_id("wooden_door"), iron = tile_id("iron_door"), hatch = tile_id("trapdoor"), wood = tile_id("wood");

    /* A door is solid to everything but a character's body. */
    CHECK(door > 0 && tile_def(door)->solid && tile_def(door)->passable && tile_def(door)->mimics == MIMIC_UPRIGHT);
    CHECK(tile_def(iron)->passable && tile_def(hatch)->passable && tile_def(hatch)->climbable && tile_def(hatch)->mimics == MIMIC_FLAT);
    CHECK(item_def(item_id("wooden_door"))->places == door && item_def(item_id("iron_trapdoor"))->places == tile_id("iron_trapdoor"));
    CHECK(!tile_def(TILE_STONE)->passable);
    /* A wall four high with a doorway in it, two doors tall. */
    for (int k = 0; k < 4; k++) world_set(w, px + 3, ground - k, k < 2 ? door : TILE_STONE);
    CHECK(world_is_solid(w, px + 3, ground));
    g.player.feet = g.player.step_to = (TilePos){ px, ground };
    world_set(w, px + 6, ground - 1, TILE_AIR);
    CHECK(game_queue_action(&g, ACTION_MOVE, px + 6, ground));
    Trace through = run_traced(&g, 10.0f);
    CHECK(g.player.feet.x == px + 6 && g.player.feet.y == ground && !through.climbed && is_idle(&g));
    /* With stone where the doors are, the only way is over the top. */
    for (int k = 0; k < 2; k++) world_set(w, px + 3, ground - k, TILE_STONE);
    CHECK(game_queue_action(&g, ACTION_MOVE, px, ground));
    Trace over = run_traced(&g, 20.0f);
    CHECK(g.player.feet.x == px && over.climbed);
    for (int k = 0; k < 4; k++) world_set(w, px + 3, ground - k, TILE_AIR);

    /* What a door would look like: the block it stands between, if that is the same above and below. */
    int dx = px + 10;
    world_set(w, dx, ground, TILE_STONE);
    world_set(w, dx, ground - 1, door);
    world_set(w, dx, ground - 2, TILE_STONE);
    CHECK(world_mimic_of(w, dx, ground - 1) == TILE_STONE && world_mimic_of(w, dx, ground) == TILE_STONE);
    /* Until it is given the look, it looks like a door; given it, it keeps it whatever is changed round it. */
    CHECK(world_looks_like(w, dx, ground - 1) == door);
    world_fix_look(w, dx, ground - 1);
    CHECK(world_looks_like(w, dx, ground - 1) == TILE_STONE);
    world_set(w, dx, ground - 2, wood);
    CHECK(world_mimic_of(w, dx, ground - 1) == door && world_looks_like(w, dx, ground - 1) == TILE_STONE);
    world_set(w, dx, ground - 2, TILE_AIR);
    CHECK(world_mimic_of(w, dx, ground - 1) == door && world_looks_like(w, dx, ground - 1) == TILE_STONE);
    /* The look goes with the door: another put there starts afresh. */
    world_set(w, dx, ground - 1, TILE_AIR);
    world_set(w, dx, ground - 1, door);
    CHECK(world_looks_like(w, dx, ground - 1) == door && world_meta(w, dx, ground - 1) == 0);
    /* Several in a row, doors and trapdoors alike, are looked past to the blocks at the ends. */
    world_set(w, dx, ground - 2, iron);
    world_set(w, dx, ground - 3, hatch);
    world_set(w, dx, ground - 4, TILE_STONE);
    for (int k = 1; k <= 2; k++) CHECK(world_mimic_of(w, dx, ground - k) == TILE_STONE);
    CHECK(world_mimic_of(w, dx, ground - 3) == hatch); /* (itself lying flat, it goes by its sides) */
    world_set(w, dx, ground - 4, wood);
    CHECK(world_mimic_of(w, dx, ground - 1) == door && world_mimic_of(w, dx, ground - 2) == iron);
    for (int k = 0; k <= 4; k++) world_set(w, dx, ground - k, TILE_AIR);
    /* A trapdoor lies flat: it goes by what is either side of it. */
    world_set(w, dx, ground - 3, wood);
    world_set(w, dx + 1, ground - 3, hatch);
    world_set(w, dx + 2, ground - 3, hatch);
    world_set(w, dx + 3, ground - 3, wood);
    CHECK(world_mimic_of(w, dx + 1, ground - 3) == wood && world_mimic_of(w, dx + 2, ground - 3) == wood);
    world_set(w, dx + 3, ground - 3, TILE_STONE);
    CHECK(world_mimic_of(w, dx + 1, ground - 3) == hatch);
    world_set(w, dx + 3, ground - 3, wood);

    /* A door is two blocks tall, and is put up and taken down as one thing. It takes its look as it goes up. */
    CHECK(tile_def(door)->height == 2 && tile_def(iron)->height == 2 && tile_def(hatch)->height == 1);
    int ex = px - 6;
    ItemId door_item = item_id("wooden_door"), hatch_item = item_id("trapdoor");
    g.player.feet = g.player.step_to = (TilePos){ px - 3, ground };
    clear_inventory(&g);
    inventory_add(inv, door_item, 2);
    inventory_add(inv, hatch_item, 1);
    inventory_select(inv, 0, -1);
    world_set(w, ex, ground - 2, TILE_STONE); /* a lintel over the doorway; the floor under it is not stone */
    world_set(w, ex, ground + 1, TILE_STONE);
    world_set(w, ex + 1, ground - 2, TILE_STONE); /* (and something beside it to hold the lintel up) */
    CHECK(game_queue_action(&g, ACTION_PLACE, ex, ground));
    run(&g, 5.0f);
    CHECK(world_get(w, ex, ground) == door && world_get(w, ex, ground - 1) == door && count_item(&g, door_item) == 1);
    CHECK(world_looks_like(w, ex, ground) == TILE_STONE && world_looks_like(w, ex, ground - 1) == TILE_STONE);
    /* The blocks round it changed, it looks as it did. */
    world_set(w, ex, ground - 2, wood);
    world_set(w, ex, ground + 1, wood);
    CHECK(world_looks_like(w, ex, ground) == TILE_STONE && world_looks_like(w, ex, ground - 1) == TILE_STONE);
    world_set(w, ex, ground - 2, TILE_AIR);
    CHECK(world_looks_like(w, ex, ground - 1) == TILE_STONE);
    /* There is no room for one under a ceiling one block up. */
    world_set(w, ex - 2, ground - 1, TILE_STONE);
    CHECK(!game_queue_action(&g, ACTION_PLACE, ex - 2, ground));
    world_set(w, ex - 2, ground - 1, TILE_AIR);
    /* A trapdoor takes its look the same way, and keeps it. */
    world_set(w, ex + 3, ground - 3, wood);
    world_set(w, ex + 5, ground - 3, wood);
    g.player.feet = g.player.step_to = (TilePos){ ex + 4, ground };
    inventory_select(inv, 1, -1);
    CHECK(game_queue_action(&g, ACTION_PLACE, ex + 4, ground - 3));
    run(&g, 5.0f);
    CHECK(world_get(w, ex + 4, ground - 3) == hatch && world_looks_like(w, ex + 4, ground - 3) == wood);
    world_set(w, ex + 5, ground - 3, TILE_STONE);
    CHECK(world_looks_like(w, ex + 4, ground - 3) == wood);
    /* The looks are kept with the world. */
    CHECK(save_game(&g, "build/test_door.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_door.world"));
    CHECK(world_looks_like(&loaded.world, ex, ground) == TILE_STONE && world_looks_like(&loaded.world, ex, ground - 1) == TILE_STONE);
    CHECK(world_looks_like(&loaded.world, ex + 4, ground - 3) == wood);
    game_free(&loaded);
    /* Taken down by either block, the whole door comes away as one door. */
    g.player.feet = g.player.step_to = (TilePos){ px - 3, ground };
    CHECK(game_queue_action(&g, ACTION_MINE, ex, ground - 1));
    run(&g, 10.0f);
    CHECK(world_get(w, ex, ground) == TILE_AIR && world_get(w, ex, ground - 1) == TILE_AIR && count_item(&g, door_item) == 2);
    CHECK(world_meta(w, ex, ground) == 0 && world_meta(w, ex, ground - 1) == 0);
    world_set(w, ex + 4, ground - 3, TILE_AIR);
    world_set(w, ex + 3, ground - 3, TILE_AIR);
    world_set(w, ex + 5, ground - 3, TILE_AIR);
    world_set(w, ex + 1, ground - 2, TILE_AIR);
    world_set(w, ex, ground + 1, w->layers[0][(size_t)(ground + 1) * w->width + world_wrap_x(w, ex - 1)]);
    world_set(w, dx + 3, ground - 3, wood);

    /* A trapdoor in a floor: stood on, let down through with the down key, and climbed up through again. */
    g.player.feet = g.player.step_to = (TilePos){ dx + 1, ground - 4 };
    run(&g, 1.0f);
    CHECK(g.player.feet.y == ground - 4 && g.player.feet.x == dx + 1);
    game_set_move_input(&g, 0, 1);
    run(&g, 3.0f);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(g.player.feet.x == dx + 1 && g.player.feet.y == ground);
    CHECK(game_queue_action(&g, ACTION_MOVE, dx + 1, ground - 5)); /* (where their head will be, standing on it) */
    run(&g, 10.0f);
    CHECK(g.player.feet.x == dx + 1 && g.player.feet.y == ground - 4 && is_idle(&g));
    /* A plain floor is not let down through. */
    g.player.feet = g.player.step_to = (TilePos){ dx, ground - 4 };
    game_set_move_input(&g, 0, 1);
    run(&g, 0.5f);
    game_set_move_input(&g, 0, 0);
    CHECK(g.player.feet.y == ground - 4);
    for (int k = 0; k <= 3; k++) world_set(w, dx + k, ground - 3, TILE_AIR);
    run(&g, 2.0f);

    /* Walls that run themselves: a block put up within 10 of the block under it gets backwall down to that block. */
    CHECK(content.rules.wall_span == 10);
    int wx = px - 6;
    g.player.feet = g.player.step_to = (TilePos){ px, ground };
    for (int k = 0; k < 12; k++) world_set_at(w, LAYER_WALL, wx, ground - k, TILE_AIR), world_set_at(w, LAYER_WALL, wx + 1, ground - k, TILE_AIR);
    for (int k = 0; k < 12; k++) world_set(w, wx - 1, ground - k, TILE_STONE); /* (something to build out from) */
    clear_inventory(&g);
    inventory_add(inv, item_id("stone"), 5);
    inventory_select(inv, 0, -1);
    world_set_at(w, LAYER_WALL, wx, ground - 2, wood); /* (a wall already there is left as it is) */
    CHECK(game_queue_action(&g, ACTION_PLACE, wx, ground - 6));
    run(&g, 20.0f);
    CHECK(world_get(w, wx, ground - 6) == TILE_STONE);
    for (int k = 0; k < 6; k++) CHECK(world_get_at(w, LAYER_WALL, wx, ground - k) == (k == 2 ? wood : TILE_STONE));
    CHECK(world_get_at(w, LAYER_WALL, wx, ground - 7) == TILE_AIR);
    /* Too far above the next block down, and none is run. */
    world_set(w, wx, ground - 6, TILE_AIR);
    for (int k = 0; k < 12; k++) world_set_at(w, LAYER_WALL, wx, ground - k, TILE_AIR);
    CHECK(game_queue_action(&g, ACTION_PLACE, wx, ground - 10)); /* ten above the ground block: eleven below it, the tenth is the ground */
    run(&g, 20.0f);
    CHECK(world_get(w, wx, ground - 10) == TILE_STONE && world_get_at(w, LAYER_WALL, wx, ground - 5) == TILE_AIR);
    world_set(w, wx, ground - 10, TILE_AIR);
    world_set_at(w, LAYER_WALL, wx, ground - 10, TILE_AIR);
    CHECK(game_queue_action(&g, ACTION_PLACE, wx, ground - 9));
    run(&g, 20.0f);
    for (int k = 0; k < 9; k++) CHECK(world_get_at(w, LAYER_WALL, wx, ground - k) == TILE_STONE);
    /* Taking the top block away - nothing behind the square over it - takes the wall under it down. */
    CHECK(game_click_tile(&g, wx, ground - 9) && g.queue[0].type == ACTION_MINE);
    run(&g, 20.0f);
    CHECK(world_get(w, wx, ground - 9) == TILE_AIR);
    for (int k = 0; k <= 9; k++) CHECK(world_get_at(w, LAYER_WALL, wx, ground - k) == TILE_AIR); /* (the one behind itself too) */
    /* With backwall over it, it is not the top of anything, and the wall stays. */
    clear_inventory(&g);
    inventory_add(inv, item_id("stone"), 5);
    inventory_select(inv, 0, -1);
    CHECK(game_queue_action(&g, ACTION_PLACE, wx, ground - 5));
    run(&g, 20.0f);
    world_set_at(w, LAYER_WALL, wx, ground - 6, TILE_STONE);
    inventory_select(inv, 3, -1);
    CHECK(game_click_tile(&g, wx, ground - 5) && g.queue[0].type == ACTION_MINE);
    run(&g, 20.0f);
    CHECK(world_get(w, wx, ground - 5) == TILE_AIR && world_get_at(w, LAYER_WALL, wx, ground - 2) == TILE_STONE);
    CHECK(world_get_at(w, LAYER_WALL, wx, ground - 5) == TILE_STONE); /* (and so does the one behind itself) */

    /* Things of the background, put up where there is no backwall, are fixed
     * to a block: the one under them first, then over, then to the left, then the right. */
    int mx = px + 20, my = ground - 8;
    for (int dy = -3; dy <= 3; dy++)
        for (int dx2 = -3; dx2 <= 3; dx2++) world_set_at(w, LAYER_WALL, mx + dx2, my + dy, TILE_AIR), world_set(w, mx + dx2, my + dy, TILE_AIR);
    CHECK(world_mount(w, mx, my) == MOUNT_NONE);
    world_set(w, mx + 1, my, TILE_STONE);
    CHECK(world_mount(w, mx, my) == MOUNT_RIGHT);
    world_set(w, mx - 1, my, TILE_STONE);
    CHECK(world_mount(w, mx, my) == MOUNT_LEFT);
    world_set(w, mx, my - 1, TILE_STONE);
    CHECK(world_mount(w, mx, my) == MOUNT_CEILING);
    world_set(w, mx, my + 1, TILE_STONE);
    CHECK(world_mount(w, mx, my) == MOUNT_FLOOR);
    world_set_at(w, LAYER_WALL, mx, my, TILE_STONE);
    CHECK(world_mount(w, mx, my) == MOUNT_WALL);
    world_set_at(w, LAYER_WALL, mx, my, TILE_AIR);
    /* A column under it counts as a block, though it is only in the background. */
    world_set(w, mx, my + 1, TILE_AIR);
    world_set(w, mx, my - 1, TILE_AIR);
    world_set_at(w, BG, mx, my + 1, tile_id("marble_column"));
    CHECK(tile_def(tile_id("marble_column"))->supports && world_mount(w, mx, my) == MOUNT_FLOOR);
    /* Each of them has a look for standing, hanging and being fixed to a side. */
    static const char *mounted[] = { "oil_lantern", "steel_lantern", "steel_uplight", "steel_downlight", "amethyst", "diamond", "time_crystal" };
    for (int k = 0; k < 7; k++) {
        const TileDef *def = tile_def(tile_id(mounted[k]));
        CHECK(def->texture_mount[0][0] && def->texture_mount[1][0] && def->texture_mount[2][0] && !def->hangs);
    }

    /* Putting them up. With nothing to fix it to, a lantern cannot be put up at all. */
    for (int dy = -3; dy <= 3; dy++)
        for (int dx2 = -3; dx2 <= 3; dx2++) world_set(w, mx + dx2, my + dy, TILE_AIR), world_set_at(w, BG, mx + dx2, my + dy, TILE_AIR);
    for (int k = 0; k < 12; k++) world_set(w, mx - 4, ground - k, TILE_STONE); /* (something to reach it from) */
    g.player.feet = g.player.step_to = (TilePos){ mx - 4, ground - 12 };
    clear_inventory(&g);
    inventory_add(inv, item_id("oil_lantern"), 4);
    inventory_add(inv, item_id("ruby_chandelier"), 4);
    inventory_add(inv, item_id("stone_stairs"), 2);
    inventory_select(inv, 0, -1);
    CHECK(!game_queue_action(&g, ACTION_PLACE, mx - 2, my));
    /* On a column, it can. */
    world_set_at(w, BG, mx - 2, my + 1, tile_id("marble_column"));
    CHECK(game_queue_action(&g, ACTION_PLACE, mx - 2, my));
    run(&g, 20.0f);
    CHECK(world_get_at(w, BG, mx - 2, my) == tile_id("oil_lantern") && world_mount(w, mx - 2, my) == MOUNT_FLOOR);
    /* Beside a block, or under one. */
    world_set(w, mx - 1, my - 3, TILE_STONE);
    CHECK(game_queue_action(&g, ACTION_PLACE, mx - 2, my - 3));
    run(&g, 20.0f);
    CHECK(world_get_at(w, BG, mx - 2, my - 3) == tile_id("oil_lantern") && world_mount(w, mx - 2, my - 3) == MOUNT_RIGHT);
    /* A chandelier goes under a block and nowhere else: not beside one, not on a column, not on backwall. */
    CHECK(tile_def(tile_id("ruby_chandelier"))->hangs && !tile_def(tile_id("oil_lantern"))->hangs);
    inventory_select(inv, 1, -1);
    CHECK(inv->slots[1].id == item_id("ruby_chandelier"));
    CHECK(!game_queue_action(&g, ACTION_PLACE, mx - 1, my - 2) || world_is_solid(w, mx - 1, my - 3));
    CHECK(!game_queue_action(&g, ACTION_PLACE, mx - 2, my - 4)); /* over the lantern: nothing above */
    CHECK(!game_queue_action(&g, ACTION_PLACE, mx, my - 3));     /* beside the block */
    world_set_at(w, LAYER_WALL, mx - 3, my - 5, TILE_STONE);
    CHECK(!game_queue_action(&g, ACTION_PLACE, mx - 3, my - 5)); /* on backwall, with no block over it */
    game_cancel_actions(&g);
    CHECK(game_queue_action(&g, ACTION_PLACE, mx - 1, my - 2));  /* under the block */
    run(&g, 20.0f);
    CHECK(world_get_at(w, BG, mx - 1, my - 2) == tile_id("ruby_chandelier"));
    /* Stairs are the exception still: a step up on the slant from other stairs, with nothing else by them. */
    inventory_select(inv, 2, -1);
    world_set_at(w, BG, mx - 3, my + 3, tile_id("stone_stairs"));
    CHECK(game_queue_action(&g, ACTION_PLACE, mx - 2, my + 2));
    game_cancel_actions(&g);
    game_free(&g);

    /* Lamps: gems, then torches, oil lanterns, steel lanterns, chandeliers. */
    int gem = tile_def(tile_id("amethyst"))->light, torch = 0, oil = tile_def(tile_id("oil_lantern"))->light,
        steel = tile_def(tile_id("steel_lantern"))->light;
    /* (The torch these tests run with is the old one; what the shipped one gives was noted before it was replaced.) */
    static const char *gems[] = { "amethyst", "sapphire", "emerald", "ruby", "diamond" };
    for (int k = 0; k < 5; k++) {
        char name[48];
        snprintf(name, sizeof name, "%s_chandelier", gems[k]);
        const TileDef *c = tile_def(tile_id(name));
        CHECK(tile_id(name) > 0 && c->light > steel && c->light <= LIGHT_MAX && !c->solid && item_def(item_id(name))->places == tile_id(name));
    }
    torch = shipped_torch_light;
    CHECK(gem < torch && torch < oil && oil < steel);

    /* An uplight throws a quarter circle of light upward, a downlight downward. */
    Game d;
    game_init(&d, 96, 96, 45, WORLD_FLAT);
    World *dw = &d.world;
    int ds = dw->surface_y, cx = 40, cy = ds + 20;
    /* (A room deep in the ground, walled behind, with the lamp in the middle of it.) */
    for (int y = cy - 9; y <= cy + 9; y++)
        for (int x = cx - 12; x <= cx + 12; x++) world_set(dw, x, y, TILE_AIR);
    static uint8_t lit[40 * 40];
    for (int kind = 0; kind < 2; kind++) {
        TileType lamp = tile_id(kind == 0 ? "steel_uplight" : "steel_downlight");
        int way = kind == 0 ? -1 : 1;
        CHECK(lamp > 0 && tile_def(lamp)->beam_dir == way && tile_def(lamp)->beam_light == LIGHT_MAX);
        world_set_at(dw, BG, cx, cy, lamp);
        light_compute(&d, cx - 20, cy - 20, 40, 40, lit);
#define LIT(X, Y) lit[((Y) - (cy - 20)) * 40 + ((X) - (cx - 20))]
        /* Bright along its line and out to 45 degrees either side; dim behind it and beside it. */
        CHECK(LIT(cx, cy + way * 6) == LIGHT_MAX - 6 && LIT(cx + 5, cy + way * 6) == LIGHT_MAX - 6 && LIT(cx - 6, cy + way * 6) == LIGHT_MAX - 6);
        CHECK(LIT(cx, cy - way * 6) < LIT(cx, cy + way * 6) && LIT(cx + 7, cy) < LIT(cx, cy + way * 7));
        CHECK(LIT(cx + 8, cy + way * 3) < LIT(cx + 3, cy + way * 3));
        /* A block in the way throws a shadow. */
        world_set(dw, cx, cy + way * 2, TILE_STONE);
        light_compute(&d, cx - 20, cy - 20, 40, 40, lit);
        CHECK(LIT(cx, cy + way * 6) < LIGHT_MAX - 6 && LIT(cx + 5, cy + way * 6) == LIGHT_MAX - 6);
#undef LIT
        world_set(dw, cx, cy + way * 2, TILE_AIR);
        world_set_at(dw, BG, cx, cy, TILE_AIR);
    }

    /* Armour: linen, fur, leather, tin, iron, then titanium and carbon fiber alike; the pieces add up. */
    static const char *suits[][2] = { { "linen_shirt", "linen" }, { "fur_coat", "fur" }, { "leather_jacket", "leather" },
                                      { "tin_chestplate", "tin" }, { "iron_chestplate", "iron" }, { "titanium_chestplate", "titanium" } };
    float last = 0.0f;
    for (int k = 0; k < 6; k++) {
        float a = item_def(item_id(suits[k][0]))->armor;
        CHECK(a > last);
        last = a;
    }
    CHECK(item_def(item_id("carbon_fiber_chestplate"))->armor == item_def(item_id("titanium_chestplate"))->armor);
    CHECK(item_def(item_id("stone"))->armor == 0.0f);
    Inventory *di = &d.inventory;
    CHECK(game_protection(&d) == 0.0f);
    d.vitals.health = 1.0f;
    game_attacked(&d, 0.4f);
    CHECK(d.vitals.health > 0.599f && d.vitals.health < 0.601f);
    static const char *iron_suit[] = { "iron_helmet", "iron_chestplate", "iron_leggings", "iron_boots" };
    float all = 0.0f;
    for (int k = 0; k < 4; k++) {
        di->armor[item_def(item_id(iron_suit[k]))->equip] = (ItemStack){ (uint16_t)item_id(iron_suit[k]), 1, 0 };
        all += item_def(item_id(iron_suit[k]))->armor;
        CHECK(game_protection(&d) > all - 0.001f && game_protection(&d) < all + 0.001f);
    }
    CHECK(all > 0.4f && all < 0.9f);
    d.vitals.health = 1.0f;
    game_attacked(&d, 0.4f);
    CHECK(d.vitals.health > 1.0f - 0.4f * (1.0f - all) - 0.001f && d.vitals.health < 1.0f - 0.4f * (1.0f - all) + 0.001f);
    /* It does nothing for hunger or drowning: only for attacks. A sting gets less of its poison in. */
    d.vitals.health = 1.0f;
    game_hurt(&d, 0.4f);
    CHECK(d.vitals.health > 0.599f && d.vitals.health < 0.601f);
    int scorpion = mob_id("scorpion");
    float moved = content.mobs[scorpion].move_chance;
    content.mobs[scorpion].move_chance = 0.0f;
    d.tick_rate = 0;
    clear_mobs(&d);
    mob_spawn(&d, scorpion, d.player.feet.x + 1, d.player.feet.y, true, false);
    mobs_tick(&d);
    CHECK(d.vitals.poison > 0 && d.vitals.poison == (int)(40.0f * (1.0f - all) + 0.5f));
    content.mobs[scorpion].move_chance = moved;
    game_free(&d);
}

/* Puts something that keeps count of itself - a generator, a battery, a bench - straight into the world. */
static Storage *put_device(Game *g, const char *tile, int x, int y)
{
    world_set_at(&g->world, tile_def(tile_id(tile))->layer, x, y, tile_id(tile));
    for (int i = 0; i < MAX_STORAGES; i++) {
        if (g->storages[i].used) continue;
        g->storages[i] = (Storage){ .used = true, .x = world_wrap_x(&g->world, x), .y = y };
        return &g->storages[i];
    }
    return NULL;
}

static int in_the_wires(const Game *g)
{
    int total = 0;
    for (int i = 0; i < MAX_POWER_FLOWS; i++)
        for (int k = 0; g->flows[i].active && k < g->flows[i].length; k++) total += g->flows[i].moving[k];
    return total;
}

/* Electricity: made, stored, carried a block a tick, and used by benches and lifts. */
static void test_power(void)
{
    Game g;
    game_init(&g, 128, 96, 46, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    clear_mobs(&g);
    g.tick_rate = 0;
    g.random_events = 0;
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;
    TileType wire = tile_id("copper_wire"), floor_wire = tile_id("copper_wire_floor");
    const TileDef *solar = tile_def(tile_id("solar_panel")), *steam = tile_def(tile_id("steam_generator")),
                  *wheel = tile_def(tile_id("flywheel"));

    /* What each thing is. */
    CHECK(solar->gen_power == 10 && solar->gen_needs == POWER_NEEDS_SUN && steam->gen_power == 10 && steam->gen_needs == POWER_NEEDS_FUEL);
    CHECK(steam->fuel_capacity > 0 && wheel->battery == 10000 && wheel->gen_power == 0);
    CHECK(wire > 0 && tile_def(wire)->wire && tile_def(wire)->layer == LAYER_BACKGROUND && tile_def(wire)->floats && !tile_def(wire)->solid);
    CHECK(tile_def(floor_wire)->wire && tile_def(floor_wire)->layer == LAYER_FOREGROUND && !tile_def(floor_wire)->solid);
    CHECK(item_def(item_id("copper_wire"))->places == wire && item_def(item_id("copper_wire"))->places_on_block == TILE_AIR);
    CHECK(tile_def(tile_id("electric_furnace"))->electric == 100 && strcmp(tile_def(tile_id("electric_furnace"))->bench_kind, "electric_furnace") == 0);
    /* (It has recipes of its own in the game. For these tests it is lent the furnace's, which the tests' own content has.) */
    TileDef *lent = tile_def_mut(tile_id("electric_furnace"));
    snprintf(lent->bench_kind, sizeof lent->bench_kind, "furnace");
    lent->bench_level = 3;
    CHECK(tile_def(tile_id("electric_furnace"))->fuel_capacity == 0 && tile_def(tile_id("furnace"))->electric == 0);
    CHECK(tile_def(tile_id("electric_elevator_motor"))->motor_power == 10 && tile_def(tile_id("elevator_shaft"))->shaft);

    /* Wire goes on a block - in the very square the block is in, over the face of it - and anywhere else, in thin air too. */
    clear_inventory(&g);
    inventory_add(inv, item_id("copper_wire"), 4);
    inventory_select(inv, 0, -1);
    TileType under = world_get(w, px + 2, s);
    CHECK(under != TILE_AIR && world_is_solid(w, px + 2, s));
    /* (A click on a block with wire in hand is what puts it there: the block is not dug out.) */
    CHECK(game_click_tile(&g, px + 2, s) && g.queue_count + (g.has_current ? 1 : 0) == 1);
    CHECK((g.has_current ? g.current.type : g.queue[0].type) == ACTION_PLACE);
    run(&g, 5.0f);
    CHECK(world_get(w, px + 2, s) == under && world_get_at(w, BG, px + 2, s) == wire && world_is_wired(w, px + 2, s));
    /* (Clicked again with wire still in hand, there is wire there already: it is the wire that comes off.) */
    CHECK(game_click_tile(&g, px + 2, s));
    CHECK((g.has_current ? g.current.type : g.queue[0].type) == ACTION_MINE);
    game_cancel_actions(&g);
    CHECK(world_is_solid(w, px + 2, s) && !world_is_wired(w, px + 2, ground));
    CHECK(!game_queue_action(&g, ACTION_PLACE, px + 2, s)); /* one wire to a square */
    world_set_at(w, LAYER_WALL, px + 2, ground - 2, TILE_AIR);
    CHECK(game_queue_action(&g, ACTION_PLACE, px + 2, ground - 2));
    run(&g, 5.0f);
    CHECK(world_get_at(w, BG, px + 2, ground - 2) == wire && world_get(w, px + 2, ground - 2) == TILE_AIR && world_is_wired(w, px + 2, ground - 2));
    CHECK(!world_is_wired(w, px + 2, ground - 1) && count_item(&g, item_id("copper_wire")) == 2);
    /* Off a block it comes first, before the block, and as the same wire; the block is still there. */
    clear_inventory(&g);
    CHECK(game_click_tile(&g, px + 2, s) && g.queue[0].type == ACTION_MINE);
    run(&g, 5.0f);
    CHECK(world_get_at(w, BG, px + 2, s) == TILE_AIR && world_get(w, px + 2, s) == under);
    CHECK(count_item(&g, item_id("copper_wire")) + drops_of(&g, item_id("copper_wire")) == 1);
    /* A block can be put where wire already is, and the wire stays. */
    world_set_at(w, BG, px + 3, ground, wire);
    inventory_add(inv, ITEM_DIRT, 1);
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (inv->slots[i].count > 0 && inv->slots[i].id == ITEM_DIRT) inventory_select(inv, i, -1);
    CHECK(game_queue_action(&g, ACTION_PLACE, px + 3, ground));
    run(&g, 5.0f);
    CHECK(world_is_solid(w, px + 3, ground) && world_get_at(w, BG, px + 3, ground) == wire && world_is_wired(w, px + 3, ground));
    world_set(w, px + 3, ground, TILE_AIR);
    world_set_at(w, BG, px + 3, ground, TILE_AIR);
    world_set_at(w, BG, px + 2, ground - 2, TILE_AIR);
    clear_inventory(&g);
    clear_drops(&g);
    /* (Wire of the kind that used to lie on top of a block, in worlds that have it, still carries.) */
    world_set(w, px + 2, ground, floor_wire);
    CHECK(world_is_wired(w, px + 2, ground));
    world_set(w, px + 2, ground, TILE_AIR);

    /* A steam generator with fuel in it, five squares of wire, and a flywheel. */
    int gx = px + 10, row = ground - 10;
    Storage *gen = put_device(&g, "steam_generator", gx, row);
    for (int k = 1; k <= 5; k++) world_set_at(w, BG, gx + k, row, wire);
    Storage *fly = put_device(&g, "flywheel", gx + 6, row);
    /* With no fuel, nothing. */
    for (int t = 0; t < 30; t++) game_tick(&g);
    CHECK(fly->power == 0 && game_power_made(&g, gx, row) == 0 && in_the_wires(&g) == 0);
    /* With fuel, 10 a tick: and it takes as many ticks to arrive as the wire is long. */
    gen->fuel = 1000;
    game_tick(&g);
    CHECK(game_power_made(&g, gx, row) == 10 && fly->power == 0);
    int sent_at = 0;
    for (int t = 0; t < 4; t++) game_tick(&g);
    CHECK(fly->power == 0 && in_the_wires(&g) > 0);
    sent_at = in_the_wires(&g);
    for (int t = 0; t < 2; t++) game_tick(&g);
    CHECK(fly->power > 0 && sent_at > 0);
    int before = fly->power;
    for (int t = 0; t < 50; t++) game_tick(&g);
    CHECK(fly->power == before + 500 && in_the_wires(&g) == 50); /* ten a tick, with five ticks' worth always on the way */
    int capacity = 0;
    CHECK(game_power_at(&g, gx + 6, row, &capacity) == fly->power && capacity == 10000);
    /* It can be seen going: a spark in each square of the wire, each headed for the next. */
    static Spark sparks[64];
    int seen = game_sparks(&g, sparks, 64);
    CHECK(seen == 5);
    for (int k = 0; k < seen; k++) CHECK(sparks[k].y == row && sparks[k].x > gx && sparks[k].x < gx + 6 && sparks[k].to_x == sparks[k].x + 1);
    /* A flywheel holds 10,000 and no more. */
    fly->power = 9970;
    for (int t = 0; t < 40; t++) game_tick(&g);
    CHECK(fly->power == 10000 && in_the_wires(&g) == 0);
    /* A cut wire carries nothing: what was in it is lost, and no more comes. */
    fly->power = 100;
    for (int t = 0; t < 10; t++) game_tick(&g);
    CHECK(fly->power > 100);
    world_set_at(w, BG, gx + 3, row, TILE_AIR);
    game_tick(&g);
    before = fly->power;
    for (int t = 0; t < 20; t++) game_tick(&g);
    CHECK(fly->power == before && in_the_wires(&g) == 0);
    world_set_at(w, BG, gx + 3, row, wire);
    /* It is the generator that looks for somewhere to put what it makes: the nearest flywheel with room,
     * and when that is full, the next. A second generator on the same wire sends its own as well. */
    {
        int ex = px + 40, er = ground - 20;
        Storage *one = put_device(&g, "steam_generator", ex, er), *two = put_device(&g, "steam_generator", ex + 4, er - 2);
        for (int k = 1; k <= 9; k++) world_set_at(w, BG, ex + k, er, wire);
        world_set_at(w, BG, ex + 4, er - 1, wire);
        Storage *nearer = put_device(&g, "flywheel", ex + 3, er + 1), *further = put_device(&g, "flywheel", ex + 10, er);
        one->fuel = two->fuel = 100000;
        nearer->power = 9900;
        for (int t = 0; t < 40; t++) game_tick(&g);
        CHECK(nearer->power == 10000 && further->power > 0); /* the near one filled, and the rest going on to the far one */
        int had = further->power;
        for (int t = 0; t < 100; t++) game_tick(&g);
        CHECK(further->power == had + 2000); /* both generators' ten a tick */
        further->power = 10000;
        for (int t = 0; t < 30; t++) game_tick(&g);
        CHECK(nearer->power == 10000 && further->power == 10000 && game_power_made(&g, ex, er) == 10);
        /* Room made in one, it is filled again. */
        nearer->power = 5000;
        for (int t = 0; t < 30; t++) game_tick(&g);
        CHECK(nearer->power > 5000);
        one->fuel = two->fuel = 0;
        for (int t = 0; t < 30; t++) game_tick(&g);
    }
    /* Kept with the world. */
    CHECK(save_game(&g, "build/test_power.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_power.world"));
    CHECK(game_power_at(&loaded, gx + 6, row, NULL) == fly->power);
    game_free(&loaded);

    /* A solar panel makes it only by day, and only with the sun straight on it. */
    Storage *panel = put_device(&g, "solar_panel", px + 30, row);
    (void)panel;
    g.tick = (uint64_t)content.rules.day_ticks / 2;
    game_tick(&g);
    CHECK(game_power_made(&g, px + 30, row) == 10);
    world_set(w, px + 30, row - 6, TILE_STONE);
    game_tick(&g);
    CHECK(game_power_made(&g, px + 30, row) == 0);
    world_set(w, px + 30, row - 6, TILE_AIR);
    g.tick = (uint64_t)content.rules.day_ticks + (uint64_t)content.rules.night_ticks / 2;
    game_tick(&g);
    CHECK(game_power_made(&g, px + 30, row) == 0);
    g.tick = 10;

    /* An electric bench. It makes what the bench it is an electric kind of makes, and burns nothing;
     * but it will not start until what the work takes has come down the wire to it. */
    gen->fuel = 0;
    fly->power = 5000;
    for (int k = 1; k <= 5; k++) world_set_at(w, BG, gx + k, row, TILE_AIR);
    int bx = px + 3, far = bx + 12;
    world_set_at(w, BG, far, ground, TILE_AIR);
    Storage *bench = put_device(&g, "electric_furnace", bx, ground);
    Storage *near = put_device(&g, "flywheel", bx + 8, ground);
    for (int k = 1; k < 8; k++) world_set(w, bx + k, ground, floor_wire);
    /* (Another flywheel, further off along the same wire.) */
    Storage *further = put_device(&g, "flywheel", bx + 20, ground);
    for (int k = 9; k < 20; k++) world_set(w, bx + k, ground, floor_wire);
    near->power = 3000;
    further->power = 3000;
    Recipe *ingot = (Recipe *)find_recipe(tile_id("electric_furnace"), "copper_ingot");
    CHECK(ingot != NULL && find_recipe(tile_id("electric_furnace"), "iron_ingot") != NULL);
    float old_time = ingot->time;
    ingot->time = 2.0f;
    int need = game_power_for(&g, tile_id("electric_furnace"), ingot, 3);
    CHECK(need == 600 && game_power_for(&g, tile_id("furnace"), ingot, 3) == 0);
    g.player.feet = g.player.step_to = (TilePos){ bx - 1, ground };
    clear_inventory(&g);
    inventory_add(inv, item_id("copper_ore"), 3);
    CHECK(game_open_bench(&g, bx, ground) && game_craft(&g, ingot, 3));
    run(&g, 0.5f);
    int arrived = -1;
    CHECK(g.craft_remaining == 3 && g.craft_power == need && game_craft_power(&g, &arrived) == need && arrived == 0);
    /* It asks the nearest thing that has any - the near flywheel, not the far one - for just that much. */
    game_tick(&g); /* asked for */
    game_tick(&g); /* sent */
    CHECK(near->power == 3000 - need && further->power == 3000 && in_the_wires(&g) == need);
    for (int t = 0; t < 5; t++) {
        game_tick(&g);
        run(&g, 0.1f);
    }
    CHECK(bench->power == 0 && g.craft_remaining == 3 && count_item(&g, item_id("copper_ingot")) == 0); /* still on its way: 7 squares */
    for (int t = 0; t < 3; t++) game_tick(&g);
    CHECK(bench->power == need && in_the_wires(&g) == 0 && near->power == 3000 - need);
    /* Now it starts, and the electricity is used. */
    run(&g, 0.2f);
    CHECK(g.craft_power == 0 && bench->power == 0 && g.craft_remaining == 3);
    run(&g, 6.5f);
    CHECK(g.craft_remaining == 0 && count_item(&g, item_id("copper_ingot")) + drops_of(&g, item_id("copper_ingot")) == 3);
    for (int t = 0; t < 30; t++) game_tick(&g);
    CHECK(near->power == 3000 - need && further->power == 3000); /* and nothing more is drawn */
    /* With nothing stored it comes straight from a generator, ten a tick: slower to start. */
    near->power = 0;
    further->power = 0;
    world_set(w, bx + 8 + 1, ground, TILE_AIR); /* (the far flywheel cut off) */
    world_set_at(w, BG, bx, ground - 1, wire);
    world_set_at(w, BG, bx, ground - 2, wire);
    Storage *boiler = put_device(&g, "steam_generator", bx, ground - 3);
    boiler->fuel = 5000;
    /* (The flywheel beside the bench fills from it too, once the bench has had its share.) */
    clear_inventory(&g);
    clear_drops(&g);
    inventory_add(inv, item_id("copper_ore"), 1);
    CHECK(game_craft(&g, ingot, 1));
    run(&g, 0.3f);
    need = game_power_for(&g, tile_id("electric_furnace"), ingot, 1);
    CHECK(need == 200 && g.craft_power == need);
    int ticks = 0;
    while (g.craft_power > 0 && ticks < 200) {
        game_tick(&g);
        run(&g, 0.05f);
        ticks++;
    }
    CHECK(ticks >= need / 10 + 2 && ticks <= need / 10 + 6);
    run(&g, 3.0f);
    CHECK(count_item(&g, item_id("copper_ingot")) + drops_of(&g, item_id("copper_ingot")) == 1);
    ingot->time = old_time;
    game_close_bench(&g);

    /* A lift: a motor, shaft above it through whatever is in the way, and a floor to get off at. */
    int lx = px - 6, top = ground - 20;
    for (int y = top - 1; y <= ground; y++) world_set_at(w, BG, lx, y, tile_id("elevator_shaft"));
    Storage *motor = put_device(&g, "electric_elevator_motor", lx, ground);
    world_set(w, lx, top + 1, TILE_STONE);            /* the upper floor, with the shaft behind it */
    world_set(w, lx - 1, top + 1, TILE_STONE);
    for (int y = ground - 9; y <= ground - 7; y++) world_set(w, lx, y, TILE_STONE); /* and rock in the way */
    CHECK(world_is_shaft(w, lx, ground - 8) && world_is_shaft(w, lx, ground) && !world_is_shaft(w, lx + 1, ground));
    g.player.feet = g.player.step_to = (TilePos){ lx, ground };
    /* With no power it does not run, and there is no other way up. */
    for (int t = 0; t < 10; t++) game_tick(&g);
    CHECK(!world_lift_runs(w, lx, ground) && !world_lift_runs(w, lx, top));
    game_queue_action(&g, ACTION_MOVE, lx, top - 1);
    run(&g, 3.0f);
    CHECK(g.player.feet.y == ground);
    game_cancel_actions(&g);
    /* A flywheel wired to the motor: it takes 10 a tick for as long as it is there to take. */
    Storage *cell = put_device(&g, "flywheel", lx - 4, ground);
    cell->power = 2000;
    for (int k = 1; k < 4; k++) world_set(w, lx - k, ground, floor_wire);
    for (int t = 0; t < 10; t++) game_tick(&g);
    CHECK(world_lift_runs(w, lx, ground) && world_lift_runs(w, lx, top) && motor->power >= 0);
    before = cell->power;
    for (int t = 0; t < 50; t++) game_tick(&g);
    CHECK(cell->power == before - 500);
    /* Up it goes, through the rock, and quickly: 20 blocks in about a second. */
    CHECK(game_queue_action(&g, ACTION_MOVE, lx, top - 1));
    Trace ride = run_traced(&g, 1.6f);
    CHECK(g.player.feet.x == lx && g.player.feet.y == top && is_idle(&g) && !ride.climbed);
    /* And down again with the down key. */
    /* (By way of the top of the rock, which is a floor like any other.) */
    game_set_move_input(&g, 0, 1);
    run(&g, 0.1f);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(g.player.feet.y == ground - 10 && g.player.feet.x == lx);
    game_set_move_input(&g, 0, 1);
    run(&g, 0.1f);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    CHECK(g.player.feet.y == ground && g.player.feet.x == lx);
    /* A door in front of the shaft at a floor does not stop it being used. */
    world_set(w, lx, top, tile_id("wooden_door"));
    world_set(w, lx, top - 1, tile_id("wooden_door"));
    CHECK(game_queue_action(&g, ACTION_MOVE, lx, top - 1));
    run(&g, 1.6f);
    CHECK(g.player.feet.y == top);
    CHECK(game_queue_action(&g, ACTION_MOVE, lx, ground));
    run(&g, 1.6f);
    CHECK(g.player.feet.y == ground);
    /* A click on a floor of a running lift is a ride there; with the power off, it takes the shaft down. */
    world_set(w, lx, top, TILE_AIR);
    world_set(w, lx, top - 1, TILE_AIR);
    CHECK(game_click_tile(&g, lx, top) && g.queue[0].type == ACTION_MOVE);
    run(&g, 1.6f);
    CHECK(g.player.feet.y == top);
    CHECK(game_click_tile(&g, lx, ground) && g.queue[0].type == ACTION_MOVE);
    run(&g, 1.6f);
    CHECK(g.player.feet.y == ground);
    /* When the power is gone, so is the lift. */
    cell->power = 0;
    for (int t = 0; t < 30; t++) game_tick(&g);
    CHECK(!world_lift_runs(w, lx, ground));
    game_queue_action(&g, ACTION_MOVE, lx, top - 1);
    run(&g, 2.0f);
    CHECK(g.player.feet.y == ground);
    game_cancel_actions(&g);
    CHECK(game_click_tile(&g, lx, ground - 1) && g.queue[0].type == ACTION_MINE);
    game_free(&g);
    snprintf(lent->bench_kind, sizeof lent->bench_kind, "electric_furnace");
    lent->bench_level = 1;
}

/* Trading as the game ships it: prices, what a sale gives, what is not dealt in, finding things first,
 * and the portal that gives everything away. And hurrying what is being made. */
static void test_trade_rules(void)
{
    TradeDef legacy_trade = content.trade;
    static int legacy_price[MAX_ITEMS];
    for (ItemId i = 1; i < content.item_count; i++) legacy_price[i] = content.items[i].price, content.items[i].price = shipped_price[i];
    content.trade.default_price = shipped_trade.default_price;
    content.trade.sell_share = shipped_trade.sell_share;
    content.trade.discover = shipped_trade.discover;

    /* Everything dealt in has a price of its own, and sells for nine tenths of it. */
    CHECK(content.trade.sell_share == 0.9f && content.trade.discover);
    int priced = 0, different = 0;
    for (ItemId i = 1; i < content.item_count; i++) {
        const ItemDef *def = &content.items[i];
        if (def->category < 0) continue;
        if (shipped_price[i] <= 0) continue; /* (things the tests' own content adds or redefines) */
        priced++;
        different += shipped_price[i] != 10;
        CHECK(trade_price(i) >= 10 && trade_sell_price(i) == trade_price(i) * 9 / 10 && trade_sell_price(i) < trade_price(i));
    }
    CHECK(priced > 300 && different > 250);
    CHECK(trade_price(ITEM_DIRT) == 10 && trade_sell_price(ITEM_DIRT) == 9);
    CHECK(trade_price(item_id("iron_ore")) > trade_price(item_id("copper_ore")) && trade_price(item_id("diamond")) > trade_price(item_id("ruby")));
    CHECK(trade_price(item_id("iron_sword")) > trade_price(item_id("bronze_sword")) && trade_price(item_id("bronze_sword")) > trade_price(item_id("flint_spear")));
    CHECK(trade_price(item_id("marble_column")) > trade_price(item_id("marble")) / 2 && trade_price(item_id("copper_coin")) == 0);

    /* Benches, things to trade at, and whatever takes crystals to make are not dealt in. */
    CHECK(trade_barred(item_id("workbench")) && trade_barred(item_id("furnace")) && trade_barred(item_id("trade_portal")) && trade_barred(item_id("dev_portal")));
    CHECK(trade_barred(item_id("portal_chest")) && trade_barred(item_id("flywheel")));
    CHECK(!trade_barred(ITEM_DIRT) && !trade_barred(item_id("chest")) && !trade_barred(item_id("iron_sword")) && !trade_barred(item_id("marble_column")));
    CHECK(strcmp(tile_def(tile_id("trade_portal"))->label, "Trade portal") == 0 && !tile_def(tile_id("trade_portal"))->trade_free);

    Game g;
    game_init(&g, 64, 48, 47, WORLD_FLAT);
    Inventory *inv = &g.inventory;
    g.tick_rate = 0;
    int x = g.player.feet.x + 2;
    ItemId copper = item_id("copper_coin");
    CHECK(place_and_open(&g, "trade_portal", x) && game_trade_open(&g) && !game_trade_free(&g));
    clear_inventory(&g);
    inventory_add(inv, copper, 99);
    /* Nothing has been found in this world yet: nothing can be bought. */
    memset(g.discovered, 0, sizeof g.discovered);
    CHECK(!game_trade_deals(&g, ITEM_DIRT) && !game_trade_buy(&g, ITEM_DIRT, 1) && coins(&g, "copper_coin") == 99);
    /* Having one in hand is finding it: dug, made, picked up or given. */
    inventory_add(inv, ITEM_DIRT, 1);
    game_tick(&g);
    CHECK(g.discovered[ITEM_DIRT] && g.discovered[copper] && !g.discovered[ITEM_STONE]);
    CHECK(game_trade_deals(&g, ITEM_DIRT) && !game_trade_deals(&g, ITEM_STONE));
    /* Bought for its price, sold for nine tenths of it. */
    CHECK(game_trade_buy(&g, ITEM_DIRT, 2) && coins(&g, "copper_coin") == 79 && count_item(&g, ITEM_DIRT) == 3);
    CHECK(game_trade_sell(&g, ITEM_DIRT, 3) && coins(&g, "copper_coin") == 79 + 27 && count_item(&g, ITEM_DIRT) == 0);
    CHECK(game_trade_deals(&g, ITEM_DIRT)); /* once found, found: having none now makes no difference */
    /* What is in a basket in the bar counts as in hand. */
    ItemStack basket = { (uint16_t)ITEM_BASKET, 1, container_alloc(inv) };
    inv->containers[basket.container].slots[0] = (ItemStack){ (uint16_t)ITEM_STONE, 2, 0 };
    inv->slots[5] = basket;
    game_note_found(&g);
    CHECK(g.discovered[ITEM_STONE] && game_trade_deals(&g, ITEM_STONE));
    /* A bench is not dealt in, found or not: neither bought nor sold. */
    inventory_add(inv, item_id("workbench"), 1);
    game_note_found(&g);
    CHECK(g.discovered[item_id("workbench")] && !game_trade_deals(&g, item_id("workbench")));
    CHECK(!game_trade_buy(&g, item_id("workbench"), 1) && !game_trade_sell(&g, item_id("workbench"), 1) && count_item(&g, item_id("workbench")) == 1);
    /* What has been found is kept with the world. */
    CHECK(save_game(&g, "build/test_found.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_found.world"));
    CHECK(loaded.discovered[ITEM_DIRT] && loaded.discovered[ITEM_STONE] && !loaded.discovered[item_id("iron_sword")]);
    game_free(&loaded);

    /* The dev portal: everything there is, benches and all, found or not, for nothing. */
    memset(g.discovered, 0, sizeof g.discovered);
    CHECK(place_and_open(&g, "dev_portal", x + 6) && game_trade_open(&g) && game_trade_free(&g));
    CHECK(tile_def(tile_id("dev_portal"))->trade_free && tile_def(tile_id("dev_portal"))->trade);
    clear_inventory(&g);
    CHECK(game_trade_deals(&g, item_id("iron_sword")) && game_trade_deals(&g, item_id("workbench")) && game_trade_deals(&g, item_id("portal_chest")));
    CHECK(game_trade_deals(&g, item_id("trade_portal")) && game_trade_deals(&g, copper));
    CHECK(!game_trade_deals(&g, item_id("new_player")) && !game_trade_deals(&g, item_id("time_crystal")));
    CHECK(game_trade_buy(&g, item_id("iron_sword"), 1) && game_trade_buy(&g, item_id("furnace"), 2));
    CHECK(count_item(&g, item_id("iron_sword")) == 1 && count_item(&g, item_id("furnace")) == 2 && trade_wallet(inv) == 0);
    /* Handing something back gives nothing for it. */
    CHECK(game_trade_sell(&g, item_id("furnace"), 1) && count_item(&g, item_id("furnace")) == 1 && trade_wallet(inv) == 0);
    game_close_bench(&g);

    /* Hurrying what is being made: a crystal for every 10 seconds it still has to go. */
    CHECK(content.rules.rush_seconds == 10.0f);
    CHECK(place_and_open(&g, "workbench", x + 12));
    Recipe *r = (Recipe *)find_recipe(TILE_WORKBENCH, "woodwork_bench");
    CHECK(r != NULL);
    float was = r->time;
    r->time = 100.0f;
    clear_inventory(&g);
    for (int i = 0; i < r->input_count; i++) inventory_add(inv, r->inputs[i].id, r->inputs[i].count * 2);
    CHECK(game_craft_rush_cost(&g) == -1); /* nothing being made */
    CHECK(game_craft(&g, r, 2));
    run(&g, 0.5f);
    CHECK(g.craft_remaining == 2 && game_craft_rush_cost(&g) == 20);
    run(&g, 44.6f);
    CHECK(game_craft_rush_cost(&g) == 16); /* 154.9 seconds to go */
    g.crystals = 15;
    CHECK(!game_craft_rush(&g) && g.crystals == 15 && g.craft_remaining == 2);
    g.crystals = 100;
    CHECK(game_craft_rush(&g) && g.crystals == 84);
    run(&g, 0.1f);
    CHECK(g.craft_remaining == 0 && count_item(&g, r->output.id) + drops_of(&g, r->output.id) == 2 && game_craft_rush_cost(&g) == -1);

    /* Or, as a setting of the world, nothing takes any time: a tick each. */
    clear_inventory(&g);
    clear_drops(&g);
    for (int i = 0; i < r->input_count; i++) inventory_add(inv, r->inputs[i].id, r->inputs[i].count * 3);
    CHECK(game_craft_seconds(&g, r) == 100.0f);
    g.quick_craft = true;
    g.tick_rate = 10;
    CHECK(game_craft_seconds(&g, r) > 0.09f && game_craft_seconds(&g, r) < 0.11f && game_craft_time(r) == 100.0f);
    g.tick_rate = 0;
    CHECK(game_craft(&g, r, 3));
    run(&g, 0.6f);
    CHECK(g.craft_remaining == 0 && count_item(&g, r->output.id) + drops_of(&g, r->output.id) == 3);
    CHECK(save_game(&g, "build/test_quick.world") && load_game(&loaded, "build/test_quick.world") && loaded.quick_craft);
    game_free(&loaded);
    r->time = was;
    game_free(&g);

    content.trade.default_price = legacy_trade.default_price;
    content.trade.sell_share = legacy_trade.sell_share;
    content.trade.discover = legacy_trade.discover;
    for (ItemId i = 1; i < content.item_count; i++) content.items[i].price = legacy_price[i];
}

/* Safes, shelves and cabinets; beds; coffee; cages; paint and what takes it off. */
static void test_furniture_cages_paint(void)
{
    Game g;
    game_init(&g, 96, 48, 48, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    clear_mobs(&g);
    g.tick_rate = 0;
    g.random_events = 0;
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;
    const RulesDef *u = &content.rules;

    /* A safe is a chest of iron: 16 slots. A shelf and a display cabinet hold 4, and show them. */
    const TileDef *safe = tile_def(tile_id("safe")), *shelf = tile_def(tile_id("shelf")), *cabinet = tile_def(tile_id("display_cabinet"));
    CHECK(safe->storage_columns * safe->storage_rows == 16 && safe->solid && !safe->shows_contents);
    CHECK(shelf->storage_columns * shelf->storage_rows == 4 && shelf->shows_contents && shelf->layer == LAYER_BACKGROUND);
    CHECK(cabinet->storage_columns * cabinet->storage_rows == 4 && cabinet->shows_contents && cabinet->layer == LAYER_BACKGROUND);
    CHECK(item_def(item_id("safe"))->places == tile_id("safe") && item_def(item_id("safe"))->container_slots == 16);
    CHECK(item_def(item_id("shelf"))->container_slots == 4 && item_def(item_id("display_cabinet"))->container_slots == 4);
    CHECK(place_and_open(&g, "shelf", px + 3));
    uint16_t box = game_open_storage(&g);
    CHECK(box != 0);
    inv->containers[box].slots[0] = (ItemStack){ (uint16_t)ITEM_STONE, 3, 0 };
    game_close_bench(&g);
    /* Taken down with things on it, it keeps them. */
    CHECK(game_click_tile(&g, px + 3, ground));
    game_cancel_actions(&g);
    CHECK(game_queue_action(&g, ACTION_MINE, px + 3, ground));
    run(&g, 5.0f);
    ItemStack *taken = inventory_find(inv, item_id("shelf"));
    CHECK(taken && taken->container != 0 && inv->containers[taken->container].slots[0].id == ITEM_STONE);

    /* A bed is two blocks wide: it wants two squares side by side on level ground, and comes up from either end as one. */
    TileType bed = tile_id("wooden_bed"), foot = tile_id("wooden_bed_foot"), soft = tile_id("soft_bed");
    CHECK(tile_def(bed)->pair == foot && tile_def(foot)->pair_of == bed && tile_def(bed)->sleep_speed == 2.0f && !tile_def(bed)->sleep_over);
    CHECK(tile_def(soft)->sleep_speed == 3.0f && tile_def(soft)->sleep_over && tile_def(soft)->pair == tile_id("soft_bed_foot"));
    g.player.feet = g.player.step_to = (TilePos){ px + 9, ground };
    clear_inventory(&g);
    inventory_add(inv, item_id("wooden_bed"), 2);
    inventory_select(inv, 0, -1);
    world_set(w, px + 12, s, TILE_AIR); /* (a hole where its foot would rest) */
    CHECK(!game_queue_action(&g, ACTION_PLACE, px + 11, ground));
    world_set(w, px + 14, ground, TILE_STONE); /* (something in the way of its foot) */
    CHECK(!game_queue_action(&g, ACTION_PLACE, px + 13, ground));
    world_set(w, px + 14, ground, TILE_AIR);
    world_set(w, px + 12, s, TILE_DIRT);
    CHECK(game_queue_action(&g, ACTION_PLACE, px + 11, ground));
    run(&g, 5.0f);
    CHECK(world_get_at(w, BG, px + 11, ground) == bed && world_get_at(w, BG, px + 12, ground) == foot && count_item(&g, item_id("wooden_bed")) == 1);
    clear_inventory(&g);
    CHECK(game_queue_action(&g, ACTION_MINE, px + 12, ground)); /* by the foot */
    run(&g, 5.0f);
    CHECK(world_get_at(w, BG, px + 11, ground) == TILE_AIR && world_get_at(w, BG, px + 12, ground) == TILE_AIR);
    CHECK(count_item(&g, item_id("wooden_bed")) + drops_of(&g, item_id("wooden_bed")) == 1);
    clear_drops(&g);

    /* Sleep: so long from a fifth of a bar on the ground, half that in a bed, a third in a soft one. */
    int plain = 0, in_bed = 0, in_soft = 0;
    g.player.feet = g.player.step_to = (TilePos){ px + 20, ground };
    bool over = true;
    CHECK(game_sleep_speed(&g, &over) == 1.0f && !over);
    g.vitals.energy = 0.2f;
    g.vitals.hunger = 1.0f;
    CHECK(game_rest(&g, REST_SLEEP));
    while (g.vitals.rest == REST_SLEEP && plain < 5000) game_tick(&g), plain++;
    CHECK(g.vitals.energy >= 0.999f && g.vitals.energy <= 1.001f && !game_rest(&g, REST_SLEEP));
    world_set_at(w, BG, px + 20, ground, bed);
    world_set_at(w, BG, px + 21, ground, foot);
    CHECK(game_sleep_speed(&g, &over) == 2.0f && !over);
    g.vitals.energy = 0.2f;
    g.vitals.hunger = 1.0f;
    CHECK(game_rest(&g, REST_SLEEP));
    while (g.vitals.rest == REST_SLEEP && in_bed < 5000) game_tick(&g), in_bed++;
    CHECK(g.vitals.energy <= 1.001f && in_bed * 2 >= plain - 4 && in_bed * 2 <= plain + 4);
    /* (Lying at the foot of it is being in it too.) */
    g.player.feet = g.player.step_to = (TilePos){ px + 21, ground };
    CHECK(game_sleep_speed(&g, &over) == 2.0f);
    /* A soft bed goes on past full: half a bar over, which shows gold and runs down like the rest. */
    CHECK(u->energy_over == 0.5f);
    world_set_at(w, BG, px + 20, ground, soft);
    world_set_at(w, BG, px + 21, ground, tile_id("soft_bed_foot"));
    g.player.feet = g.player.step_to = (TilePos){ px + 20, ground };
    CHECK(game_sleep_speed(&g, &over) == 3.0f && over);
    g.vitals.energy = 0.2f;
    g.vitals.hunger = 1.0f;
    CHECK(game_rest(&g, REST_SLEEP));
    while (g.vitals.rest == REST_SLEEP && in_soft < 5000) game_tick(&g), in_soft++;
    CHECK(g.vitals.energy > 1.499f && g.vitals.energy < 1.501f && in_soft < plain);
    CHECK(!game_rest(&g, REST_SLEEP)); /* as full as it gets */
    g.vitals.energy = 1.0f;
    CHECK(game_rest(&g, REST_SLEEP)); /* full, but in a soft bed there is further to go */
    game_wake(&g);
    g.vitals.energy = 1.5f;
    g.vitals.hunger = 1.0f;
    g.vitals.saturation = 100000.0f;
    int lasted = 0;
    while (g.vitals.energy > 1.0f && lasted < 100000) game_tick(&g), lasted++;
    CHECK(lasted > u->energy_ticks / 2 - 5 && lasted < u->energy_ticks / 2 + 5);

    /* Coffee takes it past full as well; other food does not. */
    ItemId coffee = item_id("coffee_bean");
    content.items[coffee].energy_over = true; /* (the cup of coffee does this in the game; here the cherry is made to, to try it with) */
    CHECK(item_def(coffee)->food_energy > 0.0f && !item_def(item_id("apple"))->energy_over);
    clear_inventory(&g);
    inventory_add(inv, coffee, 20);
    inventory_select(inv, 0, -1);
    g.vitals.energy = 1.0f;
    g.vitals.hunger = 1.0f;
    g.vitals.saturation = 100000.0f;
    CHECK(game_can_eat(&g, coffee) && game_eat(&g) && g.vitals.energy > 1.0f);
    for (int k = 0; k < 10 && game_can_eat(&g, coffee); k++) game_eat(&g);
    CHECK(g.vitals.energy > 1.499f && g.vitals.energy < 1.501f && !game_can_eat(&g, coffee) && count_item(&g, coffee) > 10);
    g.vitals.saturation = 0.0f;
    g.vitals.energy = 1.0f;

    /* A cage takes up a creature of the land - not of the water - as an item... */
    int dodo = mob_id("dodo"), fish = mob_id("fish"), caged = 0;
    for (int m = 0; m < content.mob_count; m++) {
        const MobDef *def = &content.mobs[m];
        CHECK(def->swims == (def->caged == ITEM_NONE));
        if (def->caged == ITEM_NONE) continue;
        caged++;
        CHECK(item_def(def->caged)->releases == m && item_def(def->caged)->category >= 0 && strncmp(item_def(def->caged)->label, "Caged ", 6) == 0);
    }
    CHECK(caged == content.mob_count - 2 && item_def(item_id("cage"))->cages && item_def(item_id("cage"))->releases < 0);
    CHECK(item_def(item_id("caged_dropbear"))->releases == mob_id("dropbear") && item_def(item_id("caged_scorpion"))->releases == mob_id("scorpion"));
    float moves = content.mobs[dodo].move_chance;
    content.mobs[dodo].move_chance = 0.0f;
    g.player.feet = g.player.step_to = (TilePos){ px + 30, ground };
    clear_inventory(&g);
    inventory_add(inv, item_id("cage"), 2);
    inventory_select(inv, 0, -1);
    int bird = mob_spawn(&g, dodo, px + 32, ground, false, false);
    CHECK(game_click_mob(&g, bird) && g.queue[0].type == ACTION_ATTACK && g.queue[0].arg == HAND_CAGE);
    run(&g, 5.0f);
    CHECK(!g.mobs[bird].active && count_mobs(&g) == 0 && count_item(&g, item_id("caged_dodo")) == 1 && count_item(&g, item_id("cage")) == 1);
    CHECK(drops_of(&g, item_id("dodo_meat")) == 0); /* (taken alive) */
    int swimmer = mob_spawn(&g, fish, px + 32, ground, true, false);
    CHECK(game_click_mob(&g, swimmer) && g.queue[0].arg == HAND_HIT);
    game_cancel_actions(&g);
    clear_mobs(&g);
    /* ...to be let out somewhere else, where it is the player's own from then on, and the cage is empty again. */
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (inv->slots[i].id == item_id("caged_dodo")) inventory_select(inv, i, -1);
    CHECK(inventory_selected(inv)->id == item_id("caged_dodo"));
    CHECK(game_click_tile(&g, px + 33, ground) && g.queue[0].type == ACTION_RELEASE);
    run(&g, 5.0f);
    CHECK(count_mobs(&g) == 1 && count_item(&g, item_id("caged_dodo")) == 0 && count_item(&g, item_id("cage")) == 2);
    for (int i = 0; i < MAX_MOBS; i++)
        if (g.mobs[i].active) CHECK(g.mobs[i].kind == dodo && g.mobs[i].owned && !g.mobs[i].baby && g.mobs[i].x == px + 33 && g.mobs[i].y == ground);
    /* Not into a block, nor into the air. */
    inventory_add(inv, item_id("caged_dodo"), 1);
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (inv->slots[i].id == item_id("caged_dodo")) inventory_select(inv, i, -1);
    CHECK(!game_queue_action(&g, ACTION_RELEASE, px + 31, ground - 6) && !game_queue_action(&g, ACTION_RELEASE, px + 31, s));
    content.mobs[dodo].move_chance = moves;
    clear_mobs(&g);

    /* Paint. (Three colours of it, for these tests.) */
    static const unsigned char colours[3][3] = { { 200, 40, 40 }, { 40, 80, 200 }, { 240, 220, 60 } };
    memcpy(content.tints, colours, sizeof colours);
    content.tint_count = 3;
    ItemId paint = item_id("paint"), stripper = item_id("paint_stripper");
    CHECK(item_def(paint)->paints && item_def(paint)->uses == 100 && item_def(paint)->tinted && item_def(stripper)->strips && item_def(stripper)->uses == 100);
    g.player.feet = g.player.step_to = (TilePos){ px + 40, ground };
    clear_inventory(&g);
    ItemStack can = { (uint16_t)paint, 1, 0 };
    CHECK(inventory_set_meter(inv, &can, 2) && inventory_add_stack(inv, can) == 0);
    inventory_add(inv, stripper, 1);
    inventory_select(inv, 0, -1);
    world_set(w, px + 42, ground, TILE_STONE);
    /* On a block: the block takes the colour, and the paint is one use the less. */
    CHECK(world_paint(w, false, px + 42, ground) == 0);
    CHECK(game_click_tile(&g, px + 42, ground) && g.queue[0].type == ACTION_PAINT);
    run(&g, 5.0f);
    CHECK(world_paint(w, false, px + 42, ground) == 2 && world_get(w, px + 42, ground) == TILE_STONE);
    CHECK(inventory_meter(inv, &inv->slots[0]) == 2 + PAINT_USED && count_item(&g, paint) == 1);
    /* The same colour again uses nothing. */
    CHECK(game_click_tile(&g, px + 42, ground));
    run(&g, 5.0f);
    CHECK(inventory_meter(inv, &inv->slots[0]) == 2 + PAINT_USED);
    /* On backwall, where there is no block: the wall takes it. */
    world_set(w, px + 42, ground - 1, TILE_AIR);
    world_set_at(w, LAYER_WALL, px + 42, ground - 1, TILE_STONE);
    CHECK(game_click_tile(&g, px + 42, ground - 1) && g.queue[0].type == ACTION_PAINT);
    run(&g, 5.0f);
    CHECK(world_paint(w, true, px + 42, ground - 1) == 2 && world_paint(w, false, px + 42, ground - 1) == 0);
    CHECK(inventory_meter(inv, &inv->slots[0]) == 2 + 2 * PAINT_USED);
    /* Thin air takes none: a click there is not a painting. */
    world_set_at(w, LAYER_WALL, px + 43, ground - 3, TILE_AIR);
    CHECK(!game_click_tile(&g, px + 43, ground - 3) || g.queue[g.queue_count - 1].type != ACTION_PAINT);
    game_cancel_actions(&g);
    /* Kept with the world. */
    CHECK(save_game(&g, "build/test_paint.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_paint.world"));
    CHECK(world_paint(&loaded.world, false, px + 42, ground) == 2 && world_paint(&loaded.world, true, px + 42, ground - 1) == 2);
    CHECK(world_paint(&loaded.world, false, px + 41, ground) == 0);
    game_free(&loaded);
    /* Stripper takes it off again, and is used the same way: a hundred times, then gone. */
    inventory_select(inv, 1, -1);
    CHECK(inventory_meter(inv, &inv->slots[1]) == 100);
    CHECK(game_click_tile(&g, px + 42, ground) && g.queue[0].type == ACTION_PAINT);
    run(&g, 5.0f);
    CHECK(world_paint(w, false, px + 42, ground) == 0 && inventory_meter(inv, &inv->slots[1]) == 99);
    CHECK(game_click_tile(&g, px + 42, ground)); /* bare already: nothing used */
    run(&g, 5.0f);
    CHECK(inventory_meter(inv, &inv->slots[1]) == 99);
    /* The hundredth use is the last of a can. */
    inventory_select(inv, 0, -1);
    inventory_set_meter(inv, &inv->slots[0], 2 + 99 * PAINT_USED);
    CHECK(game_click_tile(&g, px + 42, ground));
    run(&g, 5.0f);
    CHECK(world_paint(w, false, px + 42, ground) == 2 && count_item(&g, paint) == 0);
    inventory_select(inv, 1, -1);
    inventory_set_meter(inv, &inv->slots[1], 1);
    CHECK(game_click_tile(&g, px + 42, ground - 1));
    run(&g, 5.0f);
    CHECK(world_paint(w, true, px + 42, ground - 1) == 0 && count_item(&g, stripper) == 0);
    /* Paint goes with the block it is on. */
    clear_inventory(&g);
    CHECK(game_click_tile(&g, px + 42, ground) && g.queue[0].type == ACTION_MINE);
    run(&g, 5.0f);
    CHECK(world_get(w, px + 42, ground) == TILE_AIR && world_paint(w, false, px + 42, ground) == 0);
    content.tint_count = 0;
    game_free(&g);
}

/* A gate, compost, embers, a boat, and signs. */
static void test_gate_compost_ember_boat_sign(void)
{
    Game g;
    game_init(&g, 96, 48, 49, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    clear_mobs(&g);
    g.tick_rate = 0;
    g.random_events = 0;
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;

    /* A gate lets a character through and keeps creatures out. */
    TileType gate = tile_id("wooden_gate");
    CHECK(gate > 0 && tile_def(gate)->solid && tile_def(gate)->passable && tile_def(gate)->mimics == MIMIC_NONE && item_def(item_id("wooden_gate"))->places == gate);
    world_set(w, px + 4, ground, gate);
    world_set(w, px + 4, ground - 1, gate);
    world_set(w, px + 4, ground - 2, TILE_STONE);
    world_set(w, px + 4, ground - 3, TILE_STONE);
    int dodo = mob_id("dodo");
    float moves = content.mobs[dodo].move_chance, turns = content.mobs[dodo].turn_chance;
    content.mobs[dodo].move_chance = 1.0f;
    content.mobs[dodo].turn_chance = 0.0f;
    int bird = mob_spawn(&g, dodo, px + 7, ground, true, false);
    int nearest = 99;
    for (int t = 0; t < 400; t++) {
        g.mobs[bird].dir = -1; /* (always making for the gate) */
        mobs_tick(&g);
        if (g.mobs[bird].x < nearest) nearest = g.mobs[bird].x;
    }
    CHECK(nearest == px + 5);
    content.mobs[dodo].move_chance = moves;
    content.mobs[dodo].turn_chance = turns;
    clear_mobs(&g);
    CHECK(game_queue_action(&g, ACTION_MOVE, px + 7, ground));
    Trace through = run_traced(&g, 10.0f);
    CHECK(g.player.feet.x == px + 7 && g.player.feet.y == ground && !through.climbed);

    /* Compost is ground that plants grow three times as fast on. */
    TileType compost = tile_id("compost");
    CHECK(compost > 0 && tile_def(compost)->solid && tile_def(compost)->fertile == 3.0f && item_def(item_id("compost"))->places == compost && tile_def(TILE_DIRT)->fertile == 0.0f);
    bool sown = false;
    for (int i = 0; i < tile_def(tile_id("carrot_0"))->soil_count; i++) sown = sown || tile_def(tile_id("carrot_0"))->soil[i] == compost;
    CHECK(sown);
    Game t;
    game_init(&t, 64, 48, 50, WORLD_FLAT);
    clear_mobs(&t);
    t.character_count = 0;
    t.random_events = 10;
    int ts = t.world.surface_y, rich = 0, poor = 0;
    for (int x = 0; x < 64; x++) {
        world_set(&t.world, x, ts, x < 32 ? compost : TILE_DIRT);
        world_set_at(&t.world, BG, x, ts - 1, tile_id("carrot_0"));
    }
    for (int k = 0; k < 300; k++) game_tick(&t);
    for (int x = 0; x < 64; x++) {
        TileType c = world_get_at(&t.world, BG, x, ts - 1);
        int stage = c == tile_id("carrot_1") ? 1 : c == tile_id("carrot_2") ? 2 : c == tile_id("carrot_3") ? 3 : 0;
        if (x < 32) rich += stage;
        else poor += stage;
    }
    CHECK(poor > 3 && rich > poor * 2 && rich < poor * 5);
    game_free(&t);

    /* An ember burns down a thing of wood in a moment, and is used up; it does nothing to stone. */
    ItemId ember = item_id("ember");
    CHECK(item_def(ember)->ignites && tile_def(tile_id("wood"))->burns && tile_def(tile_id("chest"))->burns && !tile_def(TILE_STONE)->burns);
    g.player.feet = g.player.step_to = (TilePos){ px + 10, ground };
    world_set(w, px + 12, ground, tile_id("wood"));
    world_set(w, px + 12, ground - 1, TILE_STONE);
    clear_inventory(&g);
    inventory_add(inv, ember, 3);
    inventory_select(inv, 0, -1);
    CHECK(game_click_tile(&g, px + 12, ground) && g.queue[0].type == ACTION_MINE && g.queue[0].item == ember);
    float burnt = 0.0f;
    while (world_get(w, px + 12, ground) != TILE_AIR && burnt < 10.0f) run(&g, 0.05f), burnt += 0.05f;
    CHECK(burnt < 1.0f && burnt < tile_def(tile_id("wood"))->mine_time + content.rules.walk_time * 2.0f + 0.3f);
    run(&g, 2.0f);
    CHECK(count_item(&g, ember) == 2 && count_item(&g, item_id("wood")) + drops_of(&g, item_id("wood")) == 1);
    CHECK(game_click_tile(&g, px + 12, ground - 1) && g.queue[0].item == ITEM_NONE);
    run(&g, 5.0f);
    CHECK(count_item(&g, ember) == 2);
    clear_drops(&g);

    /* A boat goes on water, and nowhere else. */
    TileType boat = tile_id("boat");
    CHECK(boat > 0 && tile_def(boat)->afloat && !tile_def(boat)->solid && content.rules.boat_speed == 2.0f);
    int left = px + 20, right = px + 60;
    for (int x = left - 1; x <= right + 1; x++) {
        for (int y = s; y <= s + 3; y++) {
            bool edge = x < left || x > right || y == s + 3;
            world_set(w, x, y, edge ? TILE_STONE : TILE_AIR);
            if (!edge) world_set_water(w, x, y, WATER_FULL);
        }
    }
    g.player.feet = g.player.step_to = (TilePos){ left - 1, ground };
    clear_inventory(&g);
    inventory_add(inv, item_id("boat"), 2);
    inventory_select(inv, 0, -1);
    CHECK(!game_queue_action(&g, ACTION_PLACE, left - 3, ground)); /* dry land */
    CHECK(!game_queue_action(&g, ACTION_PLACE, left + 1, s + 1));  /* under the water, not on it */
    CHECK(game_queue_action(&g, ACTION_PLACE, left + 1, s));
    run(&g, 5.0f);
    CHECK(world_get(w, left + 1, s) == boat && count_item(&g, item_id("boat")) == 1);
    /* Swimming across, for a while... */
    game_cancel_actions(&g);
    g.player.feet = g.player.step_to = (TilePos){ left + 3, s };
    g.player.climbing = true;
    run(&g, 0.2f);
    int from = g.player.feet.x;
    game_set_move_input(&g, 1, 0);
    run(&g, 2.0f);
    game_set_move_input(&g, 0, 0);
    run(&g, 0.5f);
    int swum = g.player.feet.x - from;
    /* ...and for as long in the boat: twice as far, and the boat comes too. */
    g.player.feet = g.player.step_to = (TilePos){ left + 1, s };
    g.player.climbing = true;
    run(&g, 0.2f);
    CHECK(g.player.feet.x == left + 1 && g.player.feet.y == s);
    game_set_move_input(&g, 1, 0);
    run(&g, 2.0f);
    game_set_move_input(&g, 0, 0);
    run(&g, 0.5f);
    int sailed = g.player.feet.x - (left + 1);
    CHECK(swum >= 8 && sailed >= swum * 2 - 3 && sailed <= swum * 2 + 3);
    CHECK(world_get(w, g.player.feet.x, g.player.feet.y) == boat && world_get(w, left + 1, s) == TILE_AIR && g.player.feet.y == s);
    int boats = 0;
    for (int x = left; x <= right; x++) boats += world_get(w, x, s) == boat;
    CHECK(boats == 1);
    world_set(w, g.player.feet.x, s, TILE_AIR);

    /* Signs. One takes up two blocks, side by side, and goes up against a block - on it, under it or beside it -
     * and not on backwall alone. */
    TileType sign = tile_id("sign"), half = tile_id("sign_right");
    ItemId board = item_id("sign");
    CHECK(sign > 0 && tile_def(sign)->written && !tile_def(sign)->solid && tile_def(sign)->layer == LAYER_FOREGROUND && item_def(board)->written);
    CHECK(tile_def(sign)->pair == half && tile_def(half)->pair_of == sign && tile_def(sign)->pair_floats && !tile_def(half)->written);
    g.player.feet = g.player.step_to = (TilePos){ px - 4, ground };
    g.player.climbing = false;
    clear_inventory(&g);
    inventory_add(inv, board, 3);
    inventory_select(inv, 0, -1);
    for (int dx = 0; dx < 2; dx++) world_set_at(w, LAYER_WALL, px - 7 + dx, ground - 5, TILE_STONE);
    CHECK(!game_queue_action(&g, ACTION_PLACE, px - 7, ground - 5)); /* backwall, and nothing else */
    CHECK(game_queue_action(&g, ACTION_PLACE, px - 7, ground));       /* standing on the ground */
    run(&g, 5.0f);
    CHECK(world_get(w, px - 7, ground) == sign && world_get(w, px - 6, ground) == half && world_mount_block(w, px - 7, ground) == MOUNT_FLOOR);
    CHECK(game_sign_at(&g, px - 7, ground) == NULL && count_item(&g, board) == 2);
    /* Another cannot go in either of the squares it is in, nor where it would have to overlap one of them. */
    CHECK(!game_queue_action(&g, ACTION_PLACE, px - 6, ground) && !game_queue_action(&g, ACTION_PLACE, px - 8, ground));
    /* Hanging under a block, held by either half. */
    world_set(w, px - 10, ground - 2, TILE_STONE);
    CHECK(game_queue_action(&g, ACTION_PLACE, px - 11, ground - 1)); /* its right half is the one under the block */
    run(&g, 5.0f);
    CHECK(world_get(w, px - 11, ground - 1) == sign && world_get(w, px - 10, ground - 1) == half && world_mount_block(w, px - 10, ground - 1) == MOUNT_CEILING);
    /* One line can be written on it, by way of either half; it is cut to what a sign holds. */
    CHECK(game_sign_write(&g, px - 7, ground, "Keep out") && !game_sign_write(&g, px - 4, ground, "Nothing here"));
    const SignText *text = game_sign_at(&g, px - 7, ground);
    CHECK(text && strcmp(text->text, "Keep out") == 0 && game_sign_at(&g, px - 6, ground) == text);
    CHECK(game_sign_write(&g, px - 10, ground - 1, "This is far too long to go on a sign"));
    CHECK(strlen(game_sign_at(&g, px - 11, ground - 1)->text) == SIGN_TEXT_MAX);
    CHECK(game_sign_write(&g, px - 10, ground - 1, "Mine"));
    /* Kept with the world. */
    CHECK(save_game(&g, "build/test_sign.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_sign.world"));
    CHECK(game_sign_at(&loaded, px - 7, ground) && strcmp(game_sign_at(&loaded, px - 7, ground)->text, "Keep out") == 0);
    CHECK(game_sign_at(&loaded, px - 11, ground - 1) && strcmp(game_sign_at(&loaded, px - 11, ground - 1)->text, "Mine") == 0);
    game_free(&loaded);
    /* Taken down - by either half - each keeps what it says; and signs that say different things still stack. */
    CHECK(game_queue_action(&g, ACTION_MINE, px - 6, ground));
    run(&g, 5.0f);
    CHECK(world_get(w, px - 7, ground) == TILE_AIR && world_get(w, px - 6, ground) == TILE_AIR);
    CHECK(game_queue_action(&g, ACTION_MINE, px - 11, ground - 1));
    run(&g, 5.0f);
    CHECK(world_get(w, px - 10, ground - 1) == TILE_AIR && count_item(&g, board) == 3 && inv->slots[0].count == 3 && inv->slots[1].count == 0);
    const SignText *top = game_sign_text(&g, inventory_meter(inv, &inv->slots[0]));
    CHECK(top && strcmp(top->text, "Mine") == 0); /* the last one picked up is on top */
    /* Put up again, it says what it said. */
    CHECK(game_queue_action(&g, ACTION_PLACE, px - 7, ground));
    run(&g, 5.0f);
    CHECK(game_sign_at(&g, px - 7, ground) && strcmp(game_sign_at(&g, px - 7, ground)->text, "Mine") == 0);
    CHECK(game_queue_action(&g, ACTION_PLACE, px - 9, ground)); /* right up against the other: its two squares are free */
    run(&g, 5.0f);
    CHECK(game_sign_at(&g, px - 9, ground) && strcmp(game_sign_at(&g, px - 9, ground)->text, "Keep out") == 0);
    CHECK(game_queue_action(&g, ACTION_PLACE, px - 11, ground));
    run(&g, 5.0f);
    CHECK(world_get(w, px - 11, ground) == sign && game_sign_at(&g, px - 11, ground) == NULL); /* and the blank one is blank */
    /* Writing on one copy of a sign leaves the others as they were. */
    world_set(w, px - 14, ground, sign);
    world_set(w, px - 13, ground, half);
    world_set_meta(w, px - 14, ground, world_meta(w, px - 7, ground));
    CHECK(game_sign_write(&g, px - 14, ground, "Changed"));
    CHECK(strcmp(game_sign_at(&g, px - 7, ground)->text, "Mine") == 0 && strcmp(game_sign_at(&g, px - 14, ground)->text, "Changed") == 0);
    /* Rubbed out, it is a blank sign again. */
    CHECK(game_sign_write(&g, px - 14, ground, "") && game_sign_at(&g, px - 14, ground) == NULL);
    game_free(&g);
}

/* Islands in the sky, the trees on them, and worlds made bigger or smaller. */
static void test_islands_and_options(void)
{
    const WorldGenDef *gen = &content.worldgen;
    static const char *gems[] = { "amethyst", "sapphire", "emerald", "ruby", "diamond" };
    float off = content.worldgen.island_rate;
    content.worldgen.island_rate = shipped_island_rate;
    CHECK(gen->island_rate > 0.0f && gen->island_min_y == 80 && gen->island_min_length == 15 && gen->island_max_length == 30);
    CHECK(gen->island_min_depth == 10 && gen->island_max_depth == 15 && gen->island_base == tile_id("marble") && gen->island_surface == TILE_GRASS);
    CHECK(gen->island_soil == TILE_DIRT && gen->island_min_soil == 2 && gen->island_max_soil == 3 && gen->island_tree_count == 5);
    /* The gem trees: wood and sticks when cut, a shard of their gem for fruit, nothing to plant them with; the rarer the higher. */
    int kind[5];
    for (int k = 0; k < 5; k++) {
        char name[48];
        snprintf(name, sizeof name, "%s_tree", gems[k]);
        kind[k] = tree_id(name);
        CHECK(kind[k] >= 0 && gen->island_trees[k].tree == kind[k]);
        const TreeDef *tree = &content.trees[kind[k]];
        snprintf(name, sizeof name, "%s_shard", gems[k]);
        CHECK(tree->fruit == item_id(name) && item_def(tree->fruit)->plants < 0);
        CHECK(tile_def(tree->log)->drop == item_id("wood") && tile_def(tree->leaves)->drop == ITEM_STICK);
        CHECK(k == 0 || (gen->island_trees[k].weight < gen->island_trees[k - 1].weight && gen->island_trees[k].min_y > gen->island_trees[k - 1].min_y));
        for (ItemId i = 1; i < content.item_count; i++) CHECK(content.items[i].plants != kind[k]);
    }

    CHECK(gen->island_jag == 3 && gen->island_min_cave == 1 && gen->island_max_cave == 2);
    int islands = 0, low = 0, bad_shape = 0, trees[5] = { 0 }, too_low_tree = 0, with_wall = 0, over_spawn = 0;
    int flat_topped = 0, level_soil = 0, solid_rock = 0, smooth_under = 0;
    for (uint32_t seed = 1; seed <= 6; seed++) {
        World w;
        CHECK(world_init(&w, 2048, 401, seed, WORLD_TERRAIN));
        TileType marble = tile_id("marble");
        /* The ground: where the backwalls begin. Anything solid above that is part of an island. */
        int *land = malloc(sizeof(int) * (size_t)w.width);
        uint8_t *seen = calloc((size_t)w.width, (size_t)w.height);
        int *stack = malloc(sizeof(int) * (size_t)w.width * 64);
        for (int x = 0; x < w.width; x++) {
            land[x] = 0;
            while (land[x] < w.height && world_get_at(&w, LAYER_WALL, x, land[x]) == TILE_AIR) land[x]++;
            for (int y = 0; y < land[x]; y++) {
                TileType t = world_get_at(&w, BG, x, y);
                for (int k = 0; k < 5; k++) {
                    if (t != content.trees[kind[k]].log) continue;
                    trees[k]++;
                    if (world_row_to_y(&w, y) < gen->island_trees[k].min_y) too_low_tree++;
                }
            }
        }
        /* Each island: everything solid that hangs together, corner to corner. */
        for (int x = 0; x < w.width; x++) {
            for (int y = 0; y < land[x]; y++) {
                if (!world_is_solid(&w, x, y) || seen[(size_t)y * w.width + x]) continue;
                int n = 0, left = 0, right = 0, top_row = y, bottom_row = y;
                stack[n++] = y * w.width + x;
                seen[(size_t)y * w.width + x] = 1;
                /* (Columns are counted from where it was first met, so one across the seam of the world is all of a piece.) */
                static int col_top[128], col_soil[128], col_end[128];
                for (int k = 0; k < 128; k++) col_top[k] = col_soil[k] = col_end[k] = -1;
                bool cave = false;
                while (n > 0) {
                    int at = stack[--n], ax = at % w.width, ay = at / w.width, off = world_dx(&w, x, ax);
                    if (off < left) left = off;
                    if (off > right) right = off;
                    if (ay < top_row) top_row = ay;
                    if (ay > bottom_row) bottom_row = ay;
                    TileType t = world_get(&w, ax, ay);
                    if (world_get_at(&w, LAYER_WALL, ax, ay) != TILE_AIR) with_wall++;
                    if (abs(world_dx(&w, w.spawn_x, ax)) < 10) over_spawn++;
                    if (world_row_to_y(&w, ay) < gen->island_min_y) low++;
                    /* Grass, dirt and marble and nothing else; grass only on top, and marble never over dirt. */
                    if (t != TILE_GRASS && t != TILE_DIRT && t != marble) bad_shape++;
                    if (t == TILE_GRASS && world_is_solid(&w, ax, ay - 1)) bad_shape++;
                    if (t == TILE_DIRT && world_get(&w, ax, ay - 1) == marble) bad_shape++;
                    int c = off + 64;
                    if (c >= 0 && c < 128) {
                        if (col_top[c] < 0 || ay < col_top[c]) col_top[c] = ay;
                        if (ay > col_end[c]) col_end[c] = ay;
                        if (t == TILE_DIRT && ay > col_soil[c]) col_soil[c] = ay;
                    } else {
                        bad_shape++;
                    }
                    /* A cave: open space with the rock of the island over it and under it. */
                    if (t == marble && world_get(&w, ax, ay + 1) == TILE_AIR) {
                        int under = ay + 1;
                        while (under < land[ax] && world_get(&w, ax, under) == TILE_AIR) under++;
                        if (under < land[ax] && world_get(&w, ax, under) == marble && under - ay <= 4) cave = true;
                    }
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = world_wrap_x(&w, ax + dx), ny = ay + dy;
                            if (ny < 0 || ny >= land[nx] || !world_is_solid(&w, nx, ny) || seen[(size_t)ny * w.width + nx]) continue;
                            seen[(size_t)ny * w.width + nx] = 1;
                            stack[n++] = ny * w.width + nx;
                        }
                    }
                }
                islands++;
                int length = right - left + 1;
                if (length < gen->island_min_length || length > gen->island_max_length) bad_shape++;
                if (bottom_row - top_row + 1 > gen->island_max_depth + 2 * gen->island_jag) bad_shape++;
                /* Nothing about it is a straight line: not its top, not where the soil meets the rock, not its underside. */
                bool flat = true, level = true;
                int first_top = -1, first_soil = -1, steps_under = 0, last_end = -1;
                for (int c = 0; c < 128; c++) {
                    if (col_top[c] < 0) continue;
                    if (first_top < 0) first_top = col_top[c];
                    if (col_top[c] != first_top) flat = false;
                    if (col_soil[c] >= 0 && first_soil < 0) first_soil = col_soil[c];
                    if (col_soil[c] >= 0 && col_soil[c] != first_soil) level = false;
                    if (last_end >= 0 && abs(col_end[c] - last_end) >= 2) steps_under++;
                    last_end = col_end[c];
                }
                flat_topped += flat;
                level_soil += level;
                solid_rock += !cave;
                smooth_under += steps_under < 2;
            }
        }
        free(land);
        free(seen);
        free(stack);
        world_free(&w);
    }
    CHECK(islands > 40 && low == 0 && bad_shape == 0 && with_wall == 0 && over_spawn == 0);
    /* Every one has a cave through it; and none - or as good as none - has a flat top, a level bed of soil or a smooth underside. */
    CHECK(solid_rock == 0 && flat_topped * 20 <= islands && level_soil * 20 <= islands && smooth_under * 20 <= islands);
    CHECK(trees[0] > 0 && trees[1] > 0 && trees[0] >= trees[3] && too_low_tree == 0);
    content.worldgen.island_rate = off;

    /* A world can be made in other sizes, and with bigger or smaller mountains, valleys and veins. */
    CHECK(gen->size_count == 4 && gen->sizes[0].width == 2048 && gen->sizes[0].width == gen->width && gen->sizes[3].width > gen->sizes[2].width);
    CHECK(gen->scale_count == 4 && gen->scales[1].scale == 1.0f && gen->scales[0].scale < 1.0f && gen->scales[3].scale > gen->scales[2].scale);
    int peak[3], floor_[3], copper[3], widest = 0;
    for (int k = 0; k < 3; k++) {
        float scale = k == 0 ? gen->scales[0].scale : k == 1 ? 1.0f : gen->scales[3].scale;
        WorldOptions options = { scale, scale, scale };
        World w;
        CHECK(world_init_with(&w, k == 2 ? gen->sizes[1].width : gen->width, 401, 5, WORLD_TERRAIN, &options));
        if (w.width > widest) widest = w.width;
        peak[k] = -1000;
        floor_[k] = 1000;
        copper[k] = 0;
        for (int x = 0; x < w.width; x++) {
            int level = column_level(&w, x);
            if (level > peak[k]) peak[k] = level;
            if (level < floor_[k]) floor_[k] = level;
        }
        VeinStats st = vein_stats(&w, tile_id("coal_ore"), 1000);
        copper[k] = st.groups > 0 ? st.blocks / st.groups : 0;
        CHECK(world_row_to_y(&w, w.spawn_y) == 0);
        world_free(&w);
    }
    CHECK(peak[0] < peak[1] && peak[1] < peak[2] && floor_[0] > floor_[1] && floor_[1] > floor_[2]);
    CHECK(copper[0] < copper[1] && copper[1] < copper[2] && widest == gen->sizes[1].width);
    /* Asked for nothing in particular, a world is the same as it ever was. */
    World plain, same;
    WorldOptions usual = { 1.0f, 1.0f, 1.0f };
    CHECK(world_init(&plain, 512, 401, 9, WORLD_TERRAIN) && world_init_with(&same, 512, 401, 9, WORLD_TERRAIN, &usual));
    CHECK(memcmp(plain.layers[0], same.layers[0], (size_t)512 * 401 * sizeof(uint16_t)) == 0);
    world_free(&plain);
    world_free(&same);
}

/* Corn, basalt, windows, and the reinforced platform. */
static void test_corn_basalt_windows(void)
{
    /* Corn is one of the crops that end up two blocks tall. */
    TileType corn[4], ear = tile_id("corn_top");
    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof name, "corn_%d", i);
        corn[i] = tile_id(name);
        CHECK(corn[i] > 0 && tile_def(corn[i])->drop == item_id("corn"));
    }
    CHECK(item_def(item_id("corn"))->sows == corn[0] && tile_def(corn[0])->grows_into == corn[1] && tile_def(corn[1])->grows_into == corn[2]);
    CHECK(tile_def(corn[2])->grows_into == corn[3] && tile_def(corn[2])->grows_top == ear && tile_def(corn[3])->topped_by == ear);
    CHECK(tile_def(ear)->cut_to == corn[2] && tile_def(ear)->drop_count == 3);
    Game g;
    game_init(&g, 64, 48, 51, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    g.tick_rate = 0;
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;
    world_set_at(w, BG, px + 2, ground, corn[0]);
    for (int k = 0; k < 3; k++) game_random_event(&g, px + 2, ground);
    CHECK(world_get_at(w, BG, px + 2, ground) == corn[3] && world_get_at(w, BG, px + 2, ground - 1) == ear);
    clear_inventory(&g);
    CHECK(game_click_tile(&g, px + 2, ground - 1));
    run(&g, 3.0f);
    CHECK(world_get_at(w, BG, px + 2, ground) == corn[2] && count_item(&g, item_id("corn")) + drops_of(&g, item_id("corn")) == 3);
    clear_drops(&g);

    /* A window goes on a backwall, and is a hole in it: clear glass lets the daylight through, dark glass does not. */
    TileType pane = tile_id("window"), dark = tile_id("black_window");
    CHECK(tile_def(pane)->window && !tile_def(pane)->window_dark && tile_def(dark)->window && tile_def(dark)->window_dark);
    CHECK(tile_def(pane)->layer == LAYER_BACKGROUND && item_def(item_id("window"))->places == pane && item_def(item_id("black_window"))->places == dark);
    clear_inventory(&g);
    inventory_add(inv, item_id("window"), 2);
    inventory_select(inv, 0, -1);
    world_set_at(w, LAYER_WALL, px + 3, ground - 1, TILE_AIR);
    CHECK(!game_queue_action(&g, ACTION_PLACE, px + 3, ground - 1)); /* no wall there to put it in: not even beside a block */
    CHECK(!game_queue_action(&g, ACTION_PLACE, px + 3, ground - 4));
    world_set_at(w, LAYER_WALL, px + 3, ground - 1, TILE_STONE);
    CHECK(game_queue_action(&g, ACTION_PLACE, px + 3, ground - 1));
    run(&g, 5.0f);
    CHECK(world_get_at(w, BG, px + 3, ground - 1) == pane && world_get_at(w, LAYER_WALL, px + 3, ground - 1) == TILE_STONE);
    /* (A room walled in behind and roofed over, at midday: dark, but for what a window lets in.) */
    Game d;
    game_init(&d, 64, 64, 52, WORLD_FLAT);
    d.tick = (uint64_t)content.rules.day_ticks / 2;
    int ds = d.world.surface_y, cx = 30;
    for (int x = cx - 12; x <= cx + 12; x++) {
        for (int y = ds - 8; y < ds; y++) {
            bool shell = y == ds - 8 || x == cx - 12 || x == cx + 12;
            world_set(&d.world, x, y, shell ? TILE_STONE : TILE_AIR);
            world_set_at(&d.world, LAYER_WALL, x, y, TILE_STONE);
        }
    }
    static uint8_t lit[40 * 20];
    light_compute(&d, cx - 20, ds - 12, 40, 20, lit);
#define LIT(X, Y) lit[((Y) - (ds - 12)) * 40 + ((X) - (cx - 20))]
    int before = LIT(cx, ds - 4);
    world_set_at(&d.world, BG, cx, ds - 4, dark);
    light_compute(&d, cx - 20, ds - 12, 40, 20, lit);
    CHECK(LIT(cx, ds - 4) == before && before < light_sky(&d) - 3);
    world_set_at(&d.world, BG, cx, ds - 4, pane);
    light_compute(&d, cx - 20, ds - 12, 40, 20, lit);
    CHECK(LIT(cx, ds - 4) == light_sky(&d) && LIT(cx + 2, ds - 4) > before && LIT(cx + 2, ds - 4) < light_sky(&d));
#undef LIT
    game_free(&d);

    /* A reinforced platform is a block to build with that brings no backwall: not behind itself, and not under itself. */
    TileType deck = tile_id("reinforced_platform");
    CHECK(deck > 0 && tile_def(deck)->solid && !tile_def(deck)->wall && item_def(item_id("reinforced_platform"))->places == deck);
    for (int k = 0; k < 8; k++) world_set(w, px - 4, ground - k, TILE_STONE);
    for (int k = 0; k < 8; k++) world_set_at(w, LAYER_WALL, px - 3, ground - k, TILE_AIR);
    clear_inventory(&g);
    inventory_add(inv, item_id("reinforced_platform"), 1);
    inventory_add(inv, ITEM_STONE, 1);
    inventory_select(inv, 0, -1);
    CHECK(game_queue_action(&g, ACTION_PLACE, px - 3, ground - 5));
    run(&g, 10.0f);
    CHECK(world_get(w, px - 3, ground - 5) == deck);
    for (int k = 0; k < 8; k++) CHECK(world_get_at(w, LAYER_WALL, px - 3, ground - k) == TILE_AIR);
    /* (Where a block of stone put up in the same place would have run a wall down to the ground.) */
    world_set(w, px - 3, ground - 5, TILE_AIR);
    inventory_select(inv, 1, -1);
    CHECK(game_queue_action(&g, ACTION_PLACE, px - 3, ground - 5));
    run(&g, 10.0f);
    CHECK(world_get_at(w, LAYER_WALL, px - 3, ground - 2) == TILE_STONE);
    game_free(&g);

    /* Basalt lies in patches deep down, and nowhere near the surface; corn grows wild. */
    TileType basalt = tile_id("basalt");
    CHECK(basalt > 0 && tile_def(basalt)->drop == item_id("basalt") && tile_def(basalt)->tool_class == tool_class_id("pickaxe"));
    int total = 0, shallow = 0, wild = 0;
    for (uint32_t seed = 1; seed <= 3; seed++) {
        World world;
        CHECK(world_init(&world, 2048, 401, seed, WORLD_TERRAIN));
        VeinStats st = vein_stats(&world, basalt, 40);
        CHECK(st.groups > 10 && st.smallest >= 14 && st.over == 0);
        total += st.blocks;
        for (int x = 0; x < world.width; x++) {
            int top = world_y_to_row(&world, column_level(&world, x) - 1);
            for (int y = 0; y < world.height; y++) {
                if (world_get(&world, x, y) == basalt && y - top < 60) shallow++; /* (a patch under a hillside is nearer the valley floor than the hilltop) */
                wild += strncmp(tile_def(world_get_at(&world, BG, x, y))->name, "corn_", 5) == 0;
            }
        }
        world_free(&world);
    }
    CHECK(total > 1000 && shallow == 0 && wild > 5);
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
    /* (It does not stand still for it: a blow at where it was a moment ago misses, and is tried again.) */
    for (int hit = 0; hit < 12 && g.mobs[born].active; hit++) {
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

    /* A shard of each of the five gems, with each rarer than the last; the fruits; the six ores. */
    const char *gems[] = { "amethyst_shard", "sapphire_shard", "emerald_shard", "ruby_shard", "diamond_shard" };
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
    CHECK(count_drops(&g, item_id("leather")) >= 1);
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
    CHECK(def->layer == LAYER_BACKGROUND && !def->solid && tile_def(tile_id("portal_chest"))->layer == LAYER_BACKGROUND);
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
                if (world_get_at(tw, BG, x, y) != chest) continue;
                found++;
                where = (TilePos){ x, y };
                /* (Behind the sand it is buried in, not in place of it.) */
                if (!beach || !world_is_solid(tw, x, y) || world_get(tw, x, y - 1) != sand || !world_is_solid(tw, x, y + 1)) misplaced++;
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
    /* (Still under the sand it is not to be seen, nor opened: the sand in front of it is dug away first.) */
    CHECK(game_bench_at(&t, where.x, where.y) == TILE_AIR && !game_queue_action(&t, ACTION_USE, where.x, where.y));
    world_set(tw, where.x, where.y, TILE_AIR);
    CHECK(game_bench_at(&t, where.x, where.y) == chest);
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
        else if (strcmp(name, "amethyst_shard") == 0 || strcmp(name, "sapphire_shard") == 0 ||
                 strcmp(name, "emerald_shard") == 0 || strcmp(name, "ruby_shard") == 0) gems += st->count;
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
    CHECK(gem->choices[0].item == item_id("amethyst_shard") && gem->choices[3].item == item_id("ruby_shard"));
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
        if (strcmp(tree[i].kind, "stone_dodo") == 0) {
            /* (Stone leads three ways: to amethyst and copper, and to limestone, the first of the building stones.) */
            bool lime = false;
            for (int k = 0; k < def->lay_next_count; k++) lime = lime || def->lay_next[k] == mob_id("limestone_dodo");
            CHECK(def->lay_next_count == 3 && lime);
        } else {
            CHECK(breeds_into(tree[i].kind, tree[i].next_a, tree[i].next_b));
        }
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
                 { "dirt_dodo", "dirt_dodo_egg", "wood_dodo_egg", "flint_dodo_egg" },
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
    world_set_at(w, tile_def(tile_id("portal"))->layer, far, ground - 2, tile_id("portal"));
    /* (Its lowest block is solid: the place to arrive is on top of that.) */
    CHECK(tile_def(tile_id("portal"))->solid_foot && world_is_solid(w, far, ground) && !world_is_solid(w, far, ground - 1));
    CHECK(game_open_bench(&g, far, ground - 1) && game_teleport_cost(&g) == 10 && game_teleport(&g));
    CHECK(g.player.feet.x == far && g.player.feet.y == ground - 1 && g.crystals == 15);
    /* Put down by hand, a portal's lowest block is one to walk over and stand on, not through. */
    int at = world_wrap_x(w, far + 6);
    ItemId portal_item = item_id("portal");
    g.player.feet = g.player.step_to = (TilePos){ far + 3, ground };
    clear_inventory(&g);
    inventory_add(&g.inventory, portal_item, 1);
    inventory_add(&g.inventory, ITEM_DIRT, 1);
    inventory_select(&g.inventory, 0, -1);
    CHECK(game_queue_action(&g, ACTION_PLACE, at, ground));
    run(&g, 5.0f);
    CHECK(world_get_at(w, LAYER_BACKGROUND, at, ground) == tile_id("portal") && world_get(w, at, ground) == TILE_AIR);
    CHECK(world_is_solid(w, at, ground) && !world_is_solid(w, at, ground - 1) && !world_is_solid(w, at, ground - 2));
    inventory_select(&g.inventory, 1, -1);
    CHECK(!game_queue_action(&g, ACTION_PLACE, at, ground)); /* no block goes where it is */
    CHECK(game_queue_action(&g, ACTION_MOVE, at + 3, ground));
    Trace across = run_traced(&g, 10.0f);
    CHECK(g.player.feet.x == world_wrap_x(w, at + 3) && g.player.feet.y == ground && is_idle(&g));
    (void)across;
    CHECK(game_queue_action(&g, ACTION_MOVE, at, ground - 1));
    run(&g, 10.0f);
    CHECK(g.player.feet.x == at && g.player.feet.y == ground - 1 && is_idle(&g));
    /* It still comes up whole, by any of its blocks. */
    g.player.feet = g.player.step_to = (TilePos){ at + 1, ground };
    CHECK(game_queue_action(&g, ACTION_MINE, at, ground));
    run(&g, 10.0f);
    CHECK(world_get_at(w, LAYER_BACKGROUND, at, ground) == TILE_AIR && world_get_at(w, LAYER_BACKGROUND, at, ground - 2) == TILE_AIR);
    CHECK(!world_is_solid(w, at, ground) && count_item(&g, portal_item) == 1);
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

    /* Bows hit from up to 10 blocks off, without a step taken: a copper bow for 2. */
    ItemId bow = item_id("copper_bow"), poison_bow = item_id("gold_bow");
    CHECK(item_def(bow)->range == 10 && item_def(bow)->damage == 2 && item_def(bow)->poison == 0);
    CHECK(item_def(poison_bow)->range == 10 && item_def(poison_bow)->damage == 5);
    CHECK(item_def(poison_bow)->poison == 5 && item_def(poison_bow)->poison_every == 2);
    CHECK(game_attack_range(&g) == 0);
    clear_inventory(&g);
    inventory_add(inv, bow, 1);
    inventory_select(inv, 0, -1);
    CHECK(game_attack_range(&g) == 10 && game_attack_damage(&g) == 2);
    TilePos stood = g.player.feet;
    m = mob_spawn(&g, dodo, px + 9, ground, true, false);
    CHECK(game_click_mob(&g, m));
    run(&g, blow + 0.1f);
    CHECK(g.mobs[m].hp == 3 && g.mobs[m].poison == 0 && is_idle(&g) && g.player.feet.x == stood.x && g.shot.live);
    /* Further off than that, the player walks up until it is in range, and no nearer. */
    mob_kill(&g, m);
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    m = mob_spawn(&g, dodo, px + 16, ground, true, false);
    CHECK(game_click_mob(&g, m));
    run(&g, 20.0f);
    CHECK(g.mobs[m].hp == 3 && is_idle(&g) && g.player.feet.x > stood.x && g.mobs[m].x - g.player.feet.x >= 9);
    mob_kill(&g, m);
    /* A wall in the way stops an arrow: the player has to come round to within reach. */
    g.player.feet = g.player.step_to = stood;
    for (int y = ground - 3; y <= ground; y++) world_set(w, px + 4, y, TILE_STONE);
    m = mob_spawn(&g, dodo, px + 6, ground, true, false);
    CHECK(game_click_mob(&g, m));
    run(&g, blow + 0.1f);
    CHECK(g.mobs[m].hp == 5);
    for (int y = ground - 3; y <= ground; y++) world_set(w, px + 4, y, TILE_AIR);
    mob_kill(&g, m);
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    g.queue_count = 0;
    g.has_current = false;
    g.player.feet = g.player.step_to = stood;

    /* A gold bow does 5, and leaves poison that does 5 more: 1 every 2 ticks, over 10. */
    clear_inventory(&g);
    inventory_add(inv, poison_bow, 1);
    inventory_select(inv, 0, -1);
    content.mobs[dodo].health = 25;
    m = mob_spawn(&g, dodo, px + 8, ground, true, false);
    CHECK(game_click_mob(&g, m));
    run(&g, blow + 0.1f);
    CHECK(g.mobs[m].hp == 20 && mob_poison(&g.mobs[m]) == 5);
    for (int t = 1; t <= 10; t++) {
        mobs_tick(&g);
        CHECK(g.mobs[m].hp == 20 - t / 2);
    }
    CHECK(g.mobs[m].hp == 15 && g.mobs[m].poison == 0);
    for (int t = 0; t < 10; t++) mobs_tick(&g);
    CHECK(g.mobs[m].hp == 15);
    /* Poison finishes what the arrow did not. */
    g.mobs[m].hp = 7;
    CHECK(game_click_mob(&g, m));
    run(&g, blow + 0.1f);
    CHECK(g.mobs[m].active && g.mobs[m].hp == 2);
    for (int t = 0; t < 4; t++) mobs_tick(&g);
    CHECK(!g.mobs[m].active);
    content.mobs[dodo].health = 5;
    for (int i = 0; i < MAX_DROPS; i++) g.drops[i].active = false;
    clear_inventory(&g);
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
    CHECK(content.mobs[fish].swims && content.mobs[fish].spawn_water && content.mobs[fish].spawn_biome_count == 1 && content.mobs[fish].spawn_biomes[0] == ocean);
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

static void test_timed_work(void)
{
    Game g;
    game_init(&g, 64, 48, 161, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    clear_mobs(&g);
    g.tick_rate = 0;
    int s = w->surface_y, x = g.player.feet.x + 2;
    CHECK(place_and_open(&g, "workbench", x));

    /* A recipe takes as long as it says: nothing comes of it before that, and
     * each one after takes as long again. */
    Recipe *r = (Recipe *)find_recipe(TILE_WORKBENCH, "woodwork_bench");
    CHECK(r && game_craft_time(r) == CRAFT_TIME);
    r->time = 2.0f;
    ItemId made = item_id("woodwork_bench");
    clear_inventory(&g);
    inventory_add(inv, ITEM_WOOD, 5);
    CHECK(game_craft_time(r) == 2.0f && game_craft(&g, r, 2) && game_craft_progress(&g) == 0.0f);
    run(&g, 1.0f);
    CHECK(count_item(&g, made) == 0 && inventory_count(inv, ITEM_WOOD) == 5);
    CHECK(game_craft_progress(&g) > 0.4f && game_craft_progress(&g) < 0.6f);
    run(&g, 1.1f);
    CHECK(count_item(&g, made) == 1 && inventory_count(inv, ITEM_WOOD) == 4 && g.craft_remaining == 1);
    run(&g, 2.0f);
    CHECK(count_item(&g, made) == 2 && g.craft_remaining == 0 && game_craft_progress(&g) == -1.0f);
    r->time = 0.0f;

    /* An upgrade that takes time is begun, and is paid for and done when the time is up. */
    TileDef *bench = &content.tiles[TILE_WORKBENCH];
    float was = bench->upgrade_time;
    bench->upgrade_time = 3.0f;
    clear_inventory(&g);
    CHECK(!game_upgrade_bench(&g) && game_upgrade_progress(&g) == -1.0f); /* nothing to pay with */
    inventory_add(inv, bench->upgrade_cost[0].id, bench->upgrade_cost[0].count);
    CHECK(game_upgrade_bench(&g) && world_get_at(w, BG, x, s - 1) == TILE_WORKBENCH);
    CHECK(inventory_count(inv, bench->upgrade_cost[0].id) == bench->upgrade_cost[0].count && !game_upgrade_bench(&g));
    run(&g, 1.5f);
    CHECK(game_upgrade_progress(&g) > 0.4f && game_upgrade_progress(&g) < 0.6f && world_get_at(w, BG, x, s - 1) == TILE_WORKBENCH);
    run(&g, 1.6f);
    CHECK(world_get_at(w, BG, x, s - 1) == TILE_WORKBENCH_2 && inventory_count(inv, bench->upgrade_cost[0].id) == 0);
    CHECK(game_upgrade_progress(&g) == -1.0f);
    /* Shut the menu part-way and it comes to nothing, and costs nothing. */
    world_set_at(w, BG, x, s - 1, TILE_WORKBENCH);
    inventory_add(inv, bench->upgrade_cost[0].id, bench->upgrade_cost[0].count);
    CHECK(game_upgrade_bench(&g));
    run(&g, 1.0f);
    game_close_bench(&g);
    run(&g, 5.0f);
    CHECK(world_get_at(w, BG, x, s - 1) == TILE_WORKBENCH && inventory_count(inv, bench->upgrade_cost[0].id) == bench->upgrade_cost[0].count);
    bench->upgrade_time = was;
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

    /* Four kinds of tool, and every block that can be mined belongs to one. */
    int machete = tool_class_id("machete");
    CHECK(shovel >= 0 && pickaxe >= 0 && axe >= 0 && machete >= 0 && content.tool_class_count == 4);
    for (int t = 1; t < content.tile_count; t++)
        CHECK(!content.tiles[t].removable || content.tiles[t].tool_class >= 0);
    CHECK(tile_def(TILE_DIRT)->tool_class == shovel && tile_def(TILE_GRASS)->tool_class == shovel);
    CHECK(tile_def(tile_id("flint"))->tool_class == shovel && tile_def(tile_id("copper_dirt"))->tool_class == shovel);
    CHECK(tile_def(TILE_STONE)->tool_class == pickaxe && tile_def(tile_id("gold_ore"))->tool_class == pickaxe);
    CHECK(tile_def(tile_id("coal_ore"))->tool_class == pickaxe && tile_def(tile_id("furnace"))->tool_class == pickaxe);
    CHECK(tile_def(TILE_OAK_LOG)->tool_class == axe && tile_def(TILE_CHERRY_LEAVES)->tool_class == machete);
    /* Leaves and plants are a machete's work, and a better one makes shorter work of them. */
    CHECK(tile_def(tile_id("flax_0"))->tool_class == machete && tile_def(tile_id("oak_leaves_decayed"))->tool_class == machete);
    CHECK(item_def(item_id("flint_machete"))->tool_class == machete && item_def(item_id("iron_machete"))->tool_class == machete);
    CHECK(item_def(item_id("flint_machete"))->tool_speed > 1.0f);
    CHECK(item_def(item_id("bronze_machete"))->tool_speed > item_def(item_id("flint_machete"))->tool_speed);
    CHECK(item_def(item_id("iron_machete"))->tool_speed > item_def(item_id("bronze_machete"))->tool_speed);
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
    game_close_bench(&g); /* (as a click away from it does) */
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
    game_close_bench(&g); /* (as a click away from it does) */
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
    /* (And a dev portal, in place of the trade portal and the gold coin there used to be.) */
    CHECK(count_item(&g, item_id("dev_portal")) == 1 && count_item(&g, item_id("trade_portal")) == 0 && count_item(&g, item_id("gold_coin")) == 0);

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
    CHECK(content_load("data", err, sizeof err) && content.tile_count > 100 && content.tile_count <= tiles);
    CHECK(content.rules.start_item_count == 2);
}

/* A shipped recipe: at a kind of bench, making so many of a thing from a first input. */
static const Recipe *shipped(const char *bench, const char *output, const char *first_input)
{
    for (int i = 0; i < content.recipe_count; i++) {
        const Recipe *r = &content.recipes[i];
        if (strcmp(r->bench, bench) != 0 || r->output.id != item_id(output)) continue;
        if (first_input && (r->input_count == 0 || r->inputs[0].id != item_id(first_input))) continue;
        return r;
    }
    return NULL;
}

static bool needs(const Recipe *r, const char *item, int count)
{
    for (int i = 0; r && i < r->input_count; i++)
        if (r->inputs[i].id == item_id(item) && r->inputs[i].count == count) return true;
    return false;
}

static int shipped_count(const char *bench)
{
    int n = 0;
    for (int i = 0; i < content.recipe_count; i++) n += strcmp(content.recipes[i].bench, bench) == 0;
    return n;
}

static const ItemSource *source_of(const ItemSource *list, int count, SourceKind kind, const char *label)
{
    for (int i = 0; i < count; i++)
        if (list[i].kind == kind && strcmp(list[i].label, label) == 0) return &list[i];
    return NULL;
}

/* The recipe book: what the content says about where things come from. */
static void test_guide(void)
{
    static ItemSource s[64];
    static int r[MAX_RECIPES];
    int n;

    /* Searching is by label, in any case, anywhere in it. */
    CHECK(guide_matches(item_id("bronze_sword"), "") && guide_matches(item_id("bronze_sword"), "SWO"));
    CHECK(guide_matches(item_id("bronze_sword"), "bronze s") && !guide_matches(item_id("bronze_sword"), "iron"));

    /* What makes a thing, and what it goes into. */
    n = guide_recipes(item_id("bronze_ingot"), r, MAX_RECIPES);
    CHECK(n >= 1 && content.recipes[r[0]].output.id == item_id("bronze_ingot") && strcmp(content.recipes[r[0]].bench, "furnace") == 0);
    n = guide_uses(item_id("bronze_ingot"), r, MAX_RECIPES);
    bool sword = false;
    for (int i = 0; i < n; i++) sword = sword || content.recipes[r[i]].output.id == item_id("bronze_sword");
    CHECK(sword);
    CHECK(guide_recipes(ITEM_NONE, r, MAX_RECIPES) == 0 && guide_uses(ITEM_NONE, r, MAX_RECIPES) == 0);
    /* The portal's recipes are among them, the one with ingredients that differ by world too. */
    CHECK(guide_recipes(item_id("workbench"), r, MAX_RECIPES) == 1 && strcmp(content.recipes[r[0]].bench, "portal") == 0);
    n = guide_uses(item_id("cherry"), r, MAX_RECIPES);
    bool player = false;
    for (int i = 0; i < n; i++) player = player || content.recipes[r[i]].output.id == item_id("new_player");
    CHECK(player);
    /* A bench is known by its block, and that by the item that puts it down. */
    CHECK(guide_bench_tile("furnace", 1) == tile_id("furnace") && guide_bench_tile("furnace", 2) == tile_id("furnace_2"));
    CHECK(guide_placer(tile_id("furnace")) == item_id("furnace") && guide_bench_tile("no_such_bench", 1) == TILE_AIR);

    /* The base of it all: things dug out of the world, with where. */
    char where[200];
    CHECK(guide_tile_natural(tile_id("clay"), where, sizeof where) && strstr(where, "veins") && strstr(where, "Dirt"));
    CHECK(guide_tile_natural(tile_id("oak_leaves"), where, sizeof where) && strstr(where, "Plains"));
    CHECK(guide_tile_natural(tile_id("golden_chest"), where, sizeof where) && strstr(where, "Beach"));
    CHECK(!guide_tile_natural(tile_id("furnace"), NULL, 0) && !guide_tile_natural(TILE_AIR, NULL, 0));
    n = guide_sources(item_id("clay"), s, 64);
    const ItemSource *clay = source_of(s, n, SOURCE_BLOCK, "Clay");
    CHECK(n == 1 && clay && clay->from == tile_id("clay") && clay->tool_class == tool_class_id("shovel") && clay->chance == 1.0f);
    /* Copper: two kinds of block, a creature bred for it, and meditating. */
    n = guide_sources(item_id("copper_ore"), s, 64);
    CHECK(source_of(s, n, SOURCE_BLOCK, "Copper ore") && source_of(s, n, SOURCE_BLOCK, "Copper-bearing dirt"));
    const ItemSource *dodo = source_of(s, n, SOURCE_CREATURE, "Copper dodo"), *calm = source_of(s, n, SOURCE_MEDITATION, "Meditating");
    CHECK(dodo && dodo->chance == 0.5f && strstr(dodo->where, "Stone dodo") && calm && calm->chance > 0.0f && calm->chance < 1.0f);
    /* Fruit, a creature of the sea, a buried chest, and what a character starts with. */
    n = guide_sources(item_id("apple"), s, 64);
    CHECK(source_of(s, n, SOURCE_FRUIT, "Apple tree") && strstr(source_of(s, n, SOURCE_FRUIT, "Apple tree")->where, "Orchard"));
    n = guide_sources(item_id("shark_head"), s, 64);
    CHECK(n == 1 && source_of(s, n, SOURCE_CREATURE, "Shark")); /* (not from putting one down and taking it up) */
    n = guide_sources(item_id("titanium_ingot"), s, 64);
    CHECK(source_of(s, n, SOURCE_CHEST, "Golden chest") && source_of(s, n, SOURCE_CHEST, "Golden chest")->max == 1);
    n = guide_sources(item_id("basket"), s, 64);
    CHECK(source_of(s, n, SOURCE_START, "A new character"));
    /* A plant's stages that give the same are one line; ones that give more are their own. */
    n = guide_sources(item_id("carrot"), s, 64);
    CHECK(n == 3 && s[0].max == 1 && s[2].max == 4 && strstr(s[0].where, "Grows wild"));
    /* A block that is only ever there because it was put there gives nothing new... */
    CHECK(guide_sources(item_id("furnace"), s, 64) == 0 && guide_sources(item_id("bronze_ingot"), s, 64) == 0);
    /* ...but a bench raised a tier is a different thing from the one put down. */
    n = guide_sources(item_id("workbench_2"), s, 64);
    CHECK(n == 1 && s[0].placed_by == item_id("workbench") && strstr(s[0].where, "raised a tier"));
    /* Stone from the scars and from under the sand; what is shorn; the top of a tall plant. */
    n = guide_sources(item_id("marble"), s, 64);
    CHECK(source_of(s, n, SOURCE_BLOCK, "Marble") && strstr(source_of(s, n, SOURCE_BLOCK, "Marble")->where, "scars") &&
          source_of(s, n, SOURCE_CREATURE, "Marble dodo"));
    n = guide_sources(item_id("sandstone"), s, 64);
    CHECK(n == 2 && strstr(s[0].where, "The rock under") && strstr(s[0].where, "Desert") && strstr(s[0].where, "Ocean"));
    n = guide_sources(item_id("black_sand"), s, 64);
    CHECK(n == 1 && source_of(s, n, SOURCE_BLOCK, "Black sand") && strstr(s[0].where, "lowest layer") && strstr(s[0].where, "Sandstone"));
    n = guide_sources(item_id("yak_shavings"), s, 64);
    const ItemSource *shorn = source_of(s, n, SOURCE_SHEAR, "Yak");
    CHECK(n == 1 && shorn && shorn->min == 1 && shorn->max == 3 && shorn->placed_by == item_id("razor"));
    n = guide_sources(item_id("poison"), s, 64);
    CHECK(source_of(s, n, SOURCE_CREATURE, "Scorpion") && strstr(source_of(s, n, SOURCE_CREATURE, "Scorpion")->where, "Desert"));
    n = guide_sources(item_id("wheat"), s, 64);
    CHECK(n == 2 && s[0].max == 1 && s[1].max == 3 && strstr(s[1].where, "Grows wild"));
    n = guide_sources(item_id("tulip_seed"), s, 64);
    CHECK(source_of(s, n, SOURCE_BLOCK, "Tulip") && strstr(source_of(s, n, SOURCE_BLOCK, "Tulip")->where, "Grows wild"));
    /* A drop that only sometimes comes; milk; what hangs in caves. */
    CHECK(item_def(item_id("bucket"))->milks && item_def(item_id("coffee"))->energy_over && !item_def(item_id("coffee_bean"))->energy_over);
    /* What a sluice washes out is one of the ores, by chance: each of them is found under it. */
    n = guide_recipes(item_id("titanium_ore"), r, MAX_RECIPES);
    int sluiced = 0;
    for (int i = 0; i < n; i++) sluiced += strcmp(content.recipes[r[i]].bench, "electric_sluice") == 0 && content.recipes[r[i]].one_of_count == 6;
    CHECK(sluiced == 2);
    n = guide_sources(item_id("worms"), s, 64);
    CHECK(n == 1 && source_of(s, n, SOURCE_BLOCK, "Dirt") && s[0].chance > 0.09f && s[0].chance < 0.11f);
    n = guide_sources(item_id("milk"), s, 64);
    CHECK(n == 1 && source_of(s, n, SOURCE_MILK, "Yak") && s[0].placed_by == item_id("bucket"));
    n = guide_sources(item_id("vine"), s, 64);
    CHECK(n == 1 && strstr(s[0].where, "Hanging from the roofs of caves"));
    n = guide_sources(item_id("leather"), s, 64);
    CHECK(source_of(s, n, SOURCE_CREATURE, "Yak") && source_of(s, n, SOURCE_CREATURE, "Dropbear"));
    n = guide_sources(item_id("mango"), s, 64);
    CHECK(source_of(s, n, SOURCE_FRUIT, "Mango tree") && strstr(s[0].where, "Tropics"));
    CHECK(guide_sources(item_id("marble_column"), s, 64) == 0); /* (only ever there because it was put there) */
    /* Shards grow on the gem trees of the islands in the sky. */
    n = guide_sources(item_id("ruby_shard"), s, 64);
    CHECK(source_of(s, n, SOURCE_FRUIT, "Ruby tree tree") || source_of(s, n, SOURCE_FRUIT, "Ruby tree"));
    for (int i = 0; i < n; i++)
        if (s[i].kind == SOURCE_FRUIT) CHECK(strstr(s[i].where, "islands in the sky"));
    CHECK(guide_sources(ITEM_NONE, s, 64) == 0);

    /* Every recipe is found again under what it makes and under each thing it takes. */
    for (int i = 0; i < content.recipe_count; i++) {
        const Recipe *p = &content.recipes[i];
        bool listed = false;
        n = guide_recipes(p->output.id, r, MAX_RECIPES);
        for (int k = 0; k < n; k++) listed = listed || r[k] == i;
        CHECK(listed);
        for (int k = 0; k < p->input_count; k++) {
            listed = false;
            n = guide_uses(p->inputs[k].id, r, MAX_RECIPES);
            for (int j = 0; j < n; j++) listed = listed || r[j] == i;
            CHECK(listed);
        }
    }
}

/* The recipes the game ships with, against the list they were written from. */
static void test_shipped_recipes(void)
{
    /* How many each bench has. */
    static const struct {
        const char *bench;
        int count;
    } benches[] = { { "workbench", 30 }, { "campfire", 6 }, { "tool_bench", 25 }, { "craft_bench", 18 }, { "woodwork_bench", 12 },
                    { "tailor_bench", 12 }, { "kiln", 8 }, { "furnace", 11 }, { "press", 29 }, { "compost_bin", 24 },
                    { "builder_bench", 55 }, { "armor_bench", 16 }, { "metalwork_bench", 27 }, { "pizza_oven", 2 },
                    { "mixing_bench", 93 }, { "portal", 9 }, { "dye_bench", 0 }, { "easel", 0 }, { "refinery", 5 },
                    /* The electric benches have lists of their own: the kiln's and the metalwork bench's are the
                     * fuelled ones' over again, quicker; an egg extractor has one for every kind of dodo that leaves something. */
                    { "electric_furnace", 19 }, { "electric_stove", 8 }, { "electric_sluice", 2 }, { "egg_extractor", 20 },
                    { "electric_press", 31 }, { "electric_metalwork_bench", 27 }, { "electric_kiln", 8 } };
    int total = 0;
    for (int i = 0; i < (int)(sizeof benches / sizeof benches[0]); i++) {
        CHECK(shipped_count(benches[i].bench) == benches[i].count);
        total += benches[i].count;
    }
    CHECK(content.recipe_count == total);
    /* The egg extractor turns the egg of every kind of dodo into what that dodo is made of: the five kinds of
     * stone that came later among them. And those five are blocks: each is put down as itself. */
    static const char *const stones[5][2] = { { "limestone", "limestone_dodo_egg" }, { "sandstone", "sandstone_dodo_egg" },
                                              { "marble", "marble_dodo_egg" }, { "lapis_lazuli", "lapis_dodo_egg" },
                                              { "red_marble", "red_marble_dodo_egg" } };
    CHECK(item_def(item_id("basalt"))->places == tile_id("basalt") && item_def(item_id("ice"))->places == tile_id("ice"));
    for (int i = 0; i < 5; i++) {
        const Recipe *r = find_recipe(tile_id("egg_extractor"), stones[i][0]);
        CHECK(r && costs(r, stones[i][1], 1, NULL, 0) && r->output.count == 1);
        CHECK(item_def(item_id(stones[i][0]))->places == tile_id(stones[i][0]) && tile_id(stones[i][0]) != TILE_AIR);
    }

    /* A sample of them: what goes in, how many come out, how long it takes, and at what tier. */
    const Recipe *r = shipped("workbench", "campfire", NULL);
    CHECK(r && r->input_count == 1 && needs(r, "stick", 10) && r->time == 20.0f && r->level == 1);
    r = shipped("workbench", "portal", NULL);
    CHECK(r && needs(r, "stone", 1) && r->crystals == 120 && r->time == 80.0f && r->level == 2);
    r = shipped("workbench", "trade_portal", NULL);
    CHECK(r && needs(r, "wood", 1) && needs(r, "gold_coin", 1) && r->time == 120.0f && r->level == 2);
    CHECK(strcmp(item_def(item_id("trade_portal"))->label, "Trade portal") == 0);
    r = shipped("workbench", "refinery", NULL);
    CHECK(r && r->level == 4 && r->input_count == 4 && needs(r, "electric_furnace", 1) && needs(r, "oil", 10) && r->time == 180.0f);
    r = shipped("tool_bench", "copper_bow", NULL);
    CHECK(r && r->level == 2 && needs(r, "stick", 50) && needs(r, "copper_ingot", 5) && needs(r, "feather", 5) && needs(r, "string", 1));
    r = shipped("tool_bench", "titanium_pickaxe", NULL);
    CHECK(r && r->level == 7 && needs(r, "titanium_ingot", 5) && r->time == 180.0f);
    r = shipped("furnace", "bronze_ingot", NULL);
    CHECK(r && r->output.count == 3 && needs(r, "copper_ingot", 2) && needs(r, "tin_ingot", 1) && r->time == 40.0f);
    r = shipped("craft_bench", "basket", NULL);
    CHECK(r && needs(r, "stick", 5) && r->time == 5.0f);
    r = shipped("craft_bench", "jetpack", NULL);
    CHECK(r && r->level == 4 && needs(r, "carbon_fiber_wing", 2) && needs(r, "jet_engine", 2) && needs(r, "fuel", 10) && r->time == 300.0f);
    r = shipped("builder_bench", "platinum_stairs", NULL);
    CHECK(r && r->output.count == 2 && needs(r, "platinum_block", 1));
    r = shipped("builder_bench", "carbon_fiber_column", NULL);
    CHECK(r && r->output.count == 2 && needs(r, "carbon_fiber_sheet", 2));
    r = shipped("armor_bench", "titanium_chestplate", NULL);
    CHECK(r && r->level == 3 && needs(r, "titanium_ingot", 5) && r->time == 240.0f);
    r = shipped("metalwork_bench", "steel_lantern", NULL);
    CHECK(r && r->output.count == 5 && r->time == 10.0f && needs(r, "oil", 1));
    r = shipped("pizza_oven", "pizza", NULL);
    CHECK(r && needs(r, "mozzarella", 1) && needs(r, "tomato", 1) && r->time == 160.0f);

    /* One output, several recipes: each is its own, told apart by what goes in. */
    CHECK(shipped("furnace", "gold_ingot", "gold_ore") && shipped("furnace", "gold_ingot", "gold_coin") &&
          shipped("furnace", "gold_ingot", "golden_chest"));
    CHECK(needs(shipped("furnace", "gold_ingot", "gold_ore"), "gold_ore", 3) && needs(shipped("furnace", "gold_ingot", "gold_coin"), "gold_coin", 10));
    CHECK(shipped("furnace", "copper_ingot", "copper_ore") && shipped("furnace", "copper_ingot", "copper_coin"));
    CHECK(shipped("press", "oil", "flax_seeds") && shipped("press", "oil", "sunflower_seed") && shipped("craft_bench", "yeast", "yeast"));
    CHECK(shipped("compost_bin", "compost", "kelp") && shipped("compost_bin", "compost", "fish") && needs(shipped("compost_bin", "compost", "apple"), "apple", 5));

    /* A bucket of water is a tin bucket that is full; it is asked for full, and handed back empty. */
    r = shipped("campfire", "salt", NULL);
    CHECK(r && r->output.count == 10 && r->input_count == 1 && r->inputs[0].id == item_id("bucket") && r->inputs[0].full);
    r = shipped("workbench", "mixing_bench", NULL);
    CHECK(r && needs(r, "bucket", 1) && !r->inputs[1].full); /* (this one takes the bucket itself) */
    Inventory inv;
    inventory_init(&inv);
    memset(inv.slots, 0, sizeof inv.slots);
    inventory_add(&inv, item_id("bucket"), 2);
    int crystals = 0;
    r = shipped("campfire", "salt", NULL);
    CHECK(recipe_max_crafts(&inv, r, 0) == 0);
    inventory_set_meter(&inv, &inv.slots[0], 5);
    CHECK(recipe_max_crafts(&inv, r, 0) == 0); /* part full is not a bucket of water */
    inventory_set_meter(&inv, &inv.slots[0], 8);
    CHECK(inventory_count_full(&inv, item_id("bucket")) == 1 && recipe_max_crafts(&inv, r, 0) == 1);
    CHECK(recipe_craft(&inv, r, &crystals) == 0);
    CHECK(inventory_count(&inv, item_id("salt")) == 10 && inventory_count(&inv, item_id("bucket")) == 2);
    CHECK(inventory_count_full(&inv, item_id("bucket")) == 0 && recipe_max_crafts(&inv, r, 0) == 0);

    /* Paint: a bucket, 5 oil and one to three pigments - 8 + 28 + 56 mixes, each its own colour. */
    int paints = 0, singles = 0;
    for (int i = 0; i < content.recipe_count; i++) {
        const Recipe *p = &content.recipes[i];
        if (p->output.id != item_id("paint")) continue;
        paints++;
        singles += p->input_count == 3;
        CHECK(p->tint > 0 && p->tint <= content.tint_count && needs(p, "bucket", 1) && needs(p, "oil", 5) && p->input_count >= 3 && p->input_count <= 5);
    }
    CHECK(paints == 92 && singles == 8 && content.tint_count == 92 && item_def(item_id("paint"))->tinted);
    memset(inv.slots, 0, sizeof inv.slots);
    inventory_add(&inv, item_id("bucket"), 1);
    inventory_add(&inv, item_id("oil"), 5);
    inventory_add(&inv, item_id("ultramarine_blue"), 1);
    for (int i = 0; i < content.recipe_count; i++) {
        const Recipe *p = &content.recipes[i];
        if (p->output.id != item_id("paint") || p->input_count != 3 || !needs(p, "ultramarine_blue", 1)) continue;
        CHECK(recipe_craft(&inv, p, &crystals) == 0);
        const ItemStack *made = NULL;
        for (int k = 0; k < INVENTORY_SLOTS; k++)
            if (inv.slots[k].id == item_id("paint")) made = &inv.slots[k];
        CHECK(made && inventory_meter(&inv, made) == p->tint);
        CHECK(content.tints[p->tint - 1][2] > content.tints[p->tint - 1][0]); /* blue */
    }

    /* Tiers, and what each costs to reach. */
    static const struct {
        const char *from, *to, *item;
        int count;
        float time;
    } tiers[] = { { "workbench", "workbench_2", "stone", 5, 0 }, { "workbench_2", "workbench_3", "bronze_ingot", 5, 0 },
                  { "workbench_3", "workbench_4", "steel_ingot", 5, 0 }, { "tool_bench", "tool_bench_2", "stone", 5, 0 },
                  { "tool_bench_4", "tool_bench_5", "steel_ingot", 5, 0 }, { "tool_bench_6", "tool_bench_7", "titanium_ingot", 5, 0 },
                  { "craft_bench", "craft_bench_2", "limestone", 1, 0 }, { "craft_bench_2", "craft_bench_3", "marble", 1, 80 },
                  { "craft_bench_3", "craft_bench_4", "carbon_fiber_sheet", 5, 0 }, { "tailor_bench", "tailor_bench_2", "leather", 1, 0 },
                  { "furnace", "furnace_2", "iron_ore", 3, 0 }, { "furnace_2", "furnace_3", "gold_ore", 3, 80 },
                  { "press", "press_2", "bronze_ingot", 1, 0 }, { "press_2", "press_3", "steel_ingot", 1, 80 },
                  { "armor_bench", "armor_bench_2", "iron_ingot", 5, 0 }, { "armor_bench_2", "armor_bench_3", "titanium_ingot", 5, 0 },
                  { "metalwork_bench", "metalwork_bench_2", "iron_block", 1, 0 } };
    for (int i = 0; i < (int)(sizeof tiers / sizeof tiers[0]); i++) {
        const TileDef *t = tile_def(tile_id(tiers[i].from));
        CHECK(tile_id(tiers[i].from) > 0 && t->upgrade_to == tile_id(tiers[i].to) && t->upgrade_cost[0].id == item_id(tiers[i].item));
        CHECK(t->upgrade_cost[0].count == tiers[i].count && t->upgrade_time == tiers[i].time);
        CHECK(tile_def(t->upgrade_to)->bench_level == t->bench_level + 1 && strcmp(tile_def(t->upgrade_to)->bench_kind, t->bench_kind) == 0);
    }
    CHECK(tile_def(tile_id("press_2"))->upgrade_cost_count == 2 && tile_def(tile_id("press_2"))->upgrade_cost[1].id == item_id("diamond"));
    CHECK(tile_def(tile_id("metalwork_bench_2"))->upgrade_to == TILE_AIR && tile_id("metalwork_bench_3") < 0);
    CHECK(tile_id_or_alias("metalwork_bench_3") == tile_id("metalwork_bench_2"));

    /* Every recipe is made at a bench that exists, at a tier that bench can reach; and
     * everything it names has a picture to show. */
    for (int i = 0; i < content.recipe_count; i++) {
        const Recipe *p = &content.recipes[i];
        int top = 0;
        for (TileType t = 1; t < content.tile_count; t++)
            if (content.tiles[t].bench && strcmp(content.tiles[t].bench_kind, p->bench) == 0 && content.tiles[t].bench_level > top)
                top = content.tiles[t].bench_level;
        /* (What a portal makes, it makes at once.) */
        CHECK(top >= p->level && (p->time > 0.0f || strcmp(p->bench, "portal") == 0) && p->output.count >= 1);
    }
    int missing = 0;
    for (ItemId i = 1; i < content.item_count; i++) {
        char path[160];
        snprintf(path, sizeof path, "data/textures/%s", content.items[i].texture);
        FILE *f = fopen(path, "rb");
        if (f) fclose(f);
        else missing++, printf("no picture for %s\n", content.items[i].name);
    }
    for (TileType t = 1; t < content.tile_count; t++) {
        for (int k = 0; k < content.tiles[t].texture_count; k++) {
            char path[160];
            snprintf(path, sizeof path, "data/textures/%s", content.tiles[t].textures[k]);
            FILE *f = fopen(path, "rb");
            if (f) fclose(f);
            else missing++, printf("no picture for %s\n", content.tiles[t].name);
        }
    }
    CHECK(missing == 0);
    /* The things with "block" in their name are blocks: they can be put down, and dug up again. */
    const char *blocks[] = { "copper_block", "steel_block", "platinum_block", "carbon_fiber_block", "diamond_block" };
    for (int i = 0; i < 5; i++) {
        TileType t = tile_id(blocks[i]);
        CHECK(t > 0 && tile_def(t)->solid && item_def(item_id(blocks[i]))->places == t && tile_def(t)->drop == item_id(blocks[i]));
    }
    /* Columns and stairs go in the background; a mirror, say, is still only a thing to carry. */
    CHECK(item_def(item_id("marble_column"))->places == tile_id("marble_column") && item_def(item_id("mirror"))->places == TILE_AIR);
    CHECK(item_def(item_id("iron_helmet"))->equip == EQUIP_HEAD && item_def(item_id("fur_boots"))->equip == EQUIP_BOOTS);
}

/* The four poles, and what floats over each. */
static void test_poles(void)
{
    const WorldGenDef *gen = &content.worldgen;
    static const struct {
        const char *name, *tile, *gives;
    } want[4] = { { "north", "north_pole", "north_hat" }, { "east", "east_pole", "grass_skirt" }, { "south", "south_pole", "pole_boots" }, { "west", "west_pole", "cornucopia_hat" } };
    CHECK(gen->pole_count == 4);
    int at[4] = { 0 };
    Game g;
    CHECK(game_init(&g, 2048, 401, 9, WORLD_TERRAIN));
    World *w = &g.world;
    g.tick_rate = 0;
    clear_mobs(&g);
    for (int k = 0; k < 4; k++) {
        int pole = -1;
        for (int i = 0; i < gen->pole_count; i++)
            if (strcmp(gen->poles[i].name, want[k].name) == 0) pole = i;
        CHECK(pole >= 0);
        if (pole < 0) continue;
        TileType tile = tile_id(want[k].tile);
        char top_name[64];
        snprintf(top_name, sizeof top_name, "%s_afloat", want[k].gives);
        CHECK(gen->poles[pole].tile == tile && gen->poles[pole].top == tile_id(top_name) && gen->poles[pole].height == 10);
        /* In the background, and not to be taken down; what is on top of it is. */
        CHECK(tile != TILE_AIR && tile_def(tile)->layer == LAYER_BACKGROUND && !tile_def(tile)->solid && !tile_def(tile)->removable);
        CHECK(tile_def(tile_id(top_name))->removable && tile_def(tile_id(top_name))->drop == item_id(want[k].gives));
        int x = at[k] = world_pole_x(w, pole), top = 0, count = 0;
        while (top < w->height && world_get_at(w, BG, x, top) != tile && world_get_at(w, BG, x, top) != tile_id(top_name)) top++;
        CHECK(top < w->height && world_get_at(w, BG, x, top) == tile_id(top_name)); /* the first thing met coming down is what floats on it */
        int y = top + 1;
        for (; y < w->height && world_get_at(w, BG, x, y) == tile; y++) count++;
        /* Ten blocks of it above the ground - or above the sea, with as much again as it takes to reach the bottom. */
        bool wet = world_is_water(w, x, y - 1);
        CHECK(world_is_solid(w, x, y) && (wet ? count > 10 : count == 10));
        for (int k2 = top; k2 < y; k2++) CHECK(world_get(w, x, k2) == TILE_AIR);
    }
    /* A quarter of the way round the world from one to the next: north, east, south, west; and the world begins half way between north and east. */
    int quarter = w->width / 4;
    CHECK(world_dx(w, at[0], at[1]) == quarter && world_dx(w, at[1], at[2]) == quarter && world_dx(w, at[2], at[3]) == quarter);
    CHECK(world_dx(w, at[0], w->spawn_x) == quarter / 2 && world_dx(w, w->spawn_x, at[1]) == quarter / 2);
    /* The same in a world of another size, and none in a world that is only level soil. */
    World big, flat;
    CHECK(world_init(&big, 4096, 401, 9, WORLD_TERRAIN) && world_init(&flat, 256, 96, 9, WORLD_FLAT));
    CHECK(world_dx(&big, world_pole_x(&big, 0), big.spawn_x) == 512 && count_tile(&big, BG, tile_id("north_pole")) >= 10 && count_tile(&big, BG, tile_id("north_hat_afloat")) == 1);
    CHECK(count_tile(&flat, BG, tile_id("north_pole")) == 0);
    world_free(&big);
    world_free(&flat);

    /* What floats over a pole is taken like any block, and is then the thing itself. */
    int nx = at[0], hy = 0;
    while (world_get_at(w, BG, nx, hy) != tile_id("north_hat_afloat")) hy++;
    clear_inventory(&g);
    for (int y = hy + 1; y < hy + 12; y++) world_set(w, nx + 1, y, TILE_STONE); /* (something to stand on, up beside it) */
    g.player.feet = g.player.step_to = (TilePos){ nx + 1, hy };
    CHECK(game_queue_action(&g, ACTION_MINE, nx, hy));
    run(&g, 5.0f);
    CHECK(world_get_at(w, BG, nx, hy) == TILE_AIR && count_item(&g, item_id("north_hat")) + drops_of(&g, item_id("north_hat")) == 1);
    CHECK(!game_queue_action(&g, ACTION_MINE, nx, hy + 1)); /* the pole stays */
    game_free(&g);

    /* What each does, worn. */
    const ItemDef *hat = item_def(item_id("north_hat")), *boots = item_def(item_id("pole_boots")), *skirt = item_def(item_id("grass_skirt")),
                  *horn = item_def(item_id("cornucopia_hat"));
    CHECK(hat->equip == EQUIP_HEAD && boots->equip == EQUIP_BOOTS && skirt->equip == EQUIP_LEGS && horn->equip == EQUIP_HEAD);
    CHECK(hat->walk_speed == 0.0f && !hat->tireless && !hat->sated && hat->armor == 0.0f); /* the north hat is only to be worn */
    CHECK(boots->walk_speed == 2.0f && skirt->tireless && horn->sated);
    game_init(&g, 128, 64, 4, WORLD_FLAT);
    g.tick_rate = 0;
    clear_mobs(&g);
    clear_inventory(&g);
    Inventory *inv = &g.inventory;
    CHECK(game_walk_speed(&g) == 1.0f && !game_tireless(&g) && !game_sated(&g));
    /* Without them: a step takes its time, and hunger and energy run down. */
    game_set_move_input(&g, 1, 0);
    game_update(&g, 0.001f);
    CHECK(g.player.activity == ACTIVITY_STEPPING && g.player.duration == WALK_TIME);
    game_set_move_input(&g, 0, 0);
    run(&g, 1.0f);
    g.vitals.hunger = g.vitals.energy = 0.8f;
    g.vitals.saturation = 0.0f;
    for (int t = 0; t < 200; t++) game_tick(&g);
    CHECK(g.vitals.hunger < 0.8f && g.vitals.energy < 0.8f);
    /* The boots: twice as fast on foot. (Carried and not worn, they do nothing.) */
    inventory_add(inv, item_id("pole_boots"), 1);
    CHECK(game_walk_speed(&g) == 1.0f);
    clear_inventory(&g);
    inv->armor[EQUIP_BOOTS] = (ItemStack){ .id = (uint16_t)item_id("pole_boots"), .count = 1 };
    CHECK(game_walk_speed(&g) == 2.0f);
    game_set_move_input(&g, 1, 0);
    game_update(&g, 0.001f);
    CHECK(g.player.activity == ACTIVITY_STEPPING && g.player.duration == WALK_TIME / 2.0f);
    game_set_move_input(&g, 0, 0);
    int from = g.player.feet.x;
    run(&g, 1.0f);
    CHECK(game_queue_action(&g, ACTION_MOVE, from + 21, g.player.feet.y));
    run(&g, 10.0f * WALK_TIME + 0.3f);
    CHECK(g.player.feet.x >= from + 19); /* twenty blocks in the time ten take */
    run(&g, 5.0f);
    /* The grass skirt: energy stays where it is. The cornucopia hat: hunger does. Neither does the other's work. */
    inv->armor[EQUIP_LEGS] = (ItemStack){ .id = (uint16_t)item_id("grass_skirt"), .count = 1 };
    g.vitals.hunger = g.vitals.energy = 0.8f;
    for (int t = 0; t < 200; t++) game_tick(&g);
    CHECK(game_tireless(&g) && !game_sated(&g) && g.vitals.energy == 0.8f && g.vitals.hunger < 0.8f);
    inv->armor[EQUIP_LEGS] = (ItemStack){ 0 };
    inv->armor[EQUIP_HEAD] = (ItemStack){ .id = (uint16_t)item_id("cornucopia_hat"), .count = 1 };
    g.vitals.hunger = g.vitals.energy = 0.8f;
    for (int t = 0; t < 200; t++) game_tick(&g);
    CHECK(game_sated(&g) && !game_tireless(&g) && g.vitals.hunger == 0.8f && g.vitals.energy < 0.8f);
    inv->armor[EQUIP_HEAD] = (ItemStack){ .id = (uint16_t)item_id("north_hat"), .count = 1 };
    g.vitals.hunger = g.vitals.energy = 0.8f;
    for (int t = 0; t < 200; t++) game_tick(&g);
    CHECK(g.vitals.hunger < 0.8f && g.vitals.energy < 0.8f && game_protection(&g) == 0.0f);
    game_free(&g);
}

/* Mods: which there are, which are on, and the worlds each set of them has to itself. */
static void test_mods(void)
{
    mkdir("build/modtest", 0755);
    mkdir("build/modtest/mods", 0755);
    mkdir("build/modtest/mods/alpha", 0755);
    mkdir("build/modtest/mods/beta", 0755);
    mkdir("build/modtest/mods/not a mod", 0755);
    remove("build/modtest/saves/mods.txt");
    write_file("build/modtest/mods/alpha/mod.json", "{ \"label\": \"Alpha\", \"icon\": \"a.png\" }");
    write_file("build/modtest/mods/beta/mod.json", "{ }");
    write_file("build/modtest/mods/alpha/items.json", "{ \"items\": [ { \"name\": \"alpha_thing\", \"label\": \"Alpha thing\", \"category\": \"materials\" } ] }");
    remove("build/modtest/mods/beta/items.json");
    static ModList mods;
    mods_scan(&mods, "build/modtest/mods", "build/modtest/saves");
    CHECK(mods.count == 2 && strcmp(mods.mods[0].id, "alpha") == 0 && strcmp(mods.mods[1].id, "beta") == 0);
    CHECK(strcmp(mods.mods[0].label, "Alpha") == 0 && strcmp(mods.mods[1].label, "beta") == 0);
    CHECK(strcmp(mods.mods[0].icon, "build/modtest/mods/alpha/a.png") == 0 && !mods.mods[1].icon[0]);
    CHECK(!mods.mods[0].enabled && !mods.mods[1].enabled);
    /* With none on, the worlds are the game's own. */
    char playset[128], saves[256];
    mods_playset(&mods, playset, sizeof playset);
    mods_saves(&mods, "build/modtest/saves", saves, sizeof saves);
    CHECK(playset[0] == '\0' && strcmp(saves, "build/modtest/saves") == 0);
    /* One on: a playset of its own, with its own directory, and that is remembered. */
    mods.mods[1].enabled = true;
    CHECK(mods_remember(&mods, "build/modtest/saves"));
    mods_scan(&mods, "build/modtest/mods", "build/modtest/saves");
    CHECK(!mods.mods[0].enabled && mods.mods[1].enabled);
    mods_saves(&mods, "build/modtest/saves", saves, sizeof saves);
    CHECK(strcmp(saves, "build/modtest/saves/playsets/beta") == 0);
    write_file("build/modtest/saves/playsets/beta/kept.world", "x");
    /* Both on is another again; and back to the one is back to the same place, with what was left there. */
    mods.mods[0].enabled = true;
    mods_playset(&mods, playset, sizeof playset);
    mods_saves(&mods, "build/modtest/saves", saves, sizeof saves);
    CHECK(strcmp(playset, "alpha+beta") == 0 && strcmp(saves, "build/modtest/saves/playsets/alpha+beta") == 0);
    mods.mods[0].enabled = false;
    mods_saves(&mods, "build/modtest/saves", saves, sizeof saves);
    FILE *kept = fopen("build/modtest/saves/playsets/beta/kept.world", "r");
    CHECK(strcmp(saves, "build/modtest/saves/playsets/beta") == 0 && kept != NULL);
    if (kept) fclose(kept);
    /* Loading: the game's data, then each mod that is on. */
    char err[256];
    mods.mods[0].enabled = true;
    CHECK(mods_load_content(&mods, "data", err, sizeof err) && err[0] == '\0' && item_id("alpha_thing") > 0);
    const char *dirs[MAX_MODS + 1];
    CHECK(mods_texture_dirs(&mods, "data", dirs) == 3 && strcmp(dirs[0], "data") == 0 && strcmp(dirs[1], "build/modtest/mods/alpha") == 0);
    /* One that will not load is turned off, with a word of why, and the rest go on without it. */
    write_file("build/modtest/mods/beta/items.json", "{ \"items\": [ { \"name\": \"nowhere\" } ] }");
    CHECK(mods_load_content(&mods, "data", err, sizeof err) && strstr(err, "beta") && !mods.mods[1].enabled && mods.mods[0].enabled);
    CHECK(item_id("alpha_thing") > 0 && item_id("nowhere") <= 0);
    mods.mods[0].enabled = false;
    CHECK(mods_load_content(&mods, "data", err, sizeof err) && item_id("alpha_thing") <= 0);
    /* The mod the game comes with is found where the game looks. */
    mods_scan(&mods, MODS_DIR, "build/modtest/saves");
    bool found = false;
    for (int k = 0; k < mods.count; k++) found = found || (strcmp(mods.mods[k].id, "magic_mayhem") == 0 && strcmp(mods.mods[k].label, "Magic and Mayhem") == 0);
    CHECK(found);
}

static int mob_count(const Game *g, int kind)
{
    int n = 0;
    for (int i = 0; i < MAX_MOBS; i++) n += g->mobs[i].active && g->mobs[i].kind == kind;
    return n;
}

/* Magic and Mayhem, on top of the game. */
static void test_magic_mod(void)
{
    char err[256];
    content_reset();
    CHECK(content_load("data", err, sizeof err));
    int tiles = content.tile_count, items = content.item_count, recipes = content.recipe_count;
    if (!content_load("mods/magic_mayhem", err, sizeof err)) {
        printf("FAIL magic_mayhem does not load: %s\n", err);
        failures++;
        return;
    }
    /* What it adds; and the game's own things are still there. */
    CHECK(content.tile_count == tiles + 18 && content.item_count == items + 40 && content.recipe_count == recipes + 29);
    CHECK(content.rules.max_players == 3 && mob_id("flame_spirit") >= 0 && tree_id("magic_tree") >= 0);
    TileType lectern = tile_id("lectern"), orb = tile_id("magic_orb"), core = tile_id("magic_core");
    CHECK(tile_def(lectern)->bench && tile_def(lectern)->upgrade_to == tile_id("lectern_2") && tile_def(tile_id("lectern_2"))->upgrade_to == tile_id("lectern_3"));
    CHECK(tile_def(lectern)->upgrade_cost[0].id == item_id("elemental_essence") && tile_def(tile_id("lectern_2"))->upgrade_cost[0].id == item_id("sundial_essence"));
    CHECK(tile_def(orb)->upgrade_cost[0].id == item_id("elemental_essence") && tile_def(tile_id("magic_orb_3"))->bench_level == 3);
    CHECK(tile_def(tile_id("workbench_4"))->upgrade_to == tile_id("workbench_5") && tile_def(tile_id("workbench_4"))->upgrade_cost[0].id == item_id("obsidian") &&
          tile_def(tile_id("workbench_4"))->upgrade_cost[0].count == 5 && tile_def(tile_id("workbench_5"))->bench_level == 5);
    /* The recipes, a few of them. */
    const Recipe *r = find_recipe(TILE_WORKBENCH, "lectern");
    CHECK(r && costs(r, "wood", 2, "feather", 1) && r->level == 1 && r->time == 20.0f);
    r = find_recipe(TILE_WORKBENCH, "magic_core");
    CHECK(r && costs(r, "magic_gem", 5, "obsidian", 5) && r->level == 5);
    r = find_recipe_from(lectern, "paper", "sugarcane");
    CHECK(r && costs(r, "sugarcane", 1, NULL, 0) && r->output.count == 3);
    r = find_recipe(lectern, "fireball_scroll");
    CHECK(r && costs(r, "spellbook", 1, "flame_catalyst", 1));
    r = find_recipe(lectern, "life_scroll");
    CHECK(r && costs(r, "grandmasters_spellbook", 1, "life_catalyst", 1) && r->level == 3);
    r = find_recipe(orb, "elemental_essence");
    CHECK(r && r->input_count == 5 && r->level == 1);
    r = find_recipe(orb, "life_catalyst");
    CHECK(r && r->level == 3 && r->input_count == 3);
    r = find_recipe_from(core, "soulstone", "magic_gem");
    CHECK(r && r->output.count == 2 && r->input_count == 3);
    r = find_recipe(core, "magic_array");
    CHECK(r && r->input_count == 3 && r->time == 300.0f);
    /* The spells. */
    const ItemDef *fire = item_def(item_id("fireball_scroll")), *wet = item_def(item_id("water_scroll")), *dig = item_def(item_id("miners_scroll"));
    CHECK(fire->casts && fire->uses == 10 && fire->cast_damage == 10 && fire->cast_splash == 1 && fire->cast_cooks && fire->range == 10);
    CHECK(wet->casts && wet->uses == 50 && wet->cast_water == 8 && wet->cast_damage == 0 && wet->cast_bane == 10);
    CHECK(dig->uses == 1 && dig->cast_dig == 5 && item_def(item_id("shield_scroll"))->cast_shield == 1.0f && item_def(item_id("shield_scroll"))->uses == 10);
    CHECK(item_def(item_id("sunrise_scroll"))->cast_time == 0 && item_def(item_id("moonrise_scroll"))->cast_time == 1);
    CHECK(item_def(item_id("dodo_meat"))->cooks_into == item_id("cooked_dodo_meat"));
    /* The spirits. */
    int flame = mob_id("flame_spirit"), water = mob_id("water_spirit"), air = mob_id("air_spirit"), earth = mob_id("earth_spirit");
    CHECK(content.mobs[flame].flies && content.mobs[flame].immune && content.mobs[flame].spawn_sky && content.mobs[flame].spawn_time == 1);
    CHECK(content.mobs[air].flies && content.mobs[air].spawn_time == 2 && content.mobs[water].swims && content.mobs[water].spawn_time == 2);
    CHECK(!content.mobs[earth].flies && content.mobs[earth].spawn_cave && content.mobs[earth].spawn_time == 1);
    CHECK(content.mobs[flame].weak_to == wet->cast_element && content.mobs[water].weak_to == fire->cast_element && content.mobs[air].weak_to == dig->cast_element);
    CHECK(content.mobs[earth].weak_to == item_def(item_id("shield_scroll"))->cast_element && content.mobs[mob_id("dodo")].weak_to < 0);

    Game g;
    game_init(&g, 160, 96, 21, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    g.tick_rate = 0;
    g.random_events = 0;
    clear_mobs(&g);
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;

    /* A fireball: at a dodo six blocks off, it and the one beside it are killed and their meat is cooked; a use is gone. */
    clear_inventory(&g);
    inventory_add(inv, item_id("fireball_scroll"), 1);
    inventory_select(inv, 0, -1);
    CHECK(inventory_meter(inv, &inv->slots[0]) == 10);
    int dodo = mob_spawn(&g, mob_id("dodo"), px + 6, ground, true, false), beside = mob_spawn(&g, mob_id("dodo"), px + 7, ground, true, false);
    int far = mob_spawn(&g, mob_id("dodo"), px + 10, ground, true, false);
    CHECK(dodo >= 0 && beside >= 0 && far >= 0 && game_click_mob(&g, dodo));
    for (int i = 0; i < 120 && g.mobs[dodo].active; i++) game_update(&g, 1.0f / 60.0f);
    CHECK(!g.mobs[dodo].active && !g.mobs[beside].active && g.mobs[far].active && g.player.feet.x == px);
    CHECK(drops_of(&g, item_id("cooked_dodo_meat")) >= 2 && drops_of(&g, item_id("dodo_meat")) == 0);
    CHECK(inventory_meter(inv, &inv->slots[0]) == 9);
    clear_mobs(&g);
    clear_drops(&g);
    /* A spirit is not hurt by blows, nor by magic it is not weak to; by the magic it is weak to, it is. */
    int ws = mob_spawn(&g, water, px + 4, ground, true, false), fs = mob_spawn(&g, flame, px + 5, ground - 6, true, false);
    CHECK(!mob_hurt(&g, ws, 100) && g.mobs[ws].active && !mob_hurt_by(&g, ws, 100, wet->cast_element, false) && g.mobs[ws].active);
    CHECK(mob_weak_to(&g, ws, fire->cast_element) && !mob_weak_to(&g, fs, fire->cast_element) && !mob_weak_to(&g, ws, -1));
    CHECK(game_click_mob(&g, ws));
    for (int i = 0; i < 120 && g.mobs[ws].active; i++) game_update(&g, 1.0f / 60.0f);
    CHECK(!g.mobs[ws].active && inventory_meter(inv, &inv->slots[0]) == 8);
    CHECK(game_click_mob(&g, fs));
    run(&g, 2.0f);
    CHECK(g.mobs[fs].active && g.mobs[fs].hp > 0); /* fire does nothing to a spirit of fire */
    /* Water, on a square: a bucket's worth of it there; and the end of a flame spirit beside it. */
    clear_inventory(&g);
    inventory_add(inv, item_id("water_scroll"), 1);
    inventory_select(inv, 0, -1);
    g.mobs[fs].x = g.mobs[fs].to_x = px + 4;
    g.mobs[fs].y = g.mobs[fs].to_y = ground - 1;
    g.mobs[fs].step_left = 0;
    long before = water_total(w);
    CHECK(game_click_tile(&g, px + 4, ground) && (g.has_current ? g.current.type : g.queue[0].type) == ACTION_CAST);
    run(&g, 0.2f);
    CHECK(water_total(w) == before + 8 && !g.mobs[fs].active && inventory_meter(inv, &inv->slots[0]) == 49);
    for (int x = px - 20; x <= px + 20; x++) world_set_water(w, x, ground, 0);
    clear_mobs(&g);
    clear_drops(&g);
    /* A shield: a health bar's worth of harm is taken by it, and then it is gone. */
    clear_inventory(&g);
    inventory_add(inv, item_id("shield_scroll"), 1);
    inventory_select(inv, 0, -1);
    CHECK(game_shield(&g) == 0.0f && game_click_tile(&g, px + 30, ground - 20));
    run(&g, 0.2f);
    CHECK(game_shield(&g) == 1.0f && g.player.feet.x == px && inventory_meter(inv, &inv->slots[0]) == 9); /* (cast where they stood) */
    game_hurt(&g, 0.4f);
    CHECK(g.vitals.health == 1.0f && game_shield(&g) > 0.59f && game_shield(&g) < 0.61f);
    CHECK(save_game(&g, "build/test_magic.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_magic.world") && game_shield(&loaded) > 0.59f && game_shield(&loaded) < 0.61f);
    game_free(&loaded);
    game_hurt(&g, 0.8f);
    CHECK(game_shield(&g) == 0.0f && g.vitals.health > 0.79f && g.vitals.health < 0.81f);
    g.vitals.health = 1.0f;
    /* The miner's scroll: every plain block within five of the one it is cast at; one use, and it is spent. A bench is left standing. */
    clear_inventory(&g);
    inventory_add(inv, item_id("miners_scroll"), 1);
    inventory_select(inv, 0, -1);
    world_set_at(w, tile_def(TILE_WORKBENCH)->layer, px + 5, s + 4, TILE_WORKBENCH);
    world_set(w, px + 6, s + 3, tile_id("chest"));
    CHECK(game_click_tile(&g, px + 5, s + 3));
    run(&g, 3.0f);
    int left = 0, outside = 0;
    for (int dy = -5; dy <= 5; dy++)
        for (int dx = -5; dx <= 5; dx++)
            if (dx * dx + dy * dy <= 25 && s + 3 + dy >= s && world_get(w, px + 5 + dx, s + 3 + dy) != TILE_AIR) left++;
    for (int dx = -9; dx <= 9; dx++) outside += world_get(w, px + 5 + dx, s + 9) != TILE_AIR;
    CHECK(left == 1 && world_get(w, px + 6, s + 3) == tile_id("chest") && outside == 19); /* (only the chest) */
    CHECK(world_get_at(w, tile_def(TILE_WORKBENCH)->layer, px + 5, s + 4) == TILE_WORKBENCH && count_item(&g, item_id("miners_scroll")) == 0);
    CHECK(drops_of(&g, ITEM_DIRT) + count_item(&g, ITEM_DIRT) > 20);
    clear_drops(&g);
    /* Sunrise and moonrise: on to the next one. */
    clear_inventory(&g);
    inventory_add(inv, item_id("moonrise_scroll"), 1);
    inventory_add(inv, item_id("sunrise_scroll"), 1);
    g.player.feet = g.player.step_to = (TilePos){ px - 20, ground };
    g.tick = 100;
    inventory_select(inv, 0, -1);
    CHECK(game_click_tile(&g, px - 18, ground - 5));
    run(&g, 0.2f);
    bool day = true;
    game_day_phase(&g, &day);
    CHECK(!day && g.tick >= (uint64_t)DAY_TICKS && g.tick < (uint64_t)DAY_TICKS + 30 && count_item(&g, item_id("moonrise_scroll")) == 0);
    for (int k = 0; k < INVENTORY_SLOTS; k++)
        if (inv->slots[k].count > 0 && inv->slots[k].id == item_id("sunrise_scroll")) inventory_select(inv, k, -1);
    CHECK(game_click_tile(&g, px - 18, ground - 5));
    run(&g, 0.2f);
    game_day_phase(&g, &day);
    CHECK(day && g.tick >= (uint64_t)(DAY_TICKS + NIGHT_TICKS) && g.tick < (uint64_t)(DAY_TICKS + NIGHT_TICKS) + 30);

    /* A magic array: work at a bench within five blocks of one goes twice as fast. */
    TileType array = tile_id("magic_array");
    world_set_at(w, tile_def(lectern)->layer, px - 30, ground, lectern);
    CHECK(tile_def(array)->craft_speed == 2.0f && tile_def(array)->craft_radius == 5 && game_open_bench(&g, px - 30, ground));
    r = find_recipe(lectern, "spellbook");
    CHECK(game_craft_boost(&g, px - 30, ground) == 1.0f && game_craft_seconds(&g, r) == 30.0f);
    world_set_at(w, tile_def(array)->layer, px - 34, ground - 3, array);
    CHECK(game_craft_boost(&g, px - 30, ground) == 2.0f && game_craft_seconds(&g, r) == 15.0f);
    world_set_at(w, tile_def(array)->layer, px - 34, ground - 3, TILE_AIR);
    world_set_at(w, tile_def(array)->layer, px - 36, ground, array);
    CHECK(game_craft_boost(&g, px - 30, ground) == 1.0f); /* six off: too far */
    game_close_bench(&g);

    /* The third character: made with a life scroll and 100 crystals, and only once there are two. */
    const Recipe *second = find_recipe_from(TILE_SPAWN_PORTAL, "new_player", "apple"), *third = find_recipe_from(TILE_SPAWN_PORTAL, "new_player", "life_scroll");
    if (!second) second = find_recipe_from(TILE_SPAWN_PORTAL, "new_player", "orange");
    if (!second) second = find_recipe_from(TILE_SPAWN_PORTAL, "new_player", "cherry");
    CHECK(third && third->characters == 2 && third->crystals == 100 && third->input_count == 1);
    for (int k = 0; k < content.recipe_count && !second; k++)
        if (content.recipes[k].output.id == item_id("new_player") && content.recipes[k].characters == 1) second = &content.recipes[k];
    CHECK(second && second->characters == 1);
    clear_inventory(&g);
    inventory_add(inv, item_id("life_scroll"), 2);
    g.crystals = 1000;
    g.player.feet = g.player.step_to = (TilePos){ w->spawn_x + 1, w->spawn_y };
    CHECK(g.character_count == 1 && game_open_bench(&g, w->spawn_x, w->spawn_y) && !game_craft(&g, third, 1));
    CHECK(game_add_character(&g, (TilePos){ w->spawn_x + 2, w->spawn_y }) == 1 && g.character_count == 2);
    CHECK(second && !game_craft(&g, second, 1)); /* (the way the second is made is no longer open) */
    CHECK(game_craft(&g, third, 1));
    run(&g, 3.0f);
    CHECK(g.character_count == 3 && count_item(&g, item_id("life_scroll")) == 1 && g.crystals == 900);
    /* And three is as many as there can be. */
    game_craft(&g, third, 1);
    run(&g, 3.0f);
    CHECK(g.character_count == 3 && g.crystals == 900);
    game_free(&g);

    /* Where the spirits turn up: by day flame spirits in the sky, and by night air spirits; never the other way about. */
    game_init(&g, 160, 96, 22, WORLD_FLAT);
    w = &g.world;
    g.tick_rate = 0;
    g.random_events = 0;
    clear_mobs(&g);
    s = w->surface_y, ground = s - 1, px = g.player.feet.x;
    float chances[4] = { content.mobs[flame].spawn_chance, content.mobs[air].spawn_chance, content.mobs[earth].spawn_chance, content.mobs[water].spawn_chance };
    ((MobDef *)&content.mobs[flame])->spawn_chance = ((MobDef *)&content.mobs[air])->spawn_chance = 1.0f;
    ((MobDef *)&content.mobs[earth])->spawn_chance = 1.0f;
    g.tick = (uint64_t)DAY_TICKS / 2;
    CHECK(!mobs_try_spawn(&g, px + 5, ground - 2)); /* too near the ground for the sky */
    CHECK(mobs_try_spawn(&g, px + 5, ground - 12) && mob_count(&g, flame) == 1 && mob_count(&g, air) == 0);
    /* (An earth spirit: on stone, under ground, by day.) */
    for (int x = px - 3; x <= px + 3; x++) world_set(w, x, s + 6, TILE_AIR), world_set(w, x, s + 7, TILE_STONE);
    CHECK(world_get_at(w, LAYER_WALL, px, s + 6) != TILE_AIR && mobs_try_spawn(&g, px, s + 6) && mob_count(&g, earth) == 1);
    world_set(w, px + 10, ground + 1, TILE_STONE);
    CHECK(!mobs_try_spawn(&g, px + 10, ground) || mob_count(&g, earth) == 1); /* not on stone in the open */
    g.tick = (uint64_t)DAY_TICKS + (uint64_t)NIGHT_TICKS / 2;
    clear_mobs(&g);
    CHECK(mobs_try_spawn(&g, px - 5, ground - 12) && mob_count(&g, air) == 1 && mob_count(&g, flame) == 0);
    CHECK(!mobs_try_spawn(&g, px, s + 6) && mob_count(&g, earth) == 0);
    /* It stays up there: after a good while it has moved about, and never come down to the ground or into a block. */
    int spirit = -1;
    for (int k = 0; k < MAX_MOBS; k++)
        if (g.mobs[k].active) spirit = k;
    int lowest = 0, moved = 0, sx = g.mobs[spirit].x;
    for (int t = 0; t < 600; t++) {
        game_tick(&g);
        if (!g.mobs[spirit].active) break;
        if (g.mobs[spirit].y > lowest) lowest = g.mobs[spirit].y;
        if (g.mobs[spirit].x != sx) moved++;
        if (world_is_solid(w, g.mobs[spirit].x, g.mobs[spirit].y)) lowest = 10000;
    }
    CHECK(g.mobs[spirit].active && moved > 0 && lowest < ground);
    ((MobDef *)&content.mobs[flame])->spawn_chance = chances[0];
    ((MobDef *)&content.mobs[air])->spawn_chance = chances[1];
    ((MobDef *)&content.mobs[earth])->spawn_chance = chances[2];
    (void)chances[3];
    game_free(&g);

    /* The world: obsidian near the bottom, magic gems in the caves, sugarcane on the beaches, and a magic tree on an island here and there. */
    int obsidian = 0, shallow = 0, gems = 0, cane = 0, off_beach = 0, magic_logs = 0;
    TileType ob = tile_id("obsidian"), gem = tile_id("magic_gem"), log = tile_id("magic_tree_log");
    for (uint32_t seed = 1; seed <= 3; seed++) {
        World world;
        CHECK(world_init(&world, 2048, 401, seed, WORLD_TERRAIN));
        for (int y = 0; y < world.height; y++) {
            for (int x = 0; x < world.width; x++) {
                if (world_get(&world, x, y) == ob) {
                    obsidian++;
                    if (world_row_to_y(&world, y) > -120) shallow++;
                }
                TileType back = world_get_at(&world, BG, x, y);
                if (back == gem) gems++;
                if (back == log) magic_logs++;
                if (strncmp(tile_def(back)->name, "sugarcane", 9) == 0) {
                    cane++;
                    if (strcmp(content.biomes[world_biome(&world, x)].name, "beach") != 0) off_beach++;
                }
            }
        }
        world_free(&world);
    }
    CHECK(obsidian > 100 && shallow == 0 && gems > 20 && cane > 10 && off_beach == 0 && magic_logs > 0);

    /* And the game by itself again. */
    content_reset();
    CHECK(content_load("data", err, sizeof err) && content.tile_count == tiles && item_id("fireball_scroll") <= 0 && content.rules.max_players == 2);
}

/* A machine set down, with the slots it has. */
static Storage *put_machine(Game *g, const char *tile, int x, int y)
{
    Storage *st = put_device(g, tile, x, y);
    if (st && tile_def(tile_id(tile))->storage_columns > 0) st->container = container_alloc(&g->inventory);
    return st;
}

static int held_in(const Game *g, const Storage *st, ItemId item)
{
    int n = 0;
    for (int i = 0; st && st->container && i < CONTAINER_MAX_SLOTS; i++) {
        const ItemStack *s = &g->inventory.containers[st->container].slots[i];
        if (s->count > 0 && s->id == item) n += s->count;
    }
    return n;
}

static void fill_slot(Game *g, Storage *st, int slot, ItemId item, int count)
{
    g->inventory.containers[st->container].slots[slot] = (ItemStack){ .id = (uint16_t)item, .count = (uint16_t)count };
}

/* Industry, on top of the game. */
static void test_industry_mod(void)
{
    char err[256];
    content_reset();
    CHECK(content_load("data", err, sizeof err));
    int tiles = content.tile_count, items = content.item_count, recipes = content.recipe_count;
    if (!content_load("mods/industry", err, sizeof err)) {
        printf("FAIL industry does not load: %s\n", err);
        failures++;
        return;
    }
    CHECK(content.tile_count == tiles + 44 && content.item_count == items + 58 && content.recipe_count == recipes + 59);
    /* A world saved at version 28, when there was room for a quarter as many containers, drops, storage records and
     * creatures and they were written out whole: it loads as it was, a machine part way through its work and all. */
    {
        static Game old;
        CHECK(load_game(&old, "tests/fixtures/v28_industry.world"));
        int kept = 0, boxes = 0, held = 0, working = 0;
        for (int k = 0; k < MAX_STORAGES; k++) {
            kept += old.storages[k].used;
            working += old.storages[k].used && old.storages[k].recipe > 0 && old.storages[k].work > 0;
        }
        for (int k = 1; k < MAX_CONTAINERS; k++) {
            if (!old.inventory.containers[k].used) continue;
            boxes++;
            for (int j = 0; j < CONTAINER_MAX_SLOTS; j++) held += old.inventory.containers[k].slots[j].count;
        }
        CHECK(kept == 6 && boxes == 6 && held == 108 && working == 1 && old.tick == 709 && old.character_count == 1);
        /* Saved again it is of today's version, and comes back the same. */
        CHECK(save_game(&old, "build/test_v28_again.world"));
        static Game again;
        CHECK(load_game(&again, "build/test_v28_again.world"));
        CHECK(memcmp(again.storages, old.storages, sizeof old.storages) == 0 && memcmp(&again.inventory, &old.inventory, sizeof old.inventory) == 0);
        CHECK(memcmp(again.world.layers[0], old.world.layers[0], sizeof(uint16_t) * (size_t)old.world.width * old.world.height) == 0);
        for (int t = 0; t < 20; t++) game_tick(&again);
        game_free(&again);
        game_free(&old);
    }
    CHECK(MAX_TILES == 4096 && MAX_ITEMS == 4096 && MAX_RECIPES == 4096 && MAX_STORAGES == 1024 && MAX_CONTAINERS == 1024 && MAX_DROPS == 8192 && MAX_MOBS == 256);
    /* The workbench goes on to a fifth tier and a sixth; the alloy furnace and the industrial bench have tiers of their own. */
    CHECK(tile_def(tile_id("workbench_4"))->upgrade_to == tile_id("workbench_5") && tile_def(tile_id("workbench_4"))->upgrade_cost[0].id == item_id("heavy_bronze_ingot"));
    CHECK(tile_def(tile_id("workbench_5"))->upgrade_to == tile_id("workbench_6") && tile_def(tile_id("workbench_5"))->upgrade_cost[0].id == item_id("dreadsteel_ingot") &&
          tile_def(tile_id("workbench_5"))->upgrade_cost[0].count == 5 && tile_def(tile_id("workbench_6"))->bench_level == 6);
    TileType alloy = tile_id("alloy_furnace");
    CHECK(tile_def(alloy)->electric == 100 && tile_def(alloy)->upgrade_cost[0].id == item_id("lead_block") &&
          tile_def(tile_id("alloy_furnace_2"))->upgrade_cost[0].id == item_id("royal_bronze_block"));
    CHECK(tile_def(tile_id("industrial_bench"))->upgrade_cost[0].id == item_id("imperial_ingot") && tile_def(tile_id("industrial_bench"))->upgrade_cost[0].count == 2);
    CHECK(tile_def(tile_id("industrial_armory"))->electric == 500 && tile_def(tile_id("industrial_processor"))->electric == 1000);
    const Recipe *r = find_recipe_from(alloy, "bronze_ingot", "copper_ingot");
    CHECK(r && costs(r, "copper_ingot", 1, "tin_ingot", 1) && r->output.count == 3 && r->time == 10.0f);
    r = find_recipe_from(tile_id("alloy_furnace_3"), "imperial_ingot", "platinum_ingot");
    CHECK(r && r->input_count == 3 && r->level == 3);
    r = find_recipe_from(tile_id("alloy_furnace_2"), "heavy_bronze_ingot", "lead_ingot");
    CHECK(r && r->output.count == 2 && r->level == 2 && r->time == 30.0f);
    r = find_recipe_from(TILE_WORKBENCH, "factory", "heavy_bronze_block");
    CHECK(r && r->level == 5 && r->input_count == 4);
    r = find_recipe_from(tile_id("industrial_bench"), "conveyor", "heavy_bronze_ingot");
    CHECK(r && r->output.count == 5 && costs(r, "heavy_bronze_ingot", 3, NULL, 0));
    r = find_recipe_from(tile_id("industrial_bench_2"), "imperial_conveyor", "gilded_conveyor");
    CHECK(r && r->output.count == 3 && r->level == 2);
    CHECK(find_recipe(tile_id("industrial_smithery"), "dreadsteel_sword") && find_recipe(tile_id("industrial_armory"), "imperial_chestplate"));
    CHECK(find_recipe(tile_id("furnace_2"), "lead_ingot") && find_recipe(tile_id("metalwork_bench_2"), "dreadsteel_block") && find_recipe_from(tile_id("builder_bench"), "imperial_stairs", "imperial_block"));
    CHECK(item_def(item_id("dreadsteel_sword"))->damage == 50 && item_def(item_id("imperial_longbow"))->damage == 20 && item_def(item_id("imperial_longbow"))->range > 0);
    CHECK(item_def(item_id("imperial_helmet"))->armor > item_def(item_id("dreadsteel_helmet"))->armor && item_def(item_id("dreadsteel_helmet"))->armor > item_def(item_id("titanium_helmet"))->armor);
    CHECK(tile_def(tile_id("conveyor"))->conveyor == 1 && tile_def(tile_id("gilded_conveyor"))->conveyor == 2 && tile_def(tile_id("imperial_conveyor"))->conveyor == 5);

    Game g;
    game_init(&g, 160, 96, 31, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    g.tick_rate = 0;
    g.random_events = 0;
    clear_mobs(&g);
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;
    ItemId copper_ore = item_id("copper_ore"), iron_ore = item_id("iron_ore"), iron = item_id("iron_ingot");

    /* A source ore is mined and is still there: again and again, an ore each time. */
    TileType source = tile_id("copper_source_ore");
    CHECK(tile_def(source)->endless && tile_def(source)->drop == copper_ore && !tile_def(tile_id("copper_ore"))->endless);
    world_set(w, px + 2, ground, source);
    clear_inventory(&g);
    for (int k = 0; k < 3; k++) {
        CHECK(game_queue_action(&g, ACTION_MINE, px + 2, ground));
        run(&g, 6.0f);
        CHECK(world_get(w, px + 2, ground) == source);
    }
    CHECK(count_item(&g, copper_ore) + drops_of(&g, copper_ore) >= 3);
    clear_drops(&g);
    clear_inventory(&g);

    /* An electric miner beside one: with electricity, an ore every twenty ticks, ten units a tick; with none, nothing. */
    int row = ground - 12, mx = px + 20;
    world_set(w, mx, row, source);
    Storage *miner = put_machine(&g, "electric_miner", mx + 1, row);
    CHECK(tile_def(tile_id("electric_miner"))->draw == 10 && tile_def(tile_id("electric_miner"))->miner_ticks == 20 && miner && miner->container != 0);
    for (int t = 0; t < 100; t++) game_tick(&g);
    CHECK(held_in(&g, miner, copper_ore) == 0);
    Storage *wheel = put_device(&g, "flywheel", mx + 2, row);
    wheel->power = 5000;
    for (int t = 0; t < 205; t++) game_tick(&g);
    int mined = held_in(&g, miner, copper_ore);
    CHECK(mined >= 9 && mined <= 10 && wheel->power <= 5000 - mined * 200 && wheel->power >= 5000 - (mined + 1) * 200 - 80);
    /* Away from the ore it does nothing, and takes nothing. */
    world_set(w, mx, row, TILE_AIR);
    for (int t = 0; t < 20; t++) game_tick(&g);
    int left = wheel->power, had = held_in(&g, miner, copper_ore);
    for (int t = 0; t < 100; t++) game_tick(&g);
    CHECK(wheel->power == left && held_in(&g, miner, copper_ore) == had);

    /* An extractor beside the miner, a run of conveyor, and a chest at the end of it: an item a tick. */
    int cy = row;
    world_set_at(w, tile_def(tile_id("electric_miner"))->layer, mx + 1, row, TILE_AIR);
    miner->used = false;
    wheel->used = false;
    world_set_at(w, tile_def(tile_id("flywheel"))->layer, mx + 2, row, TILE_AIR);
    int ex = px + 40;
    Storage *from = put_machine(&g, "chest", ex, cy), *pull = put_machine(&g, "conveyor_extractor", ex + 1, cy);
    for (int k = 2; k <= 6; k++) world_set_at(w, tile_def(tile_id("conveyor"))->layer, ex + k, cy, tile_id("conveyor"));
    Storage *to = put_machine(&g, "chest", ex + 7, cy);
    fill_slot(&g, from, 0, copper_ore, 50);
    fill_slot(&g, from, 1, iron_ore, 30);
    for (int t = 0; t < 10; t++) game_tick(&g);
    CHECK(held_in(&g, to, copper_ore) == 10 && held_in(&g, from, copper_ore) == 40 && held_in(&g, to, iron_ore) == 0);
    /* Told one kind of thing - by one of it in its slot - it takes only that. */
    fill_slot(&g, pull, 0, iron_ore, 1);
    for (int t = 0; t < 10; t++) game_tick(&g);
    CHECK(held_in(&g, to, copper_ore) == 10 && held_in(&g, to, iron_ore) == 10 && held_in(&g, pull, iron_ore) == 1);
    fill_slot(&g, pull, 0, ITEM_NONE, 0);
    /* A quicker conveyor all the way carries more; one slow stretch in it holds the whole run to its pace. */
    for (int k = 2; k <= 6; k++) world_set_at(w, tile_def(tile_id("conveyor"))->layer, ex + k, cy, tile_id("imperial_conveyor"));
    for (int t = 0; t < 4; t++) game_tick(&g);
    CHECK(held_in(&g, to, copper_ore) == 30);
    world_set_at(w, tile_def(tile_id("conveyor"))->layer, ex + 4, cy, tile_id("gilded_conveyor"));
    for (int t = 0; t < 5; t++) game_tick(&g);
    CHECK(held_in(&g, to, copper_ore) == 40);
    /* The nearest thing that will have it gets it: a crate half way along, before the chest at the end. A crate that is
     * sorting takes only the kinds of thing already in its first four slots. */
    Storage *crate = put_machine(&g, "industrial_crate", ex + 4, cy - 1);
    CHECK(tile_def(tile_id("industrial_crate"))->sorts == 4);
    for (int t = 0; t < 3; t++) game_tick(&g);
    CHECK(held_in(&g, crate, copper_ore) == 6 && held_in(&g, to, copper_ore) == 40);
    for (int k = 0; k < CONTAINER_MAX_SLOTS; k++) fill_slot(&g, crate, k, ITEM_NONE, 0);
    fill_slot(&g, crate, 0, iron_ore, 1);
    for (int t = 0; t < 2; t++) game_tick(&g);
    CHECK(held_in(&g, crate, copper_ore) == 0 && held_in(&g, to, copper_ore) == 44 && held_in(&g, from, copper_ore) == 0);
    for (int t = 0; t < 30; t++) game_tick(&g);
    CHECK(held_in(&g, crate, iron_ore) == 21 && held_in(&g, from, iron_ore) == 0); /* (what was left of the iron, to the crate that sorts for it) */
    /* With the conveyor broken, nothing has anywhere to go, and nothing moves. */
    fill_slot(&g, from, 0, copper_ore, 20);
    world_set_at(w, tile_def(tile_id("conveyor"))->layer, ex + 2, cy, TILE_AIR);
    for (int t = 0; t < 10; t++) game_tick(&g);
    CHECK(held_in(&g, from, copper_ore) == 20);

    /* The industrial furnace: what an electric furnace makes, in half the time, by itself, for 50 units a tick. */
    int fx = px + 60;
    Storage *furnace = put_machine(&g, "industrial_furnace", fx, cy), *battery = put_device(&g, "flywheel", fx + 1, cy);
    const TileDef *fdef = tile_def(tile_id("industrial_furnace"));
    CHECK(fdef->automat && fdef->automat_inputs == 1 && fdef->automat_speed == 2.0f && fdef->draw == 50 && strcmp(fdef->automat_kind, "electric_furnace") == 0);
    CHECK(game_machine_inputs(&g, fx, cy) == 1 && !game_machine_chooses(&g, fx, cy));
    const Recipe *smelt = find_recipe_from(tile_id("electric_furnace"), "iron_ingot", "iron_ore");
    CHECK(smelt && smelt->inputs[0].count == 2 && smelt->time == 40.0f);
    fill_slot(&g, furnace, 0, iron_ore, 5);
    for (int t = 0; t < 100; t++) game_tick(&g);
    CHECK(held_in(&g, furnace, iron) == 0 && game_machine_progress(&g, fx, cy) < 0.0f); /* no electricity: not begun */
    battery->power = 10000;
    for (int t = 0; t < 110; t++) game_tick(&g);
    CHECK(held_in(&g, furnace, iron) == 0 && held_in(&g, furnace, iron_ore) == 3 && game_machine_progress(&g, fx, cy) > 0.3f);
    CHECK(game_machine_recipe(&g, fx, cy) == smelt);
    /* (Kept with the world, part way through.) */
    CHECK(save_game(&g, "build/test_industry.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_industry.world"));
    CHECK(game_machine_recipe(&loaded, fx, cy) == smelt && game_machine_progress(&loaded, fx, cy) > 0.3f);
    game_free(&loaded);
    for (int t = 0; t < 110; t++) game_tick(&g);
    CHECK(held_in(&g, furnace, iron) == 1); /* 20 seconds: 200 ticks */
    CHECK(battery->power <= 600); /* at 50 a tick: all but the last of what the flywheel had */
    battery->power = 10000;
    for (int t = 0; t < 220; t++) game_tick(&g);
    CHECK(held_in(&g, furnace, iron) == 2 && held_in(&g, furnace, iron_ore) == 1 && battery->power <= 600);
    battery->power = 10000;
    /* With one ore left it has nothing it can make; and with something else in the way of what it makes, it does not start. */
    for (int t = 0; t < 50; t++) game_tick(&g);
    CHECK(game_machine_progress(&g, fx, cy) < 0.0f && held_in(&g, furnace, iron_ore) == 1);
    fill_slot(&g, furnace, 0, iron_ore, 2);
    fill_slot(&g, furnace, 1, copper_ore, 1);
    battery->power = 10000;
    for (int t = 0; t < 250; t++) game_tick(&g);
    CHECK(held_in(&g, furnace, iron_ore) == 2 && held_in(&g, furnace, iron) == 0);
    /* A conveyor puts into it only what it can make something of, and only where things go in; an extractor takes out only what it has made. */
    fill_slot(&g, furnace, 0, ITEM_NONE, 0);
    fill_slot(&g, furnace, 1, iron, 3);
    Storage *feed = put_machine(&g, "chest", fx - 4, cy), *push = put_machine(&g, "conveyor_extractor", fx - 3, cy);
    world_set_at(w, tile_def(tile_id("conveyor"))->layer, fx - 2, cy, tile_id("conveyor"));
    world_set_at(w, tile_def(tile_id("conveyor"))->layer, fx - 1, cy, tile_id("conveyor"));
    fill_slot(&g, feed, 0, ITEM_DIRT, 9);
    fill_slot(&g, feed, 1, iron_ore, 4);
    for (int t = 0; t < 6; t++) game_tick(&g);
    CHECK(held_in(&g, feed, ITEM_DIRT) == 9 && held_in(&g, feed, iron_ore) == 0 && held_in(&g, furnace, ITEM_DIRT) == 0);
    Storage *out = put_machine(&g, "conveyor_extractor", fx, cy - 1), *bin = put_machine(&g, "chest", fx, cy - 3);
    world_set_at(w, tile_def(tile_id("conveyor"))->layer, fx, cy - 2, tile_id("conveyor"));
    for (int t = 0; t < 5; t++) game_tick(&g);
    CHECK(held_in(&g, bin, iron) >= 3 && held_in(&g, bin, iron_ore) == 0);
    (void)push;
    (void)out;

    /* A factory: beside a bench, told one of that bench's recipes, it makes it from what is put in it. */
    int qx = px - 30;
    world_set_at(w, tile_def(alloy)->layer, qx, cy, alloy);
    Storage *factory = put_machine(&g, "factory", qx + 1, cy), *cell = put_device(&g, "flywheel", qx + 2, cy);
    cell->power = 10000;
    const TileDef *qdef = tile_def(tile_id("factory"));
    CHECK(qdef->automat && !qdef->automat_kind[0] && qdef->automat_inputs == 4 && qdef->draw == 50 && qdef->upgrade_to == tile_id("factory_2"));
    CHECK(tile_def(tile_id("factory_2"))->automat_inputs == 6 && tile_def(tile_id("factory_2"))->automat_speed == 2.0f && qdef->upgrade_cost[0].id == item_id("processing_chip"));
    const Recipe *could[64];
    int n = game_machine_recipes(&g, qx + 1, cy, could, 64);
    const Recipe *bronze = find_recipe_from(alloy, "bronze_ingot", "copper_ingot");
    CHECK(game_machine_chooses(&g, qx + 1, cy) && n == 2); /* (a first-tier alloy furnace: bronze and steel) */
    CHECK(game_machine_recipe(&g, qx + 1, cy) == NULL && !game_machine_set_recipe(&g, qx + 1, cy, smelt)); /* not a recipe of a bench beside it */
    ItemId cu = item_id("copper_ingot"), tin = item_id("tin_ingot"), bz = item_id("bronze_ingot");
    /* Until it is told, a conveyor puts nothing in it. */
    Storage *stock = put_machine(&g, "chest", qx + 1, cy - 3), *tap = put_machine(&g, "conveyor_extractor", qx + 1, cy - 2);
    world_set_at(w, tile_def(tile_id("conveyor"))->layer, qx + 1, cy - 1, tile_id("conveyor"));
    fill_slot(&g, stock, 0, cu, 4);
    fill_slot(&g, stock, 1, ITEM_DIRT, 4);
    fill_slot(&g, stock, 2, tin, 4);
    for (int t = 0; t < 10; t++) game_tick(&g);
    CHECK(held_in(&g, stock, cu) == 4 && held_in(&g, factory, cu) == 0);
    CHECK(game_machine_set_recipe(&g, qx + 1, cy, bronze) && game_machine_recipe(&g, qx + 1, cy) == bronze);
    for (int t = 0; t < 12; t++) game_tick(&g);
    /* Told: only what the recipe takes, each in its own slot. */
    CHECK(held_in(&g, stock, ITEM_DIRT) == 4 && held_in(&g, stock, cu) == 0 && held_in(&g, stock, tin) == 0);
    const Container *in = &inv->containers[factory->container];
    CHECK(in->slots[0].id == cu && in->slots[1].id == tin && in->slots[2].count == 0);
    /* Ten seconds apiece at the bench's own pace: 100 ticks. */
    for (int t = 0; t < 95; t++) game_tick(&g);
    CHECK(held_in(&g, factory, bz) == 3 && in->slots[4].id == bz);
    for (int k = 0; k < 3; k++) {
        cell->power = 10000;
        for (int t = 0; t < 110; t++) game_tick(&g);
    }
    CHECK(held_in(&g, factory, bz) == 12 && held_in(&g, factory, cu) == 0 && held_in(&g, factory, tin) == 0);
    /* The bench gone, it can make nothing: what it was told is forgotten. */
    world_set_at(w, tile_def(alloy)->layer, qx, cy, TILE_AIR);
    for (int t = 0; t < 3; t++) game_tick(&g);
    CHECK(game_machine_recipe(&g, qx + 1, cy) == NULL && game_machine_recipes(&g, qx + 1, cy, could, 64) == 0);
    (void)tap;

    /* The power plant: a hundred units a tick, burning dreadsteel and imperial ingots and nothing else. */
    const TileDef *plant = tile_def(tile_id("power_plant"));
    CHECK(plant->gen_power == 100 && plant->fuel_count == 2 && plant->fuel_capacity >= 1800);
    Storage *station = put_device(&g, "power_plant", px - 50, ground);
    g.player.feet = g.player.step_to = (TilePos){ px - 49, ground };
    clear_inventory(&g);
    inventory_add(inv, item_id("coal"), 2);
    inventory_add(inv, item_id("dreadsteel_ingot"), 1);
    inventory_add(inv, item_id("imperial_ingot"), 1);
    CHECK(game_open_bench(&g, px - 50, ground));
    CHECK(game_fuel_value(&g, item_id("coal")) == 0 && game_fuel_value(&g, item_id("dreadsteel_ingot")) == 1000 && game_fuel_value(&g, item_id("imperial_ingot")) == 1800);
    CHECK(!game_add_fuel(&g, item_id("coal")) || station->fuel == 0);
    run(&g, 0.2f);
    CHECK(station->fuel == 0 && count_item(&g, item_id("coal")) == 2);
    CHECK(game_add_fuel(&g, item_id("dreadsteel_ingot")));
    run(&g, 0.2f);
    CHECK(station->fuel > 900 && station->fuel <= 1000 && count_item(&g, item_id("dreadsteel_ingot")) == 0);
    game_tick(&g);
    CHECK(game_power_made(&g, px - 50, ground) == 100);
    game_close_bench(&g);
    game_free(&g);

    /* The world: lead in veins like iron's; and, rarely, an ore that is a source. */
    int lead = 0, iron_blocks = 0, sources = 0, ores = 0;
    for (uint32_t seed = 1; seed <= 3; seed++) {
        World world;
        CHECK(world_init(&world, 2048, 401, seed, WORLD_TERRAIN));
        for (int y = 0; y < world.height; y++) {
            for (int x = 0; x < world.width; x++) {
                const TileDef *def = tile_def(world_get(&world, x, y));
                if (!def->ore) continue;
                ores++;
                if (def->endless) sources++;
                if (world_get(&world, x, y) == tile_id("lead_ore")) lead++;
                if (world_get(&world, x, y) == tile_id("iron_ore")) iron_blocks++;
            }
        }
        world_free(&world);
    }
    CHECK(lead > iron_blocks / 2 && lead < iron_blocks * 2 && sources > 10 && sources * 60 < ores);

    /* It and Magic and Mayhem both remake the workbench: they cannot be on together, and asked for together no mod is on. */
    static ModList mods;
    mods_scan(&mods, MODS_DIR, "build/modtest/none");
    int magic = -1, industry = -1;
    for (int k = 0; k < mods.count; k++) {
        if (strcmp(mods.mods[k].id, "magic_mayhem") == 0) magic = k;
        if (strcmp(mods.mods[k].id, "industry") == 0) industry = k;
    }
    CHECK(magic >= 0 && industry >= 0);
    char what[64];
    CHECK(magic >= 0 && industry >= 0 && mods_conflict(&mods.mods[magic], &mods.mods[industry], what, sizeof what) && strncmp(what, "workbench", 9) == 0);
    mods.mods[magic].enabled = mods.mods[industry].enabled = true;
    CHECK(mods_load_content(&mods, "data", err, sizeof err) && strstr(err, "Industry") && strstr(err, "Magic and Mayhem"));
    CHECK(!mods.mods[magic].enabled && !mods.mods[industry].enabled && content.tile_count == tiles && item_id("conveyor") <= 0 && item_id("fireball_scroll") <= 0);
    char playset[128];
    mods_playset(&mods, playset, sizeof playset);
    CHECK(playset[0] == '\0');
    /* Either by itself is as it should be. */
    mods.mods[industry].enabled = true;
    CHECK(mods_load_content(&mods, "data", err, sizeof err) && err[0] == '\0' && mods.mods[industry].enabled && item_id("conveyor") > 0);
    content_reset();
    CHECK(content_load("data", err, sizeof err) && content.tile_count == tiles);
}

/* Digital Storage, on top of the game. */
static void test_digital_mod(void)
{
    char err[256];
    content_reset();
    CHECK(content_load("data", err, sizeof err));
    int tiles = content.tile_count, items = content.item_count, recipes = content.recipe_count;
    if (!content_load("mods/digital_storage", err, sizeof err)) {
        printf("FAIL digital_storage does not load: %s\n", err);
        failures++;
        return;
    }
    CHECK(content.tile_count == tiles + 16 && content.item_count == items + 29 && content.recipe_count == recipes + 28);
    CHECK(tile_def(tile_id("network_cable"))->net_role == NET_CABLE && tile_def(tile_id("network_controller"))->net_role == NET_CONTROLLER &&
          tile_def(tile_id("network_controller"))->draw == 10 && tile_def(tile_id("storage_drive"))->net_role == NET_DRIVE &&
          tile_def(tile_id("wireless_access_point"))->net_range == 32);
    ItemId cell1 = item_id("storage_cell_1k"), cell64 = item_id("storage_cell_64k");
    CHECK(item_def(cell1)->cell_bytes == 1024 && item_def(cell64)->cell_bytes == 65536 && item_def(cell1)->max_stack == 1 && item_def(item_id("wireless_terminal"))->remote);
    CHECK(find_recipe(tile_id("inscriber"), "storage_cell_1k") && find_recipe_from(TILE_WORKBENCH, "inscriber", "steel_block"));

    Game g;
    game_init(&g, 200, 96, 41, WORLD_FLAT);
    World *w = &g.world;
    Inventory *inv = &g.inventory;
    g.tick_rate = 0;
    g.random_events = 0;
    clear_mobs(&g);
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x, row = ground - 10;
    TileType cable = tile_id("network_cable");
#define CABLE(x, y) world_set_at(w, tile_def(cable)->layer, (x), (y), cable)
    /* A network: a controller with a flywheel beside it, cable, a drive with one 1k cell in it, and a terminal. */
    int nx = px + 10;
    Storage *power = put_device(&g, "flywheel", nx - 1, row), *heart = put_machine(&g, "network_controller", nx, row);
    CABLE(nx + 1, row);
    Storage *drive = put_machine(&g, "storage_drive", nx + 2, row);
    CABLE(nx + 3, row);
    world_set_at(w, tile_def(tile_id("storage_terminal"))->layer, nx + 4, row, tile_id("storage_terminal"));
    int tx = nx + 4;
    inv->containers[drive->container].slots[0] = (ItemStack){ .id = (uint16_t)cell1, .count = 1 };
    NetInfo info;
    /* With no electricity it is there, and does nothing. */
    for (int t = 0; t < 20; t++) game_tick(&g);
    CHECK(digital_info(&g, tx, row, &info) && info.found && !info.online && info.controller.x == nx && digital_insert(&g, tx, row, ITEM_DIRT, 5) == 0);
    power->power = 10000;
    for (int t = 0; t < 10; t++) game_tick(&g);
    CHECK(digital_info(&g, tx, row, &info) && info.online && info.bytes_total == 1024 && info.bytes_used == 0 && info.cells == 1 && info.drives == 1 && info.types == 0);
    CHECK(power->power < 10000 && power->power > 10000 - 400); /* ten units a tick */
    (void)heart;
    /* Things go in and are counted; a kind costs 8 bytes of a 1k cell, and eight things a byte. */
    CHECK(digital_insert(&g, tx, row, ITEM_DIRT, 100) == 100 && digital_count(&g, tx, row, ITEM_DIRT) == 100);
    CHECK(digital_info(&g, tx, row, &info) && info.bytes_used == 8 + 13 && info.types == 1);
    CHECK(digital_insert(&g, tx, row, ITEM_STONE, 40) == 40 && digital_extract(&g, tx, row, ITEM_DIRT, 30) == 30 && digital_count(&g, tx, row, ITEM_DIRT) == 70);
    static NetItem list[MAX_NET_KINDS];
    CHECK(digital_list(&g, tx, row, list, MAX_NET_KINDS) == 2 && list[0].item == ITEM_DIRT && list[0].count == 70 && list[1].item == ITEM_STONE && !list[0].craftable);
    /* It holds what it holds and no more; and nothing that has anything inside it or a meter to read. */
    int fits = (1024 - 2 * 8) * 8 - 110;
    CHECK(digital_insert(&g, tx, row, ITEM_DIRT, 20000) == fits && digital_insert(&g, tx, row, ITEM_DIRT, 1) == 0);
    CHECK(digital_insert(&g, tx, row, cell1, 1) == 0 && digital_insert(&g, tx, row, item_id("bucket"), 1) == 0);
    CHECK(digital_extract(&g, tx, row, ITEM_DIRT, 30000) == fits + 70 && digital_extract(&g, tx, row, ITEM_STONE, 40) == 40);
    CHECK(digital_info(&g, tx, row, &info) && info.bytes_used == 0 && info.types == 0);
    /* Sixty-three kinds to a cell. */
    int kinds_in = 0;
    for (ItemId k = 1; k < content.item_count && kinds_in < 70; k++)
        if (item_def(k)->meter_max == 0 && item_def(k)->crystals == 0 && digital_insert(&g, tx, row, k, 1) == 1) kinds_in++;
    CHECK(kinds_in == CELL_TYPES);
    for (ItemId k = 1; k < content.item_count; k++) digital_extract(&g, tx, row, k, 10);
    /* From what the character carries, and back. */
    for (int k = 0; k < INVENTORY_SLOTS; k++) inv->slots[k] = (ItemStack){ 0 };
    inv->slots[0] = (ItemStack){ .id = (uint16_t)ITEM_DIRT, .count = 50 };
    CHECK(digital_deposit(&g, tx, row, 0, 20) == 20 && inv->slots[0].count == 30 && digital_deposit(&g, tx, row, 0, 0) == 30 && inv->slots[0].count == 0);
    CHECK(digital_withdraw(&g, tx, row, ITEM_DIRT, 15) == 15 && count_item(&g, ITEM_DIRT) == 15 && digital_count(&g, tx, row, ITEM_DIRT) == 35);
    /* Kept with the world; and the cell keeps what is in it when it is taken out of the drive and put in another. */
    CHECK(save_game(&g, "build/test_digital.world"));
    Game loaded;
    CHECK(load_game(&loaded, "build/test_digital.world"));
    for (int t = 0; t < 3; t++) game_tick(&loaded);
    CHECK(digital_count(&loaded, tx, row, ITEM_DIRT) == 35);
    game_free(&loaded);
    ItemStack taken = inv->containers[drive->container].slots[0];
    inv->containers[drive->container].slots[0] = (ItemStack){ 0 };
    CHECK(digital_count(&g, tx, row, ITEM_DIRT) == 0 && digital_info(&g, tx, row, &info) && info.bytes_total == 0);
    inv->containers[drive->container].slots[3] = taken;
    CHECK(digital_count(&g, tx, row, ITEM_DIRT) == 35);

    /* A storage bus makes the chest beside it part of the store: counted, drawn on, and filled when the cells are full. */
    CABLE(nx + 2, row + 1);
    world_set_at(w, tile_def(tile_id("storage_bus"))->layer, nx + 2, row + 2, tile_id("storage_bus"));
    Storage *chest = put_machine(&g, "chest", nx + 2, row + 3);
    fill_slot(&g, chest, 0, ITEM_STONE, 25);
    CHECK(digital_count(&g, tx, row, ITEM_STONE) == 25 && digital_extract(&g, tx, row, ITEM_STONE, 10) == 10 && held_in(&g, chest, ITEM_STONE) == 15);
    inv->containers[drive->container].slots[3] = (ItemStack){ 0 };
    CHECK(digital_insert(&g, tx, row, ITEM_STONE, 5) == 5 && held_in(&g, chest, ITEM_STONE) == 20); /* (no cell: into the chest) */
    inv->containers[drive->container].slots[3] = taken;

    /* An import bus takes what is in the chest beside it, four things a tick; told a kind, only that. An export bus puts
     * the kind it is told out into the chest beside it. */
    CABLE(nx + 3, row - 1);
    Storage *import = put_machine(&g, "import_bus", nx + 3, row - 2), *feed = put_machine(&g, "chest", nx + 3, row - 3);
    fill_slot(&g, feed, 0, item_id("copper_ore"), 10);
    fill_slot(&g, feed, 1, item_id("iron_ore"), 10);
    fill_slot(&g, import, 0, item_id("iron_ore"), 1);
    for (int t = 0; t < 2; t++) game_tick(&g);
    CHECK(digital_count(&g, tx, row, item_id("iron_ore")) == 8 && held_in(&g, feed, item_id("iron_ore")) == 2 && held_in(&g, feed, item_id("copper_ore")) == 10);
    fill_slot(&g, import, 0, ITEM_NONE, 0);
    for (int t = 0; t < 4; t++) game_tick(&g);
    CHECK(held_in(&g, feed, item_id("copper_ore")) == 0 && held_in(&g, feed, item_id("iron_ore")) == 0 && digital_count(&g, tx, row, item_id("copper_ore")) == 10);
    CABLE(nx + 1, row - 1);
    Storage *export = put_machine(&g, "export_bus", nx + 1, row - 2), *bin = put_machine(&g, "chest", nx + 1, row - 3);
    for (int t = 0; t < 3; t++) game_tick(&g);
    CHECK(held_in(&g, bin, item_id("iron_ore")) == 0); /* told nothing, it sends nothing */
    fill_slot(&g, export, 0, item_id("iron_ore"), 1);
    for (int t = 0; t < 2; t++) game_tick(&g);
    CHECK(held_in(&g, bin, item_id("iron_ore")) == 8 && digital_count(&g, tx, row, item_id("iron_ore")) == 2);
    fill_slot(&g, export, 0, ITEM_NONE, 0);

    /* Making things: an assembler beside a bench teaches the network that bench's recipes; a crafting CPU does the work.
     * What is short is made too, and what that takes: three 1k components and then the 4k one. */
    ItemId quartz = item_id("certus_quartz"), logic = item_id("logic_processor"), calc = item_id("calculation_processor");
    ItemId comp1 = item_id("cell_component_1k"), comp4 = item_id("cell_component_4k");
    char why[64];
    CHECK(!digital_request(&g, tx, row, comp4, 1, why, sizeof why) && why[0]); /* nothing here makes it yet */
    CABLE(nx + 4, row + 1);
    world_set_at(w, tile_def(tile_id("assembler"))->layer, nx + 4, row + 2, tile_id("assembler"));
    world_set_at(w, tile_def(tile_id("inscriber"))->layer, nx + 4, row + 3, tile_id("inscriber"));
    CHECK(digital_recipe_for(&g, tx, row, comp4) != NULL && digital_recipe_for(&g, tx, row, cell1) == NULL); /* (a cell is not a plain thing) */
    CHECK(!digital_request(&g, tx, row, comp4, 1, why, sizeof why) && strstr(why, "CPU"));
    CABLE(nx + 5, row);
    world_set_at(w, tile_def(tile_id("crafting_cpu"))->layer, nx + 6, row, tile_id("crafting_cpu"));
    CHECK(!digital_request(&g, tx, row, comp4, 1, why, sizeof why) && strstr(why, "Missing"));
    digital_insert(&g, tx, row, quartz, 12);
    digital_insert(&g, tx, row, logic, 3);
    digital_insert(&g, tx, row, calc, 1);
    CHECK(digital_request(&g, tx, row, comp4, 1, why, sizeof why));
    const NetJob *jobs[MAX_NET_JOBS];
    CHECK(digital_jobs(&g, tx, row, jobs, MAX_NET_JOBS) == 1 && jobs[0]->step_count == 2 && jobs[0]->steps[0].crafts == 3 && jobs[0]->steps[1].crafts == 1 && jobs[0]->wanted == comp4);
    CHECK(!digital_request(&g, tx, row, comp1, 1, why, sizeof why) && strstr(why, "busy")); /* one CPU, one job */
    CHECK(digital_list(&g, tx, row, list, MAX_NET_KINDS) > 0);
    bool listed = false;
    for (int k = 0; k < digital_list(&g, tx, row, list, MAX_NET_KINDS); k++) listed = listed || (list[k].item == comp4 && list[k].craftable && list[k].count == 0);
    CHECK(listed);
    /* Three at 60 seconds and one at 120: 3,000 ticks. Half way, some of the first are made and the last is not. */
    for (int k = 0; k < 15; k++) {
        power->power = 10000;
        for (int t = 0; t < 100; t++) game_tick(&g);
    }
    CHECK(digital_count(&g, tx, row, comp4) == 0 && digital_count(&g, tx, row, quartz) < 12 && digital_jobs(&g, tx, row, jobs, MAX_NET_JOBS) == 1);
    for (int k = 0; k < 16; k++) {
        power->power = 10000;
        for (int t = 0; t < 100; t++) game_tick(&g);
    }
    CHECK(digital_count(&g, tx, row, comp4) == 1 && digital_count(&g, tx, row, comp1) == 0 && digital_count(&g, tx, row, quartz) == 0 &&
          digital_count(&g, tx, row, calc) == 0 && digital_jobs(&g, tx, row, jobs, MAX_NET_JOBS) == 0);
    /* A co-processor halves the time; and a job can be given up. */
    world_set_at(w, tile_def(tile_id("crafting_coprocessor"))->layer, nx + 7, row, tile_id("crafting_coprocessor"));
    digital_insert(&g, tx, row, quartz, 8);
    digital_insert(&g, tx, row, logic, 2);
    CHECK(digital_info(&g, tx, row, &info) && info.cpus == 1 && info.coprocessors == 1 && digital_request(&g, tx, row, comp1, 2, why, sizeof why));
    power->power = 10000;
    for (int t = 0; t < 320; t++) game_tick(&g);
    CHECK(digital_count(&g, tx, row, comp1) == 1);
    CHECK(digital_jobs(&g, tx, row, jobs, MAX_NET_JOBS) == 1 && digital_cancel(&g, jobs[0]) && digital_jobs(&g, tx, row, jobs, MAX_NET_JOBS) == 0);

    /* Planes: one breaks the plain block beside it into the store; one puts the block it is told out into the open square beside it. */
    power->power = 10000;
    CABLE(nx + 6, row + 1);
    put_machine(&g, "annihilation_plane", nx + 6, row + 2);
    world_set(w, nx + 6, row + 3, TILE_STONE);
    long stone = digital_count(&g, tx, row, ITEM_STONE);
    for (int t = 0; t < 25; t++) game_tick(&g);
    CHECK(world_get(w, nx + 6, row + 3) == TILE_AIR && digital_count(&g, tx, row, ITEM_STONE) == stone + 1);
    world_set_at(w, tile_def(tile_id("annihilation_plane"))->layer, nx + 6, row + 2, TILE_AIR);
    CABLE(nx + 7, row - 1);
    Storage *former = put_machine(&g, "formation_plane", nx + 7, row - 2);
    fill_slot(&g, former, 0, ITEM_STONE, 1);
    for (int t = 0; t < 25; t++) game_tick(&g);
    int placed = 0;
    for (int d = -1; d <= 1; d++) placed += world_get(w, nx + 7 + d, row - 2 - (d == 0)) == TILE_STONE;
    CHECK(placed >= 1 && digital_count(&g, tx, row, ITEM_STONE) < stone + 1);

    /* A quantum bridge: a terminal a long way off, on cable of its own, is on the same network. */
    put_device(&g, "quantum_bridge", nx + 8, row);
    int far = px + 120;
    put_device(&g, "quantum_bridge", far, row);
    CABLE(far + 1, row);
    world_set_at(w, tile_def(tile_id("storage_terminal"))->layer, far + 2, row, tile_id("storage_terminal"));
    CABLE(nx + 8, row + 1); /* (joined to the co-processor's end of things by the cable under it) */
    CABLE(nx + 7, row + 1);
    CHECK(digital_info(&g, far + 2, row, &info) && info.found && info.online && info.controller.x == nx && digital_count(&g, far + 2, row, ITEM_DIRT) == 35);
    /* Reached from a distance: with a wireless terminal in hand, within range of an access point. */
    TilePos at;
    for (int k = 0; k < INVENTORY_SLOTS; k++) inv->slots[k] = (ItemStack){ 0 };
    inv->slots[0] = (ItemStack){ .id = (uint16_t)item_id("wireless_terminal"), .count = 1 };
    inventory_select(inv, 0, -1);
    g.player.feet = g.player.step_to = (TilePos){ far + 20, ground };
    CHECK(!digital_remote(&g, &at)); /* no access point */
    put_device(&g, "wireless_access_point", far + 3, row);
    CHECK(digital_remote(&g, &at) && at.x == world_wrap_x(w, far + 3) && at.y == row && digital_count(&g, at.x, at.y, ITEM_DIRT) == 35);
    g.player.feet = g.player.step_to = (TilePos){ far + 60, ground };
    CHECK(!digital_remote(&g, &at)); /* too far */
    g.player.feet = g.player.step_to = (TilePos){ far + 20, ground };
    inventory_select(inv, 1, -1);
    CHECK(!digital_remote(&g, &at)); /* nothing in hand */
#undef CABLE
    game_free(&g);

    /* The world has certus quartz in it, as often as iron. */
    World world;
    CHECK(world_init(&world, 2048, 401, 3, WORLD_TERRAIN));
    int quartz_blocks = count_tile(&world, 0, tile_id("quartz_ore")), iron_blocks = count_tile(&world, 0, tile_id("iron_ore"));
    CHECK(quartz_blocks > iron_blocks / 2 && quartz_blocks < iron_blocks * 2);
    world_free(&world);

    /* It goes with either of the others. */
    static ModList mods;
    mods_scan(&mods, MODS_DIR, "build/modtest/none");
    for (int a = 0; a < mods.count; a++) {
        if (strcmp(mods.mods[a].id, "digital_storage") != 0) continue;
        for (int b = 0; b < mods.count; b++) {
            char what[64];
            if (a != b) CHECK(!mods_conflict(&mods.mods[a], &mods.mods[b], what, sizeof what));
        }
    }
    content_reset();
    CHECK(content_load("data", err, sizeof err) && content.tile_count == tiles);
}

/* Culinary Delights, on top of the game. */
static void test_culinary_mod(void)
{
    char err[256];
    content_reset();
    CHECK(content_load("data", err, sizeof err));
    int tiles = content.tile_count, items = content.item_count, recipes = content.recipe_count, trees = content.tree_count, biomes = content.biome_count;
    if (!content_load("mods/culinary_delights", err, sizeof err)) {
        printf("FAIL culinary_delights does not load: %s\n", err);
        failures++;
        return;
    }
    CHECK(content.tile_count == tiles + 160 && content.item_count == items + 147 && content.recipe_count == recipes + 106);
    CHECK(content.tree_count == trees + 12 && content.biome_count == biomes + 6);
    /* Everything it adds can be got: grown, picked, or made of things that can be. */
    static uint8_t got[MAX_ITEMS];
    memset(got, 0, sizeof got);
    for (ItemId i = 1; i < items; i++) got[i] = 1; /* (the game's own things are taken as read) */
    for (TileType t = 1; t < content.tile_count; t++) {
        const TileDef *def = tile_def(t);
        if (def->drop != ITEM_NONE) got[def->drop] = 1;
        for (int k = 0; k < def->extra_drop_count; k++) got[def->extra_drops[k].id] = 1;
    }
    for (int t = 0; t < content.tree_count; t++)
        if (content.trees[t].fruit != ITEM_NONE) got[content.trees[t].fruit] = 1;
    for (int pass = 0; pass < 12; pass++) {
        for (int k = 0; k < content.recipe_count; k++) {
            const Recipe *r = &content.recipes[k];
            bool can = true;
            for (int i = 0; i < r->input_count; i++) can = can && got[r->inputs[i].id];
            if (can) got[r->output.id] = 1;
        }
    }
    int missing = 0;
    for (ItemId i = items; i < content.item_count; i++) {
        if (got[i]) continue;
        printf("  cannot be got: %s\n", item_def(i)->name);
        missing++;
    }
    CHECK(missing == 0);
    /* The kitchen: six benches, the chef's table in three tiers, each bought with a meal of the tier before. */
    TileType chef = tile_id("chefs_table");
    CHECK(tile_def(chef)->upgrade_to == tile_id("chefs_table_2") && tile_def(chef)->upgrade_cost[0].id == item_id("picnic_basket") &&
          tile_def(tile_id("chefs_table_2"))->upgrade_cost[0].id == item_id("harvest_feast") && tile_def(tile_id("chefs_table_3"))->bench_level == 3);
    CHECK(find_recipe(TILE_WORKBENCH, "prep_table") && find_recipe(TILE_WORKBENCH, "cooking_pot") && find_recipe(TILE_WORKBENCH, "baking_oven") &&
          find_recipe(TILE_WORKBENCH, "mortar") && find_recipe(TILE_WORKBENCH, "fermenting_barrel") && find_recipe(TILE_WORKBENCH, "chefs_table"));
    /* The long way round: the last dish is five dishes, and the first of those is five feasts, each of five dishes more. */
    const Recipe *last = find_recipe(tile_id("chefs_table_3"), "ambrosia"), *banquet = find_recipe(tile_id("chefs_table_3"), "royal_banquet");
    CHECK(last && last->level == 3 && last->input_count == 5 && last->inputs[0].id == item_id("royal_banquet"));
    CHECK(banquet && banquet->level == 3 && banquet->input_count == 5);
    const Recipe *feast = find_recipe(tile_id("chefs_table_2"), "italian_feast");
    CHECK(feast && feast->level == 2 && feast->input_count == 5 && find_recipe_from(tile_id("cooking_pot"), "tomato_sauce", "tomato") && find_recipe_from(tile_id("baking_oven"), "lasagna", "pasta_dough"));
    /* (How deep it goes: the number of makings, one inside another, under the last dish.) */
    static int depth[MAX_ITEMS];
    memset(depth, 0, sizeof depth);
    for (int pass = 0; pass < 12; pass++) {
        for (int k = 0; k < content.recipe_count; k++) {
            const Recipe *r = &content.recipes[k];
            int deep = 0;
            for (int i = 0; i < r->input_count; i++)
                if (depth[r->inputs[i].id] > deep) deep = depth[r->inputs[i].id];
            if (r->output.id >= items && deep + 1 > depth[r->output.id]) depth[r->output.id] = deep + 1;
        }
    }
    CHECK(depth[item_id("ambrosia")] >= 6);
    /* What it is to eat it. */
    const ItemDef *ambrosia = item_def(item_id("ambrosia"));
    CHECK(ambrosia->food == 1.0f && ambrosia->food_health == 1.0f && ambrosia->food_energy == 1.0f && item_def(item_id("strawberry"))->food > 0.0f);
    Game g;
    game_init(&g, 96, 64, 5, WORLD_FLAT);
    World *w = &g.world;
    g.tick_rate = 0;
    clear_inventory(&g);
    inventory_add(&g.inventory, item_id("ambrosia"), 1);
    inventory_select(&g.inventory, 0, -1);
    g.vitals.hunger = 0.1f;
    g.vitals.health = 0.3f;
    g.vitals.energy = 0.2f;
    CHECK(game_eat(&g) && g.vitals.hunger > 0.99f && g.vitals.health > 0.99f && g.vitals.energy > 0.99f && count_item(&g, item_id("ambrosia")) == 0);
    /* A crop is sown from what it bears and grows up by stages; a tall one gets its top, and is cut back to grow again. */
    int s = w->surface_y, ground = s - 1, px = g.player.feet.x;
    CHECK(item_def(item_id("rice"))->sows == tile_id("rice_0") && item_def(item_id("strawberry"))->sows == tile_id("strawberry_0"));
    world_set_at(w, BG, px + 4, ground, tile_id("rice_0"));
    for (int k = 0; k < 6; k++) world_grow_plant(w, BG, px + 4, ground);
    CHECK(world_get_at(w, BG, px + 4, ground) == tile_id("rice_3") && world_get_at(w, BG, px + 4, ground - 1) == tile_id("rice_top"));
    world_set_at(w, BG, px + 6, ground, tile_id("pumpkin_0"));
    for (int k = 0; k < 6; k++) world_grow_plant(w, BG, px + 6, ground);
    CHECK(world_get_at(w, BG, px + 6, ground) == tile_id("pumpkin_3") && world_get_at(w, BG, px + 6, ground - 1) == TILE_AIR);
    CHECK(item_def(item_id("lemon"))->plants == tree_id("lemon_tree") && content.trees[tree_id("cocoa_pod_tree")].fruit == item_id("cocoa_pod"));
    game_free(&g);

    /* The world: each of the six new kinds of country is somewhere in it, with its own crops and trees growing there. */
    static const char *const lands[6] = { "berry_meadow", "citrus_grove", "spice_jungle", "vineyard_hills", "rice_terraces", "harvest_fields" };
    static const char *const grows[6] = { "strawberry", "basil", "ginger", "grape", "rice", "pumpkin" };
    static const char *const stands[6] = { "plum_tree_log", "lemon_tree_log", "banana_tree_log", "olive_tree_log", "peach_tree_log", "walnut_tree_log" };
    int columns[6] = { 0 }, crops[6] = { 0 }, trunks[6] = { 0 };
    for (uint32_t seed = 1; seed <= 4; seed++) {
        World world;
        CHECK(world_init(&world, 4096, 401, seed, WORLD_TERRAIN));
        for (int x = 0; x < world.width; x++) {
            const char *land = content.biomes[world_biome(&world, x)].name;
            for (int k = 0; k < 6; k++) {
                if (strcmp(land, lands[k]) != 0) continue;
                columns[k]++;
                for (int y = 0; y < world.height; y++) {
                    const char *name = tile_def(world_get_at(&world, BG, x, y))->name;
                    if (strncmp(name, grows[k], strlen(grows[k])) == 0) crops[k]++;
                    if (strcmp(name, stands[k]) == 0) trunks[k]++;
                }
            }
        }
        world_free(&world);
    }
    for (int k = 0; k < 6; k++) {
        if (columns[k] < 100 || crops[k] < 3 || trunks[k] < 1) printf("  %s: %d columns, %d %s, %d %s\n", lands[k], columns[k], crops[k], grows[k], trunks[k], stands[k]);
        CHECK(columns[k] >= 100 && crops[k] >= 3 && trunks[k] >= 1);
    }

    /* It goes with every other mod. */
    static ModList mods;
    mods_scan(&mods, MODS_DIR, "build/modtest/none");
    int found = 0;
    for (int a = 0; a < mods.count; a++) {
        if (strcmp(mods.mods[a].id, "culinary_delights") != 0) continue;
        found++;
        for (int b = 0; b < mods.count; b++) {
            char what[64];
            if (a != b) CHECK(!mods_conflict(&mods.mods[a], &mods.mods[b], what, sizeof what));
        }
    }
    CHECK(found == 1);
    content_reset();
    CHECK(content_load("data", err, sizeof err) && content.tile_count == tiles);
}

/* Explorer's Delight, on top of the game. */
static void test_explorers_mod(void)
{
    char err[256];
    content_reset();
    CHECK(content_load("data", err, sizeof err));
    int tiles = content.tile_count, items = content.item_count, recipes = content.recipe_count, trees = content.tree_count, biomes = content.biome_count;
    if (!content_load("mods/explorers_delight", err, sizeof err)) {
        printf("FAIL explorers_delight does not load: %s\n", err);
        failures++;
        return;
    }
    CHECK(content.tile_count == tiles + 145 && content.item_count == items + 134 && content.recipe_count == recipes + 101);
    CHECK(content.tree_count == trees + 6 && content.biome_count == biomes + 13 && content.cave_biome_count == 6);
    /* Everything it adds can be got: dug, cut, picked, or made of things that can be. */
    static uint8_t got[MAX_ITEMS];
    memset(got, 0, sizeof got);
    for (ItemId i = 1; i < items; i++) got[i] = 1;
    for (TileType t = 1; t < content.tile_count; t++) {
        const TileDef *def = tile_def(t);
        if (def->drop != ITEM_NONE) got[def->drop] = 1;
        for (int k = 0; k < def->extra_drop_count; k++) got[def->extra_drops[k].id] = 1;
    }
    for (int t = 0; t < content.tree_count; t++)
        if (content.trees[t].fruit != ITEM_NONE) got[content.trees[t].fruit] = 1;
    for (int pass = 0; pass < 6; pass++) {
        for (int k = 0; k < content.recipe_count; k++) {
            const Recipe *r = &content.recipes[k];
            bool can = true;
            for (int i = 0; i < r->input_count; i++) can = can && got[r->inputs[i].id];
            if (can) got[r->output.id] = 1;
        }
    }
    int missing = 0;
    for (ItemId i = items; i < content.item_count; i++) {
        if (got[i]) continue;
        printf("  cannot be got: %s\n", item_def(i)->name);
        missing++;
    }
    CHECK(missing == 0);
    /* Each of the nine stones has its bricks, its polished block, its tiles, its column and its stairs, made at the builder's bench. */
    static const char *const stones[13] = { "terracotta", "red_sandstone", "rock_salt", "scoria", "pumice", "slate", "granite", "mossy_stone", "glacial_ice",
                                            "coral_stone", "dripstone", "geode", "magma_rock" };
    TileType builder = tile_id("builder_bench");
    for (int k = 0; k < 13; k++) {
        char name[64];
        TileType rock = tile_id(stones[k]);
        CHECK(rock != TILE_AIR && tile_def(rock)->solid && item_def(item_id(stones[k]))->places == rock);
        snprintf(name, sizeof name, "%s_bricks", stones[k]);
        const Recipe *r = find_recipe_from(builder, name, stones[k]);
        CHECK(tile_def(tile_id(name))->solid && r && r->output.count == 4 && r->inputs[0].count == 4);
        snprintf(name, sizeof name, "polished_%s", stones[k]);
        CHECK(tile_def(tile_id(name))->solid && find_recipe(builder, name));
        char polished[64];
        snprintf(polished, sizeof polished, "%s", name);
        snprintf(name, sizeof name, "%s_tiles", stones[k]);
        CHECK(tile_def(tile_id(name))->solid && find_recipe_from(builder, name, polished));
        snprintf(name, sizeof name, "%s_column", stones[k]);
        CHECK(tile_def(tile_id(name))->supports && tile_def(tile_id(name))->layer == LAYER_BACKGROUND && find_recipe_from(builder, name, stones[k]));
        snprintf(name, sizeof name, "%s_stairs", stones[k]);
        CHECK(tile_def(tile_id(name))->steps && find_recipe_from(builder, name, stones[k]));
    }
    /* Each of the six woods is a block of its own, with its column and stairs; and any of them will do where wood is wanted. */
    static const char *const woods[6] = { "acacia", "baobab", "birch", "willow", "bamboo", "glowcap" };
    for (int k = 0; k < 6; k++) {
        char name[64], wood[64];
        snprintf(wood, sizeof wood, "%s_wood", woods[k]);
        snprintf(name, sizeof name, "%s_log", woods[k]);
        CHECK(tile_def(tile_id(name))->drop == item_id(wood) && tile_def(tile_id(wood))->solid && item_def(item_id(wood))->fuel > 0);
        CHECK(find_recipe_from(TILE_WORKBENCH, "wood", wood) != NULL);
        snprintf(name, sizeof name, "%s_stairs", woods[k]);
        CHECK(tile_def(tile_id(name))->steps && find_recipe_from(builder, name, wood));
        snprintf(name, sizeof name, "%s_tree", woods[k]);
        CHECK(tree_id(name) >= 0 && content.trees[tree_id(name)].fruit != ITEM_NONE && item_def(content.trees[tree_id(name)].fruit)->plants == tree_id(name));
    }
    CHECK(tile_def(tile_id("glowcap_leaves"))->light > 0 && find_recipe_from(TILE_WORKBENCH, "salt", "rock_salt"));

    /* The world: each of the ten new kinds of country is in it, with its own ground on top, its own rock under that, and what grows there. */
    static const struct {
        const char *land, *top, *rock, *grows;
    } lands[13] = { { "coral_reef", "coral_sand", "coral_stone", "red_coral" }, { "frozen_sea", "sea_gravel", "glacial_ice", "sea_grass" },
                    { "seagrass_meadow", "sand", NULL, "sea_grass" }, { "red_mesa", "red_sand", "terracotta", "dead_bush" }, { "salt_flats", "salt_crust", "rock_salt", NULL },
                    { "volcanic_wastes", "ash", "scoria", NULL }, { "frozen_tundra", "packed_snow", "glacial_ice", NULL },
                    { "slate_highlands", "highland_grass", "slate", "heather" }, { "misty_marsh", "mud", NULL, "willow_log" },
                    { "savanna", "dry_grass", NULL, "acacia_log" }, { "birch_woods", "grass", NULL, "birch_log" },
                    { "mossy_hollow", "moss", NULL, "glowcap_log" }, { "bamboo_grove", "grass", NULL, "bamboo_log" } };
    int columns[13] = { 0 }, topped[13] = { 0 }, rocks[13] = { 0 }, growing[13] = { 0 }, pockets[4] = { 0 };
    /* (And under ground: six kinds of cave, each with its own rock round it and its own things growing in it.) */
    static const char *const linings[4] = { "dripstone", "mycelium", "geode", "magma_rock" };
    static const char *const cave_life[7] = { "icicle", "glow_vine", "stalactite", "stalagmite", "glowshroom", "crystal_cluster", "ember_bloom" };
    int lined[4] = { 0 }, living[7] = { 0 }, floating = 0;
    for (uint32_t seed = 1; seed <= 4; seed++) {
        World world;
        CHECK(world_init(&world, 4096, 401, seed, WORLD_TERRAIN));
        /* (The rocks that are only under the heart of a country are in pockets in the rock everywhere as well.) */
        pockets[0] += count_tile(&world, 0, tile_id("granite"));
        pockets[1] += count_tile(&world, 0, tile_id("mossy_stone"));
        pockets[2] += count_tile(&world, 0, tile_id("pumice"));
        pockets[3] += count_tile(&world, 0, tile_id("red_sandstone"));
        for (int k = 0; k < 4; k++) lined[k] += count_tile(&world, 0, tile_id(linings[k]));
        for (int k = 0; k < 7; k++) living[k] += count_tile(&world, BG, tile_id(cave_life[k]));
        /* (What hangs, hangs from something; what stands, stands on something.) */
        for (int y = 1; y < world.height - 1; y++) {
            for (int x = 0; x < world.width; x++) {
                TileType t = world_get_at(&world, BG, x, y);
                if (t == tile_id("stalactite") && !world_is_solid(&world, x, y - 1) && world_get_at(&world, BG, x, y - 1) != t) floating++;
                if ((t == tile_id("stalagmite") || t == tile_id("crystal_cluster")) && !world_is_solid(&world, x, y + 1)) floating++;
            }
        }
        if (seed == 1) {
            /* The same seed makes the same world, caves and all. */
            World again;
            CHECK(world_init(&again, 4096, 401, seed, WORLD_TERRAIN));
            size_t cells = (size_t)world.width * world.height;
            CHECK(memcmp(again.layers[0], world.layers[0], cells * sizeof(uint16_t)) == 0 && memcmp(again.layers[1], world.layers[1], cells * sizeof(uint16_t)) == 0);
            world_free(&again);
        }
        for (int x = 0; x < world.width; x++) {
            const char *land = content.biomes[world_biome(&world, x)].name;
            for (int k = 0; k < 13; k++) {
                if (strcmp(land, lands[k].land) != 0) continue;
                columns[k]++;
                TileType top = tile_id(lands[k].top), rock = lands[k].rock ? tile_id(lands[k].rock) : TILE_AIR, grows = lands[k].grows ? tile_id(lands[k].grows) : TILE_AIR;
                bool has_top = false, has_rock = false;
                for (int y = 0; y < world.height; y++) {
                    has_top = has_top || world_get(&world, x, y) == top;
                    has_rock = has_rock || (rock != TILE_AIR && world_get(&world, x, y) == rock);
                    if (grows != TILE_AIR && world_get_at(&world, BG, x, y) == grows) growing[k]++;
                }
                topped[k] += has_top;
                rocks[k] += has_rock;
            }
        }
        world_free(&world);
    }
    for (int k = 0; k < 13; k++) {
        bool ok = columns[k] >= 100 && topped[k] * 2 > columns[k] && (!lands[k].rock || rocks[k] * 2 > columns[k]) && (!lands[k].grows || growing[k] >= 3);
        if (!ok) printf("  %s: %d columns, %d with %s, %d with %s, %d %s\n", lands[k].land, columns[k], topped[k], lands[k].top, rocks[k],
                        lands[k].rock ? lands[k].rock : "-", growing[k], lands[k].grows ? lands[k].grows : "-");
        CHECK(ok);
    }

    /* (Red sandstone is the scarcest in the ground: it is in the sandstone under beaches and seas, of which the new seas leave less. It is made of red sand as well.) */
    CHECK(pockets[0] > 500 && pockets[1] > 200 && pockets[2] > 200 && pockets[3] > 10);
    for (int k = 0; k < 4; k++) {
        if (lined[k] < 300) printf("  %s: %d blocks\n", linings[k], lined[k]);
        CHECK(lined[k] >= 300);
    }
    for (int k = 0; k < 7; k++) {
        if (living[k] < 20) printf("  %s: %d\n", cave_life[k], living[k]);
        CHECK(living[k] >= 20);
    }
    CHECK(floating == 0);
    CHECK(tile_def(tile_id("red_coral"))->needs_water && item_def(item_id("red_coral"))->sows == tile_id("red_coral") && find_recipe(tile_id("builder_bench"), "red_coral_block"));
    CHECK(tile_def(tile_id("prism_block"))->light > 0 && find_recipe(tile_id("builder_bench"), "pearl_block") && tile_def(tile_id("crystal_cluster"))->drop == item_id("prism_shard"));

    /* It goes with every other mod. */
    static ModList mods;
    mods_scan(&mods, MODS_DIR, "build/modtest/none");
    int found = 0;
    for (int a = 0; a < mods.count; a++) {
        if (strcmp(mods.mods[a].id, "explorers_delight") != 0) continue;
        found++;
        for (int b = 0; b < mods.count; b++) {
            char what[64];
            if (a != b) CHECK(!mods_conflict(&mods.mods[a], &mods.mods[b], what, sizeof what));
        }
    }
    CHECK(found == 1);
    content_reset();
    CHECK(content_load("data", err, sizeof err) && content.tile_count == tiles);
}

/* The character's skin, and a picture of everything that can be worn, for the game and for each mod. */
static bool texture_there(const char *mod, const char *file)
{
    char path[512];
    const char *dirs[2] = { "data", mod };
    for (int i = 0; i < 2; i++) {
        if (!dirs[i]) continue;
        snprintf(path, sizeof path, "%s/textures/%s", dirs[i], file);
        FILE *f = fopen(path, "rb");
        if (f) {
            fclose(f);
            return true;
        }
    }
    return false;
}

static void test_worn(void)
{
    const char *mods[] = { NULL, "mods/magic_mayhem", "mods/industry", "mods/digital_storage", "mods/culinary_delights",
                           "mods/explorers_delight" };
    for (int m = 0; m < (int)(sizeof mods / sizeof mods[0]); m++) {
        char err[256];
        content_reset();
        CHECK(content_load("data", err, sizeof err));
        if (mods[m]) CHECK(content_load(mods[m], err, sizeof err));
        CHECK(content.rules.player_skin[0] && texture_there(mods[m], content.rules.player_skin));
        int worn = 0;
        for (int i = 1; i < content.item_count; i++) {
            const ItemDef *d = item_def((ItemId)i);
            if (d->equip == EQUIP_NONE) {
                CHECK(!d->worn[0]);
                continue;
            }
            worn++;
            if (!d->worn[0] || !texture_there(mods[m], d->worn)) {
                printf("FAIL %s has no picture of it worn\n", d->name);
                failures++;
            }
        }
        CHECK(worn >= 30);
    }
    /* The jetpack goes on the body, as a breastplate does. */
    CHECK(item_def(item_id("jetpack"))->equip == EQUIP_CHEST && item_def(item_id("jetpack"))->worn[0]);
    /* What is worn is named by the item, and a different item has a different picture. */
    CHECK(strcmp(item_def(item_id("north_hat"))->worn, item_def(item_id("iron_helmet"))->worn) != 0);
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

    /* What the game ships with is checked first, as it is. The tests after
     * that are of how the game works, and are run against a small fixed set
     * of content of their own - the recipes and benches in tests/legacy -
     * so that changing what the game ships with does not change them. */
    test_shipped_light();
    test_shipped_recipes();
    test_guide();
    shipped_torch_light = tile_def(tile_id("torch"))->light;
    content.recipe_count = 0;
    content.tint_count = 0;
    if (!content_load("tests/legacy", err, sizeof err)) {
        printf("could not load tests/legacy: %s\n", err);
        return 1;
    }
    /* Likewise the creatures that came later keep out of the tests written
     * before them: each is let in by the test that is about it. */
    /* And no islands in the sky, which came after the tests of the land under it. */
    shipped_island_rate = content.worldgen.island_rate;
    content.worldgen.island_rate = 0.0f;
    /* And one random event to a chunk, as there was when they were written. */
    content.rules.random_events_per_chunk = 1;
    /* And trade as it was: everything a copper coin, to buy or to sell, found or not. (The prices, the
     * share a sale gives and the finding of things have a test of their own, against what the game ships.) */
    shipped_trade = content.trade;
    for (ItemId i = 1; i < content.item_count; i++) shipped_price[i] = content.items[i].price, content.items[i].price = 0;
    content.trade.default_price = 1;
    content.trade.sell_share = 1.0f;
    content.trade.discover = false;
    content.mobs[mob_id("yak")].spawn_chance = 0.0f;
    tile_def_mut(tile_id("dirt"))->extra_chance[0] = 0.0f; /* (no worms in the dirt, but where they are looked for) */
    content.mobs[mob_id("scorpion")].spawn_chance = 0.0f;

    test_world();
    test_mine_range();
    test_keys();
    test_click_move();
    test_click_mine();
    test_queue();
    test_unqueue();
    test_monkey_bars();
    test_crawl();
    test_crawl_into_gaps();
    test_debug_world();
    test_queue_waits();
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
    test_timed_work();
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
    test_scars_and_sandstone();
    test_tall_plants();
    test_tulips();
    test_scorpions_and_yaks();
    test_stairs_and_more();
    test_steps_beds_and_order();
    test_seasons_and_weather();
    test_doors_walls_lamps_armor();
    test_power();
    test_trade_rules();
    test_furniture_cages_paint();
    test_gate_compost_ember_boat_sign();
    test_islands_and_options();
    test_corn_basalt_windows();
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
    test_poles();
    test_content();
    test_mods();
    test_magic_mod();
    test_industry_mod();
    test_digital_mod();
    test_culinary_mod();
    test_explorers_mod();
    test_worn();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
