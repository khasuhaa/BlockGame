#ifndef CONTENT_H
#define CONTENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Everything the game is made of - blocks, items, recipes, trees, the shape
 * of the land, the pace of the rules - is read from JSON files in a data
 * directory. The engine knows none of it by name: it only knows what the
 * fields mean. Adding or changing content means editing data, not code. */

#define CONTENT_NAME_LEN 32
#define CONTENT_PATH_LEN 64
#define MAX_TILES 256 /* ids are stored in a byte */
#define MAX_ITEMS 256
#define MAX_RECIPES 256
#define MAX_TREES 32
#define MAX_FEATURES 16
#define MAX_TILE_TEXTURES 4
#define MAX_TREE_ROWS 12
#define MAX_SWELLS 4
#define MAX_START_ITEMS 8
#define RECIPE_MAX_INPUTS 3

/* Ids are positions in the tables below, fixed only for one run of the game.
 * 0 is always "nothing": empty space, or no item. */
typedef int TileType;
typedef int ItemId;
#define TILE_AIR 0
#define ITEM_NONE 0

enum {
    LAYER_FOREGROUND, /* blocks the player collides with */
    LAYER_BACKGROUND, /* blocks behind the player */
    LAYER_COUNT
};

/* Armor slots, top to bottom. */
typedef enum {
    EQUIP_HEAD = 0,
    EQUIP_CHEST,
    EQUIP_LEGS,
    EQUIP_BOOTS,
    EQUIP_NONE
} EquipSlot;

typedef struct {
    char name[CONTENT_NAME_LEN];
    bool solid;      /* in the foreground: blocks movement and can be stood on */
    bool bench;      /* a workbench: can be opened to craft */
    bool removable;  /* can be mined */
    bool climbable;  /* can be held on to by a player in front of it */
    float mine_time; /* seconds to mine */
    ItemId drop;     /* item left behind when mined */
    int log_of, leaves_of; /* tree this is the trunk / the leaves of, or -1 */

    /* Looks. Textures are 32 pixels to the tile; a larger one overhangs its
     * tile, centred and resting on the tile's bottom edge. */
    char textures[MAX_TILE_TEXTURES][CONTENT_PATH_LEN]; /* one is picked per position */
    int texture_count;
    int shade;   /* how much brightness varies from tile to tile, 0..255 */
    bool front;  /* drawn after the other tiles, so its overhang shows */
    float pulse; /* how strongly its brightness breathes, 0..1 */
} TileDef;

typedef struct {
    char name[CONTENT_NAME_LEN];
    char label[CONTENT_NAME_LEN]; /* shown to the player */
    int max_stack;
    bool container;  /* carries item slots of its own */
    EquipSlot equip; /* armor slot it can be worn in, or EQUIP_NONE */
    TileType places; /* block it becomes when placed, or TILE_AIR */
    char texture[CONTENT_PATH_LEN];
} ItemDef;

typedef struct {
    ItemId id;
    int count;
} RecipeItem;

typedef struct {
    TileType bench; /* the kind of bench that makes it */
    RecipeItem output;
    RecipeItem inputs[RECIPE_MAX_INPUTS];
    int input_count;
} Recipe;

/* One row of a full-grown tree's leaves. */
typedef struct {
    int offset;      /* rows below the top of the trunk; -1 is the row above it */
    int left, right; /* how far the leaves reach either side of the trunk */
    bool center;     /* also a leaf in the trunk's own column (rows above the trunk) */
} TreeRow;

typedef struct {
    char name[CONTENT_NAME_LEN];
    TileType log, leaves;
    ItemId fruit;
    int fruit_ticks; /* how long a fruit lasts */
    int layer;       /* which layer of the world it grows in */
    int max_height;  /* trunk blocks when full grown */
    int weight;      /* how common it is relative to other trees */
    bool mirror_by_column; /* swap left and right on odd columns */
    TreeRow rows[MAX_TREE_ROWS];
    int row_count;
} TreeDef;

typedef enum {
    PROFILE_PEAK, /* rounded foot, pointed top */
    PROFILE_BOWL, /* one smooth curve */
    PROFILE_BASIN /* sheer sides and a wide flat middle */
} FeatureProfile;

/* A mountain, a valley, or anything else that rises from or sinks into the plains. */
typedef struct {
    char name[CONTENT_NAME_LEN];
    int direction; /* +1 rises, -1 sinks */
    int min_size, max_size; /* blocks from the plains to its peak or floor */
    float width_per_size;   /* half width = size * this + width_base */
    int width_base;
    FeatureProfile profile;
    float steepness; /* PROFILE_BASIN: how much of the width is flat floor */
    float shoulders; /* how lumpy the slopes are part-way up, 0..1 */
    int ledge_min, ledge_max; /* columns per ledge; 0 for a continuous slope */
    int max_step;    /* most neighbouring columns may differ by; 0 for no limit */
    float jitter;    /* random roughness on the slopes, in blocks */
} FeatureDef;

typedef struct {
    float amplitude;
    int wavelength;
} Swell;

typedef struct {
    int width, height;

    int plains_level; /* middle height of the plains */
    Swell swells[MAX_SWELLS];
    int swell_count;
    int gap_min, gap_max; /* plains between two features */

    TileType surface, soil, rock;
    float soil_depth, soil_variation;
    float bare_at; /* how far into a feature the soil has thinned to nothing, 0..1 */

    int spawn_min, spawn_max; /* columns the spawn may be in */
    int first_feature_at;
    int flat_radius, ease_radius; /* level ground round the spawn, and the slope down to it */
    TileType portal_base, portal_body, portal_top;
    int portal_height;

    int tree_one_in, tree_spacing, tree_spawn_clearance;
    float tree_grown_chance; /* share of generated trees that start full grown */
    int tree_young_steps;    /* most growth steps given to the rest */
} WorldGenDef;

typedef struct {
    int tick_rate; /* ticks per second in a new world */
    int day_ticks, night_ticks, twilight_ticks;
    int chunk_width; /* columns per chunk */
    int random_events_per_chunk; /* random blocks hit in each chunk every tick */

    float walk_time, fall_time, climb_time, hop_time; /* seconds per step */
    float hop_height;  /* peak of a hop's arc, in tiles */
    int max_wall_hop;  /* columns a wall-to-wall hop can cover */
    float place_time;
    float craft_time;  /* per item crafted */
    RecipeItem start_items[MAX_START_ITEMS];
    int start_item_count;

    float drop_fall_speed;   /* tiles per second */
    float drop_pickup_delay; /* how long a fresh drop lies before it can be collected */
    float drop_collect_time; /* flight into the player once collected */
    float denial_time;       /* how long a refused action's marker shows */
} RulesDef;

typedef struct {
    TileDef tiles[MAX_TILES];
    int tile_count;
    ItemDef items[MAX_ITEMS];
    int item_count;
    Recipe recipes[MAX_RECIPES];
    int recipe_count;
    TreeDef trees[MAX_TREES];
    int tree_count;
    FeatureDef features[MAX_FEATURES];
    int feature_count;
    WorldGenDef worldgen;
    RulesDef rules;
} Content;

extern Content content;

/* Empties the tables, leaving only air and the empty item. */
void content_reset(void);
/* Reads every data file found in a directory on top of what is already
 * loaded: a definition whose name is new is added, one whose name exists
 * replaces it. Loading a second directory is how a mod would work. On failure
 * returns false with a description in `error`. */
bool content_load(const char *dir, char *error, size_t error_size);

/* Look up by name; -1 if there is no such thing. */
TileType tile_id(const char *name);
ItemId item_id(const char *name);
int tree_id(const char *name);

/* Out-of-range ids give the definition of nothing. */
const TileDef *tile_def(TileType t);
const ItemDef *item_def(ItemId id);

#endif
