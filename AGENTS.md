# BlockGame

A 2D block-based game engine in C (raylib). See `README.md` for what the game does and how to build it.

## Quick guide: read this before adding anything

A checklist for a new session. The sections after it give the reasons.

**Before starting**

- `git fetch origin`, then `git status`. The owner edits files between sessions (recipes, the plans in `ModDescriptions/`, sometimes data). An uncommitted change that is not yours is theirs: do not revert it and do not commit it without asking. If it breaks a test, say so and ask.
- `saves/` is the owner's real worlds and is not in git. Only ever read it. Never write there from a test or a script; tests write under `build/`.
- Run `arch -arm64 make all test` first, so that you know it passed before you touched anything.

**Where a new thing goes**

| Adding | Do this |
| --- | --- |
| A block, item, recipe, creature, tree, ore vein | Data only: `data/*.json` and a texture. No C. |
| Something no existing field can express | A new field: declare it in `src/content.h`, read it in `src/content.c` with a default that leaves old data working and an error that names the file, then write the engine against the field. |
| Something that belongs to a mod | The mod's own files under `mods/<name>/`. If it needs the engine to do something new, that is a new field the base game could use too. Never `if (mod is on)`. |
| A new kind of saved state | Bump `SAVE_VERSION` in `src/save.c` and keep every older version loading (see below). |
| Interface text | A control's name and nothing else; explanations go in `README.md`. Check it fits at 1280x720. |

**Every time, without being asked**

1. **An item needs a `"category"` and a `"price"`**, or the game will not start. That includes the item that places a new block, and each tier of a bench.
2. **A block that holds things** (`"storage"`) needs its item to have `"container": {"slots": N}`, or its contents are lost when it is picked up.
3. **Add or change a test** in `tests/test_game.c` for each rule. When shipped recipes change, fix the counts in `test_shipped_recipes`; when a mod's content changes, the counts in its `test_*_mod`. Leave `tests/legacy/` alone.
4. **Regenerate `obtainment.txt`** when recipes, drops or world generation change: `python3 tools/obtain.py .`
5. **A new way of getting an item** (a new kind of drop, loot or reward) needs a line in `src/guide.c`, or the recipe book says it cannot be got.
6. **Update `README.md`** (what it does, for a player) and this file (what a future session must know).
7. **Check the owner's saves still load** after any change to `src/save.c` or to a saved structure, by loading each `saves/*.world` in a throwaway program. Read only.
8. **Commit and push** each finished change, with a short message that says what changed. No `Co-authored-by` or tool attribution in commit messages. Never rewrite history.

**Things that have gone wrong before**

- **The art is 16x16 pixels drawn double** (32x32 files for blocks and creatures; item icons are 48x48 - a block's icon is its 32x32 texture pasted at 8,8). Single-pixel detail at 32x32 looks out of place and was rejected. Use the palettes of the existing textures; `tools/bench_art.py` shows the manner.
- **Ids change from run to run.** Anything saved that names a block, item or recipe has to be translated on load. Blocks, items and creatures are (by name tables). Recipe positions are not: a machine's chosen recipe (`Storage.recipe`) and a door's kept look are only checked for sense after loading.
- **The tests also run at the old pace, without fog, and letting characters drop.** The same wrapper puts step times back to 0.12 / 0.16 / 0.22 (walk / climb / crawl) and `max_drop` to 0 (`test_no_dropping` has the shipped rule), and `game_init` is wrapped to turn the fog of war off, because the tests put characters down anywhere and click on what was never uncovered. `test_pace_and_flight` restores the shipped times; `test_fog` makes its world with `(game_init)`.
- **A step that is only a fall is filtered out in one place**: `drops_too_far` in `src/game.c`, applied by `list_moves` (what a route may use) and `key_step` (what the keys ask for). A new kind of step goes into `every_move` / `key_step_any` and is filtered with the rest.
- **The world is drawn in depth, by `src/world3d.c`.** The world pass in `session_draw` runs between `world3d_begin` and `world3d_end`: a real perspective camera looking at the flat picture, which is the front of the scene (z 0). Three rules keep it working:
  - Every texture in `src/assets.c` is drawn through `world3d_texture`, never `DrawTexturePro`. It draws flat unless a depth has been set (`world3d_solid` for a block or a picture with thickness, `world3d_plane` for a flat picture further back), and the code that sets one puts it back with `world3d_flat`. `block_depth` in `src/main.c` decides for tiles; the depths are the `Z_` constants there. The kinds of model are in `src/world3d.h` (material, box, built frame, body, panel, crossed plant): a new tile gets one from what it is (a plant if it is sown or grows; a box if it is a bench or holds things; otherwise by its picture), and `"model"` in the data overrides. Look at a new non-block tile in the debug world before shipping it. A solid tile that is a thing and not material (`stands_alone`) is one block deep, and the backwall is drawn behind it.
  - Plain raylib drawing (rectangles, text, lines) inside the pass lies on the front of the scene, exactly where it would in 2D; wrap it in `world3d_push(z)` / `world3d_pop()` to put it further back.
  - What is under the cursor is found in depth (`tile_at_screen`, `world_at`): do not turn the mouse into a square by dividing by the zoom.
  Interface drawn outside the pass is untouched. The darkness and fog are laid over the front with the depth test off (weighed against the block faces they lie on, they flicker as the zoom changes), and it is switched back on for what glows, which is drawn again where it stands. The view's origin is kept to thirty-seconds of a pixel so that neighbouring edges are the very same numbers: do not place world drawing by any other arithmetic than `origin + whole pixels`, or hairlines open between blocks while zooming.
- **The character's poses are all eased** (`draw_player` in `src/main.c`): a state works out a target pose and the drawn one moves toward it. Sleeping, meditating and dying are states like any other there - do not draw a state directly and return, or it will snap.
- **Flying is swimming in air.** `World.aloft` is set by the game for the character in hand (`set_aloft` in `src/game.c`) and makes `buoyed` true in open air; everything that plans a move goes through that. A new entry point that plans or moves must call `set_aloft` first.
- **Pace comes from happiness alone** (`game_pace` in `src/game.c`): the average of energy, hunger and the environment score, capped by health - and past full, a fifth faster, when all three are kept past full and health is whole. Food is `cooked` (and can keep hunger past full) if a recipe makes it, unless its data says; that is worked out in `validate` in `src/content.c`. Anything new that should slow a character goes into `happiness_of`, not into the step timings. The tests' wrapper zeroes `happy_slow` as well, so that hungry or hurt test characters keep their old timings; `test_environment` restores it.
- **The tests run with the weather off.** `tests/test_game.c` wraps `content_load` to zero `env_fall_ticks`, because its characters stand about with nothing on for thousands of ticks; `test_environment` puts the shipped value back. A new test that needs the environment score does the same. Anything wearable needs a `"warmth"` (its share of the score it keeps), or it only counts as "something on".
- **What runs on rails is not blocks.** Vehicles are `Game.vehicles` (`src/train.c`), saved whole from version 31, each naming its item; their fuel and cargo are containers of the inventory, which is why the tests' `clear_inventory` must not be called once a vehicle is down (the railway test empties the bar by hand). Coupling is touching: there is no link to keep. A character riding has `Game.ride` set and is moved by the vehicle; `game_click_tile`, `game_queue_action` and `start_next_activity` all refuse while it is. A new kind of vehicle is an item with `"vehicle"` and a picture - no C.
- **`World.meta` is one number per square, shared**: tulip colours, ice, a lift motor's power flag, a sign's text, a door's look. A new use must clear it when the block goes (`world_set_at`) and must not collide with a block that already uses it in the other layer.
- **`Storage` is written to saves raw.** Adding a field to it (or to `Vitals`, `Inventory`, `Drop`, `Mob`) changes the file layout: add a `StorageNN` copy of the old layout and a reader for it, as `read_storages_26` does.
- **A click means one thing.** `game_click_tile` and the chain in `handle_input` (`src/main.c`) decide by what is in hand and what is under the cursor, in order. A new use of a click goes in that chain at the right place, and the test should drive it through the click, not through `game_queue_action` - a rule that works when queued directly can still be unreachable by clicking.
- **The tests' `clear_inventory` frees every container**, including those of chests and machines already set down in the test world. Set bar slots directly when the test has machines in it.
- **World generation shares one random stream.** Drawing a number in a new place changes every world made afterwards from the same seed. Guard new draws so that with the feature off (a rate of 0, a list that is empty) none is made - the legacy tests rely on it.
- **Background blocks behind a solid block are hidden**: not drawn, not opened (`usable_at`), not glowing. Wire is the exception: it is drawn over the block and comes off first.
- **Cave biomes use no random numbers from the world's run**: where they are and what grows in them comes from hashes of the seed and the square (`dress_caves`). Keep it so, or every world's trees and plants shift.
- **Biomes are placed by rectangles of heat and dryness**, and where two overlap the one loaded later wins - mods load in order of name. A new biome takes its rectangle out of whatever was there, so check it against every other mod's rectangles (they are in each mod's `worldgen.json`) and do not overlap them. A biome's `"soil"` is a block deep at the edge of its country and deepens inward; its `"under"` rock comes in only at the very heart, which a small rectangle seldom has - so a rock that is meant to be found goes in as the soil, or in veins as well.
- **Two mods that define the same name cannot both be on**, and both turn off. Give a mod's things names of its own; only redefine a base thing (as both Magic and Mayhem and Industry redefine `workbench_4`) when you accept that it will clash with any other mod that does.
- **Tools that do not survive the session**: scratch scripts and screenshot harnesses live outside the repository and are gone next time. Anything worth keeping goes in `tools/`.

**Checking what cannot be tested**

Rendering and menus have no tests. To look at them, write a small C file that does `#define main real_main`, `#include "main.c"`, `#undef main`, then its own `main` that loads content, sets a scene up, calls `session_draw` and `TakeScreenshot` - built against every `src/*.c` but `main.c`, and run from the repository root. Keep such files out of the repository (raylib's `TakeScreenshot` writes into the directory the program is run from, whatever path it is given: move the picture out of the repository root afterwards). Say plainly in the report what was checked this way and what was not.

**`tools/`** (each takes the repository root as its argument; rerunning one with nothing changed changes nothing)

- `obtain.py` - writes `obtainment.txt` from the data.
- `prices.py` - reprices items from their recipes. It overwrites prices in `data/items.json`: run it on purpose, not as routine.
- `mod_magic_mayhem.py`, `mod_industry.py`, `mod_digital_storage.py`, `mod_culinary_delights.py`, `mod_explorers_delight.py` - each wrote one mod's data files and textures. Either edit the script and rerun it, or edit the mod's files by hand and stop using the script: not both, or the script will undo the hand edits.
- `bench_art.py` - drew the bench textures in `data/textures/tiles/`.
- `trade_tiers.py` - applies the owner's `TradeTiers.txt` to `data/items.json` (category, tier, order; everything it leaves out to `special`) and writes `TradeObtainment.txt`. How that document is read is a table in the script, set down by hand: change it when the document changes.
- `train_art.py` - draws the rail, the ramp, the station board, the four vehicles (`data/textures/vehicles/`) and their icons. A vehicle's bottom five rows are left clear so that its wheels stand on the rail; its windows are clear so that riders show through.
- `portal_art.py` - draws every portal's blocks and icons (the arch, its spiral, the tier's stone in the keystone). A new kind of portal is a new line at the bottom of it.
- `worn_art.py` - draws the character's skin (`data/textures/player/base.png`) and, for every item with an `"equip"` slot in the game and in each mod, what it looks like worn (`textures/worn/<name>.png`), and sets the item's `"worn"`. **Run it after adding anything that can be worn**; `test_worn` fails for an item without a picture. Give a new kind of clothing its look there, by a word in its name.

## Core directive: keep everything data driven

The goal is mod support. Anything a mod might want to add or change must live in data, not in C.

**The engine knows what fields mean. It does not know any content by name.**

- Content is defined in `data/*.json` and drawn from `data/textures/`. The loader is `src/content.c`; the tables it fills are described in `src/content.h`.
- No block, item, recipe, tree, terrain feature or tuning number is written into the C code. There is no `TILE_DIRT`: code asks `tile_def(t)->solid`, or uses a tile that a data file named for that role (for example `content.worldgen.soil`).
- Ids are positions in the loaded tables and change from run to run. Never store or compare a literal id other than `TILE_AIR` and `ITEM_NONE` (both 0). Save files record names, not ids.
- `content_load` is additive: loading a second directory adds new definitions and replaces ones with the same name. Keep it that way; it is how mods will load.

### When adding a feature

1. Decide what a modder would want to vary, and put that in a data file: a new field on an existing definition, or a new kind of definition.
2. Teach `src/content.c` to read it, with a sensible default so existing data files keep working, and an error message that names the file and the problem when the data is wrong.
3. Write the engine code against the field, never against a specific block or item.
4. If the feature only makes sense for one piece of content, that is a sign the behaviour belongs in a data field (as `climbable`, `bench` and `front` are) rather than in a special case.

A new constant in C needs a reason it could not be data. Fixed sizes of saved structures (inventory slots, basket slots) and limits on table sizes are the accepted exceptions.

### Every item has a category and a price

Every item in the game except the coins belongs to a trade category and has a buying price. The dev portal lists them all by category; trade portals deal in the ones that are not barred. Whenever an item is added, removed or renamed:

- **Adding an item** (including the item that places a new bench or block, and each tier of one): give it a `"category"` from `data/trade.json` in `data/items.json`, and a `"price"` in copper coins that fits what it is made of (10 is the least). Add a new category to `data/trade.json` if none fits.
- **Removing an item:** take it out of `data/items.json` and out of anything that names it - recipes, upgrade costs, drops, starting items, currencies.
- Only currencies listed in `data/trade.json`, and items that are never held - ones that turn into crystals when picked up (`"crystals"`), or into a character when made (`"becomes_player"`) - may be left without a category.
- **The categories, tiers and order come from the owner's `TradeTiers.txt`**, applied to `data/items.json` by `tools/trade_tiers.py` (its `READ` table is that document, read by hand). A new base-game item the document names: add it to `READ` and rerun. One it does not name: `"category": "special"`, which only the dev portal lists (`"free_only"` in `data/trade.json`). Rerun the tool after adding items either way: it also moves items into the document's order and rewrites `TradeObtainment.txt`.
- A mod's item gives a `"category"` (the old names still work through `"also"` in `data/trade.json`) and may give a `"tier"`; with none it gets one from its price (`"tier_prices"`).
- Trade portals do not deal in things to trade at, in anything a recipe takes crystals to make, in anything of a `"free_only"` category, or in anything above the tier the portal has been raised to (`"trade_level"`). The first three are `trade_barred` in `src/trade.c`, worked out from the data: do not add a flag for them.
- The trade menu keeps a place for every item a portal could deal in, tier by tier and within a tier in the order `items.json` defines them, and shows a `?` until the item is found (`game_trade_listing` in `src/trade.c`). Do not filter or sort that list in the menu: found things must not move.
- Selling gives `"sell_share"` of the price. Do not add a recipe that turns coins into something worth more than the coins.

The loader enforces the category: an item with none that is neither a currency nor never held stops the game at startup, naming the item. Do not weaken that check to get a new item in; give the item a category.

### Mods

- A mod is a directory under `mods/` loaded on top of `data/` by `content_load` (see `src/mods.c`); the ones the game comes with are `mods/magic_mayhem` and `mods/industry`, each written out from its plan in `ModDescriptions/`, and `mods/digital_storage` (storage networks: `src/digital.c`, with the terminal's screen in `src/termui.c`), `mods/culinary_delights` (crops, trees, biomes and a kitchen) and `mods/explorers_delight` (biomes and building blocks) - the first data only; the second needed two things of the engine (seas chosen by climate, and `"cave_biomes"`, both in `src/world.c`). Everything a mod does it does through the data: if a mod needs the engine to do something new, that is a new field the base game could use as well, never a check for the mod.
- A mod's items follow the same rules as the game's: every one has a category and a price.
- A mod adds to lists the game already has by naming things (tiles, items, trees, mobs, veins, cave decorations) or through the lists made for adding (`"biome_plants"`, `"island_trees"`, recipes). Do not make a mod restate a whole biome to add a plant to it.
- Worlds, crystals and the portal chest belong to the playset the enabled mods make (`mods_saves`). Anything new that is kept between worlds goes in that directory too, not in `saves/` itself.
- `tests/test_game.c` loads each mod on top of the shipped data (`test_magic_mod`, `test_industry_mod`, `test_digital_mod`, `test_culinary_mod`, `test_explorers_mod`) and checks its counts: change them there when a mod's content changes.
- Two mods that define the same name cannot be on together (`mods_conflict`): asked for together, none is loaded. Do not make a mod depend on another being on.

### Still in code, to be moved when touched

- The interface: layouts, wording and colours of the inventory bar, bench menu and menus.
- Default key bindings and their labels (`src/menu.c`).
- The character's poses and how fast they move through them (`draw_player` in `src/main.c`: each frame a pose is worked out from what the character is doing and the drawn one eased toward it, kept per character in `body_ease`; a new pose is a new branch there, never a jump) and the shape of the body (`src/playermodel.c`): the skin and what is worn over it are pictures named in the data. The sky, sun and moon (`src/main.c`).
- The movement rules themselves (walking, hopping, climbing) are code; their timings are data.

## Working here

- Build with `make`, run with `make run`, test with `make test`. In a shell running under Rosetta, prefix with `arch -arm64`.
- The game and the tests read `data/` relative to the directory they are run from: run them from the repository root.
- The recipe book in the pause menu (`src/recipeui.c`) is drawn from `src/guide.c`, which works out from the tables where each item comes from. A new way of getting an item (a new kind of drop, loot or reward) needs a line there, or the book will say the item cannot be got.
- `recipes.txt` is the list the newest batch of recipes was written out from (the earlier batches are in its history); nothing is removed from `data/recipes.json` because it is not in that file. `obtainment.txt` lists what they leave unobtainable or without a use; it is produced from the data, so regenerate or correct it when recipes, drops or world generation change.
- Block ids take two bytes in the world and in saves (there is room for 4,095 kinds of block; 349 are in use by the game itself, and about 560 with every mod that can be on together). A feature that wants a great many look-alike blocks (a colour, a size) should still keep a number with the block (`World.meta`, as two-tone flowers do) rather than add a block for each.
- The interface carries no explanations: a menu names its controls and nothing else, and what a thing does is said in `README.md`. Check that new text fits where it is drawn at 1280 by 720.
- `tests/legacy/` is a small fixed set of recipes and benches that the tests of how the game works run against, so that changing what the game ships with does not break them. The shipped recipes have a test of their own (`test_shipped_recipes`) which runs first, against `data/` alone. When recipes change, change that test; leave `tests/legacy` alone.
- `tests/test_game.c` is headless and covers the rules. It looks content up by name, and its last test loads a small mod on top of the shipped data. Add a test with each rule change.
- Rendering code (`main.c`, `hud.c`, `benchui.c`, `menu.c`, `assets.c`, `world3d.c`, `playermodel.c`) needs a window and is not covered by tests.
- `third_party/cjson` is vendored; do not edit it.
- There is room for 64 biomes and 128 trees (`MAX_BIOMES`, `MAX_TREES` in `src/content.h`). With Culinary Delights and Explorer's Delight both on, 28 biomes and 34 trees are in use.
- The save format is at version 33 (`SAVE_VERSION` in `src/save.c`; 30 added the environment score, 31 what is on the rails, 32 a vehicle's speed, 33 moved the chest and the safe to the background layer). **Moving a block from the foreground to the background in the data is a change of format**: raise the version and extend the loop in `load_game` that carries such blocks across in older worlds (`tests/fixtures/v32_chests.world` is the check). A save says how long each of its vehicles is, so a field added to the end of `Vehicle` needs only the version raised; `tests/fixtures/v31_train.world` is a train saved at 31, loaded by `test_trains`. Every earlier version still loads; keep it so. `tests/fixtures/v28_industry.world` is a world saved at version 28, loaded by `test_industry_mod`, and `tests/fixtures/v29_plain.world` one saved at 29 (written by the previous commit's code built in a scratch directory), loaded by `test_environment`: when the format changes again, keep that test passing and add a fixture of the version being left behind.
- **The limits** are the `MAX_*` constants, mostly in `src/content.h` and `src/game.h`. All of them were multiplied by four at version 29. Four are also the sizes of arrays a save holds whole - `MAX_CONTAINERS`, `MAX_DROPS`, `MAX_STORAGES`, `MAX_MOBS` - so changing any of those is a change of save format: the sizes older saves were written with are kept as `V28_*` in `src/save.c`, and every reader of an older version uses those, never the live constant. The table of definitions is static and is about 66 MB now (`sizeof(Content)`); a `Game` is about 470 KB, and tests keep two or three on the stack at once.
- Limits that were deliberately not raised, and why: `INVENTORY_SLOTS`, `BASKET_SLOTS`, `ARMOR_SLOTS`, `CONTAINER_MAX_SLOTS` and `HUD_MAX_EXTRA_SLOTS` (the bar and the menus are laid out for them); `MAX_CHARACTERS` (the buttons down the side, and the rules say how many a world may have); `MAX_TINTS` (a paint is one byte in the world, so 250 is all there can be); `MAX_SEASONS`, `LIGHT_MAX`, `WATER_FULL`, `CELL_TYPES`, `SIGN_TEXT_MAX`, `MAX_CRAFT_AMOUNT` and the tone and paint encodings (rules of the game, not room for things); the lengths of names (`CONTENT_NAME_LEN` is how names are written in saves); and `MAX_TICK_RATE` and `MAX_TICKS_PER_UPDATE` (there so a slow frame cannot snowball). Creature kinds are a byte in saves, so `MAX_MOB_KINDS` (192) cannot pass 255 without a change of format.
- The plans the owner writes for mods are in `ModDescriptions/`. Where a plan leaves something open, choose, and say what was chosen in the report and in `README.md`.
