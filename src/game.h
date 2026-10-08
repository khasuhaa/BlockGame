#ifndef GAME_H
#define GAME_H

#include "craft.h"
#include "inventory.h"
#include "tree.h"
#include "water.h"
#include "world.h"

#define MAX_QUEUED_ACTIONS 128
#define MAX_DENIALS 32
#define MAX_MOVES 192
#define MAX_LIFT_STOPS 64 /* floors a lift offers each way from where it is boarded */
#define MAX_DROPS 8192
#define MAX_TICK_RATE 1000
#define MAX_TICKS_PER_UPDATE 200 /* so a slow frame cannot snowball */
#define MAX_STORAGES 1024
#define MAX_MOBS 256
#define MAX_VEHICLES 64 /* things on rails in a world */
#define VEHICLE_SEATS 2
#define CHARACTER_NAME_MAX 16
#define MAX_CHARACTERS 4
#define MAX_EFFECTS 16 /* things a character can be under the effect of at once */ /* characters a world has room for; the rules say how many it may have */
#define MAX_ATTACK_TRIES 32 /* times a hunt re-plans after its quarry has moved */
#define MAX_SEARCH 240000 /* poses a route search looks at before giving up */

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
    ACTION_ATTACK,  /* go after a creature, and kill it */
    ACTION_BUCKET,  /* go within reach of a tile, then scoop its water up or pour some out */
    ACTION_PAINT,   /* go within reach of a tile, then paint its block or backwall, or strip the paint off */
    ACTION_RELEASE, /* walk next to the target tile, then let out there the creature in the item held */
    ACTION_SLEEP,   /* go to the target tile - a bed - and lie down to sleep there */
    ACTION_CAST     /* get within range of the target tile, then cast there the spell in the item held */
} ActionType;

/* What can be done at an open bench or trade block. Its menu can be looked
 * at from anywhere, but each of these is done standing at it: asked for from
 * further off, it is queued, and the player walks there first. */
typedef enum {
    WORK_NONE,
    WORK_CRAFT,   /* arg: which of content.recipes; amount: how many */
    WORK_UPGRADE,
    WORK_FUEL,    /* arg: the item to burn */
    WORK_BUY,     /* arg: the item; amount: how many */
    WORK_SELL,
    WORK_CONVERT  /* arg: the coin given; amount: the coin wanted */
} Work;

/* What a hand does to a creature it is laid on (an ACTION_ATTACK's `arg`). */
enum {
    HAND_HIT = 0,
    HAND_SHEAR,
    HAND_MILK,
    HAND_CAGE
};

typedef struct {
    ActionType type;
    TilePos target;
    uint16_t item; /* ACTION_PLACE, ACTION_PLANT: the item being used up */
    bool wall;    /* ACTION_MINE: take the backwall, not a block */
    int drop;     /* ACTION_COLLECT, ACTION_ATTACK: which drop or creature, and the id it had
                     when queued */
    uint32_t drop_id;
    uint8_t tries; /* ACTION_ATTACK: how often the route has been planned again */
    /* ACTION_USE: something to do at the bench once there, if anything. */
    uint8_t work; /* a Work */
    int arg, amount;
} Action;

/* One voluntary move. Standing: walk (+-1,0), hop a one-block gap (+-2,0),
 * pull up onto a ledge (+-1,-1), go over an edge onto its wall (+-1,+1) or
 * jump to catch a wall (0,-1). Climbing: up or down the wall (0,+-1), hop to
 * the facing wall (+-n,0) or pull up onto the top (+-1,-1).
 * `climb` says whether the step ends hanging on a wall. Falling is never
 * planned; gravity is applied between steps. */
typedef struct {
    int8_t dx;
    int16_t dy; /* (more than a block or two only for a ride in a lift) */
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
    ACTIVITY_PLACING,
    ACTIVITY_ATTACKING /* swinging at a creature */
} Activity;

/* The player is 1 tile wide and 2 tall: feet at (x, y), head at (x, y-1). */
typedef struct {
    TilePos feet;
    bool climbing; /* hanging on a wall rather than standing */
    int facing;    /* -1 or +1: last horizontal direction moved */
    Activity activity;
    Step step;       /* the step in progress while ACTIVITY_STEPPING */
    bool step_hop;   /* that step is a hop, drawn as an arc */
    bool step_swim;  /* that step is through water: straight there, even on the slant */
    bool step_boat;  /* that step is in a boat, which goes along with it */
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
    float death;      /* the bar under health: what is left between no health and dead */
    int32_t respawn;  /* dead, with this many ticks to go before coming back; 0 for alive */
    int32_t poison;   /* ticks of being poisoned still to go */
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
    uint16_t item;
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
    int32_t state;  /* a MobState; for a creature that is milked and not one of the trees, ticks before it can be again */
    int32_t home_x; /* the column of its tree */
    int32_t timer;  /* ticks left of the chase, or before it will come down again; for a creature that
                       stings, before it will sting again; for one that is shorn, before it has grown back */
    int32_t hp;     /* what it can still take; 0 in a record from before creatures had any: full */
    int32_t size;   /* blocks long; 0 or 1 for the ordinary size */
    int32_t poison; /* poison in it still to do its work: see mob_poison */
} Mob;

typedef enum {
    MOB_WANDER, /* an ordinary creature, going where it likes */
    MOB_PERCH,  /* up its tree */
    MOB_HUNT,   /* down after the player */
    MOB_RETURN, /* on its way back to its tree */
    MOB_SEEK    /* without a tree, looking for one */
} MobState;

/* How far a character has got in cultivation (see dao.h). A mortal is at stage 0. */
typedef struct {
    int32_t stage;       /* 0 a mortal; 1 the first stage of the first realm; one past the last, an immortal */
    float qi;            /* what they hold, of what the stage holds */
    float toxin;         /* pill toxin: 0 none, 1 as much as there can be */
    uint8_t roots[DAO_ELEMENTS]; /* their spirit roots: how strong in each element, 0 for none */
    uint8_t meridians;   /* how many are open */
    uint8_t foundation;  /* how well the realm they are in was built: 0 flawed to 4 perfect */
    uint8_t rising;      /* the tribulation overhead is for a new realm, not one called down for its own sake */
    uint16_t manual;     /* the manual they cultivate by: an item, or 0 */
    uint16_t failed;     /* breakthroughs that did not take */
    int32_t bolts;       /* a tribulation: the bolts still to come, */
    int32_t bolt_wait;   /* the ticks before the next, */
    float bolt_share;    /* and the share of a health bar each is */
    uint64_t revived;    /* the tick they last survived a death at; 0 for never */
} Dao;

/* Something a character has taken that is still working on them: the item it was, and the ticks it has left
 * (less than nothing for one that does not wear off). */
typedef struct {
    uint16_t item;
    int32_t ticks;
} Effect;

/* What a block placed in the world keeps besides its tile: the contents of a
 * storage block such as a chest, the fuel left in a kiln or furnace. */
typedef struct {
    bool used;
    int x, y;
    uint16_t container; /* 0 if it stores nothing */
    int fuel;           /* ticks of burning left */
    /* Electricity: what a battery has in it, or a bench or a motor has had
     * sent to it and not yet used; and for a generator, what it made this
     * tick and how much of that has been taken. */
    int power;
    int16_t made, given;
    /* A machine: the recipe it is making, or has been set to make (which of
     * content.recipes, plus one; 0 for none), and how many ticks of work it
     * has done on the one in hand (0 for none in hand). */
    int16_t recipe, work;
} Storage;

/* One text for a sign: a single line. */
typedef struct SignText {
    bool used;
    char text[SIGN_TEXT_MAX + 1];
} SignText;

/* Electricity on its way along a wire from one thing to another. It moves a
 * block a tick: `moving` is how much is in each square of the way. */
#define MAX_WIRE_PATH 2048
#define MAX_POWER_FLOWS 256
typedef struct {
    bool active;
    int from_x, from_y, to_x, to_y;
    int rate;      /* units put in each tick; 0 for all of it at once */
    int remaining; /* units still to be put in; below 0 for as long as it is wanted */
    int length;    /* squares of wire between the two */
    int32_t cells[MAX_WIRE_PATH];  /* which squares, from the source's end */
    int32_t moving[MAX_WIRE_PATH];
} PowerFlow;

/* Everything that is one character's own: where they are and what they are
 * doing, what they carry, what they have queued, how they are. A world can
 * have more than one character. The game works on one of them at a time -
 * its state is in the Game itself, in the fields of the same names - and
 * the others wait here, each taking its turn (see game_use_character). */
typedef struct {
    char name[CHARACTER_NAME_MAX];
    Player player;
    ItemStack slots[INVENTORY_SLOTS];
    ItemStack armor[ARMOR_SLOTS];
    int sel_slot, sel_inner;
    int8_t choice[INVENTORY_SLOTS];
    bool has_current;
    Action current;
    Step *steps;
    int step_count, step_index;
    Action queue[MAX_QUEUED_ACTIONS];
    int queue_count;
    int move_dx, move_dy;
    Vitals vitals;
    bool bench_open;
    TilePos bench_pos;
    const Recipe *craft_recipe;
    int craft_total, craft_remaining;
    float craft_timer;
    int craft_power;
    bool upgrading;
    float upgrade_timer;
    bool menu_hold;
    float shield;
    float chill;
    int ride, ride_seat;
    Dao dao;
    Effect effects[MAX_EFFECTS];
    uint16_t relic_box;
} Character;

/* Something on the rails. (Saved as it is: add to the end.) */
typedef struct {
    uint8_t used;
    int8_t dir;        /* going left (-1) or right (1), or standing (0) */
    int8_t pull;       /* being moved this step, by itself or by what it is coupled to: which way */
    uint8_t has_dest;  /* it stops when its middle gets to `dest` */
    uint16_t item;     /* the item it is, which says what kind of vehicle that is */
    uint16_t fuel;     /* the container of what it burns, if it burns anything */
    uint16_t cargo;    /* and of what it carries */
    int8_t seats[VEHICLE_SEATS]; /* who is in each: a character plus one, or 0 */
    int32_t x, y;      /* the square of track under its middle */
    int32_t dest_x, dest_y;
    int32_t burn;      /* ticks of fire left from the last thing burnt */
    float step;        /* how far it has got toward the next square, 0 to 1 */
    float speed;       /* something worked by hand: how fast it is going the way it is going, in squares a second */
} Vehicle;

typedef struct {
    World world;
    Player player;
    Inventory inventory;
    Drop drops[MAX_DROPS];
    uint32_t next_drop_id;
    Storage storages[MAX_STORAGES];
    Mob mobs[MAX_MOBS];
    uint32_t next_mob_id;
    /* What is on the world's rails (see train.h). */
    Vehicle vehicles[MAX_VEHICLES];
    int ride, ride_seat;    /* the vehicle the live character is riding, plus one (0 for none), and which seat */
    int open_vehicle;       /* the vehicle whose slots are open, plus one */
    int open_vehicle_cargo; /* and whether it is what it carries (1) or what it burns (0) */
    Dao dao; /* how far the live character has got in cultivation */
    int lightning, lightning_x; /* a bolt has just struck: ticks it is still to be seen for, and the column. Not saved. */
    Effect effects[MAX_EFFECTS]; /* what the live character is under the effect of (see Effect) */
    /* Weather made to order: rain (1) or none (2) until a tick. And creatures held still: all within so far of a square, until a tick. Not saved. */
    int weather_made;
    uint64_t weather_until, held_until;
    TilePos held_at;
    int held_reach;
    uint16_t relic_box; /* the container the live character's relics are in; 0 until there are relics to have */
    float flail[MAX_CHARACTERS]; /* seconds each character has still to throw their arms about, having just been attacked */

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

    /* The characters of this world. `live` is the one whose state is in the
     * fields above just now; `controlled` is the one the player is playing,
     * which is the live one whenever nothing is in the middle of happening. */
    Character characters[MAX_CHARACTERS];
    int character_count, live, controlled;
    char name[CHARACTER_NAME_MAX]; /* the live character's */
    /* Whether this world's first character has been made yet: the one who is
     * given what a new world starts a player with. */
    bool started;

    /* Fog of war: how far each tile has been uncovered, 0 (never seen) to
     * 255. Once uncovered a tile stays so. */
    uint8_t *explored;
    bool fog; /* whether this world hides what has not been uncovered */
    int random_events; /* random blocks hit in each chunk every tick: a setting of the world */
    bool quick_craft;  /* a setting of the world: everything made takes one tick */
    /* Which items somebody in this world has had in their hands: the trade portal deals only in those. */
    uint8_t discovered[MAX_ITEMS];
    /* What is written on the world's signs: each text has a number, which a
     * sign that is up keeps with its square and one that is carried has as
     * its meter. Number 0 is no text. */
    struct SignText *signs;
    PowerFlow *flows;  /* electricity on its way along the wires (see power.h); not kept in a save */
    /* What the bench still has to be sent before it starts on what was asked for: an electric one waits for it. */
    int craft_power;
    /* Snow lying on each column, in eighths of a block. It is only looked at: nothing stands on it. */
    uint8_t *snow;

    /* The bench or storage block the player has open, and the crafting run
     * going on at it. */
    bool bench_open;
    TilePos bench_pos;
    const Recipe *craft_recipe;
    int craft_total, craft_remaining;
    float craft_timer;
    /* An upgrade of that bench under way, if it is one that takes time. */
    bool upgrading;
    float upgrade_timer;
    /* Storage networks: what each storage cell in the world has in it (a cell
     * item's meter says which), and the crafting jobs that are running. */
    struct CellStore *cells;
    struct NetJob *jobs;
    /* How far the weather has got to the character: 0 for not at all, 1 for an environment score of nothing. */
    float chill;
    /* A shield a spell has put round the character: so much of a health bar of harm it will still take for them. */
    float shield;
    /* What is open was opened by something queued, and is for the player to
     * close: nothing queued after it starts until they have. */
    bool menu_hold;

    /* The last shot loosed, for drawing: where from, where to, and how long ago. Not kept. */
    struct {
        float from_x, from_y, to_x, to_y, age;
        bool live;
    } shot;

    /* Scratch space for planning. */
    World sim;
    /* Where the character in hand last went from into a place that is only so wide: where they come back out. Not saved. */
    /* Signals: what every square with anything to do with them has this tick, and had last (see signal.h). Not saved. */
    uint8_t *signal, *signal_was;
    size_t signal_cells;
    int travels;      /* how many times anybody has gone from one place to another: the view watches it. Not saved. */
    TilePos gate_from;
    bool gate_from_set;
    int32_t *bfs_parent;
    uint32_t *bfs_seen; /* poses reached in search number bfs_gen */
    uint32_t bfs_gen;
    int32_t *bfs_queue;
    Step *bfs_step;
} Game;

bool game_init(Game *g, int width, int height, uint32_t seed, WorldKind kind);
/* The same, with what the world is to be made as: NULL for everything as usual. */
bool game_init_with(Game *g, int width, int height, uint32_t seed, WorldKind kind, const WorldOptions *options);

/* Characters. Each has a body, a bar of items, a queue of actions and needs
 * of their own, and all of them carry on with what they have been told to
 * do, whichever is being played. */
/* Makes character `c` the one the game is working on: its state changes
 * places with the live one's. For looking at or acting as one that is not
 * being played; put the played one back afterwards. */
void game_use_character(Game *g, int c);
/* Hands control to a character: it is the one the player now plays. */
void game_control(Game *g, int c);
/* Adds a character, standing at a place, with nothing. Returns its number,
 * or -1 if the world has all it may. */
int game_add_character(Game *g, TilePos at);
/* A world can be without any character at all: a new one is, until its
 * first is made, and so is one whose characters have all been removed. */
/* Takes every character away, as a world that has yet to have its first. */
void game_clear_characters(Game *g);
/* Makes a character at a portal the player has uncovered, with a name. The
 * first of a world needs nothing but that, and is given what a new player
 * starts with; after that they are made at the bench. Returns the
 * character's number, or -1 if that is no such portal or there is no room. */
int game_create_character(Game *g, TilePos portal, const char *name);
/* True if the block at a place is a portal a character can be made at or carried to. */
bool game_is_portal(const Game *g, int x, int y);
const char *game_character_name(const Game *g, int c);
TilePos game_character_pos(const Game *g, int c);

/* Dying. */
/* Hurts the live character by so much of a health bar. With health gone it
 * comes off the death bar; with that gone they are dead. */
void game_hurt(Game *g, float amount);
/* True if the live character is there and alive. */
bool game_alive(const Game *g);
bool game_character_dead(const Game *g, int c);
/* What it costs to bring a dead character back now: the full price right
 * after dying, less the more of the wait is done. -1 if they are not dead. */
int game_respawn_cost(const Game *g, int c);
/* Pays that and brings them back, at the world's spawn portal. */
bool game_respawn(Game *g, int c);
/* Takes a dead character out of the world for good, leaving all they
 * carried lying where they died. Its place can be filled by a new one. */
bool game_remove_character(Game *g, int c);

/* How a character is, whether or not it is the one being played. */
const Vitals *game_character_vitals(const Game *g, int c);
/* The character nearest a place. */
int game_nearest_character(const Game *g, int x, int y);
/* Works the inputs of recipes that differ from world to world out for this world. */
void game_roll_recipes(const Game *g);
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
/* Whether the character has room for something lying on the ground: a click on one they have not is a click on
 * whatever is behind it. */
bool game_can_collect(const Game *g, int drop);
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

/* Shows the menu of the bench or trade block at a position, from wherever the
 * player is. False for anything else, a chest included: that is walked to. */
bool game_open_bench(Game *g, int x, int y);
/* Whether the player is standing where the open bench can be worked. */
bool game_at_bench(const Game *g);
/* Does a piece of work at the open bench: at once if the player is at it,
 * otherwise by queueing the walk there with the work to follow. game_craft
 * and the rest below all go through this. False if it could not be done or
 * queued. */
bool game_bench_work(Game *g, Work work, int arg, int amount);
/* What it would cost in crystals to teleport to the open bench, or -1 if it
 * is not one that offers that. */
int game_teleport_cost(const Game *g);
/* Carries the player to the open bench for that many crystals. False if it
 * does not offer it, they cannot pay, they are already there, or there is no
 * room to stand. */
bool game_teleport(Game *g);
/* Dimensions. Whether the block in a square is a way to another place that works where it stands; going
 * through it (the place is made, the first time, and a way back put up where they come out); and making sure
 * the world has a dimension: the region it is, or -1. */
bool game_gate_open(const Game *g, int x, int y);
bool game_travel(Game *g, int x, int y);
int game_enter_dimension(Game *g, int dim);
/* Makes the game's own room for a world that has just been given more rows. False if there was not the memory. */
bool game_world_grew(Game *g);
/* What machinery does to the world (see signal.h). Digs the plain block in a square, leaving what it leaves;
 * puts down there a block out of what the block at (sx, sy) holds; takes everything plain within so many
 * blocks; moves whoever is in a square a step, if there is room for them where they would go; and says
 * whether anybody (or anything lying there, if asked) is in a square. */
bool game_break_block(Game *g, int x, int y);
bool game_deploy_block(Game *g, int sx, int sy, int x, int y);
void game_blast(Game *g, int x, int y, int radius);
bool game_shove(Game *g, int x, int y, int dx, int dy);
bool game_square_stood(const Game *g, int x, int y, bool items);
/* What a character is under the effect of: how many things, each the item it was and how many ticks it has left
 * (less than nothing for ever); and whether one is. */
int game_effects(const Game *g, Effect *out, int max);
bool game_has_effect(const Game *g, ItemId item);
/* Whether creatures within reach of a held square are being held still just now. */
bool game_mob_held(const Game *g, int x, int y);
/* Whether what is in hand can dig a block at all: some are too hard for a lesser tool. */
bool game_can_dig(const Game *g, TileType t);

/* Starts crafting `count` of a recipe at the open bench, one every
 * CRAFT_TIME. Anything that does not fit in the inventory is dropped. */
bool game_craft(Game *g, const Recipe *r, int count);
/* Seconds a recipe takes to make one of. */
float game_craft_time(const Recipe *r);
/* The same in this world, where a setting may make everything take one tick. */
float game_craft_seconds(const Game *g, const Recipe *r);
/* Hurrying what is being made, for crystals: what finishing all of it now
 * would cost (one for every so many seconds still to go), or -1 if there is
 * nothing to hurry; and doing it. */
/* Whether the character has work in hand at the open bench - something being
 * made or an upgrade under way, or on their way there to start one. It is
 * seen through: what is queued after it waits, and its menu stays up. */
bool game_bench_busy(const Game *g);
/* Whether what is queued is waiting: on work at a bench, or on a menu that a
 * queued action opened being closed. */
bool game_queue_held(const Game *g);
/* Gives up what is being made at the open bench, and any upgrade under way. The bench stays open. */
void game_craft_stop(Game *g);
int game_craft_rush_cost(const Game *g);
bool game_craft_rush(Game *g);
/* Notes everything the character is carrying as found in this world. */
void game_note_found(Game *g);
/* How far along the item being made now is, and the upgrade under way, each 0..1; -1 if there is none. */
float game_craft_progress(const Game *g);
float game_upgrade_progress(const Game *g);
/* Upgrades the open bench in place, paying its cost. False if it cannot be
 * upgraded or the cost is not held. One that takes time is started, and is
 * paid for and done when the time is up, the player still being there. */
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
/* What is left of the character's shield, as a share of a health bar; 0 for none. */
/* How many ticks an item burns for in the bench that is open: what the item says, or what the bench says of it. 0 if it does not burn there. */
int game_fuel_value(const Game *g, ItemId item);
/* What the character has on: how many times as fast it makes them walk (1 for no faster), and whether any of it keeps them from tiring, or from hungering. */
float game_walk_speed(const Game *g);
bool game_tireless(const Game *g);
bool game_sated(const Game *g);
float game_shield(const Game *g);
/* The same for any character of the world, by number. */
float game_character_shield(const Game *g, int c);
/* How many times faster work at the bench at a square goes for what stands near it; 1 for no faster. */
float game_craft_boost(const Game *g, int x, int y);
/* The bed at a square, if there is one there, in front or behind: the block
 * it is (its first half, for either half of it). TILE_AIR if there is none. */
TileType game_bed_at(const Game *g, int x, int y);
/* Queues going to the bed at a square and sleeping in it. False if there is no bed there. */
bool game_sleep_in(Game *g, int x, int y);
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

/* The year: which season it is (one of content.rules.seasons), which year
 * and which day of it (both from 1), and how many ticks into the day. */
int game_season(const Game *g);
void game_date(const Game *g, int *year, int *day, int *tick);
/* The environment score: 1 at its best, running down out of doors in rain or
 * cold and coming back under cover. */
float game_environment(const Game *g);
float game_character_environment(const Game *g, int c);
/* The least it can fall to in what the character has on: 0 with nothing on,
 * when the weather can kill; 1 when it cannot touch them. */
float game_environment_floor(const Game *g);
/* Whether the weather is getting at the character where they are. */
bool game_exposed(const Game *g);
/* Happiness: how the character is doing all told, 0 to 1 - or past 1, by the
 * rules' boost, when they are whole and energy, hunger and environment are
 * all kept past full. Short of that it is the average of how rested and how
 * well fed they are and their environment score, and never more than their
 * health. */
float game_happiness(const Game *g);
float game_character_happiness(const Game *g, int c);
/* Whether what they have on keeps their environment score past full. */
bool game_environment_saturated(const Game *g);
bool game_character_environment_saturated(const Game *g, int c);
/* How fast they walk and work, as a share of their usual speed: the less happy, the slower. */
float game_pace(const Game *g);
/* The loot pools a square is in: its biome's, its height's and every one that is everywhere. Fills `out` (room for
 * MAX_POOLS) if given; returns how many. */
int game_area_pools(const Game *g, int x, int y, int *out);
/* Something found there: one thing picked from all of those pools together, by weight. ITEM_NONE if there is nothing. */
ItemId game_area_find(Game *g, int x, int y);
/* What boons come to, in words: a line to each thing they do. Fills `lines` (each of BOON_LINE); returns how many. */
#define BOON_LINE 48
int game_boon_lines(const Boons *b, char lines[][BOON_LINE], int max);
/* The same for everything an item does for whoever has it on - its boons, and what armour does besides. */
int game_item_lines(ItemId item, char lines[][BOON_LINE], int max);
/* Whether the character carries something that reads these off a thing. */
bool game_identifies(const Game *g);
/* Relics: where the character carries them - a container of RELIC_SLOTS slots, made the first time it is asked for;
 * 0 if the game has no relics to carry. */
uint16_t game_relic_box(Game *g);
/* Everything the character has on, taken together: armour, relics, and what enough of a set gives besides.
 * (Speeds are multiplied together and are 1 for no difference; shares and damage are added.) */
Boons game_boons(const Game *g);
/* How many of a set the character has on. */
int game_set_pieces(const Game *g, int set);
/* Whether the character has something on that lets them fly, and whether they are in the air on it now. */
bool game_lifted(const Game *g);
bool game_player_flying(const Game *g);
/* How hard it is raining - or, in a season that snows, snowing - from 0 for
 * not at all to 1; it comes on and eases off at the ends of a spell. */
float game_rain(const Game *g);
/* What the weather does to one column: snow lies or melts, open water freezes, ice thaws. */
void game_weather_event(Game *g, int x, int y);
/* Snow lying on a column, in eighths of a block; the tops above the snow line always have the most there can be. */
int game_snow(const Game *g, int x);
/* The row of the highest block of a column that is open to the sky, where
 * snow lies and rain stops; -1 for none. `water` says whether water counts as ground. */
int game_sky_floor(const Game *g, int x, bool water);
/* The share of an attack that what the player is wearing stops, 0..1. */
float game_protection(const Game *g);
/* An attack on the player: as game_hurt, less what their armor stops. */
void game_attacked(Game *g, float amount);
/* How long the character has still to flail, having just been attacked: seconds, 0 for not. */
float game_flail(const Game *g);
/* What is written on the sign in a square: its text, or NULL for a blank one or no sign. */
const SignText *game_sign_at(const Game *g, int x, int y);
/* The text with a number (what a carried sign's meter reads), or NULL. */
const SignText *game_sign_text(const Game *g, int id);
/* Writes on the sign in a square - either of the two it takes up: one line, cut to what a sign holds. False if there is no sign there. */
bool game_sign_write(Game *g, int x, int y, const char *text);
/* How quick sleep is where the character is: more than 1 in a bed; and whether it can go on past a full bar there. */
float game_sleep_speed(const Game *g, bool *over);
/* Poisons the player for so many ticks: each takes a little health. A
 * second dose does not add to the first, it only makes it last as long. */
void game_poison(Game *g, int ticks);
/* How far off the player can hit from with what is in their hand: a bow's
 * range, or 0 for only what is in reach. */
int game_attack_range(const Game *g);
/* What a blow from the player takes off a creature: what is in their hand
 * if that is a weapon, a bare hand's worth otherwise. */
int game_attack_damage(const Game *g);
/* True while any part of the player is in water. */
bool game_player_in_water(const Game *g);
/* Leaves one of a sized item lying in a tile, of a given size. */
void game_drop_item_sized(Game *g, ItemId id, int x, int y, int size);
/* Leaves one of an item lying in a tile. */
void game_drop_item(Game *g, ItemId id, int x, int y);
/* The same for a whole stack, with whatever is inside it. */
void game_drop_stack(Game *g, ItemStack stack, int x, int y);
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
