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
| `tiles.json` | Blocks: whether they are solid, climbable or minable, which layer they are placed in, how many blocks tall they stand, names they used to have, how long they take to mine and with what kind of tool, what they drop, their textures, for benches their kind, tier, upgrade and how much fuel they hold, and for storage blocks their grid of slots |
| `items.json` | Items: label, stack size, how many slots they hold inside them and whether those open from the bar, what block they place, what tree they plant, how long they burn as fuel, what kind of tool they are and how fast, their trade category and price, and their texture |
| `recipes.json` | What each kind of bench makes, from what, for how many crystals, and at what bench tier |
| `trees.json` | Tree kinds: their blocks, fruit, full height, the shape of the crown row by row, which layer they grow in, and what they can be planted on |
| `worldgen.json` | World size, the plains, the mountains and valleys (any number of kinds), soil layers, caves and what is set into them, veins (what, in what, how big, how deep, how common, what shape), the spawn and its portal, and how trees are scattered |
| `trade.json` | The categories items are sold in, the coins and what each is worth, the default price, and the crystals a new player starts with |
| `rules.json` | Tick rate, day and night length, how bright the sun and moon are and how fast light fades, player step timings, starting items, and how drops behave |
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

Clicks queue up and run in order. Clicking a square that already has an action queued takes that action back, even if it has already started; the same goes for clicking an item again. A click that cannot be carried out is refused with a red cross on the tile. Pressing a movement key cancels the queue.

## Movement

Holding a direction, or a queued click, takes the player:

- up a one-block ledge;
- over a one-block gap with a short hop (hold `S` as well to go down into it instead);
- down a one-block drop with a plain step;
- over the edge of a drop of two or more blocks into a climbing pose on its wall, then down the wall;
- from one wall to a facing wall up to two columns away with a hop, so a pit two or three blocks wide is crossed near the top without going to the bottom;
- up any wall, pulling onto the top;
- through a gap one block high, crawling; the player is one block tall while under the low ceiling and stands up again on the far side;
- along the underside of a row of blocks, hand over hand like monkey bars.

To get onto a row of blocks overhead, press `W` under it: the player jumps and catches it if there is one clear block between their head and it. Holding a direction past the end of the row lets go. Pressing `W` at the end instead swings round onto the side of the last block, from where it is an ordinary climb onto the top; `S` on the side of a block swings back in under it rather than dropping off. A queued click uses all of these to find its way.

## World

The world is 2048 blocks round and wraps: keep going right and you come back to where you started. Height runs from Y −200 to Y 200, and the player spawns at Y 0. The top line of the window shows the player's X and Y.

The land is plains broken up by mountains and valleys, always with a stretch of plains between two of them:

- **Plains** roll gently between Y 0 and Y 20.
- **Mountains** rise 10 to 60 blocks above the plains in ledges of up to 3 blocks.
- **Steep valleys** drop 10 to 60 blocks down sheer walls to a flat floor.
- **Smooth valleys** drop 10 to 60 blocks along a long, gentle slope.

**Caves** wander through the stone, mostly sideways. They are carved out of stone and nothing else: a cave stops dead at the dirt, so none opens onto the plains, but where a mountain is bare stone a cave can break out through its side. Set into the caves, against a wall or on the floor:

- **Gems** - amethyst, sapphire, emerald, ruby, diamond, each rarer and deeper than the one before. They glow, more faintly than a torch. Tap one to pick it up; with a gem selected, click a backwall to hang it there as decoration.
- **Time crystals**, on cave floors. Mining one gives 5 crystals, which go straight to the player's crystals: they are never held in the inventory.

**Backwalls.** Behind every block there is a backwall of the same kind. Mining the block leaves the backwall, which is what the inside of a cave or a tunnel shows, drawn darker. Placing a block where there is no backwall gives it one; placing a block in front of an existing backwall leaves that backwall as it was. To take a backwall down, select a tool of the kind that mines it - shovel for dirt, pickaxe for stone, axe for wood - and click it while nothing stands in front. It drops nothing. Without that tool in hand a click on bare backwall is a move, as on any empty tile.

**Light.** Everything is lit from 0 to 15. The open sky is as bright as the sun by day (15) and the moon by night (5): it beams straight down every column until a block stops it, and fills any open space that has no backwall. Torches (14), a burning campfire (12), gems (6) and time crystals (4) give light of their own. From there light spreads, losing 1 with every tile of air and 2 with every block, so the ground is dark a few blocks down and a cave is dark until something lights it. A torch or a gem can be placed against a block or on a backwall.

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

The world has four layers from back to front: the backwall; the background layer, which holds the trees and the benches and is drawn slightly dimmed; the foreground layer, which holds the terrain and everything the player collides with; and the item layer, where dropped items and fruit are drawn in front of the blocks.

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

## Reach

The player can mine, place, plant, open and pick up within the ten tiles around their body, or the eight around them while crawling. A tile at a corner of that range cannot be reached when the two tiles between it and the body are both blocks: there is no reaching diagonally through a closed corner. The player goes somewhere else to do it if there is anywhere, and otherwise the action is refused.

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
| Workbench | Any portal | 1 dirt | The other benches, and a portal (1 stone, 100 crystals). Tier 2 costs 5 stone, tier 3 costs 5 iron ingots |
| Woodwork bench | Workbench | 1 wood | Chest (1 wood) |
| Tool bench | Workbench | 1 stone, 1 flint | Tools, a tier at a time (see Tools) |
| Craft bench | Workbench | 1 dirt | Basket (10 sticks) |
| Kiln | Workbench tier 2 | 10 dirt, 1 stick | Charcoal (1 stick) |
| Furnace | Workbench tier 2 | 5 stone, 1 stick | One ingot from one ore: copper and tin. Tier 2, for 5 iron ore: iron and gold. Tier 3, for 5 titanium ore: platinum and titanium |
| Campfire | Workbench | 10 sticks | Torch (1 stick). Burns fuel like the kiln, and gives light while it does |
| Metalwork bench | Workbench tier 3 | 3 iron ingots, 1 stone | Coins (see Money) |
| Trade portal | Any portal | 1 portal, 1 gold coin | Nothing: it is where things are bought and sold |

### Portals

The portal made at the workbench and the spawn portal are the same kind of bench: they offer the same recipes and climb the same tiers. A made portal and a trade portal stand three blocks tall, like the spawn portal. Each is one thing: placing it needs room for all three blocks, any of its blocks opens it, upgrading changes all of it, and removing any block takes the whole portal down and gives one item back. The spawn portal cannot be removed.

A portal makes a workbench (1 dirt) and a trade portal (1 portal and 1 gold coin), and has six tiers, each bought with one gem and each unlocking that gem's pickaxe (3 of the gem and 1 stick):

| Portal tier | Costs | Unlocks | Ore multiplier |
| --- | --- | --- | --- |
| 2 | 1 amethyst | Amethyst pickaxe | 2× |
| 3 | 1 sapphire | Sapphire pickaxe | 3× |
| 4 | 1 emerald | Emerald pickaxe | 4× |
| 5 | 1 ruby | Ruby pickaxe | 5× |
| 6 | 1 diamond | Diamond pickaxe, portal chest | 6× |

A gem pickaxe makes every ore block drop that many times over: the metal ores, coal, and time crystals. Like any tool it only has to be carried, and the best one counts.

The **portal chest** (1 chest and 100 crystals, at a tier 6 portal) opens onto one store of 16 slots that belongs to the player: every portal chest, in every world, shows the same contents. It is kept with the crystals in `saves/profile.txt`. It takes only plain items - a basket or chest with something in it is refused - and taking the block down leaves the contents where they are.

### Money

**Crystals** belong to the player, not to a world: the same number is there in every world. They are shown in a box at the top right, kept in `saves/profile.txt`, and written as soon as they change. A new player starts with 1000. They are spent on the portal and portal chest recipes, and gained by mining time crystals in caves.

**Coins** are ordinary items, in stacks of up to 99: 100 copper coins are worth 1 gold coin, and 100 gold coins 1 platinum coin. The metalwork bench makes and exchanges them:

| Tier | Recipes |
| --- | --- |
| 1 | 1 copper ingot → 10 copper coins; 1 gold coin → 100 copper coins |
| 2 (5 copper ingots) | 1 gold ingot → 1 gold coin; 100 copper coins → 1 gold coin; 1 platinum coin → 100 gold coins |
| 3 (5 gold ingots) | 5 platinum ingots → 1 platinum coin; 100 gold coins → 1 platinum coin |

### Trade portal

For trying things out, a new world starts the player with a trade portal and 1 gold coin as well as the basket, and any portal makes a trade portal or a gold coin for 1 dirt each. Both are in the data (`rules.json` and `recipes.json`) and are meant to come out again.

Clicking a placed trade portal offers to open or remove it. Open, it shows the coins held and a tab for each category of item - blocks, benches, materials, tools, food - plus a Convert tab.

- Pick an item to see its price and how many are held, then Buy 1, Buy 10, Sell 1 or Sell 10. Every item but the coins themselves is for sale, and for now everything costs 1 copper coin. An item cannot be added to the game without a category to sell it in: the game refuses to start and names the item.
- **Buying** spends the smallest coins first, and breaks a bigger coin into change when they do not cover the price: with 1 gold coin, buying a 1 copper item leaves 99 copper coins.
- **Selling** pays in copper coins and never exchanges them upward: selling that item back leaves a stack of 99 copper coins and 1 more.
- The **Convert** tab exchanges between neighbouring coins in either direction, such as 100 copper for 1 gold.

Anything bought or given as change that does not fit in the inventory is dropped at the player's feet.

### Tools

Every block that can be mined belongs to one kind of tool, and a tool of that kind makes mining it faster. The tool only has to be carried, in the bar or in a basket in the bar; the best one for the block is used.

| Tool kind | Blocks |
| --- | --- |
| Shovel | Dirt, grass, flint, copper- and tin-bearing dirt |
| Pickaxe | Stone, coal, all ores, kiln, furnace |
| Axe | Tree trunks and leaves, wood, chest, workbench, woodwork bench, tool bench, craft bench |

Every tool also takes 1 stick.

| Tool bench tier | Costs | Tools | From | Speed |
| --- | --- | --- | --- | --- |
| 1 | | Wooden pickaxe, wooden shovel | 1 wood | 1.5× |
| 1 | | Flint axe, flint shovel | 2 flint | 2× |
| 2 | 5 stone | Stone pickaxe, stone shovel | 5 stone | 3× |
| 3 | 5 bronze ingots | Bronze pickaxe, bronze axe | 5 bronze ingots | 4× |
| 3 | | Tin spade (a shovel) | 3 tin ingots | 4× |
| 4 | 5 iron ingots | Iron pickaxe, iron axe | 5 iron ingots | 5.5× |
| 4 | | Gold shovel | 3 gold ingots | 5.5× |
| 5 | 5 titanium ingots | Titanium pickaxe, titanium axe | 5 titanium ingots | 8× |

Bronze ingots are made at the metalwork bench, tier 1, from 1 copper ingot and 2 tin ingots.

Without the right tool a block takes its ordinary time. Tools do not stack and do not wear out.

Benches are placed in the background layer, like trees: the player walks in front of them, and an ordinary block can be placed in front of one. Chests are still solid blocks.

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
- `src/trade.c` — buying, selling and exchanging coins
- `src/light.c` — how well lit each tile is
- `src/game.c` — player, movement rules, path planning, action queue, item drops, crafting
- `src/save.c` — saving and loading a world
- `src/assets.c` — textures for blocks and items
- `src/hud.c` — the on-screen inventory bar
- `src/benchui.c` — the bench popup and crafting menu
- `src/tradeui.c` — the trade portal menu
- `src/menu.c` — main menu, pause menu and key bindings
- `src/view.h` — placing the wrapping world on screen
- `src/main.c` — window, input, camera and rendering
- `tests/test_game.c` — tests for the rules, with no window
- `third_party/cjson/` — the JSON parser (MIT licence)
- `AGENTS.md` — the standing rule for working on this project: keep everything data driven
