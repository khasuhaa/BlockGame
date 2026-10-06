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
#define MAX_VEINS 48
#define MAX_TOOL_CLASSES 16
#define MAX_CATEGORIES 16
#define MAX_CURRENCIES 8
#define MAX_VEIN_HOSTS 4
#define MAX_VEIN_SIZE 64
#define MAX_TILE_TEXTURES 4
#define MAX_TREE_ROWS 12
#define MAX_SWELLS 4
#define MAX_START_ITEMS 8
#define RECIPE_MAX_INPUTS 3
#define MAX_TREE_SOILS 8

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
    ItemId id;
    int count;
} RecipeItem;

typedef struct {
    char name[CONTENT_NAME_LEN];
    char label[CONTENT_NAME_LEN]; /* shown to the player */
    bool solid;      /* in the foreground: blocks movement and can be stood on */
    int layer;       /* the layer it goes in when placed */
    bool bench;      /* a workbench: can be opened to craft */
    bool trade;      /* can be opened to buy and sell */
    bool removable;  /* can be mined */
    bool climbable;  /* can be held on to by a player in front of it */
    float mine_time; /* seconds to mine, with bare hands */
    int tool_class;  /* the kind of tool that mines it faster, or -1 */
    ItemId drop;     /* item left behind when mined */
    int log_of, leaves_of; /* tree this is the trunk / the leaves of, or -1 */

    /* Benches. Recipes belong to a kind of bench and may need a level of it;
     * a bench can be upgraded in place into another tile, usually the next
     * level of the same kind. */
    char bench_kind[CONTENT_NAME_LEN];
    int bench_level;
    char popup_name[CONTENT_NAME_LEN]; /* what "Open ..." and "Remove ..." call it */
    int storage_columns, storage_rows; /* grid of item slots it holds; 0 for none */
    int fuel_capacity; /* ticks of fuel it can hold; 0 if it needs none to work */
    TileType upgrade_to; /* TILE_AIR if it cannot be upgraded */
    RecipeItem upgrade_cost[RECIPE_MAX_INPUTS];
    int upgrade_cost_count;

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
    int container_slots; /* item slots it carries inside it; 0 for none */
    bool opens_in_bar;   /* those slots can be opened from the inventory bar */
    EquipSlot equip; /* armor slot it can be worn in, or EQUIP_NONE */
    TileType places; /* block it becomes when placed, or TILE_AIR */
    int fuel;        /* ticks it burns for as fuel; 0 if it is not fuel */
    int category;    /* where it is listed for trading, or -1 if it is not traded */
    char category_name[CONTENT_NAME_LEN];
    int price;       /* what one costs, in the smallest currency; 0 for the default */
    int tool_class;  /* the kind of tool it is, or -1 */
    float tool_speed; /* how many times faster it mines blocks of its class */
    int plants;      /* tree it grows into when planted, or -1 */
    char plants_name[CONTENT_NAME_LEN];
    char texture[CONTENT_PATH_LEN];
} ItemDef;

typedef struct {
    char bench[CONTENT_NAME_LEN]; /* the kind of bench that makes it */
    int level;                    /* lowest bench level that can */
    RecipeItem output;
    RecipeItem inputs[RECIPE_MAX_INPUTS];
    int input_count;
    int crystals; /* crystals it costs on top of the inputs */
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
    TileType soil[MAX_TREE_SOILS]; /* blocks it can be planted on */
    int soil_count;
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

typedef enum {
    VEIN_BLOB,  /* a clump */
    VEIN_STREAK /* a long thin line, like a scratch */
} VeinShape;

/* A deposit of one block inside others: flint in soil, ore in rock. */
typedef struct {
    char name[CONTENT_NAME_LEN];
    TileType tile;
    TileType hosts[MAX_VEIN_HOSTS]; /* blocks it may replace */
    int host_count;
    int min_size, max_size;   /* blocks in one vein */
    float per_100_columns;    /* how many veins to try for every 100 columns of world */
    int min_depth, max_depth; /* blocks below the surface a vein may start */
    bool buried;              /* never touches open air */
    VeinShape shape;
} VeinDef;

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

    float walk_time, fall_time, climb_time, hop_time, crawl_time; /* seconds per step */
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

/* Buying and selling, and the money it is done with. */
typedef struct {
    struct {
        char name[CONTENT_NAME_LEN];
        char label[CONTENT_NAME_LEN];
    } categories[MAX_CATEGORIES];
    int category_count;
    /* Coins, smallest first. Each is an item worth so many of the smallest. */
    struct {
        ItemId item;
        int value;
    } currencies[MAX_CURRENCIES];
    int currency_count;
    int default_price;
    /* Crystals are not an item: one number, shared by every world. */
    int start_crystals;
    char crystal_icon[CONTENT_PATH_LEN];
} TradeDef;

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
    VeinDef veins[MAX_VEINS];
    int vein_count;
    /* Kinds of tool, such as "shovel". There is no fixed list: a kind exists
     * because some block or item names it. */
    char tool_classes[MAX_TOOL_CLASSES][CONTENT_NAME_LEN];
    int tool_class_count;
    WorldGenDef worldgen;
    RulesDef rules;
    TradeDef trade;
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
int tool_class_id(const char *name);

/* Out-of-range ids give the definition of nothing. */
const TileDef *tile_def(TileType t);
const ItemDef *item_def(ItemId id);

#endif
