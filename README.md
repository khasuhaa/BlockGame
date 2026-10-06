# BlockGame

A small 2D block-based game engine in C, using [raylib](https://www.raylib.com) for the window, input and drawing.

## Build and run

```sh
make run          # build and start the game
make test         # headless tests of the game rules
```

The first build downloads raylib 5.5 into `vendor/` and compiles it. The game reads its content from `data/`, so run it from the repository root.

On an Apple Silicon Mac, if the shell is running under Rosetta the compiler may fail with an `xcrun` error; use `arch -arm64 make run` instead.

## Data

Everything the game contains is defined in `data/`, not in the code, so it can be changed without recompiling:

| File | Defines |
| --- | --- |
| `tiles.json` | Blocks: whether they are solid, climbable or minable, how long they take to mine and with what kind of tool, what they drop, their textures, for benches their kind, tier, upgrade and how much fuel they hold, and for storage blocks their grid of slots |
| `items.json` | Items: label, stack size, how many slots they hold inside them and whether those open from the bar, what block they place, what tree they plant, how long they burn as fuel, what kind of tool they are and how fast, and their texture |
| `recipes.json` | What each kind of bench makes, from what, and at what bench level |
| `trees.json` | Tree kinds: their blocks, fruit, full height, the shape of the crown row by row, which layer they grow in, and what they can be planted on |
| `worldgen.json` | World size, the plains, the mountains and valleys (any number of kinds), soil layers, veins (what, in what, how big, how deep, how common, what shape), the spawn and its portal, and how trees are scattered |
| `rules.json` | Tick rate, day and night length, player step timings, starting items, and how drops behave |
| `textures/` | PNG images, 32 pixels to the block. A block can list several to vary between; one larger than a block overhangs it |

Blocks and items refer to each other by name. A mistake, such as a recipe for an item that does not exist, stops the game at startup with a message naming the file and the problem.

The loader can read a second directory on top of the first, adding new definitions and replacing ones with the same name. Nothing uses that yet; it is there for mods.

## Menus and saves

The game opens on a main menu listing the saved worlds. Pick one to join it, or choose "New world", give it a name, and a fresh world is generated from a random seed.

Worlds are saved in `saves/<name>.world`, next to where the game is run from. A save records its blocks and items by name, so it still loads after the data files change; anything that no longer exists is removed from it. Saves made by earlier versions of the game still load. A world is saved when it is created, every two minutes while playing, on "Save and quit to menu", and when the window is closed. Actions still queued at that moment are not kept.

`Esc` opens the pause menu, which stops the world and offers:

- **World settings** — the tick rate, from 1 to 60 ticks a second (10 by default). It is saved with the world.
- **Keybinds** — the keys for moving left and right, climbing up and down, and making the camera follow the player. Click one, then press the new key; a key already in use swaps with the one it replaces. Bindings are shared by all worlds and kept in `saves/keybinds.txt`.

## Controls

| Input | Effect |
| --- | --- |
| `A` / `D` | Move left / right over whatever is in the way (see below) |
| `W` / `S` | Climb up / down a wall; `W` on the ground jumps and catches a wall if one is beside you |
| Click an air tile | Queue a move that puts the player's feet or head on that tile |
| Click a ground tile | Queue walking next to that tile and mining it |
| Click air beside a block, holding a block | Queue walking next to that tile and placing the block |
| Click dirt or grass, holding an acorn or cherry | Queue walking next to it and planting a tree on top |
| Click a bench or a chest | Open it, or choose between opening and removing it |
| Click a dropped item or a fruit | Queue walking up to it and picking it up |
| `Esc` | Pause menu |
| Drag | Pan the map |
| `Space` | Make the camera follow the player again |
| `1`–`8` | Select an inventory slot |

The movement keys and `Space` can be changed in the pause menu.

Clicks queue up and run in order. A click that cannot be carried out is refused with a red cross on the tile. Pressing a movement key cancels the queue.

## Movement

Holding a direction, or a queued click, takes the player:

- up a one-block ledge;
- over a one-block gap with a short hop (hold `S` as well to go down into it instead);
- down a one-block drop with a plain step;
- over the edge of a drop of two or more blocks into a climbing pose on its wall, then down the wall;
- from one wall to a facing wall up to two columns away with a hop, so a pit two or three blocks wide is crossed near the top without going to the bottom;
- up any wall, pulling onto the top.

## World

The world is 2048 blocks round and wraps: keep going right and you come back to where you started. Height runs from Y −200 to Y 200, and the player spawns at Y 0. The top line of the window shows the player's X and Y.

The land is plains broken up by mountains and valleys, always with a stretch of plains between two of them:

- **Plains** roll gently between Y 0 and Y 20.
- **Mountains** rise 10 to 60 blocks above the plains in ledges of up to 3 blocks.
- **Steep valleys** drop 10 to 60 blocks down sheer walls to a flat floor.
- **Smooth valleys** drop 10 to 60 blocks along a long, gentle slope.

Veins of other blocks run through the ground:

| Vein | Found in | Size | How common |
| --- | --- | --- | --- |
| Flint | Dirt, never touching open air | 4–10 | Fairly common |
| Copper, tin | Stone, down to about 90 below the surface | 2–6 | Common |
| Coal | Stone, down to about 90 below the surface | 5–10 | Common, the same as copper and tin |
| Copper, tin | Dirt | 2–6 | Fairly common |
| Copper, tin "scratches" | Stone: one long thin line on a shallow slant | 10–30 | Rare |
| Iron, gold | Stone, deeper | 2–8 | Rarer |
| Platinum, titanium | Stone, deepest | 2–3 | Rarest |

Veins of the same material never touch, so each stays within its size. Mining flint gives flint and mining any ore gives that ore; the harder ores take longer to mine.

Columns are grass over dirt over stone. The dirt is 4 or 5 blocks deep on the plains and thins out gradually up a mountain or down a valley, so peaks and valley floors are bare stone. Stone takes twice as long to mine as dirt. Grass drops dirt.

## Time

World time runs in ticks, 10 a second unless the world's tick rate is changed. A day is 1000 ticks and a night 800, with the light fading over 80 ticks at dusk and dawn. Movement, mining and crafting run in real time and are not affected by the tick rate.

Every tick, one random block in each 16-column chunk gets a random event. A chunk is the full height of the world, so any one block is hit about once every 6400 ticks.

## Trees

Trees are generated on the plains at every stage of growth, and keep growing:

- A random event on a **trunk** block makes its tree grow one step: either one new leaf, added outward from the trunk, or one more block of trunk. When the trunk extends, all the leaves move up a row with it; a leaf with a block in its way is lost.
- A random event on a **leaf** hangs a fruit on it: an acorn on an oak, a cherry on a cherry tree. Fruit hovers on its leaf, never falls off by itself, and disappears after 1800 ticks. It is not collected by walking past; click it to queue picking it. If its leaf is broken it falls and can be collected like any other item.

With an acorn or a cherry selected, clicking dirt or grass that has room above it queues planting: the player walks over and a single trunk block appears on top, which then grows by random events like any other tree. Where it cannot be planted (other blocks, or something in the way above), the click mines as usual.

An **oak** grows to a 7-block trunk with a ball of leaves on top: rows 3, 5, 5 and 3 wide over 4 blocks of bare trunk. A **cherry** grows to an 8-block trunk with leaf rows 3, 3, 4, 4, 5 and 5 wide from the top down, over 2 blocks of bare trunk.

Breaking a leaf drops a stick; breaking a trunk drops wood, which can be placed as an ordinary block.

Trees stand in the background layer. The player walks straight past them, but can hold on to them: `W` in front of a tree climbs it, and the movement keys go up, down and sideways through the crown. Clicking any part of a tree queues mining that block, and the player climbs the tree to get within reach. A block in the foreground is always mined before the tree behind it.

The world has three layers from back to front: the background layer, which holds the trees and is drawn slightly dimmed; the foreground layer, which holds the terrain and everything the player collides with; and the item layer, where dropped items and fruit are drawn in front of the blocks.

## Inventory

The bar along the bottom has a head button followed by 8 slots. Stacks hold up to 99. The player starts with one basket (item ID 1).

- Tap a slot to select it. Tapping a basket also opens or closes its 4 slots, which extend upward from it.
- Tap an item inside a basket to select it; the closed basket then shows that item. Tapping an empty basket slot selects the basket.
- Tap the head button to open the 4 armor slots (head, chest, legs, boots) and every basket at once.
- Press and hold on a stack to pick items up from it: 1 after a moment, then 2, 3 … 10, then 15, 20, 25 … up to one fewer than the stack holds. The count stops rising once the cursor leaves the slot. Release over an empty slot or an equal stack to put them there; release anywhere else to put them back.
- Drag a stack away without waiting to move all of it: equal stacks merge, anything else swaps.
- A basket can be put inside another basket and keeps its contents, but only baskets sitting in the bar can be opened. Baskets never stack.
- Clicking or dragging the world, or pressing a movement key, closes everything.

A mined block drops as an item where it was. It is collected when the player is on or next to its tile, a moment after it drops, as long as there is room in the bar or in a basket in the bar.

## Building and benches

With a block selected (dirt or a workbench), clicking an air tile that touches a block queues placing it there, marked with a green outline and a faded copy of the block. Other air tiles still queue a move. Select a slot that is not a block to walk onto tiles next to the ground.

The spawn portal stands where the player spawns, three blocks tall on a base block set into the ground. It is a workbench that cannot be removed, so clicking it opens it straight away. Clicking a placed workbench shows "Open bench" and "Remove bench". Opening is a queued action: the player walks up to the bench first.

An open bench shows a menu beside it in the world:

- a row of the items it can make, which scrolls sideways (wheel or drag) only when there are more than fit;
- for the selected item, its recipe and output, a slider from 1 to 99 that stops at what the inventory can pay for, and a Craft button or "Not enough items".

Each item takes 100 ms to craft. Output that does not fit in the inventory is dropped at the player's feet. The bench closes when the world is clicked, a movement key is pressed, or the player moves out of reach, and that ends any crafting still running.

A bench can have tiers. Its menu shows its tier and, if it can go higher, an Upgrade button with the cost; upgrading happens in place and keeps the menu open. A recipe that needs a higher tier is still shown in the row, darkened with a padlock and the tier it needs, and cannot be selected. A bench that is removed comes back at the tier it had reached.

| Bench | Made at | From | Makes |
| --- | --- | --- | --- |
| Workbench | Spawn portal | 1 dirt | The other benches. Tier 2 costs 5 stone |
| Woodwork bench | Workbench | 1 wood | Chest (1 wood) |
| Tool bench | Workbench | 1 stone, 1 flint | Wooden pickaxe, wooden shovel (1 wood, 1 stick each); flint axe, flint shovel (2 flint, 1 stick each). Tier 2, for 5 stone: stone pickaxe, stone shovel (5 stone, 1 stick each) |
| Craft bench | Workbench | 1 dirt | Basket (10 sticks) |
| Kiln | Workbench tier 2 | 10 dirt, 1 stick | Charcoal (1 stick) |
| Furnace | Workbench tier 2 | 5 stone, 1 stick | One ingot from one ore: copper and tin. Tier 2, for 5 iron ore: iron and gold. Tier 3, for 5 titanium ore: platinum and titanium |

### Tools

Every block that can be mined belongs to one kind of tool, and a tool of that kind makes mining it faster. The tool only has to be carried, in the bar or in a basket in the bar; the best one for the block is used.

| Tool kind | Blocks |
| --- | --- |
| Shovel | Dirt, grass, flint, copper- and tin-bearing dirt |
| Pickaxe | Stone, coal, all ores, kiln, furnace |
| Axe | Tree trunks and leaves, wood, chest, workbench, woodwork bench, tool bench, craft bench |

| Tool | Speed |
| --- | --- |
| Wooden pickaxe, wooden shovel | 1.5× |
| Flint axe, flint shovel | 2× |
| Stone pickaxe, stone shovel | 3× |

Without the right tool a block takes its ordinary time. Tools do not stack and do not wear out.

### Fuel

The kiln and the furnace burn fuel. A gauge on the side of their menu shows how much is left; it runs down all the time, by the tick, whether or not anything is being made. With none left the Craft button is replaced by "Needs fuel", and a crafting run in progress stops.

Clicking the gauge opens a list of the fuel in the inventory, each with how much of the gauge one of it fills; clicking an item burns one. Sticks, wood and coal are fuel, coal much the best. A bench that is removed loses the fuel in it.

## Chests

Clicking a placed chest offers "Open chest" and "Remove chest". Opening it (the player walks up first) shows its 16 slots in a 4×4 grid beside it. Items are dragged between the chest and the inventory bar exactly as they are within the bar, including picking up part of a stack by holding. A basket put in a chest keeps what it holds.

Removing a chest gives a chest item that still holds everything that was in it, and placing that item puts it all back.

What an item holds is part of the item, and two stacks only merge if they hold the same things. So two chests with 5 wood in each stack together, and neither stacks with an empty chest. Taking one off such a stack gives it its own copy of the contents. Baskets never stack.

## Layout

- `data/` — all game content (see Data above)
- `src/content.c` — loads the data files into the tables the engine reads
- `src/world.c` — the wrapping tile grid and terrain generation
- `src/tree.c` — tree shapes and growth
- `src/inventory.c` — stacks, baskets and the rules for moving them
- `src/craft.c` — crafting from the recipes
- `src/game.c` — player, movement rules, path planning, action queue, item drops, crafting
- `src/save.c` — saving and loading a world
- `src/assets.c` — textures for blocks and items
- `src/hud.c` — the on-screen inventory bar
- `src/benchui.c` — the bench popup and crafting menu
- `src/menu.c` — main menu, pause menu and key bindings
- `src/view.h` — placing the wrapping world on screen
- `src/main.c` — window, input, camera and rendering
- `tests/test_game.c` — tests for the rules, with no window
- `third_party/cjson/` — the JSON parser (MIT licence)
- `AGENTS.md` — the standing rule for working on this project: keep everything data driven
