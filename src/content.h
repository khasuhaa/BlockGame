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
#define MAX_TILES 4096 /* ids are stored in two bytes: 65,535 is as far as this can ever go */
#define MAX_ITEMS 8192 /* ids are stored in two bytes */
#define MAX_RECIPES 4096
#define MAX_TINTS 250
#define MAX_TREES 128
#define MAX_FEATURES 64
#define MAX_VEINS 192
#define MAX_DECOR 64
#define MAX_POLES 16
#define MAX_DIMENSIONS 8
#define MAX_REALMS 8
#define MAX_DAO_STAGES 96
#define DAO_ELEMENTS 5
#define DAO_MERIDIANS 12
#define MAX_STRUCTURES 128
#define MAX_SEAS 16
#define MAX_SEA_DRIFT 64
#define MAX_SEA_ALONGSIDE 32
/* What a block adds to the ship it is part of ("ship" on a block): what the hull will bear and what is aboard, sail
 * and engine, and the best or the worst of the rest. */
enum { SHIP_BURDEN, SHIP_WEIGHT, SHIP_SAIL, SHIP_THRUST, SHIP_STRENGTH, SHIP_REACH, SHIP_DEPTH, SHIP_CABLE, SHIP_LIGHT, SHIP_PUMP, SHIP_GUNS, SHIP_STATS };
/* What a sea may ask of a ship ("needs" on a sea). */
enum { NEED_KEEL, NEED_WAY, NEED_FREEBOARD, NEED_STRENGTH, NEED_CABLE, NEED_DEPTH, NEED_LIGHT, NEED_GUNS, NEED_REACH, SEA_NEEDS };
#define STRUCTURE_MAX_WIDTH 32
#define STRUCTURE_MAX_HEIGHT 20
#define MAX_VILLAGE_KINDS 24
#define MAX_VILLAGE_HOUSES 16
#define MAX_BIOME_SPIKES 8
#define MAX_GEODES 4
#define MAX_STRATA 4
#define MAX_CAVE_BIOMES 16
#define MAX_CAVE_GROWTH 8 /* kinds of thing on the floor, or hanging from the roof, of one kind of cave */
#define MAX_BIOMES 64
#define MAX_SETS 64         /* kinds of set that worn things belong to */
#define MAX_SET_TIERS 4
#define MAX_POOLS 96        /* places with finds of their own */
#define MAX_POOL_LOOT 64
#define MAX_POOL_BIOMES 16
#define RELIC_SLOTS 4
#define MAX_MOB_KINDS 1024
#define MAX_MOB_DROPS 16
#define MAX_MOB_SPAWN_TILES 16
#define MAX_MOB_TREES 32
#define MAX_MOB_NEXT 16
#define MAX_BIOME_TREES 32
#define MAX_BIOME_PLANTS 48
#define MAX_TILE_SOILS 16
#define MAX_EXTRA_DROPS 8
#define MAX_ROW_CELLS 16
#define MAX_TOOL_CLASSES 64
#define MAX_CATEGORIES 64
#define MAX_CATEGORY_NAMES 8
#define MAX_TRADE_TIERS 16
#define MAX_CURRENCIES 32
#define MAX_VEIN_HOSTS 16
#define MAX_VEIN_SIZE 256
#define MAX_TILE_TEXTURES 16
#define MAX_TILE_ALIASES 8
#define MAX_TILE_JOINS 16
#define MAX_TILE_LOOT 32
#define MAX_ITEM_SIZES 32
#define MAX_LOOT_CHOICES 24
#define MAX_TREE_ROWS 48
#define MAX_SWELLS 16
#define MAX_START_ITEMS 32
#define MAX_LOOT 128
#define MAX_MOB_FRAMES 16
#define RECIPE_MAX_INPUTS 20
#define MAX_TREE_SOILS 32

/* Ids are positions in the tables below, fixed only for one run of the game.
 * 0 is always "nothing": empty space, or no item. */
enum { MIMIC_NONE = 0, MIMIC_UPRIGHT, MIMIC_FLAT };
enum { POWER_NEEDS_NOTHING = 0, POWER_NEEDS_SUN, POWER_NEEDS_FUEL, POWER_NEEDS_SKY, POWER_NEEDS_HEAT };
/* What something in the background is fixed to (see world_mount). */
/* What a block is in a storage network. */
enum {
    NET_NONE = 0,
    NET_CABLE,       /* joins things up */
    NET_CONTROLLER,  /* the heart of one: it takes the electricity (its "draw"), and there is one network to each */
    NET_DRIVE,       /* holds storage cells in its slots */
    NET_TERMINAL,    /* where what is stored is got at */
    NET_IMPORT,      /* takes things out of whatever holds them beside it, into the network */
    NET_EXPORT,      /* puts the kind of thing in its slot out of the network into whatever holds things beside it */
    NET_STORAGE_BUS, /* makes whatever holds things beside it part of what the network stores */
    NET_ACCESS,      /* a point the network can be reached from at a distance (its "network_range") */
    NET_CPU,         /* runs one crafting job at a time */
    NET_COPROCESSOR, /* makes every job go faster */
    NET_ASSEMBLER,   /* teaches the network the recipes of the benches beside it */
    NET_BRIDGE,      /* joined to every other bridge in the world, however far off */
    NET_PLANE_BREAK, /* breaks the blocks beside it into the network */
    NET_PLANE_PLACE, /* puts the kind of block in its slot out of the network into the open squares beside it */
    NET_ROLE_COUNT
};
enum { MOUNT_NONE = 0, MOUNT_WALL, MOUNT_FLOOR, MOUNT_CEILING, MOUNT_LEFT, MOUNT_RIGHT };

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

#define MAX_ZOOM_LEVELS 64
#define MAX_RECIPE_CHOICES 64
#define MAX_ISLAND_TREES 48
#define MAX_CELLS 1024      /* storage cells with anything in them that a world can have */
#define CELL_TYPES 63      /* kinds of thing one cell holds */
#define MAX_ELEMENTS 64 /* kinds of magic there can be */
#define MAX_WORLD_CHOICES 32
#define MAX_SIGN_TEXTS 2048 /* different texts a world's signs can carry */
#define SIGN_TEXT_MAX 14 /* letters a sign holds: one line, across its two blocks */
#define MAX_SEASONS 4
#define MAX_MOB_BIOMES 32

/* Two-tone things - a flower with a body of one colour and tips of another -
 * carry both colours as one number: a meter on the item, a number kept with
 * the block. Each colour is one of 216: six levels each of red, green, blue. */
/* A painted thing keeps which colour in the low byte of its meter and how many times it has been used above that. */
#define PAINT_USED 256
#define TONE_LEVELS 6
#define TONE_COLORS (TONE_LEVELS * TONE_LEVELS * TONE_LEVELS)
#define TONE_MAX (TONE_COLORS * TONE_COLORS)
#define MAX_TONE_COLORS 8
/* The number for a pair of colours (never 0, which is "none given"), and back. */
int tone_pack(int body, int tip);
void tone_unpack(int tones, int *body, int *tip);
/* The colour nearest to a red, green and blue of 0..255; and what a colour is in those. */
int tone_nearest(int red, int green, int blue);
void tone_rgb(int color, unsigned char rgb[3]);

typedef struct {
    ItemId id;
    int count;
    bool full; /* each has to be one whose meter reads full - a bucket of water - and is left empty, not used up */
} RecipeItem;

/* One thing a storage block may be found holding: so many of an item, or one
 * picked from several by weight. */
typedef struct {
    ItemId item; /* ITEM_NONE when it is one of the choices instead */
    int min, max;
    struct {
        ItemId item;
        int weight;
    } choices[MAX_LOOT_CHOICES];
    int choice_count;
    bool area; /* it is whatever is found where the block stands: picked from the loot pools of that place */
} LootDef;

/* What a thing does for whoever has it on - worn as armour, or carried as a relic. Each is nothing unless given. */
typedef struct {
    float walk, mine, craft; /* so many times as fast on foot, at digging, at a bench: 0 for no difference */
    float regen, lung;       /* so many times as fast at mending, as long a breath */
    float appetite, stamina; /* so many times the pace hunger and tiredness come on at: less than 1 is slower */
    float armor, warmth;     /* a share more of each attack stopped, of the environment score kept */
    float luck;              /* the chance that a block dug leaves one more of what it leaves */
    int strength, thorns;    /* damage more to every blow; damage done to whatever bites or stings them */
    /* Cultivation: qi is gathered so many times as fast (0 for no difference); so much is added to the chance of
     * a breakthrough; and such a share of every bolt of a tribulation is turned. */
    float qi, breakthrough, ward;
    /* Each of these is on if it is more than nothing: a drop of any height is a step; it is never darker to them
     * than this (0..15); creatures that hunt do not see them; things lying within so many squares come to them;
     * what burns does not burn them; the air holds them up. */
    float glide, sight, unseen, magnet, fireproof, flight;
} Boons;

/* A set: things that go together. With enough of one on, there is something more besides. */
typedef struct {
    char name[CONTENT_NAME_LEN];
    char label[CONTENT_NAME_LEN];
    struct {
        int pieces;
        Boons boons;
    } tiers[MAX_SET_TIERS];
    int tier_count;
} SetDef;

/* Somewhere with finds of its own: so many biomes (any, if none are named) between two heights. A block whose
 * loot is "area" holds something from every pool its place is in, taken together. */
typedef struct {
    char name[CONTENT_NAME_LEN];
    char label[CONTENT_NAME_LEN];
    char biome_names[MAX_POOL_BIOMES][CONTENT_NAME_LEN]; /* as the data names them: one the game has not got is no matter */
    int biome_name_count;
    int biomes[MAX_POOL_BIOMES];
    int biome_count;
    bool any_biome;
    int y_min, y_max; /* heights, as the readout gives them */
    struct {
        ItemId item;
        int weight;
    } loot[MAX_POOL_LOOT];
    int loot_count;
} PoolDef;

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
    bool trade_free; /* at this one everything there is can be had for nothing, found or not */
    int trade_level; /* it deals in nothing of a higher tier than this; 0 for any */
    bool removable;  /* can be mined */
    bool climbable;  /* can be held on to by a player in front of it */
    bool wall;       /* leaves a backwall of itself behind where it stands */
    int light;       /* light it gives off, 0..15 */
    bool light_needs_fuel; /* only while it has fuel burning */
    bool ore;        /* what it drops is multiplied by an ore-multiplying tool */
    int drop_count;  /* how many of its drop it leaves */
    RecipeItem extra_drops[MAX_EXTRA_DROPS]; /* other things it leaves as well */
    float extra_chance[MAX_EXTRA_DROPS];     /* the chance of each of those; usually 1 */
    int extra_drop_count;

    /* Plants. One that grows is a chain of blocks, each turning into the
     * next when a random event lands on it. */
    TileType grows_into; /* TILE_AIR when full grown, or not a plant */
    TileType soil[MAX_TILE_SOILS]; /* blocks it can be sown on top of */
    int soil_count;
    bool needs_water;  /* only lives and grows under water */
    bool breaks_above; /* removing one removes every one of it stacked on top */
    bool breaks_below; /* and the same downward, for something that hangs */
    /* Steps: not solid - it is walked through like anything in the
     * background - but it can be stood on top of, and walked up onto from beside it. */
    bool steps;
    /* A door: solid to everything else - water, light, creatures, and it can
     * be stood on - but a character's body goes through it. */
    bool passable;
    /* Work at a bench within so many blocks of this goes so many times faster; 0 for something that does no such thing. */
    float refines;     /* "quickens": {"refines"}: what is made at a skilled bench within its radius has this much more chance of coming out double */
    int comprehension; /* "comprehension": which craft is learned by making things at it (see DAO_CRAFTS), -1 for none */
    float bestows_qi;  /* "bestows_qi": used, it fills so much of the stage's measure; "needs_stage": by somebody that far along */
    int tile_needs_stage;
    float craft_speed;
    int craft_radius;
    /* A block that takes on the look of what it stands between: the same
     * block above and below it (MIMIC_UPRIGHT) or either side of it
     * (MIMIC_FLAT). Others like it in between are looked through. It takes
     * the look when it is put up and keeps it (the block it looks like is the
     * number kept with its square). */
    int mimics;
    /* A tall block in the background whose lowest tile is solid all the
     * same: something to stand on, and in the way. */
    bool solid_foot;
    /* A lamp that throws its light one way: so bright at the lamp, spreading
     * in a quarter circle up (-1) or down (+1) from it. */
    int beam_dir, beam_light;
    /* Building pieces of the background hold on to each other: one that
     * attaches can be put up beside another that does, with no block or backwall to hold it. */
    bool attaches;
    bool floats;    /* it can be put up with nothing at all to hold it */
    bool shows_contents; /* what is kept in it is drawn on it, small: a shelf */
    float fertile;     /* a plant growing on top of it grows this many times as fast; 0 for no faster */
    /* A window: put up on a backwall, it is a hole in it - what is behind the
     * wall shows through - and, unless its glass is dark, lets the daylight in as an open square would. */
    bool window, window_dark;
    bool see_through;  /* though it is solid, the backwall behind it is drawn: it is not quite opaque */
    bool burns;        /* something that sets fire to things takes it down in a moment */
    bool afloat;       /* it can only be put down on water: on the top of it */
    /* Track: things that run on rails go along it, from one square of it to the next beside it, or beside and one up or down. */
    bool rail;
    /* What kind of thing it is to look at, where the data says ("model"): 0 for unsaid, then block, box, frame,
     * body, panel, cross - see world3d.h, whose kinds these are, in the same order. */
    int8_t model;
    char texture_ramp[CONTENT_PATH_LEN]; /* how it looks where it climbs to the next piece, drawn rising to the right */
    /* A way to another place: used, it takes whoever uses it to the dimension it names - or, from there, back
     * to where the world began. In the world's first place it only works so far above the ground the world
     * began on, or so near the bottom of the world (0 for anywhere). */
    char leads_to[CONTENT_NAME_LEN];
    int leads_dim;     /* which of content.dimensions that is; -1 for none */
    int opens_above, opens_below;
    /* One more level of a kind of bench (or of the blocks to trade at), put on after the highest there is,
     * whatever mod that came from: that one is raised into this for `extend_cost`. Its level is worked out. */
    bool extends;
    RecipeItem extend_cost[RECIPE_MAX_INPUTS];
    int extend_cost_count, extend_crystals;
    /* Signals. A signal is a strength, 0 to 15, that some blocks make, some carry and some act on (see signal.h).
     * - `signal_wire` carries it, a little weaker with every square.
     * - `signal_out` is what it makes all the time; `signal_clock` makes the most there is for so many ticks and
     *   then nothing for as many; `signal_daylight` makes it by day.
     * - A block with `signal_in` reads the squares on those sides of it and makes a signal of its own from them,
     *   toward `signal_toward` (every way but its inputs, if that is 0,0): the most there is if any input is on
     *   (all of them, for `signal_and`) - or, for `signal_invert`, if none is. `signal_delay` is how many ticks
     *   its inputs must have stood before it follows them (never fewer than one).
     * - Anything else is powered while a signal reaches the square it is in or one beside it: it turns into
     *   `powered_becomes` when it gets one, `unpowered_becomes` when it loses it, and `pulse_becomes` each time
     *   one arrives. */
    bool signal_wire, signal_daylight, signal_invert, signal_and;
    int signal_out, signal_clock, signal_delay;
    int8_t signal_in[3][2], signal_toward[2];
    int signal_in_count;
    TileType powered_becomes, unpowered_becomes, pulse_becomes;
    /* Using it - a click, with nothing in hand that digs it - turns it into another block; one that reverts
     * turns into another by itself after so many ticks; one that is stood on turns into `stood_becomes` while
     * anybody (or, for `stood_by_items`, anything lying there) is in its square, and `clear_becomes` when not. */
    TileType use_becomes, reverts_to, stood_becomes, clear_becomes;
    /* A thing with a front: the block it is when turned to face the next way round. Something that turns things
     * - a wrench - in hand, a click on it turns it. */
    TileType turns_to;
    bool is_piston_head; /* (worked out: some piston has this for its head, and pushes it out the way `piston_dir` says) */
    int revert_ticks;
    bool stood_by_items;
    /* A piston: powered, it pushes the blocks in front of it a square along (`piston_dir`) and puts its head
     * there; unpowered, the head comes back - and a sticky one brings the block that was against it. `sticky`
     * is a block that takes its neighbours with it when it is pushed; `immovable` is one nothing can push. */
    int8_t piston_dir[2];
    TileType piston_head;
    bool piston, piston_out, piston_sticky, sticky, immovable;
    /* Things done to the square in front when a signal arrives: the block there is dug (`breaks`); a block out
     * of what this holds is put there (`deploys`); so many eighths of water are poured there (`spout`, or taken,
     * if less than nothing) for as long as the signal lasts; everything within `blast` blocks goes, this included. */
    bool breaks, deploys;
    int8_t act_dir[2];
    int spout, blast;
    /* A moving walkway: whoever stands on it is carried a square that way (-1 left, 1 right) every so many ticks. */
    int walkway, walk_ticks;
    bool device;       /* (worked out: it has some part in any of that) */
    /* Qi. A block may be a source of it: everywhere within `qi_reach` of it is so much richer. A plant that
     * `needs_qi` grows only where there is that much. And a block may be the heart of a FORMATION: lit - it
     * burns fuel - with `formation_flags` set out round it in its pattern, every one the same distance off
     * (between the least and the most), everything inside that distance has its qi raised, its plants hurried,
     * whoever is in it given `aura`, and the creatures in it hurt so much a second; and using one that
     * `formation_gate`s takes whoever uses it to the nearest other that is lit. */
    float qi;
    int qi_reach;
    float needs_qi;
    /* "grow_one_in": it grows on only one time in so many that its turn comes (0 or 1: every time). "insight": what
     * using it gives somebody who cultivates (with "use_becomes", once). */
    int grow_one_in, insight;
    /* A dungeon: "dungeon_gate" is where one is chosen and gone into (its menu is its own; `gate_beside`, if given, is a
     * kind of bench that must stand within `gate_reach` of it, whose level says how far down the list it may go);
     * "loot_forge" is where what is found there is upgraded and sold; "dungeon_door" is shut until nothing hostile is
     * left on the near side of it; "dungeon_spawn" is a mark that becomes one of the dungeon's creatures (1) or its
     * boss (2) when somebody comes near; "dungeon_exit" is the way out. */
    bool dungeon_gate, loot_forge, dungeon_door, dungeon_exit;
    /* Ships (a world that is "adrift"). "ship": what this block adds to the ship it is part of (see SHIP_BURDEN).
     * "keel": with one of these aboard the ship may be so long, so high above the water and so deep under it; the
     * highest level aboard counts. "catches": standing in the sea, it takes what drifts into its square and keeps
     * it. "helm": where a course is set (its menu is its own). "trawl": every so many ticks under way it brings
     * up something from the bed of the sea the ship is in. */
    int ship[SHIP_STATS];
    int keel_level, keel_long, keel_above, keel_below;
    bool catches, helm;
    int trawl;
    /* "holed": what this piece of hull becomes when something bites through it - a block the sea comes in by.
     * "needs_under": it is put down only on one of these (a forge on a hearth). */
    TileType holed;
    TileType needs_under[8];
    int needs_under_count;
    int dungeon_spawn, gate_reach;
    char gate_beside[CONTENT_NAME_LEN];
    bool of_tree;    /* (worked out) part of a tree: its trunk or its leaves, or what either decays into */
    bool buried_treasure; /* "buried_treasure": a treasure map may be of where one of these lies */
    /* Rocketry. "rocket": a part of one - the fuel it holds, the thrust it gives, how likely it is to work, whether it
     * saves the crew when one does not. "pad": what a rocket stands on. "launch": on a way to another place - it is gone
     * through only from on top of a rocket with so much fuel and thrust. "air": lit, it fills the sealed room it is in
     * with air. "needs_air": it grows only in air. */
    int rocket_fuel, rocket_thrust, launch_fuel, launch_thrust;
    float rocket_reliability;
    bool rocket_escape, pad, gives_air, needs_air;
    bool formation, formation_gate, formation_bars, formation_wanders;
    TileType formation_flags;
    int formation_pattern, formation_min, formation_max; /* 0 a cross: four, level and plumb; 1 a square: four, at the corners; 2 a ring: all eight */
    float formation_qi, formation_grows, formation_harms;
    Boons aura;
    int village;       /* it is the heart of a village: everywhere within so many blocks of it is in that village; 0 for not */
    int hardness;      /* nothing with less `power` than this digs it; 0 for anything */
    float harm;        /* the share of a health bar it takes, each second, from whoever stands on it or against it */
    bool needs_tool;   /* it is only taken up with its tool in hand: otherwise a click on it is a click on the square */
    bool on_floor;     /* it can only be put down on a block: one directly beneath */
    /* A station: where a train sent that way stops, and what it is called is what is written on it. */
    bool station;
    TileType lays;     /* putting it up lays this under it, a piece each side as well, and taking it down takes those up */
    bool written;      /* it has words on it: the number kept with it says which of the world's texts */
    /* Two blocks wide: this one is put down with `pair` to the right of it, on
     * level ground under both, and the two come up as one. (`pair_of` is worked out: the block this is the right half of.) */
    TileType pair, pair_of;
    bool pair_floats; /* its two halves need no ground under them: it is held up some other way, like anything else */
    /* A bed: sleeping at it is this many times as quick, and in a soft one goes on past a full bar. */
    float sleep_speed;
    bool sleep_over;
    /* Electricity. A generator makes so much a tick while it has what it
     * needs; a battery holds so much; a wire carries it, a block a tick; an
     * electric bench takes so much for each second of what it makes, before
     * it starts; a motor takes so much every tick, and while it gets it, runs
     * the lift whose shaft it is in. */
    int gen_power, gen_needs; /* a POWER_NEEDS_* */
    int battery;
    bool wire;
    int electric;   /* units for each second of crafting; 0 for a bench that needs none */
    int net_role;    /* a NET_* */
    int net_range;   /* NET_ACCESS: how many blocks off it can be reached from */
    /* Industry. A block that is mined and is still there, to be mined again. */
    bool endless;
    /* A conveyor: so many items a tick go along it. And an extractor: it takes
     * items out of whatever holds any beside it and sends them along the
     * conveyors beside it, to the nearest thing that will have them; the item
     * in its one slot, if there is one, is the only kind it takes. */
    int conveyor;
    bool extractor;
    /* A machine: what it takes of electricity each tick it is working. */
    int draw;
    /* A miner: beside a block that is `endless`, it takes one of what that drops every so many ticks of work. */
    int miner_ticks;
    /* A machine that makes things by itself. Its first `automat_inputs` slots
     * are what goes in and the one after them is what comes out. It makes
     * the recipes of one kind of bench (`automat_kind`), choosing by what is
     * put in it - or, with no kind of its own, one recipe chosen for it from
     * those of the benches beside it. So many times the usual speed. */
    bool automat;
    char automat_kind[CONTENT_NAME_LEN];
    int automat_inputs;
    float automat_speed;
    /* A store that conveyors fill only with the kinds of thing already in its first so many slots, if any are; 0 for anything. */
    int sorts;
    /* A generator that burns only these, each for so many ticks, in place of whatever burns. */
    struct {
        ItemId item;
        int ticks;
    } fuels[4];
    int fuel_count;
    int motor_power;
    bool shaft;
    bool supports;  /* though it is in the background, things can be stood on top of it as on a block */
    bool hangs;     /* it can only be put up under a block: hung from its underside */
    /* How it looks put up with no backwall behind it: standing on what is
     * under it, hanging from what is over it, or fixed to what is beside it
     * (drawn for something to its left; mirrored for the right). Empty for no different. */
    char texture_mount[3][CONTENT_PATH_LEN];
    /* A plant that ends up two blocks tall. Its last low stage grows into
     * its foot and puts `grows_top` on that; the foot is `topped_by` the
     * same, which goes when the foot does; and taking the top alone leaves
     * the block under it as the top's `cut_to`: the stage before. */
    TileType grows_top, topped_by, cut_to;
    /* Two-tone: every one of these in the world has two colours of its own,
     * which what it drops and what it grows into carry on. */
    bool two_tone;
    char tone_textures[2][CONTENT_PATH_LEN]; /* drawn over it in the body colour and the tip colour; either may be empty */
    unsigned char tone_colors[MAX_TONE_COLORS][3]; /* the colours wild ones come in, body and tips alike */
    int tone_color_count;

    /* An egg: a random event on it turns it into a young creature. */
    int hatches;      /* kind of creature, or -1 */
    char hatches_name[CONTENT_NAME_LEN];
    bool hatch_owned; /* what comes out belongs to the player, and so never wanders off for good */
    /* Decay. The trunk and leaves of a tree turn into `decays_into` when the
     * tree is cut through beneath them. A block that `vanishes` is taken away
     * by the first random event to land on it. */
    int drop_meter;       /* what the meter of the item it leaves reads: the size of a trophy, say; 0 for as new */
    TileType decays_into; /* TILE_AIR if it does not decay */
    bool vanishes;
    float drop_chance; /* chance it leaves its drop when it goes, by whatever means; usually 1 */
    /* What one that the world was generated with holds when it is first
     * opened or dug up. One the player put down holds only what they put in. */
    LootDef loot[MAX_TILE_LOOT];
    int loot_count;
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
    float upgrade_time; /* seconds the upgrade takes; 0 for at once */
    int upgrade_crystals; /* and crystals it takes besides */

    /* Looks. Textures are 32 pixels to the tile; a larger one overhangs its
     * tile, centred and resting on the tile's bottom edge. */
    char textures[MAX_TILE_TEXTURES][CONTENT_PATH_LEN]; /* one is picked per position */
    int texture_count;
    /* How it looks in each season, for one that changes with them: the same
     * number of pictures as above, season by season. Empty for a season it looks as usual in. */
    char season_textures[MAX_SEASONS][MAX_TILE_TEXTURES][CONTENT_PATH_LEN];
    int season_texture_count[MAX_SEASONS];
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
    TileType places_on_block; /* the block it becomes instead when it is put down on top of a block, or TILE_AIR */
    int fuel;        /* ticks it burns for as fuel; 0 if it is not fuel */
    float food;      /* share of the hunger bar eating one fills; 0 if it cannot be eaten */
    float food_health; /* share of the health bar it mends; below 0 for the usual share of `food` */
    float food_energy; /* share of the energy bar it gives back */
    bool energy_over;  /* what it gives can take the energy bar past full */
    /* Cooked food, eaten on a full stomach, keeps the hunger bar full for a while; anything else only fills it.
     * Unless the data says, food is cooked if some recipe makes it. */
    bool cooked;
    int8_t cooked_said; /* what the data said of that: 1, 0, or -1 for nothing */
    float food_oxygen; /* share of the oxygen bar it gives back */
    /* A meter: a number each single item carries, shown as a bar under it -
     * how much water a bucket holds, how much use a tool has left. Items of
     * a kind stack whatever their meters read; it is the one on top of the
     * stack that is used. */
    int meter_max;     /* the most it reads; 0 if the item has no meter */
    int meter_start;   /* what a new one reads */
    bool holds_water;  /* the meter is eighths of water, scooped up and poured out */
    bool sized;        /* the meter is how big the thing is: a trophy from a creature of that size */
    bool tinted;       /* the meter is which colour it is: one of content.tints, plus one */
    bool two_tone;     /* the meter is a pair of colours (see tone_pack); ones that differ do not stack together */
    char tone_textures[2][CONTENT_PATH_LEN]; /* drawn over it in each of the two colours */
    /* Crossing: planted where `crosses_with` already grows, it turns that
     * into `crosses_into`, with colours bred from the two of them. */
    TileType crosses_with, crosses_into;
    bool shears;       /* used on a creature that can be shorn, it shears it and does it no harm */
    bool milks;        /* used on a creature that gives milk, it milks it */
    bool ignites;      /* used on something that burns, it takes it down at once, and is used up */
    bool written;      /* the meter is which of the world's texts is written on it; 0 for a blank one */
    bool cages_kept;   /* "cages_kept": it is not used up by taking a creature */
    int feeds;         /* "feeds": given to a creature that is theirs, it grows */
    bool natal;        /* "natal": worn, it gives more the further its wearer has got past the stage it needs */
    float flight_qi;   /* "flight_qi": what holding its wearer up costs them a tick, off the ground */
    int technique_element; /* "technique": {"element"}: 0 to 4, or -1 */
    int hook;          /* "hook": in hand, something adrift this near is hauled in by a click on it */
    bool cast_blink;   /* "cast": {"blink"}: the caster is there */
    bool cages;        /* used on a creature of the land, it takes it up: the item that creature is when caged */
    int releases;      /* the kind of creature that is in it, to be let out where it is put down; -1 for none */
    /* Paint and what takes it off. A thing that paints colours the block or
     * backwall it is used on with the colour it is (see `tinted`); one that
     * strips takes the colour off. Either is good for so many uses. */
    bool paints, strips;
    int uses;
    TileType places_sized[MAX_ITEM_SIZES]; /* for a sized item, the block each size is put down as */
    float armor;       /* worn, the share of an attack it stops; the pieces worn add up */
    int damage;        /* what a blow with it in hand takes off a creature; 0 for no better than a bare hand */
    int range;         /* how many blocks off it can hit from: a bow; 0 for at arm's length */
    int poison;        /* what it does over time besides: this much more, a point at a time */
    int poison_every;  /* ticks between those points */
    int category;    /* where it is listed for trading, or -1 if it is not traded */
    char category_name[CONTENT_NAME_LEN];
    int tier;        /* the tier the data gives it, or 0 for none given */
    int trade_tier;  /* how far a trade block must have been raised to deal in it: 1 and up */
    int price;       /* what one costs, in the smallest currency; 0 for the default */
    int tool_class;  /* the kind of tool it is, or -1 */
    float tool_speed; /* how many times faster it mines blocks of its class */
    int ore_multiplier; /* how many times over it makes ore blocks drop; 1 for no effect */
    int crystals;    /* crystals it turns into when picked up; such an item is never held */
    /* Something that casts a spell when it is used, so many times ("uses") before it is spent. A spell is of an
     * element, and does any of these: hurts the creature it is cast at and those within `splash` blocks of it
     * (`damage` to any that can be hurt, `bane` to one that is weak to its element); leaves what it kills cooked;
     * puts so many eighths of water in a square; shields the one who casts it against so much of a health bar of
     * harm; breaks every plain block within so many blocks of a square; brings on sunrise or moonrise. */
    bool casts;
    int cast_element;      /* which of content.elements; -1 for none */
    int cast_damage, cast_bane, cast_splash;
    bool cast_cooks;
    int cast_water, cast_dig;
    float cast_shield;
    int cast_time;         /* -1 for none; 0 for sunrise, 1 for moonrise */
    int cell_bytes;        /* a storage cell: how much it holds, in bytes (8 things to a byte, and a little for each kind); 0 for anything else */
    bool remote;           /* in hand, it opens the storage network whose access point is in range */
    char worn[CONTENT_PATH_LEN]; /* how it looks on: a skin laid out like the character's own, clear where it does not cover; empty for something that does not show */
    /* Worn: walking goes so many times as fast (0 or 1 for no faster); energy does not run down; hunger does not run down. */
    float walk_speed;
    bool tireless, sated;
    bool shelters;         /* worn, the weather is nothing to them: their environment score is kept past full */
    /* A relic: it goes in a relic slot, and does what its boons say while it is there. `set` is the set it is one
     * of (-1 for none), as armour may be too. */
    bool relic;
    Boons boons;
    int set;
    char set_name[CONTENT_NAME_LEN];
    bool reveals_all;      /* carried, everything there is counts as found: a trade block deals in all of it */
    bool identifies;       /* carried, what a thing does for whoever has it on can be read off it */
    int power;             /* a tool: what `hardness` of block it can dig */
    bool glides;           /* worn, a drop of any height is a step like another: they come down slowly */
    float cooling;         /* worn, how much of the environment score it keeps where it is hot: a share, 0 to 1 */
    /* Cultivation (see DaoDef). Taken, it gives so much qi, leaves so much pill toxin, opens the next meridian,
     * gives the character other spirit roots, or calls a tribulation down on them. A manual is studied, not
     * eaten: it becomes the method they cultivate by - qi gathered so many times as fast, most of all by roots
     * in its element (-1 for none). A technique is a spell that costs qi, not uses, and wants a stage reached.
     * And what is worn that needs a stage does nothing for anybody short of it. */
    float qi_gain, toxin;
    bool opens_meridian, washes_roots, calls_tribulation;
    bool manual;
    float manual_rate;
    int manual_element;
    bool technique;
    float technique_qi;
    int technique_stage, needs_stage;
    /* "technique": {"insight"}: the insight it takes to use. "tames": used on a creature whose "tame" is no more, it is theirs. */
    int technique_insight, tames;
    int insight_gain; /* "insight": taken, it gives so much */
    /* "rolled": one of these is not like the next. Its meter says how its numbers fell (see game_roll in game.h):
     * `roll_power` and `roll_health` are what it has, give or take a tenth; `roll_ups` how many times it can be
     * upgraded, give or take a tenth; and each upgrade adds `roll_gain` of power and `roll_health_gain` of health.
     * "grade": 1 to 6, common to ultimate - a colour wherever it is named. "class": 1 a warrior's, 2 a mage's. */
    /* A spell that is never used up: "cooldown" in its "cast" is the seconds before it can be cast again, and
     * "scales" how many times the caster's best weapon of its class (and what they have on) it does, in place of a
     * damage of its own. */
    float cast_cooldown, cast_scales;
    bool rolled;
    long long roll_power, roll_ups, roll_health; /* (they run past what a plain number holds) */
    int grade, loot_class;
    double roll_gain, roll_health_gain;
    bool boards;      /* "boards": used on a ship that can be boarded, it is */
    bool treasure_map; /* "treasure_map": in hand, it shows the lie of the land about something buried (which, its meter says) */
    int meridian;     /* "meridian": which one it opens, from 1 - and only when those before it are open (0: whichever is next) */
    /* Something taken - drunk, eaten - that does something for a while: these boons, for so many ticks (less
     * than nothing for ever after: a thing that can be taken only once by anybody). Taken again while it lasts,
     * it lasts from then. It may also cure poison, take them back to where the world begins, and leave something
     * behind in their hands - the bottle it was in. */
    Boons effect;
    int effect_ticks;
    bool cures, recalls;
    ItemId leaves;
    /* More that a spell may do where it lands: grow every plant within so many squares a stage; turn the water
     * within so many to ice; take the water within so many away; hold every creature within its splash still
     * for so many ticks; bring on rain (1) or clear weather (2) for so many ticks; and leave a block there. */
    int cast_grow, cast_freeze, cast_drain, cast_hold, cast_weather, cast_weather_ticks;
    TileType cast_places;
    bool turns;            /* in hand, a click on a thing with a front turns it to face another way */
    bool speciality;       /* it is sold only in villages, and only in the few of them that happen to deal in it */
    bool pack;             /* worn, it is a pack on the back as well: a box there, drawn from the top right of its picture */
    bool lifts;            /* worn, it lets them fly: the air holds them up wherever they go in it */
    /* Something that runs on rails: put down on them, it is not a block but a vehicle (see train.h). */
    struct {
        bool is;
        int width;       /* squares of track it stands on: 1 or 3 */
        bool engine;     /* it goes by itself, burning what is put in it */
        bool hand;       /* it goes as whoever is on it says */
        int cargo_slots; /* what it carries besides */
        int seats;       /* places to ride */
        float speed;     /* squares a second: an engine's pace, or the most something worked by hand can be got up to */
        /* Something worked by hand goes by what pushes it, all in squares a second, each second: */
        float accel;     /* what whoever is working it adds on the level */
        float climb;     /* and what they add going up, which is less */
        float gravity;   /* what a slope takes off going up it, and adds going down */
        float drag;      /* what the rails take off all the time */
        float brake;     /* what stopping it takes off */
        char texture[CONTENT_PATH_LEN];
    } vehicle;
    float warmth;          /* worn, how much of the environment score it keeps whatever the weather: a share, 0 to 1 */
    ItemId cooks_into;     /* what it is when a spell that cooks has killed what dropped it; ITEM_NONE for no different */
    bool becomes_player; /* made at a bench, it is not an item at all but another character to play */
    int plants;      /* tree it grows into when planted, or -1 */
    TileType sows;   /* plant block it puts down when planted, or TILE_AIR */
    char plants_name[CONTENT_NAME_LEN];
    char texture[CONTENT_PATH_LEN];
} ItemDef;

typedef struct {
    char bench[CONTENT_NAME_LEN]; /* the kind of bench that makes it */
    char at[CONTENT_NAME_LEN];    /* or the block that does, by name: the kind and the level are then that block's */
    int level;                    /* lowest bench level that can */
    RecipeItem output;
    RecipeItem inputs[RECIPE_MAX_INPUTS];
    int input_count;
    int crystals; /* crystals it costs on top of the inputs */
    int characters; /* it can be made only while the world has just this many characters; 0 for any number */
    float time;   /* seconds to make one; 0 for the usual */
    int tint;     /* which of content.tints what it makes is coloured, plus one; 0 for none */
    /* Inputs that differ from world to world: so many items in all, shared
     * out among these kinds by the world's seed. They are worked out into
     * `inputs` when a world is opened. */
    ItemId random_items[RECIPE_MAX_INPUTS];
    int random_item_count, random_total;
    /* What it makes, when that is one of several things by chance: each with its weight. (`output` is the first of them.) */
    struct {
        ItemId item;
        int weight;
    } one_of[MAX_RECIPE_CHOICES];
    int one_of_count;
    ItemId formula; /* "formula": it can be made only in a world where this has been found */
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
    VEIN_STREAK, /* a long thin line, like a scratch */
    VEIN_SCAR    /* a long gash several blocks thick, cut across whatever it may replace */
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
    int min_thick, max_thick; /* VEIN_SCAR: how thick at its middle; its size is its length */
    char dimension_name[CONTENT_NAME_LEN]; /* as a biome's: there, depths are rows below the top of the place */
    int dimension;
    /* Now and then a block of it is something else instead: one in so many. TILE_AIR for never. */
    TileType rare;
    int rare_one_in;
} VeinDef;

/* Something set into the caves: a gem on a wall, a crystal on the floor. */
typedef struct {
    char name[CONTENT_NAME_LEN];
    TileType tile;
    float per_100_columns;
    int min_depth, max_depth;
    bool on_floor; /* only where there is a block directly beneath */
    char dimension_name[CONTENT_NAME_LEN];
    int dimension;
    /* Something that hangs: it starts under a block and reaches down this many of itself. */
    int hang_min, hang_max;
} DecorDef;

/* A kind of cave: a stretch of the underground, between two depths, where
 * the caves are lined with a rock of their own - the rock round the open
 * space, and the wall behind it - and have their own things growing on the
 * floor and hanging from the roof. Which stretches are which is settled by
 * the world's seed: `share` of the columns, in runs about `wavelength` long. */
typedef struct {
    char name[CONTENT_NAME_LEN];
    char label[CONTENT_NAME_LEN];
    int min_depth, max_depth; /* blocks below the surface */
    int wavelength;
    float share;
    TileType lining;          /* TILE_AIR to leave the rock as it is */
    struct {
        TileType tile;
        int one_in;
        int hang_min, hang_max; /* (the roof only) how many of itself it reaches down */
    } floor[MAX_CAVE_GROWTH], roof[MAX_CAVE_GROWTH];
    int floor_count, roof_count;
} CaveBiomeDef;

/* A kind of country. Which one a column is comes from how hot and how dry it
 * is there, from whether it is under the sea, and from how near the sea it is. */
typedef struct {
    char name[CONTENT_NAME_LEN];
    char label[CONTENT_NAME_LEN]; /* shown to the player */
    bool has_climate;  /* chosen by heat and dryness, each -1..1 */
    float heat_min, heat_max, dry_min, dry_max;
    bool below_sea;    /* this is what land under sea level is */
    int near_sea;      /* this is the shore: it reaches this many columns from the sea */
    /* Something buried here and there at the bottom of its soil, on the rock. */
    TileType buried;
    int buried_one_in; /* one column in this many has it */
    /* What lies under its soil in place of the rock: so many blocks of it,
     * thinning out into the rock below and toward the biome's edges. */
    TileType under;
    int under_depth;
    /* Where that rock lies, the lowest layer of the ground over it is partly
     * another block: so much of it, scattered through. */
    TileType seam;
    float seam_share;
    TileType surface, soil;
    float soil_depth;  /* a fixed depth of soil, or 0 for the usual rule */
    bool cold;         /* out of doors here wears the environment score down whatever the season */
    float qi;          /* how rich in qi it is, beside the usual: 0 for no different */
    bool hot;          /* here wears it down indoors and out, and it is what cools that keeps it, not what warms */
    char dimension_name[CONTENT_NAME_LEN]; /* the dimension it is country of; empty for the world as it began */
    int dimension;     /* which of content.dimensions; -1 for none */
    /* In a dimension: things that stand up out of its ground, or hang from whatever is over it - a column of a
     * block so many high, on one spot in so many. An arch is two of them with a beam across their tops. */
    struct {
        TileType tile;
        int one_in, min, max;
        bool hang, arch;
    } spikes[MAX_BIOME_SPIKES];
    int spike_count;
    /* And something left lying on its ground, here and there: a chest with things in it, say. */
    TileType treasure;
    int treasure_one_in;
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

/* Something built: a picture of blocks, set down whole when a world is made. Each square of it may have a
 * block in front, something behind, and a backwall - or be left as the world had it. */
typedef struct {
    char name[CONTENT_NAME_LEN];
    int width, height;
    struct {
        TileType front, back, wall;
        bool keep; /* nothing of the world's is changed here */
    } cells[STRUCTURE_MAX_HEIGHT][STRUCTURE_MAX_WIDTH];
} StructureDef;

/* A sea: one of the waters a ship adrift may be in. What it asks of her before she can go there; what drifts by
 * in it (by day, by night or both), and how much; what lies on its bed, by how deep; what comes alongside, how
 * often, and what cable and reach each wants; and how it looks. */
typedef struct {
    char name[CONTENT_NAME_LEN], label[CONTENT_NAME_LEN];
    int needs[SEA_NEEDS];
    int passage_ticks;
    float drift_rate; /* things in a thousand ticks, lying still */
    struct {
        ItemId item;
        int weight, when; /* 0 at any time, 1 by day, 2 by night */
    } drift[MAX_SEA_DRIFT];
    int drift_count;
    struct {
        ItemId item;
        int weight, depth;
    } bed[MAX_SEA_DRIFT];
    int bed_count;
    struct {
        char structure[CONTENT_NAME_LEN], label[CONTENT_NAME_LEN];
        int weight, cable, reach, stay, sunk; /* (sunk: how many of its bottom rows are under water) */
    } alongside[MAX_SEA_ALONGSIDE];
    int alongside_count, alongside_every;
    unsigned char sky[3], water[4];
    bool has_sky, has_water, dark;
    float swell;
} SeaDef;

enum { VILLAGE_GROUND, VILLAGE_CAVE, VILLAGE_ISLAND };

/* A kind of village: where it is put - on the ground, in a hall cut for it under ground, or on an island made
 * for it in the sky - how many a world has, and what it is built of: one building in the middle (which has
 * the block that is the village's heart) and so many others either side, chosen from a list, with a street
 * of `street` between them on ground of `ground`. */
typedef struct {
    char name[CONTENT_NAME_LEN];
    int where;
    int count_min;           /* a world has at least this many */
    float per_1000_columns;  /* and this many more, about, for its width */
    int houses_min, houses_max;
    int centre;              /* which of content.structures */
    int houses[MAX_VILLAGE_HOUSES];
    int house_count;
    TileType street, ground;
} VillageDef;

enum { DIM_ISLANDS, DIM_CAVERNS, DIM_VOID, DIM_GROVE, DIM_SURFACE, DIM_DUNGEON };

/* A dungeon: a run of rooms built afresh, in the place whose generator is "dungeon", each time somebody goes in.
 * What it is built of, what is met there (creatures by name: they are read after this is), and what its chest holds:
 * each thing in `loot` has a `rank` from 0 to 1 - how far up the dungeon's list it is - and a difficulty gives the
 * ranks between two shares of it. */
/* A creature's body, built of boxes (see mobmodel.h): each a box hung from a point, wrapped in its part of the skin,
 * and moved in one of these ways - a leg strides with each step (and the other legs against it), a wing beats, a tail
 * sways from side to side, a head nods, a length of body goes along in a wave, a thing sways from where it hangs. */
#define MAX_MODEL_PARTS 40
enum { MODEL_STILL, MODEL_LEG, MODEL_LEG_OTHER, MODEL_WING, MODEL_WING_OTHER, MODEL_TAIL, MODEL_HEAD, MODEL_WAVE, MODEL_SWAY };
typedef struct {
    float pivot[3];      /* where it hangs from, in the body: 16 to a block; x across, y up, z toward its front */
    float corner[3];     /* the box's least corner, from there */
    float size[3];
    short u, v;          /* its corner on the skin */
    unsigned char move;  /* one of the above */
    bool mirror;         /* the other of a pair: the same skin, turned about */
    float by, lag;       /* how far it turns, in degrees; and how far behind the others it is in turning, in radians */
} ModelPart;

/* Quests: things to do, each with a task the game can see done and something to claim for it. Any directory of
 * content may bring a "quests.json": tabs, and quests in them. They are only kept track of, and shown, where the
 * rules say there is a questbook ("questbook": true) - without that they are so much unread data. */
#define MAX_QUESTS 2048
#define MAX_QUEST_TABS 24
#define MAX_QUEST_NEEDS 3
#define MAX_QUEST_REWARDS 4
enum { QUEST_HAVE, QUEST_KILL, QUEST_NEAR, QUEST_IN, QUEST_BIOME, QUEST_STAGE, QUEST_CHECK };
typedef struct {
    char name[CONTENT_NAME_LEN], label[CONTENT_NAME_LEN];
    ItemId icon;
    int order;              /* tabs are listed lowest first */
} QuestTabDef;
typedef struct {
    char name[CONTENT_NAME_LEN];
    char label[48], text[256];
    int tab;
    ItemId icon;
    char need_names[MAX_QUEST_NEEDS][CONTENT_NAME_LEN];
    int needs[MAX_QUEST_NEEDS], need_count; /* the quests that have to be done before this one can be */
    /* The task. "have": so many of an item in the bar, in what is open in it, or on. "kill": so many of a creature
     * dead. "near": a block within a few squares. "in": being in a dimension. "biome": standing in one. "stage":
     * having reached a stage of cultivation. "check": nothing the game can see - it is ticked by hand. */
    int kind, target, count;
    struct {
        ItemId item;
        int count;
    } rewards[MAX_QUEST_REWARDS];
    int reward_count;
    /* "pick": of what it gives, one is chosen. "crystals": so many crystals besides, whatever is chosen. "at": where
     * it stands on its tab's page, in squares; "hub": drawn larger - one that others hang from. */
    bool pick, hub;
    int crystals, x, y;
} QuestDef;

#define MAX_DUNGEONS 32
#define MAX_DUNGEON_LOOT 320
#define MAX_DIFFICULTIES 6
#define MAX_GRADES 7
typedef struct {
    char name[CONTENT_NAME_LEN], label[CONTENT_NAME_LEN];
    int tier;               /* the level a gate's bench has to have for this one to be offered */
    TileType wall, floor, back, door, mark, boss_mark, chest, exit, light;
    TileType decor[8], hanging[4];
    int decor_count, hanging_count;
    int rooms_min, rooms_max;
    char mobs[8][CONTENT_NAME_LEN];
    int mob_count;
    struct {
        ItemId item;
        float rank;
    } loot[MAX_DUNGEON_LOOT];
    int loot_count;
    int first_hard;         /* "difficulties_from": the first of the difficulties it can be entered at - and its creatures are as written at that one */
    ItemId spells[MAX_DIFFICULTIES][2]; /* "spells": what its chest may hold at each difficulty besides - a warrior's and a mage's */
    ItemId stone;           /* what raises the portal that leads here: found in the chest, as often as the difficulty says */
    long gold;              /* copper in the chest on the first difficulty, give or take a quarter */
} DungeonDef;
typedef struct {
    char label[CONTENT_NAME_LEN];
    float health, damage;   /* how many times what the dungeon's creatures have and do */
    float from, to;         /* the ranks of loot it gives */
    int grades[MAX_GRADES]; /* how often each grade, 1 to 6, against the others */
    int loot_min, loot_max; /* how many things in the chest */
    float stone;            /* how many of the dungeon's stone, on average */
    float gold;             /* how many times the dungeon's gold */
    int more;               /* creatures more to a room */
} DifficultyDef;

/* A realm of cultivation: so many stages, the first holding so much qi and each after it so many times the last. */
typedef struct {
    char label[CONTENT_NAME_LEN];
    int stages;
    float qi_first, qi_growth;
    Boons boons;       /* what whoever has reached it has, from then on */
    ItemId pill;       /* what coming into it takes, besides a full measure of qi; ITEM_NONE for nothing */
    int bolts;         /* the tribulation that comes with it: so many bolts of lightning, */
    float bolt;        /* each this share of a health bar, before anything turns it */
    bool revives;      /* whoever has reached it survives a death, once a day */
    float temper_qi;   /* "temper_qi": a stage of it broken through where the qi is at least this is tempered */
    bool core;         /* "core": coming into it forms a core, of a grade that is kept */
    int domain;        /* "domain": so many squares about them are under their strongest root's rule */
    bool soul;         /* "soul": meditating, they see wherever the view is taken */
} RealmDef;

/* The road: the realms in order, and the rules of travelling it. A game with none has no cultivation at all. */
typedef struct {
    RealmDef realms[MAX_REALMS];
    int realm_count;
    char immortal[CONTENT_NAME_LEN]; /* what is past the last stage of the last realm */
    Boons immortal_boons;
    float stage_gain;                /* every stage reached: this much quicker at everything, and hardier */
    float rate;                      /* qi a tick, sitting, where the qi is 1 and nothing helps */
    float chance_first, chance_last; /* the chance of a breakthrough from one stage to the next: at the first stage of a realm, and at its last */
    float fail_loss;                 /* the share of their qi a failure costs */
    float toxin_fade;                /* pill toxin lost a tick */
    /* What a stage needs before it can be left, besides qi: indexed by the stage being left, 1 for the first. */
    RecipeItem needs[MAX_DAO_STAGES][3];
    int need_count[MAX_DAO_STAGES];
    /* ...and the insight it takes, which is not spent. What grows where a tribulation struck. */
    int need_insight[MAX_DAO_STAGES];
    TileType scorch;
    /* A tribulation called down on purpose: so many bolts of such a share, and what it leaves whoever lives. */
    bool manual_costs, death_costs; /* "manual_costs_stage", "death_costs_qi" */
    int called_bolts;
    float called_bolt;
    RecipeItem called_leaves;
    char elements[DAO_ELEMENTS][CONTENT_NAME_LEN];
} DaoDef;

/* Another place the same world has: made the first time somebody goes there, as wide as the world (unless it
 * says how wide) and so many rows tall, and kept with it. Its biomes, veins and cave decorations are the ones
 * that name it. */
typedef struct {
    char name[CONTENT_NAME_LEN];
    char label[CONTENT_NAME_LEN];
    int generator;       /* DIM_ISLANDS: islands in open sky. DIM_CAVERNS: rock, with great caves. DIM_VOID: nothing but one island. */
    int width, height;   /* width 0 for the world's own */
    bool sky;            /* the sun and the weather reach it */
    TileType rock;       /* what it is made of */
    TileType edge;       /* what bounds it: something nobody can dig */
    TileType platform;   /* what is put under whoever arrives where there is nothing to stand on */
    bool falls_home;     /* whoever falls out of the bottom of it comes down out of the sky where the world began */
    int climate_wavelength; /* how long a stretch of one biome runs */
    /* Islands: so many to every 100 columns, so long and so deep. Banks of `cloud` drift along the bottom. */
    float island_rate;
    int island_min_length, island_max_length, island_min_depth, island_max_depth;
    TileType cloud;
    /* Caverns: so many to every 100 columns, each a hall that wanders so far and is so high; water lies in so
     * many of them; `magma` lies in the lowest so many rows; and the rock changes with depth (each from a share of the way down). */
    float cavern_rate;
    int cavern_min_length, cavern_max_length;
    float cavern_min_radius, cavern_max_radius;
    float lake_share;
    TileType magma;
    int magma_rows;
    struct {
        TileType tile;
        float from;
    } strata[MAX_STRATA];
    int strata_count;
    /* Geodes: hollow balls of a rock of their own, set in the rock, with a gem growing all over the inside. */
    struct {
        TileType shell, gem;
        float per_100_columns;
        int min_radius, max_radius, min_depth, max_depth;
    } geodes[MAX_GEODES];
    int geode_count;
    int ambient;         /* it is never darker there than this, 0..15 */
    bool has_sky_color;  /* its sky is of its own colours: by day, and by night */
    bool stars;          /* "stars": there are stars in it at every hour */
    /* "gravity": 1 as where the world began; less, and any drop is a step; 0, and everybody floats. "airless": nobody
     * breathes there but in a sealed room with air let into it. */
    float gravity;
    bool airless;
    /* "surface": ground with a sky over it - how far down the place it lies (a share), how much it rises and falls,
     * craters to 100 columns and how wide, lava tubes to 100 columns, and a canyon so wide and so deep. */
    float surface_level, crater_rate, tube_rate;
    int surface_relief, crater_min, crater_max, canyon_width, canyon_depth;
    unsigned char sky_day[3], sky_night[3];
    float qi;            /* how rich in qi the whole of it is, beside the usual: 0 for no different */
    /* Things built that stand on its islands: so many islands in a hundred have one, chosen from these. And
     * between islands that are near and level, a bridge of `bridge`, broken here and there. */
    int structures[24];
    int structure_count;
    float structure_share;
    TileType bridge;
    /* A grove: trunks of `trunk` so wide, so far apart, the whole height of the place, standing in `rock` under
     * water; boughs of `bough` out from them to walk on, `leaf` heaped on the boughs, `vine` hanging from them. */
    TileType trunk, bough, leaf, vine;
    int trunk_width, trunk_gap;
    int void_length, void_depth; /* "void": how far the one island of a void runs either way, and how deep it is */
    /* The void: what grows on its one island. */
    char tree_name[CONTENT_NAME_LEN];
    int tree;
} DimensionDef;

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
    /* "ocean": the world is sea with islands in it - how far apart, how long, how high; every so many of them a great
     * one; how deep the sea lies; and what is sunk in it. (With none of this the world is as it always was.) */
    bool ocean;
    /* "adrift": the world is open sea and nothing else, with one structure afloat in the middle of it: the ship.
     * She never moves; the sea goes by. How big a new world is, the row the water begins under, what she starts
     * as, how big she may be with no keel, how many ticks a thing adrift takes to pass one square when she lies
     * still, and how far ahead of her things appear. "seas": the waters she may be in, the first where she begins. */
    bool adrift;
    int adrift_waterline, keel_long, keel_above, keel_below, drift_ticks, drift_ahead;
    char adrift_start[CONTENT_NAME_LEN];
    SeaDef seas[MAX_SEAS];
    int sea_count;
    int isle_gap_min, isle_gap_max, isle_min, isle_max, isle_peak, great_every, great_min, great_max, great_peak, sea_depth_min, sea_depth_max;
    char buried_name[CONTENT_NAME_LEN];
    TileType buried; /* "buried": what lies under the beaches, so many to 1,000 columns */
    float buried_rate;
    char wrecks[16][CONTENT_NAME_LEN]; /* (structures, by name: they are read after this is) */
    int wreck_count;
    float wreck_rate;
    char isle_biomes[16][CONTENT_NAME_LEN]; /* what an island may be, by name: one to an island, or to each stretch of a long one */
    int isle_biome_count;
    /* Climate: two smooth maps round the world, which pick the biome. */
    int heat_wavelength, dryness_wavelength;
    float climate_blend;       /* how far into a climate its border is still mixed */
    int climate_spawn_clearance; /* columns round the spawn kept as the first biome */

    /* Caves: tunnels that wander through the rock and nothing else. */
    float cave_per_100_columns;
    int cave_min_length, cave_max_length;
    float cave_min_radius, cave_max_radius;
    int cave_min_depth, cave_max_depth;

    /* Islands in the sky: so many to every 100 columns, each a wedge of rock
     * - widest at its top, narrowing to a point below - under a few blocks of
     * soil and a surface. None hangs lower than `island_min_y`. Some carry
     * one of a few kinds of tree, the rarer ones only higher up. */
    float island_rate;
    int island_min_y, island_max_y;
    int island_min_length, island_max_length, island_min_depth, island_max_depth;
    int island_min_soil, island_max_soil;
    /* Poles: markers a fixed way round the world from where it begins - `at`
     * is that way as a share of the whole world, to the right for more than
     * nothing and to the left for less. Each is so many blocks of its tile
     * standing on the ground (on the sea, where there is sea), in the
     * background, with one block of something else on top. */
    struct {
        char name[CONTENT_NAME_LEN], label[CONTENT_NAME_LEN];
        float at;
        TileType tile, top;
        int height;
    } poles[MAX_POLES];
    int pole_count;
    /* How ragged they are: how many rows the top of one rises and falls by
     * along its length, and its underside juts by; 0 for a smooth wedge. And
     * how tall the cave through the base rock of each is, least and most; 0 for none. */
    int island_jag, island_min_cave, island_max_cave;
    TileType island_surface, island_soil, island_base;
    float island_tree_chance;
    struct {
        int tree, weight, min_y;
    } island_trees[MAX_ISLAND_TREES];
    int island_tree_count;
    /* What a new world can be made as: how wide, and how much bigger or
     * smaller than usual its mountains, its valleys and seas, and its veins. */
    struct {
        char label[CONTENT_NAME_LEN];
        int width;
    } sizes[MAX_WORLD_CHOICES];
    int size_count;
    struct {
        char label[CONTENT_NAME_LEN];
        float scale;
    } scales[MAX_WORLD_CHOICES];
    int scale_count;
    int tree_one_in, tree_spacing, tree_spawn_clearance;
    float tree_grown_chance; /* share of generated trees that start full grown */
    int tree_young_steps;    /* most growth steps given to the rest */
} WorldGenDef;

typedef struct {
    int tick_rate; /* ticks per second in a new world */
    int day_ticks, night_ticks, twilight_ticks;
    int chunk_width; /* columns per chunk */
    int random_events_per_chunk; /* random blocks hit in each chunk every tick, in a new world */
    /* How many of those the things that grow and turn up go by: with more
     * than this many events, each is that much less likely to be one of
     * theirs, so they keep their pace whatever the number is set to. */
    int random_event_base;
    int max_random_events;       /* the most the number can be set to in the settings */
    /* The year. Each season lasts so many days, one after another. */
    int season_days;
    struct {
        char label[CONTENT_NAME_LEN];
        bool snows;   /* what falls is snow, and it lies */
        bool freezes; /* open water turns to ice */
        bool thaws;   /* ice turns back to water */
        bool cold;    /* out of doors in it wears the environment score down; as "snows" unless said */
    } seasons[MAX_SEASONS];
    int season_count;
    /* Weather. Time is cut into stretches of so many ticks; in each it rains
     * (or snows) or not, by a chance. It wets nothing. Snow lies on the
     * ground in layers of an eighth of a block, so many deep at most, and
     * the tops of the land above the snow line always have it. */
    int weather_ticks;
    float rain_chance;
    int snow_max, snow_line;
    float snow_chance, melt_chance;   /* chance a random event on a column adds a layer, or takes one */
    char ice_name[CONTENT_NAME_LEN];
    TileType ice;                     /* what water freezes into; TILE_AIR for nothing */
    float freeze_chance, thaw_chance; /* chance a random event on open water freezes it, or on ice thaws it */
    float lift_time; /* seconds a lift takes over each block it goes */
    float burn_time; /* seconds something set fire to takes to come down */
    float boat_speed; /* times as fast as swimming that a boat goes */
    float energy_over;  /* how far past full the energy bar can be taken, by a soft bed or by coffee */
    float rush_seconds; /* seconds of crafting that one crystal buys off; 0 for no hurrying */
    int wall_span;   /* a block put up within this many blocks above another has backwall put in between */

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
    float drop_float_speed;  /* tiles per second it rises through water to float on top; 0 to sink */
    float denial_time;       /* how long a refused action's marker shows */

    /* Light, 0..15. Open sky is as bright as the sun or moon; light loses
     * some with every tile it travels, more through a block. */
    int light_sun, light_moon;
    int light_air_falloff, light_solid_falloff;
    int light_minimum; /* the least anything is ever lit, so the dark is not blind */
    /* How the light is shown. The dark is so opaque at its darkest (0..1; 1 if not given);
     * how fast it closes in as the light falls off (1 for evenly, more for a quicker fall to black);
     * and how much brighter than its true colours the square of something glowing at the full is drawn (0 for none). */
    float light_darkness, light_contrast, light_bloom;
    int light_glow;    /* how much brighter than the light it gives something that gives light is itself shown; 0 for no brighter */
    int light_water_depth; /* blocks of water that cost one level of sky light */
    int water_color[4];    /* red, green, blue, alpha of the tint drawn over water */
    int water_flow;        /* eighths of water a level stretch moves sideways in one tick */
    float swim_time;       /* seconds per step, swimming */
    float fly_speed;       /* flying: so many times as fast as walking is, whatever is on their feet */
    int oxygen_ticks;      /* ticks under water for a full oxygen bar to run out; 0 to need no air */
    int oxygen_refill_ticks; /* ticks in the air for it to fill again */
    int drown_ticks;       /* ticks for health to run down with no oxygen left */
    /* The environment score: it runs down out of doors in rain or cold, to no lower than what is worn keeps, and comes back under cover. */
    int env_fall_ticks;    /* ticks for it to run from full to nothing; 0 for no such thing */
    int env_rise_ticks;    /* and to come back */
    int env_harm_ticks;    /* ticks for health to run down with none left */
    float env_clothed;     /* the least it can fall to with anything on at all */
    /* Happiness: how they are doing, all told. It is what slows them. */
    float happy_slow;      /* with none, they walk and work with this share of their speed gone: 0.5 is half as fast */
    float happy_boost;     /* past full - whole, and energy, hunger and environment all kept past full - they are this much faster */

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
    /* Teleporting: from the menu of a bench of this kind and level, the
     * player can be carried to it for crystals. */
    char teleport_bench[CONTENT_NAME_LEN]; /* the kind of bench; empty for none */
    int teleport_level;    /* lowest level of it that offers it */
    int teleport_cost;     /* crystals, up to teleport_free blocks away */
    int teleport_free;
    int teleport_per;      /* one crystal more for every this many blocks beyond */
    int max_players;       /* how many characters a world can have to play */
    /* Dying. With health gone, what hurts comes off a second bar; when that
     * is gone too the character is dead, and comes back by themselves after
     * a while - or sooner, for crystals. */
    float death_bar;       /* how much the second bar takes, in health bars */
    int respawn_ticks;     /* how long the dead stay dead */
    float flail_time;      /* seconds someone attacked throws their arms about */
    int max_drop;          /* how far they will let themselves fall by a step of their own: blocks; 0 for any distance */
    int respawn_cost;      /* crystals to come back at once, right after dying; less as the wait runs down */
    bool questbook;        /* "questbook": quests are kept track of, and there is a book of them */
    int dungeon_health;    /* what a character's health adds up to with nothing on, where blows are numbers */
    float upgrade_cost;    /* what upgrading a rolled thing all the way costs: so many times its price */
    int upgrade_steps;     /* in how many goes */
    char grade_labels[MAX_GRADES][CONTENT_NAME_LEN]; /* what each grade of a rolled thing is called, and its colour */
    unsigned char grade_colors[MAX_GRADES][3];
    DifficultyDef difficulties[MAX_DIFFICULTIES];
    int difficulty_count;
    int hand_damage;       /* what a blow with nothing better in hand takes off a creature */
    float poison_damage;   /* share of the health bar each tick of being poisoned takes */
    float attack_time;     /* seconds a blow takes */
    float zoom[MAX_ZOOM_LEVELS];
    char player_skin[CONTENT_PATH_LEN]; /* the character's own skin: body, hair and face, and the clothes they are never without */
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
    int health;         /* blows' worth of damage it takes to kill */
    long long health_unit; /* "health_unit": its health is in so many of damage each (for one with more than a number can hold); 1 as a rule */
    int size_min, size_max; /* how many blocks long one can be; it is drawn that big */
    /* A creature of the water. It swims about and never leaves it. One that
     * hunts goes for a player who is in the water, bites, makes off, and comes
     * round again. */
    bool swims, hunts;
    /* A creature of the air: it drifts about the open sky as one of the water does the sea, and never lands. */
    bool flies;
    /* "ship": ridden, whoever rides fights with its guns - so much, so far - and what is aimed at them strikes it; sunk,
     * it leaves "wreck" on the bottom. "boarding": with something that boards in hand it is grappled - it is gone, there
     * is a deck where it was, and so many of "crew" on it and their "captain". */
    int gun_damage, gun_range;
    TileType wreck, deck;
    char crew[CONTENT_NAME_LEN], captain[CONTENT_NAME_LEN];
    int crew_count;
    /* "mount": owned, it can be ridden, at so many times walking pace. "strike": it hits whoever it can see from afar. */
    bool mount;
    float mount_speed;
    float strike_share;
    int strike_range, strike_rest;
    /* "power" in "sting" or "strike": the blow is a number, taken from what the character's health adds up to
     * (game_max_health) - not a share of the bar. "boss": its health is shown across the top of the screen. */
    /* "model": its body in boxes, and the skin they are wrapped in. With none it is its flat picture. */
    char model_skin[CONTENT_PATH_LEN];
    int model_w, model_h, model_part_count;
    float model_bob, model_hop, model_turn;
    ModelPart model_parts[MAX_MODEL_PARTS];
    long long sting_power, strike_power;
    bool boss;
    /* "tame": what grade of thing that tames it takes (0: it cannot be). "companion": owned, it keeps by whoever is
     * nearest and goes for wild creatures it can see, doing so much to them at a time. */
    int tame;
    bool companion;
    int companion_bite, companion_sight;
    /* One that blows do nothing to: it is hurt by magic of the one element it is weak to, and by nothing else. */
    bool immune;
    int weak_to;        /* which of content.elements it is weak to; -1 for none in particular */
    bool spawn_sky;     /* it turns up in the open sky */
    bool spawn_cave;    /* it turns up only under ground: where there is backwall behind it */
    int spawn_time;     /* 0 at any time; 1 only by day; 2 only by night */
    bool spawn_water;   /* it turns up in water */
    unsigned spawn_seas; /* "spawn": {"seas"}: only while the ship is in one of these seas (a bit each); 0 for any */
    int spawn_biomes[MAX_MOB_BIOMES]; /* the only biomes it turns up in; anywhere, if none are named */
    int spawn_biome_count;
    /* A creature that stings. It makes for a player who comes near and
     * poisons them: so many ticks of it, then leaves off for a while. */
    int sting_ticks;    /* ticks of poison a sting gives; 0 for a creature that does not sting */
    int sting_rest;     /* ticks before it stings again */
    int sting_sight;    /* how near a player has to come for it to make for them */
    /* A creature that can be shorn: what comes off, and how long it takes to grow back. */
    ItemId shear_item;
    int shear_min, shear_max, shear_ticks;
    char texture_shorn[CONTENT_PATH_LEN]; /* how it looks until then; empty for no different */
    ItemId caged;       /* what it is as an item once it has been caged; ITEM_NONE for one that cannot be */
    /* A creature that can be milked: what that gives, and how long before it can be again. */
    ItemId milk_item;
    int milk_ticks;
    int sight;          /* how far off it notices a swimmer */
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
    /* Breeding on. Now and then what it lays is not its own egg but that of
     * one of the kinds that come after it, each as likely as the others. */
    int lay_next[MAX_MOB_NEXT];
    int lay_next_count;
    float lay_next_chance; /* share of its eggs that are a next kind's */
    int baby_ticks;     /* how long a young one takes to grow up */
    struct {
        ItemId item;
        int min, max;
        float chance; /* chance it leaves this at all; usually 1 */
        bool sized;   /* the item is a trophy the size of the creature */
    } drops[MAX_MOB_DROPS]; /* what a grown one leaves when killed */
    int drop_count;
} MobDef;

/* Buying and selling, and the money it is done with. */
typedef struct {
    struct {
        char name[CONTENT_NAME_LEN];
        char label[CONTENT_NAME_LEN];
        bool free_only; /* listed only where everything is given away: what no trade portal deals in */
        /* Other names items may give for it: categories there used to be, which mods still name. */
        char also[MAX_CATEGORY_NAMES][CONTENT_NAME_LEN];
        int also_count;
    } categories[MAX_CATEGORIES];
    int category_count;
    /* An item that names no tier is of the highest one whose price it reaches: these are those prices, lowest tier first. */
    int tier_prices[MAX_TRADE_TIERS];
    int tier_count;
    /* Villages. Where trade is only done in them, a block to trade at anywhere else does nothing. Each village
     * has prices of its own: so many of all the things there are (`village_share`) cost up to `village_swing`
     * more or less there than usual, and which, and by how much, changes every so many days (0 for never).
     * Each also deals in so many of the specialities, which no other place sells. */
    bool villages_only;
    float village_swing, village_share;
    int village_days, village_specialities;
    /* Coins, smallest first. Each is an item worth so many of the smallest. */
    struct {
        ItemId item;
        int value;
    } currencies[MAX_CURRENCIES];
    int currency_count;
    int default_price;
    float sell_share; /* what selling a thing gives, as a share of what buying it costs */
    bool discover;    /* a thing can only be traded in a world once somebody there has had one */
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
    /* Colours that things can be made in, each given by the recipe that makes it. */
    unsigned char tints[MAX_TINTS][3];
    /* The elements of magic that the data names: a spell is of one, and a creature may be weak to one. */
    char elements[MAX_ELEMENTS][CONTENT_NAME_LEN];
    int element_count;
    int tint_count;
    TreeDef trees[MAX_TREES];
    int tree_count;
    FeatureDef features[MAX_FEATURES];
    int feature_count;
    VeinDef veins[MAX_VEINS];
    int vein_count;
    DecorDef decor[MAX_DECOR];
    CaveBiomeDef cave_biomes[MAX_CAVE_BIOMES];
    int cave_biome_count;
    int decor_count;
    BiomeDef biomes[MAX_BIOMES]; /* the first is the one a column is unless another claims it */
    int biome_count;
    MobDef mobs[MAX_MOB_KINDS];
    int mob_count;
    int mob_spawn_radius; /* creatures appear, and wild ones last, only this near the player */
    int mob_cap;          /* no more creatures of the land appear while this many are that near */
    int mob_water_cap;    /* and the same for creatures of the water, counted apart */
    /* Kinds of tool, such as "shovel". There is no fixed list: a kind exists
     * because some block or item names it. */
    char tool_classes[MAX_TOOL_CLASSES][CONTENT_NAME_LEN];
    int tool_class_count;
    WorldGenDef worldgen;
    StructureDef structures[MAX_STRUCTURES];
    int structure_count;
    VillageDef villages[MAX_VILLAGE_KINDS];
    int village_count;
    DaoDef dao;
    DimensionDef dimensions[MAX_DIMENSIONS];
    int dimension_count;
    DungeonDef dungeons[MAX_DUNGEONS];
    int dungeon_count;
    QuestTabDef quest_tabs[MAX_QUEST_TABS];
    int quest_tab_count;
    QuestDef quests[MAX_QUESTS];
    int quest_count;
    RulesDef rules;
    SetDef sets[MAX_SETS];
    int set_count;
    PoolDef pools[MAX_POOLS];
    int pool_count;
    bool has_relics; /* some item is a relic: characters have somewhere to carry them */
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
