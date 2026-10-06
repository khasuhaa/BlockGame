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
#define MAX_DECOR 16
#define MAX_BIOMES 16
#define MAX_MOB_KINDS 16
#define MAX_MOB_DROPS 4
#define MAX_MOB_SPAWN_TILES 4
#define MAX_MOB_TREES 8
#define MAX_BIOME_TREES 8
#define MAX_BIOME_PLANTS 8
#define MAX_TILE_SOILS 4
#define MAX_EXTRA_DROPS 2
#define MAX_ROW_CELLS 4
#define MAX_TOOL_CLASSES 16
#define MAX_CATEGORIES 16
#define MAX_CURRENCIES 8
#define MAX_VEIN_HOSTS 4
#define MAX_VEIN_SIZE 64
#define MAX_TILE_TEXTURES 4
#define MAX_TILE_ALIASES 2
#define MAX_TILE_JOINS 4
#define MAX_TREE_ROWS 12
#define MAX_SWELLS 4
#define MAX_START_ITEMS 8
#define MAX_LOOT 32
#define MAX_MOB_FRAMES 4
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
    LAYER_WALL,       /* the backwall: what is left behind a block when it is removed */
    LAYER_COUNT
};
#define BLOCK_LAYERS 2 /* the layers that hold blocks, as opposed to backwall */

/* Armor slots, top to bottom. */
typedef enum {
    EQUIP_HEAD = 0,
    EQUIP_CHEST,
    EQUIP_LEGS,
    EQUIP_BOOTS,
    EQUIP_NONE
} EquipSlot;

#define MAX_ZOOM_LEVELS 16

typedef struct {
    ItemId id;
    int count;
} RecipeItem;

typedef struct {
    char name[CONTENT_NAME_LEN];
    char label[CONTENT_NAME_LEN]; /* shown to the player */
    bool solid;      /* in the foreground: blocks movement and can be stood on */
    int layer;       /* the layer it goes in when placed */
    int height;      /* tiles tall: it is placed, removed and upgraded as one column of itself */
    /* Names it used to have, or that other blocks now folded into it had:
     * an old save that knows one of these gets this block. */
    char aliases[MAX_TILE_ALIASES][CONTENT_NAME_LEN];
    int alias_count;
    bool bench;      /* a workbench: can be opened to craft */
    bool trade;      /* can be opened to buy and sell */
    bool removable;  /* can be mined */
    bool climbable;  /* can be held on to by a player in front of it */
    bool wall;       /* leaves a backwall of itself behind where it stands */
    int light;       /* light it gives off, 0..15 */
    bool light_needs_fuel; /* only while it has fuel burning */
    bool ore;        /* what it drops is multiplied by an ore-multiplying tool */
    int drop_count;  /* how many of its drop it leaves */
    RecipeItem extra_drops[MAX_EXTRA_DROPS]; /* other things it leaves as well */
    int extra_drop_count;

    /* Plants. One that grows is a chain of blocks, each turning into the
     * next when a random event lands on it. */
    TileType grows_into; /* TILE_AIR when full grown, or not a plant */
    TileType soil[MAX_TILE_SOILS]; /* blocks it can be sown on top of */
    int soil_count;
    bool needs_water;  /* only lives and grows under water */
    bool breaks_above; /* removing one removes every one of it stacked on top */

    /* An egg: a random event on it turns it into a young creature. */
    int hatches;      /* kind of creature, or -1 */
    char hatches_name[CONTENT_NAME_LEN];
    bool hatch_owned; /* what comes out belongs to the player, and so never wanders off for good */
    bool storage_shared; /* its slots are the one store shared by every such block, in every world */
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
    /* Used instead on the top and the bottom tile of a column of it. */
    char texture_top[CONTENT_PATH_LEN];
    char texture_base[CONTENT_PATH_LEN];
    /* Fire: frames shown one after another, instead of the texture above,
     * while the block has fuel burning. */
    char textures_lit[MAX_TILE_TEXTURES][CONTENT_PATH_LEN];
    int lit_count;
    float lit_fps;
    /* Blocks it joins up with when they are beside it, itself included if
     * listed: a cactus and its arms, say. One that joins is drawn from a
     * sheet of sixteen pictures, four to a row, one for every combination of
     * neighbours: picture number = 1 for one above + 2 right + 4 below + 8 left. */
    TileType joins[MAX_TILE_JOINS];
    int join_count;
    char texture_joined[CONTENT_PATH_LEN];
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
    float food;      /* share of the hunger bar eating one fills; 0 if it cannot be eaten */
    float food_health; /* share of the health bar it mends; below 0 for the usual share of `food` */
    float food_energy; /* share of the energy bar it gives back */
    int category;    /* where it is listed for trading, or -1 if it is not traded */
    char category_name[CONTENT_NAME_LEN];
    int price;       /* what one costs, in the smallest currency; 0 for the default */
    int tool_class;  /* the kind of tool it is, or -1 */
    float tool_speed; /* how many times faster it mines blocks of its class */
    int ore_multiplier; /* how many times over it makes ore blocks drop; 1 for no effect */
    int crystals;    /* crystals it turns into when picked up; such an item is never held */
    int plants;      /* tree it grows into when planted, or -1 */
    TileType sows;   /* plant block it puts down when planted, or TILE_AIR */
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
    /* Leaves at exact distances from the trunk (negative is left), for shapes
     * that are not a solid row: the arm of a cactus, say. */
    int cells[MAX_ROW_CELLS];
    int cell_count;
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
    bool connected;        /* a leaf only grows touching the trunk or another leaf: no gaps */
    bool needs_water;      /* only grows where the block above it is under water */
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

/* Something set into the caves: a gem on a wall, a crystal on the floor. */
typedef struct {
    char name[CONTENT_NAME_LEN];
    TileType tile;
    float per_100_columns;
    int min_depth, max_depth;
    bool on_floor; /* only where there is a block directly beneath */
} DecorDef;

/* A kind of country. Which one a column is comes from how hot and how dry it
 * is there, from whether it is under the sea, and from how near the sea it is. */
typedef struct {
    char name[CONTENT_NAME_LEN];
    char label[CONTENT_NAME_LEN]; /* shown to the player */
    bool has_climate;  /* chosen by heat and dryness, each -1..1 */
    float heat_min, heat_max, dry_min, dry_max;
    bool below_sea;    /* this is what land under sea level is */
    int near_sea;      /* this is the shore: it reaches this many columns from the sea */
    TileType surface, soil;
    float soil_depth;  /* a fixed depth of soil, or 0 for the usual rule */
    int tree_one_in, tree_spacing;
    struct {
        int tree, weight;
    } trees[MAX_BIOME_TREES];
    int tree_count;
    struct {
        TileType tile; /* its first stage; it is put down at any stage */
        int one_in;
    } plants[MAX_BIOME_PLANTS];
    int plant_count;
} BiomeDef;

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

    /* The sea: everything open to the sky below this height is under water. */
    bool has_sea;
    int sea_level;
    /* Climate: two smooth maps round the world, which pick the biome. */
    int heat_wavelength, dryness_wavelength;
    float climate_blend;       /* how far into a climate its border is still mixed */
    int climate_spawn_clearance; /* columns round the spawn kept as the first biome */

    /* Caves: tunnels that wander through the rock and nothing else. */
    float cave_per_100_columns;
    int cave_min_length, cave_max_length;
    float cave_min_radius, cave_max_radius;
    int cave_min_depth, cave_max_depth;

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
    float drop_lifetime;     /* seconds an item lies in the world before it is gone; 0 for ever */
    float denial_time;       /* how long a refused action's marker shows */

    /* Light, 0..15. Open sky is as bright as the sun or moon; light loses
     * some with every tile it travels, more through a block. */
    int light_sun, light_moon;
    int light_air_falloff, light_solid_falloff;
    int light_minimum; /* the least anything is ever lit, so the dark is not blind */
    int light_water_depth; /* blocks of water that cost one level of sky light */
    int water_color[4];    /* red, green, blue, alpha of the tint drawn over water */

    /* Fog of war: the world is dark until the player has been near it. */
    bool fog_enabled; /* whether a new world starts with it */
    int fog_radius;   /* blocks round the player that are uncovered */
    int fog_fade;     /* of which the outermost this many fade out toward the dark */
    /* The view: how many pixels a block is at each step of the zoom, from
     * closest to farthest, and the one a world opens at. Between map_from
     * and map_to pixels the view fades into the world map. */
    /* The player's needs. Each bar runs from 0 to 1. A time of 0 turns that
     * part off. */
    int max_tick_rate;      /* the fastest the tick rate can be set in the settings */
    int hunger_ticks;       /* ticks for a full hunger bar to run down */
    int energy_ticks;       /* and a full energy bar, awake */
    int sleep_ticks;        /* ticks asleep that fill the energy bar */
    int health_regen_ticks; /* ticks for health to mend from nothing, when not starving */
    int starve_ticks;       /* ticks for health to run down with hunger at nothing */
    float starve_floor;     /* starving never takes health below this */
    float food_health;      /* share of a food's hunger that it mends health by */
    int saturation_ticks;   /* ticks hunger is held off for, per bar of food eaten when full */
    float full_margin;      /* a bar this near the top counts as full */
    int rest_speed;         /* times faster the ticks go while the player sleeps or meditates */
    int meditation_ticks;   /* ticks of meditating for each thing it brings */
    struct {
        ItemId item;
        int weight;
    } loot[MAX_LOOT];       /* what meditating can bring, and how often each */
    int loot_count;
    float zoom[MAX_ZOOM_LEVELS];
    int zoom_count, zoom_start;
    float map_from, map_to;
} RulesDef;

/* A creature. */
typedef struct {
    char name[CONTENT_NAME_LEN];
    char label[CONTENT_NAME_LEN];
    char texture[CONTENT_PATH_LEN], texture_baby[CONTENT_PATH_LEN]; /* drawn facing right */
    /* Frames of walking, shown in turn while it is on the move. */
    char walk[MAX_MOB_FRAMES][CONTENT_PATH_LEN], walk_baby[MAX_MOB_FRAMES][CONTENT_PATH_LEN];
    int walk_count, walk_baby_count;
    float hop;          /* how high it bobs with each stride, in tiles */
    int step_ticks;     /* ticks it takes to walk one block */
    float move_chance;  /* chance each tick, standing still, that it sets off */
    float turn_chance;  /* chance that it sets off the other way */
    TileType spawn_above[MAX_MOB_SPAWN_TILES]; /* ground it turns up on, in the open air */
    int spawn_above_count;
    float spawn_chance; /* chance that a random event on such a spot brings one */
    int cap;            /* no more of this kind appear while this many are near; 0 for no limit of its own */
    /* A creature that lives in trees. It turns up at the top of one and sits
     * there. When the player comes by underneath it runs down, bites them
     * and runs back up; if its tree goes, it looks for another. */
    bool perches;
    int perch_trees[MAX_MOB_TREES]; /* the kinds of tree it will live in; any, if none are named */
    int perch_tree_count;
    int ambush_range;   /* columns either side of its tree that count as underneath */
    int ambush_depth;   /* how far below it the player can be and still be gone for */
    float bite;         /* share of the health bar a bite takes */
    int chase_ticks;    /* how long it keeps after the player before giving up */
    int bite_rest;      /* ticks after a bite, or giving up, before it will come down again */
    int seek_radius;    /* columns it can see a tree from, and will chase the player over */
    TileType lays;      /* what it leaves where it stands now and then, or TILE_AIR */
    float lay_chance;   /* chance each tick */
    int baby_ticks;     /* how long a young one takes to grow up */
    struct {
        ItemId item;
        int min, max;
    } drops[MAX_MOB_DROPS]; /* what a grown one leaves when killed */
    int drop_count;
} MobDef;

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
    DecorDef decor[MAX_DECOR];
    int decor_count;
    BiomeDef biomes[MAX_BIOMES]; /* the first is the one a column is unless another claims it */
    int biome_count;
    MobDef mobs[MAX_MOB_KINDS];
    int mob_count;
    int mob_spawn_radius; /* creatures appear, and wild ones last, only this near the player */
    int mob_cap;          /* no more appear while this many are that near */
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
/* As tile_id, but also by any name the block used to go by. */
TileType tile_id_or_alias(const char *name);
ItemId item_id(const char *name);
int tree_id(const char *name);
int tool_class_id(const char *name);
int mob_id(const char *name);

/* Out-of-range ids give the definition of nothing. */
const TileDef *tile_def(TileType t);
const ItemDef *item_def(ItemId id);

#endif
