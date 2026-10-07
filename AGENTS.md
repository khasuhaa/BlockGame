# BlockGame

A 2D block-based game engine in C (raylib). See `README.md` for what the game does and how to build it.

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
- Selling gives `"sell_share"` of the price. Do not add a recipe that turns coins into something worth more than the coins.

The loader enforces the category: an item with none that is neither a currency nor never held stops the game at startup, naming the item. Do not weaken that check to get a new item in; give the item a category.

### Still in code, to be moved when touched

- The interface: layouts, wording and colours of the inventory bar, bench menu and menus.
- Default key bindings and their labels (`src/menu.c`).
- How the player, sky, sun and moon are drawn (`src/main.c`).
- The movement rules themselves (walking, hopping, climbing) are code; their timings are data.

## Working here

- Build with `make`, run with `make run`, test with `make test`. In a shell running under Rosetta, prefix with `arch -arm64`.
- The game and the tests read `data/` relative to the directory they are run from: run them from the repository root.
- The recipe book in the pause menu (`src/recipeui.c`) is drawn from `src/guide.c`, which works out from the tables where each item comes from. A new way of getting an item (a new kind of drop, loot or reward) needs a line there, or the book will say the item cannot be got.
- `recipes.txt` is the list the newest batch of recipes was written out from (the earlier batches are in its history); nothing is removed from `data/recipes.json` because it is not in that file. `obtainment.txt` lists what they leave unobtainable or without a use; it is produced from the data, so regenerate or correct it when recipes, drops or world generation change.
- Block ids take two bytes in the world and in saves (up to 1023 kinds of block; 341 are in use). A feature that wants a great many look-alike blocks (a colour, a size) should still keep a number with the block (`World.meta`, as two-tone flowers do) rather than add a block for each.
- The interface carries no explanations: a menu names its controls and nothing else, and what a thing does is said in `README.md`. Check that new text fits where it is drawn at 1280 by 720.
- `tests/legacy/` is a small fixed set of recipes and benches that the tests of how the game works run against, so that changing what the game ships with does not break them. The shipped recipes have a test of their own (`test_shipped_recipes`) which runs first, against `data/` alone. When recipes change, change that test; leave `tests/legacy` alone.
- `tests/test_game.c` is headless and covers the rules. It looks content up by name, and its last test loads a small mod on top of the shipped data. Add a test with each rule change.
- Rendering code (`main.c`, `hud.c`, `benchui.c`, `menu.c`, `assets.c`) needs a window and is not covered by tests.
- `third_party/cjson` is vendored; do not edit it.
