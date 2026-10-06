#ifndef GAME_H
#define GAME_H

#include "craft.h"
#include "inventory.h"
#include "tree.h"
#include "water.h"
#include "world.h"

#define MAX_QUEUED_ACTIONS 32
#define MAX_DENIALS 8
#define MAX_MOVES 12
#define MAX_DROPS 2048
#define MAX_TICK_RATE 1000
#define MAX_TICKS_PER_UPDATE 200 /* so a slow frame cannot snowball */
#define MAX_STORAGES 256
#define MAX_MOBS 64
#define MAX_ATTACK_TRIES 8 /* times a hunt re-plans after its quarry has moved */
#define MAX_SEARCH 60000 /* poses a route search looks at before giving up */

/* The pace of the game comes from the rules data; these are shorthands. */
#define WALK_TIME (content.rules.walk_time)
#define FALL_TIME (content.rules.fall_time)
#define CLIMB_TIME (content.rules.climb_time)
#define CRAWL_TIME (content.rules.crawl_time)
#define HOP_TIME (content.rules.hop_time)
#define HOP_HEIGHT (content.rules.hop_height)
#define MAX_WALL_HOP (content.rules.max_wall_hop)
#define PLACE_TIME (content.rules.place_time)
#define CRAFT_TIME (content.rules.craft_time)
#define DENIAL_TIME (content.rules.denial_time)
#define DROP_FALL_SPEED (content.rules.drop_fall_speed)
#define DROP_PICKUP_DELAY (content.rules.drop_pickup_delay)
#define DROP_COLLECT_TIME (content.rules.drop_collect_time)
#define DROP_LIFETIME (content.rules.drop_lifetime)
#define DEFAULT_TICK_RATE (content.rules.tick_rate)
#define DAY_TICKS (content.rules.day_ticks)
#define NIGHT_TICKS (content.rules.night_ticks)
#define TWILIGHT_TICKS (content.rules.twilight_ticks)
#define CHUNK_WIDTH (content.rules.chunk_width)

typedef struct {
    int x, y;
} TilePos;

typedef enum {
    ACTION_MOVE, /* put one of the player's two blocks on the target tile */
    ACTION_MINE, /* walk within reach of the target tile, then mine it: the foreground block,
                    or failing that the background block behind it */
    ACTION_PLACE, /* walk next to the target tile, then put a block there */
    ACTION_PLANT, /* walk next to the target tile, then plant a tree on top of it */
    ACTION_USE,  /* walk up to the bench on the target tile, then open it */
    ACTION_COLLECT, /* walk up to a dropped item, then pick it up */
    ACTION_ATTACK   /* go after a creature, and kill it */
} ActionType;

typedef struct {
    ActionType type;
    TilePos target;
    uint8_t item; /* ACTION_PLACE, ACTION_PLANT: the item being used up */
    bool wall;    /* ACTION_MINE: take the backwall, not a block */
    int drop;     /* ACTION_COLLECT, ACTION_ATTACK: which drop or creature, and the id it had
                     when queued */
    uint32_t drop_id;
    uint8_t tries; /* ACTION_ATTACK: how often the route has been planned again */
} Action;

/* One voluntary move. Standing: walk (+-1,0), hop a one-block gap (+-2,0),
 * pull up onto a ledge (+-1,-1), go over an edge onto its wall (+-1,+1) or
 * jump to catch a wall (0,-1). Climbing: up or down the wall (0,+-1), hop to
 * the facing wall (+-n,0) or pull up onto the top (+-1,-1).
 * `climb` says whether the step ends hanging on a wall. Falling is never
 * planned; gravity is applied between steps. */
typedef struct {
    int8_t dx, dy;
    bool climb;
} Step;

/* Where the player is and whether they are hanging on a wall. */
typedef struct {
    TilePos feet;
    bool climbing;
} Pose;

typedef enum {
    ACTIVITY_IDLE,
    ACTIVITY_STEPPING,
    ACTIVITY_MINING,
    ACTIVITY_PLACING
} Activity;

/* The player is 1 tile wide and 2 tall: feet at (x, y), head at (x, y-1). */
typedef struct {
    TilePos feet;
    bool climbing; /* hanging on a wall rather than standing */
    int facing;    /* -1 or +1: last horizontal direction moved */
    Activity activity;
    Step step;       /* the step in progress while ACTIVITY_STEPPING */
    bool step_hop;   /* that step is a hop, drawn as an arc */
    TilePos step_to; /* and its destination */
    float timer, duration;
} Player;

/* What the player is doing instead of getting about. */
typedef enum {
    REST_NONE,
    REST_SLEEP,   /* lying down: energy comes back */
    REST_MEDITATE /* sitting still: now and then it brings something */
} Rest;

/* The player's needs, each a bar from 0 to 1. */
typedef struct {
    float health, hunger, energy;
    float saturation; /* ticks for which a full stomach holds hunger off */
    float meditation; /* progress toward the next thing meditating brings, 0..1 */
    int32_t rest;     /* a Rest */
    float oxygen;     /* breath left, which runs down with the head under water */
    int32_t reserved[3];
} Vitals;

/* Fading marker shown on a tile whose action was refused. */
typedef struct {
    TilePos pos;
    float ttl;
} Denial;

/* An item in the item layer, in front of the blocks. It either lies in the
 * world, falling until something holds it up, or is a fruit attached to a
 * leaf, where it stays put and has to be picked by hand. */
typedef struct {
    bool active;
    uint8_t item;
    int x;         /* tile column */
    float y;       /* tile row; fractional while falling */
    float age;     /* seconds it has lain there; at DROP_LIFETIME it is gone */
    float collect; /* 0 while lying; otherwise 0..1 progress flying to the player */
    bool attached; /* growing on a leaf */
    int ttl;       /* ticks until it disappears; 0 lasts forever */
    uint32_t id;   /* unique, so a queued action can tell if its drop is gone */
    uint16_t container; /* what is inside the item, as in an ItemStack */
    /* Put down by the player on purpose: not picked up by their standing
     * there, until they have walked out of reach of it and come back. Asking
     * for it - a tap on it - picks it up regardless. */
    bool held_off;
} Drop;

/* A creature. It stands in one tile, and walks to the next over several ticks. */
typedef struct {
    bool active;
    uint8_t kind;  /* which of content.mobs */
    bool owned;    /* the player's: it does not go away with distance */
    bool baby;
    int x, y;      /* the tile it is in */
    int to_x, to_y; /* the tile it is walking to */
    int step_left, step_total; /* ticks of that walk to go, and in all */
    int dir;       /* -1 or +1: which way it faces */
    int age;       /* ticks since it was born */
    uint32_t id;
    /* For a creature that lives in trees. */
    int32_t state;  /* a MobState */
    int32_t home_x; /* the column of its tree */
    int32_t timer;  /* ticks left of the chase, or before it will come down again */
    int32_t reserved[3];
} Mob;

typedef enum {
    MOB_WANDER, /* an ordinary creature, going where it likes */
    MOB_PERCH,  /* up its tree */
    MOB_HUNT,   /* down after the player */
    MOB_RETURN, /* on its way back to its tree */
    MOB_SEEK    /* without a tree, looking for one */
} MobState;

/* What a block placed in the world keeps besides its tile: the contents of a
 * storage block such as a chest, the fuel left in a kiln or furnace. */
typedef struct {
    bool used;
    int x, y;
    uint16_t container; /* 0 if it stores nothing */
    int fuel;           /* ticks of burning left */
    int reserved[2];
} Storage;

typedef struct {
    World world;
    Player player;
    Inventory inventory;
    Drop drops[MAX_DROPS];
    uint32_t next_drop_id;
    Storage storages[MAX_STORAGES];
    Mob mobs[MAX_MOBS];
    uint32_t next_mob_id;

    /* World time. Movement runs in real time; ticks drive the day, tree
     * growth and fruit. */
    uint64_t tick;
    int tick_rate; /* ticks per second */
    float tick_timer;
    uint32_t rng;

    /* Action being carried out: walk `steps`, then mine if it is a mine. */
    bool has_current;
    Action current;
    Step *steps;
    int step_count, step_index;

    /* Actions waiting behind the current one, oldest first. */
    Action queue[MAX_QUEUED_ACTIONS];
    int queue_count;

    /* Held movement keys, -1/0/+1 per axis. Used only when no action runs. */
    int move_dx, move_dy;

    Denial denials[MAX_DENIALS];

    /* Crystals: a currency that belongs to the player rather than the world.
     * Whoever runs the game loads it in and saves it away; it is not part of
     * a world's save. */
    int crystals;

    Vitals vitals;

    /* Fog of war: how far each tile has been uncovered, 0 (never seen) to
     * 255. Once uncovered a tile stays so. */
    uint8_t *explored;
    bool fog; /* whether this world hides what has not been uncovered */

    /* The bench or storage block the player has open, and the crafting run
     * going on at it. */
    bool bench_open;
    TilePos bench_pos;
    const Recipe *craft_recipe;
    int craft_total, craft_remaining;
    float craft_timer;

    /* Scratch space for planning. */
    World sim;
    int32_t *bfs_parent;
    uint32_t *bfs_seen; /* poses reached in search number bfs_gen */
    uint32_t bfs_gen;
    int32_t *bfs_queue;
    Step *bfs_step;
} Game;

bool game_init(Game *g, int width, int height, uint32_t seed, WorldKind kind);
void game_free(Game *g);
void game_update(Game *g, float dt);

/* Queues the action a click on this tile stands for, judged against the world
 * as it will be once everything already queued has run: plant on a block the
 * selected item can grow a tree on, otherwise mine for a block, place for air
 * beside a block while a placeable item is selected, otherwise move. Returns
 * false, and shows a denial, if it is unattainable.
 *
 * A click on a tile that already has an action queued takes that action back
 * instead (see game_unqueue_tile). */
bool game_click_tile(Game *g, int tx, int ty);
/* Takes back the most recently queued action on a tile, whether it is still
 * waiting or already under way. A planting counts for both the soil and the
 * square above it where its marker is drawn. Returns false if there is none. */
bool game_unqueue_tile(Game *g, int tx, int ty);
/* A click on a dropped item: queues collecting it, or takes that back if it
 * is already queued. */
bool game_click_drop(Game *g, int drop);

/* Queues one specific action on a tile, with the same checks. Placing uses
 * the selected item. */
bool game_queue_action(Game *g, ActionType type, int tx, int ty);

/* Seconds the player takes to mine a kind of block: its own time, divided by
 * the speed of the best tool for it that they are carrying. */
float game_mine_time(const Game *g, TileType t);

/* Queues walking up to a drop and picking it up. */
bool game_queue_collect(Game *g, int drop);
/* The drop whose centre is nearest a point in tile space and within reach of
 * a tap, or -1. */
int game_drop_at(const Game *g, float x, float y);
/* Where a drop sits, in tile units. */
void game_drop_center(const Game *g, int drop, float *x, float *y);

/* Advances world time by one tick: a random event in every chunk, and fruit
 * ageing. game_update calls this tick_rate times a second. */
void game_tick(Game *g);
/* 1 in full daylight, 0 at night, in between at dawn and dusk. */
float game_daylight(const Game *g);
/* Progress through the current day or night, 0..1, and which it is. */
float game_day_phase(const Game *g, bool *is_day);

/* Starts crafting `count` of a recipe at the open bench, one every
 * CRAFT_TIME. Anything that does not fit in the inventory is dropped. */
bool game_craft(Game *g, const Recipe *r, int count);
/* Upgrades the open bench in place, paying its cost. False if it cannot be
 * upgraded or the cost is not held. */
bool game_upgrade_bench(Game *g);
/* How visible a tile is through the fog of war: 0 for never seen, 255 for
 * plain to see, in between at the edge of what has been uncovered. Always 255
 * with fog turned off. */
int game_explored(const Game *g, int x, int y);
/* Uncovers the fog round a point, as the player's being there does. */
void game_reveal(Game *g, int x, int y);

/* Whether an item can be eaten at all, and whether the player could eat one
 * now: not while a full stomach is still holding hunger off. */
bool game_is_food(ItemId id);
bool game_can_eat(const Game *g, ItemId id);
/* Eats one of the selected item. It fills hunger and mends a little health;
 * eaten on a full stomach it holds hunger off for a while instead. */
bool game_eat(Game *g);
/* True while the player's head is under water. */
bool game_player_underwater(const Game *g);
/* True while the player is afloat: in water, with nothing under their feet. */
bool game_player_swimming(const Game *g);
/* True while a full stomach is holding hunger off. */
bool game_saturated(const Game *g);
/* Whether a bar is as good as full. */
bool game_bar_full(float value);
/* Lies down to sleep, or sits to meditate, dropping whatever was queued. False
 * if the player is not standing on the ground, or asks to sleep with energy
 * full. Sleep ends by itself once energy is full; anything the player is told
 * to do ends either. While it lasts the ticks go faster. */
bool game_rest(Game *g, Rest kind);
void game_wake(Game *g);
/* Ticks per second as they are going now: the world's rate, sped up at rest. */
int game_tick_speed(const Game *g);
/* Where something part-way through a step of (dx, dy) is, relative to where
 * it set off. A diagonal step goes round the corner rather than through it:
 * `rise_first` picks up-or-down then across, otherwise across first. */
void game_step_offset(int dx, int dy, bool rise_first, float t, float *ox, float *oy);

/* Drops up to `count` items out of an inventory slot into the world, with
 * whatever is inside them, right where the player is. They are not picked
 * straight back up: only once the player has gone out of reach of them and
 * come back, or asks for them. Returns how many went. */
int game_discard(Game *g, SlotRef from, int count);

/* What a random event does when it lands on a tile; game_tick lands them. */
void game_random_event(Game *g, int x, int y);
/* Kills a tree from one block of its trunk upward, as cutting it there does:
 * the trunk above and the leaves that hung on it decay. */
void game_decay_tree(Game *g, int x, int y);

/* Leaves one of an item lying in a tile. */
void game_drop_item(Game *g, ItemId id, int x, int y);
/* The world's own random numbers, saved with it. */
uint32_t game_random(Game *g);

/* Hands the player items: into the inventory, or dropped at their feet if
 * there is no room. */
void game_give_items(Game *g, ItemId id, int count);

/* The bench or storage block at a position, whichever layer it is in, or
 * TILE_AIR if there is nothing there to open. */
TileType game_bench_at(const Game *g, int x, int y);

/* Fuel left in the open bench, in ticks, and how much it can hold; -1 if it
 * does not use fuel. A bench that uses fuel only crafts while it has some. */
int game_bench_fuel(const Game *g, int *capacity);
/* Fuel left in whatever block is at a position; 0 if there is none. */
int game_fuel_at(const Game *g, int x, int y);
/* Feeds one of an item to the open bench. False if the item is not fuel, none
 * is held, or the bench is full. */
bool game_add_fuel(Game *g, ItemId item);

/* The container of the storage block that is open, or 0 if what is open is
 * not one. Its slots are reached with SLOT_CONTAINER. */
uint16_t game_open_storage(const Game *g);
/* Closes the open bench and abandons the rest of its crafting run. */
void game_close_bench(Game *g);

void game_set_move_input(Game *g, int dx, int dy);
/* Drops the current and queued actions; a step in progress still finishes. */
void game_cancel_actions(Game *g);

/* True if a player standing at `feet` can mine `target`: the 10 tiles
 * surrounding the player's two blocks. Takes no account of the world wrapping. */
bool game_in_mine_range(TilePos feet, TilePos target);

/* Feet position in tile units, interpolated across the step in progress. */
void game_player_visual_pos(const Game *g, float *x, float *y);
/* True while the player is one tile tall, going through a gap one block high. */
bool game_player_crawling(const Game *g);
/* Side (-1 or +1) of the wall the player is hanging on, or 0 if not climbing. */
int game_player_grip_side(const Game *g);

#endif
