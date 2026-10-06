#ifndef GAME_H
#define GAME_H

#include "inventory.h"
#include "world.h"

#define MAX_QUEUED_ACTIONS 32
#define MAX_DENIALS 8
#define MAX_WALL_HOP 2 /* columns a wall-to-wall hop can cover */
#define MAX_MOVES 8
#define MAX_DROPS 256

#define WALK_TIME 0.12f
#define FALL_TIME 0.07f
#define CLIMB_TIME 0.16f
#define HOP_TIME 0.2f
#define HOP_HEIGHT 0.35f /* peak of a hop's arc, in tiles */
#define MINE_TIME 0.35f
#define DENIAL_TIME 0.6f
#define DROP_FALL_SPEED 12.0f   /* tiles per second */
#define DROP_PICKUP_DELAY 0.45f /* how long a fresh drop lies before it can be collected */
#define DROP_COLLECT_TIME 0.15f /* flight into the player once collected */

typedef struct {
    int x, y;
} TilePos;

typedef enum {
    ACTION_MOVE, /* put one of the player's two blocks on the target tile */
    ACTION_MINE  /* walk next to the target tile, then mine it */
} ActionType;

typedef struct {
    ActionType type;
    TilePos target;
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
    ACTIVITY_MINING
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

/* Fading marker shown on a tile whose action was refused. */
typedef struct {
    TilePos pos;
    float ttl;
} Denial;

/* An item lying in the world, waiting to be walked up to. */
typedef struct {
    bool active;
    uint8_t item;
    int x;         /* tile column */
    float y;       /* tile row; fractional while falling */
    float age;
    float collect; /* 0 while lying; otherwise 0..1 progress flying to the player */
} Drop;

typedef struct {
    World world;
    Player player;
    Inventory inventory;
    Drop drops[MAX_DROPS];

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

    /* Scratch space for planning. */
    World sim;
    int32_t *bfs_parent;
    int32_t *bfs_queue;
    Step *bfs_step;
} Game;

bool game_init(Game *g, int width, int height, uint32_t seed);
void game_free(Game *g);
void game_update(Game *g, float dt);

/* Queues the action a click on this tile stands for (move for air, mine for
 * ground), judged against the world as it will be once everything already
 * queued has run. Returns false, and shows a denial, if it is unattainable. */
bool game_click_tile(Game *g, int tx, int ty);

void game_set_move_input(Game *g, int dx, int dy);
/* Drops the current and queued actions; a step in progress still finishes. */
void game_cancel_actions(Game *g);

/* True if a player standing at `feet` can mine `target`: the 10 tiles
 * surrounding the player's two blocks. */
bool game_in_mine_range(TilePos feet, TilePos target);

/* Feet position in tile units, interpolated across the step in progress. */
void game_player_visual_pos(const Game *g, float *x, float *y);
/* Side (-1 or +1) of the wall the player is hanging on, or 0 if not climbing. */
int game_player_grip_side(const Game *g);

#endif
