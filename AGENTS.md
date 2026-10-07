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
- `trade_obtain.py` - writes `TradeObtainment.txt`: which of the game's items the owner's `TradeTiers.txt` leaves out, and what it names that the game has not got. How that document is read is a table in the script, set down by hand: change it when the document changes.
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
- Trade portals do not deal in benches or other things to trade at, nor in anything a recipe takes crystals to make. That is worked out from the data (`trade_barred` in `src/trade.c`), not listed anywhere: do not add a flag for it.
- The trade menu keeps a place for every item a portal could deal in, in the order `items.json` defines them, and shows a `?` until the item is found (`game_trade_listing` in `src/trade.c`). Do not filter or sort that list in the menu: found things must not move.
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
- Rendering code (`main.c`, `hud.c`, `benchui.c`, `menu.c`, `assets.c`) needs a window and is not covered by tests.
- `third_party/cjson` is vendored; do not edit it.
- There is room for 64 biomes and 128 trees (`MAX_BIOMES`, `MAX_TREES` in `src/content.h`). With Culinary Delights and Explorer's Delight both on, 28 biomes and 34 trees are in use.
- The save format is at version 29 (`SAVE_VERSION` in `src/save.c`). Every earlier version still loads; keep it so. `tests/fixtures/v28_industry.world` is a world saved at version 28, loaded by `test_industry_mod`: when the format changes again, keep that test passing and add a fixture of the version being left behind.
- **The limits** are the `MAX_*` constants, mostly in `src/content.h` and `src/game.h`. All of them were multiplied by four at version 29. Four are also the sizes of arrays a save holds whole - `MAX_CONTAINERS`, `MAX_DROPS`, `MAX_STORAGES`, `MAX_MOBS` - so changing any of those is a change of save format: the sizes older saves were written with are kept as `V28_*` in `src/save.c`, and every reader of an older version uses those, never the live constant. The table of definitions is static and is about 66 MB now (`sizeof(Content)`); a `Game` is about 470 KB, and tests keep two or three on the stack at once.
- Limits that were deliberately not raised, and why: `INVENTORY_SLOTS`, `BASKET_SLOTS`, `ARMOR_SLOTS`, `CONTAINER_MAX_SLOTS` and `HUD_MAX_EXTRA_SLOTS` (the bar and the menus are laid out for them); `MAX_CHARACTERS` (the buttons down the side, and the rules say how many a world may have); `MAX_TINTS` (a paint is one byte in the world, so 250 is all there can be); `MAX_SEASONS`, `LIGHT_MAX`, `WATER_FULL`, `CELL_TYPES`, `SIGN_TEXT_MAX`, `MAX_CRAFT_AMOUNT` and the tone and paint encodings (rules of the game, not room for things); the lengths of names (`CONTENT_NAME_LEN` is how names are written in saves); and `MAX_TICK_RATE` and `MAX_TICKS_PER_UPDATE` (there so a slow frame cannot snowball). Creature kinds are a byte in saves, so `MAX_MOB_KINDS` (192) cannot pass 255 without a change of format.
- The plans the owner writes for mods are in `ModDescriptions/`. Where a plan leaves something open, choose, and say what was chosen in the report and in `README.md`.
