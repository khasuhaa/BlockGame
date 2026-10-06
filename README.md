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
| `tiles.json` | Blocks: whether they are solid, climbable or minable, what they grow into and on what soil, everything they drop, which layer they are placed in, how many blocks tall they stand and how the top and bottom of one look, names they used to have, how long they take to mine and with what kind of tool, what they drop, their textures, for benches their kind, tier, upgrade and how much fuel they hold, and for storage blocks their grid of slots |
| `items.json` | Items: label, stack size, how many slots they hold inside them and whether those open from the bar, what block they place, what tree or plant they grow, how long they burn as fuel, what kind of tool they are and how fast, their trade category and price, and their texture |
| `recipes.json` | What each kind of bench makes, from what, for how many crystals, and at what bench tier |
| `trees.json` | Tree kinds: their blocks, fruit, full height, the shape of the crown row by row or leaf by leaf, which layer they grow in, what they can be planted on, and whether they need water |
| `worldgen.json` | World size, the plains, the mountains and valleys (any number of kinds), soil layers, sea level, the climate maps and the biomes (their ground, trees and plants), caves and what is set into them, veins (what, in what, how big, how deep, how common, what shape), the spawn and its portal, and how trees are scattered |
| `mobs.json` | Creatures: how they move and the frames they walk through, where they turn up, what they lay, how long they take to grow up, what they leave, and the spawning range and cap |
| `trade.json` | The categories items are sold in, the coins and what each is worth, the default price, and the crystals a new player starts with |
| `rules.json` | Tick rate and the fastest it can be set, day and night length, the player's hunger, health and energy, what meditating brings, how bright the sun and moon are and how fast light fades, the fog of war, the steps of the zoom and where it becomes the world map, player step timings, starting items, and how drops behave |
| `textures/` | PNG images, 32 pixels to the block. A block can list several to vary between; one larger than a block overhangs it |

Blocks and items refer to each other by name. A mistake, such as a recipe for an item that does not exist, stops the game at startup with a message naming the file and the problem.

The loader can read a second directory on top of the first, adding new definitions and replacing ones with the same name. Nothing uses that yet; it is there for mods.

## Menus and saves

The game opens on a main menu with the saved worlds side by side. Each is shown as a picture of its spawn area - the ten blocks around the spawn portal, with whatever has been built or dug there - fading out toward the edges. The arrow keys, the mouse wheel, the arrow buttons or a click on a world to the side slide the row along; Play, Enter or a click on the world in the middle joins it. "New world" asks for a name and generates a fresh world from a random seed.

Worlds are saved in `saves/<name>.world`, next to where the game is run from. A save records its blocks and items by name, so it still loads after the data files change; anything that no longer exists is removed from it. Saves made by earlier versions of the game still load. A world is saved when it is created, every two minutes while playing, on "Save and quit to menu", and when the window is closed. Actions still queued at that moment are not kept.

`Esc` opens the pause menu, which stops the world and offers:

- **World settings** — the tick rate, from 1 to 300 ticks a second (10 by default), and whether the fog of war is on. Both are saved with the world.
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
| Click a creature | Queue hunting it |
| `Esc` | Pause menu |
| `F3` | Hide or show the readout in the corner |
| Drag | Pan the map |
| Mouse wheel | Zoom in and out; far enough out, the world map |
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

A step that goes up or down as well as across - onto a ledge, off one, over an edge onto a wall - is drawn going round the corner of the block: up and then across, or across and then down, whichever way the body has room. Nothing is drawn passing through a block. The player's legs and arm swing as they walk.

## World

The world is 2048 blocks round and wraps: keep going right and you come back to where you started. Height runs from Y −200 to Y 200, and the player spawns at Y 0. The top line of the window shows the player's X and Y.

The land is plains broken up by mountains and valleys, always with a stretch of plains between two of them:

- **Plains** roll gently between Y 0 and Y 20.
- **Mountains** rise 10 to 60 blocks above the plains in ledges of up to 3 blocks.
- **Steep valleys** drop 10 to 60 blocks down sheer walls to a flat floor.
- **Smooth valleys** drop 10 to 60 blocks along a long, gentle slope.

**Biomes.** Two smooth maps run round the world, one of heat and one of dryness, and what kind of country a column is follows from them and from the sea:

- **Plains** are the ordinary country: grass over dirt, with oaks, and carrots and flax growing wild.
- Each kind of tree keeps to its own country, so like grows with like: a **cherry grove** where it is cold, an **orchard** of apple trees where it is mild and damp, and the **tropics** - coffee and orange trees - where it is hot and damp. All three are grass over dirt like the plains. Which biome a column is is recorded with the world; a world saved before that has it worked out from its climate and the lie of the land, so there the name may not match the trees that were generated.
- **Desert** is where it is both hot and dry: sand instead of grass and dirt, and cacti instead of trees. The spawn is always on plains. Where a desert or a beach meets other country its sand does not stop at a line or break into stripes: it thins to a skin one block deep over the ordinary dirt at the edge, and deepens inward.
- **Ocean** is everything below Y 0. Its bed is sand, kelp and coral grow on it, and the open space above it is water up to Y 0. Valleys reach below Y 0, so every valley is a sea.
- **Beach** is the land within nine columns of a sea: sand, more of it the nearer the water.

One biome gives way to another gradually: toward the edge fewer and fewer columns belong to it, so the ground changes over a stretch rather than at a line.

**Water** is not a block: it is wherever the sky is open below sea level, with nothing solid and no backwall. There is no swimming; the player walks the sea bed and climbs kelp. Sunlight fades with depth. A hole dug into the sea bed has a backwall, so it stays dry.

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

An **apple** tree and an **orange** tree are smaller round-crowned trees with their own trunks and leaves, bearing apples and oranges; planting the fruit grows the tree. A **coffee** tree is a bush three blocks tall bearing coffee beans, and a bean planted on dirt or grass grows one. Eating a bean gives a fifth of a bar of energy, with a little hunger and health.

A **cactus** grows on sand: a trunk up to 5 blocks, with two arms that leave it sideways and turn up, and no leaves. Prickly pears grow on the arms, and planting one on sand grows a cactus. An arm grows out from the trunk a block at a time, so no part of one ever hangs in the air (`"connected"` in `trees.json`), and the blocks of a cactus are drawn joined up: one round stem that bends at the elbows and is capped at the tips. A block that lists others it `"joins"` in `tiles.json` is drawn from a sheet of sixteen pictures, one for each combination of neighbours.

**Kelp** grows on the sea bed, one straight stem up through the water and never above it, with no leaves and no fruit. It can be climbed. Cutting it part-way up takes away everything above the cut as well; what is below stays.

An **oak** grows to a 7-block trunk with a ball of leaves on top: rows 3, 5, 5 and 3 wide over 4 blocks of bare trunk. A **cherry** grows to an 8-block trunk with leaf rows 3, 3, 4, 4, 5 and 5 wide from the top down, over 2 blocks of bare trunk.

Breaking a leaf drops a stick; breaking a trunk drops wood, which can be placed as an ordinary block.

**Cutting a tree through kills what is above the cut.** Mining any block of a trunk turns every trunk block above it into a decayed trunk, and the leaves that hung on the trunk from the cut upward - the ones beside those blocks and beside the cut one, and any leaves reaching on from them sideways or upward - into decayed leaves. What is below the cut is untouched and goes on growing. Decayed blocks are no longer part of a tree: nothing grows or fruits on them, though they can still be climbed. A random event landing on one takes it away; a decayed trunk leaves its wood three times in four and a decayed leaf its stick one time in two, and mining them by hand goes by the same odds. Cacti die the same way, arms and all; kelp, as before, simply comes away above the cut. Which block a block decays into, that it `vanishes`, and its `drop_chance` are in `tiles.json`.

Trees stand in the background layer. The player walks straight past them, but can hold on to them: `W` in front of a tree climbs it, and the movement keys go up, down and sideways through the crown. Clicking any part of a tree queues mining that block, and the player climbs the tree to get within reach. A block in the foreground is always mined before the tree behind it.

The world has four layers from back to front: the backwall; the background layer, which holds the trees and the benches and is drawn slightly dimmed; the foreground layer, which holds the terrain and everything the player collides with; and the item layer, where dropped items and fruit are drawn in front of the blocks.

## Creatures

**Dodos** are the first creature. They walk about at random, along the level and up or down a step of one block; they will not cross a gap, drop further than one block, or go into water.

- **Where they come from.** A random event on open air just above grass, within 50 blocks of the player, can bring a wild dodo. None come while there are already 10 creatures within that range.
- **Where they go.** A wild dodo that gets more than 50 blocks from the player is gone. One the player owns never is.
- **Eggs.** A grown dodo has a small chance every tick of leaving a big blue egg where it stands, if nothing is there already. An egg is picked up by clicking it, like a gem, and can be put down again. A random event on an egg hatches it into a chick, which is full grown 600 ticks later.
- **Whose they are.** A dodo that hatches from an egg the player put down is the player's. One from an egg a dodo laid is wild, whoever the parent belonged to - unless the player picked that egg up and put it down again. A wild egg waits to hatch while the cap of 10 is reached; the player's eggs hatch regardless.
- **Hunting.** Clicking a dodo queues going after it, marked with a red ring; the player follows it as it moves and kills it on reaching it. Clicking it again calls that off. A grown dodo leaves 1 or 2 dodo meat and 1 or 2 feathers; a chick leaves nothing. A campfire cooks dodo meat.
- **How they move.** A dodo strides through its walking frames and bobs as it goes, setting off and stopping gently. Up a step it hops up and then forward, and down one it walks off and then drops, so it goes round the corner of a block and never through it. The frames and the height of the bob are in `mobs.json`.

**Dropbears** live at the top of trees. A random event on the trunk of a tree near the player can bring one, at most three near the player at once and one to a tree.

- **Ambush.** When the player comes within two columns of its tree, below it, it runs down the trunk, runs at them and bites - 15% of the health bar, and enough to wake a sleeper - then runs back up. It then leaves the player alone for 150 ticks. If the player gets away for long enough it gives up and goes home.
- **Homeless.** If its tree is cut down it drops to the ground and runs for the nearest tree within 24 columns, and goes up that. With none in sight it runs about until one is.
- Hunted like a dodo, it leaves 1 or 2 fur. Health never goes below nothing, and nothing more happens when it is at nothing.

## Plants

Carrots, flax and coral are single blocks that grow through four stages, one stage for each random event that lands on them. They can be pulled up at any stage, and give more the further they have grown.

| Plant | Grows | Planted with | On | Gives, by stage |
| --- | --- | --- | --- | --- |
| Carrot | Plains | A carrot | Dirt or grass | 1, 1, 2, 4 carrots |
| Flax | Plains | Flax seeds | Dirt or grass | 1 seed; 1 seed; 1 seed and 1 flax; 2 seeds and 3 flax |
| Coral | Ocean | Coral | Sand, under water | 1, 1, 2, 3 coral |

Coral only grows while it is under water. Plants stand in the background layer: they are walked past, and clicking one pulls it up.

## Inventory

The bar along the bottom has a head button followed by 8 slots. Stacks hold up to 99. The player starts with one basket (item ID 1).

- Tap a slot to select it. Tapping a basket also opens or closes its 4 slots, which extend upward from it. The name of the selected item shows above its slot for a couple of seconds whenever the selection changes, and the name of anything being dragged shows under the cursor.
- Tap an item inside a basket to select it; the closed basket then shows that item. Tapping an empty basket slot selects the basket.
- Tap the head button to open the 4 armor slots (head, chest, legs, boots) and every basket at once.
- Press and hold on a stack to pick items up from it: 1 after a moment, then 2, 3 … 10, then 15, 20, 25 … up to one fewer than the stack holds. The count stops rising once the cursor leaves the slot. Release over an empty slot or an equal stack to put them there; release anywhere else to put them back.
- Drag a stack away without waiting to move all of it: equal stacks merge, anything else swaps.
- Drag a stack off the inventory and let go over the world to drop it there - as many as were picked up, or the whole stack. It lands two blocks in front of the player (behind, if the way in front is blocked), far enough that it is not picked straight back up; walking over it or tapping it collects it again. A basket or chest is dropped with what is in it. Letting go over a menu drops nothing.
- A basket can be put inside another basket and keeps its contents, but only baskets sitting in the bar can be opened. Baskets never stack.
- Clicking or dragging the world, or pressing a movement key, closes everything.
- An item's picture is centred in its slot by what is actually drawn in it, whatever empty space its image has round it, and sits a little inside the slot.
- A selected food has an "Eat" button above it (see Health, hunger and energy).

Anything lying in the world - mined, dropped by the player, spilled from a bench, left by a creature, fallen from a tree - is gone 10 minutes after it landed, and blinks for its last 10 seconds. Fruit still on its tree does not count as lying anywhere. The time is `lifetime` under `drops` in `rules.json`.

A mined block drops as an item where it was. It is collected when the player is on or next to its tile, a moment after it drops, as long as there is room in the bar or in a basket in the bar.

## Health, hunger and energy

The button in the top left corner, under the readout, is the player. Under it are two small bars, health and hunger. Clicking it flies a panel out beside it with those two and a third, energy, and two buttons: **Sleep** and **Meditate**. Bars are green on dark red.

- **Hunger** runs down all the time: a full bar lasts 3600 ticks. Selecting a food in the inventory puts an **Eat** button above it; eating one fills some of the bar, a cooked meal much more than a berry. Eating to full and then eating again turns the bar golden: nothing more can be eaten (the button reads "Full"), and hunger holds off until that wears off - longer the bigger the meal.
- **Health** mends by itself, slowly, and a little with everything eaten - much less than the food does for hunger. With hunger at nothing it runs down instead, though never below a tenth.
- **Energy** runs down while awake: a full bar lasts 1800 ticks. It has no golden state.
- **Sleeping** lays the player on the ground and brings energy back; they wake by themselves when it is full. The button is greyed out while energy is full.
- **Meditating** sits the player down, with a progress bar over their head. Every 900 ticks it reaches 100% and something pops out beside them to be picked up: a gem (amethyst most often, diamond least), a fruit, or one of the six ores found in stone. (Coffee beans count as a fruit.)
- Either needs the player to be standing on the ground, and ends as soon as they are told to do anything else - a click on the world, a movement key - or the button is pressed again.
- While the player sleeps or meditates the ticks go three times as fast (30 a second instead of 10), and arrows at the top of the screen say so.

All the numbers, what each food is worth (`"food"` in `items.json`) and the list of what meditating brings are data.

## The readout

The top left corner shows, a line each: frames per second; the player's X and Y; the biome they are standing in; how many creatures are within range against the cap; and the time of day with the tick count and the rate ticks are going at. `F3` hides it and brings it back. The crystals are in the top right corner.

## Fog of war and the world map

A world starts blacked out. The player uncovers everything within 20 blocks of themselves as they move, and what has been uncovered stays uncovered: it is saved with the world. The edge is not a line; the outer 8 blocks of the circle fade into the black. Fog of war can be turned off and on in the pause menu's world settings - turning it off shows everything, and turning it back on brings back exactly what had been explored. A save from before the fog existed starts with only the area round the player uncovered. The radius, the fade and whether new worlds start fogged are in `rules.json`.

The mouse wheel zooms the view in and out in steps. Zoomed out past a threshold, the view fades into the world map: the whole world at a few pixels a block, each block drawn as the one average colour of its texture, with a marker where the player is. The map can be dragged about like the world, and wraps round like it, but nothing on it can be clicked. The fog of war covers the map too. Zooming back in returns to the world.

## Reach

The player can mine, place, plant, open and pick up within the ten tiles around their body, or the eight around them while crawling. A tile at a corner of that range cannot be reached when the two tiles between it and the body are both blocks: there is no reaching diagonally through a closed corner. The player goes somewhere else to do it if there is anywhere, and otherwise the action is refused.

## Building and benches

With a block selected (dirt or a workbench), clicking an air tile that touches a block queues placing it there, marked with a green outline and a faded copy of the block. Other air tiles still queue a move. Select a slot that is not a block to walk onto tiles next to the ground.

The spawn portal stands where the player spawns: two portal blocks on a base block set into the ground. It is a workbench that cannot be removed, so clicking it opens it straight away. Clicking a placed workbench shows "Open bench" and "Remove bench". Opening is a queued action: the player walks up to the bench first.

An open bench shows a menu beside it in the world:

- a row of the items it can make, lowest tier first, which scrolls sideways (wheel or drag) only when there are more than fit;
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
| Campfire | Workbench | 10 sticks | Torch (1 stick); cooked dodo meat (1 dodo meat). Burns fuel like the kiln, and gives light while it does |
| Metalwork bench | Workbench tier 3 | 3 iron ingots, 1 stone | Coins (see Money) |
| Trade portal | Any portal | 1 portal, 1 gold coin | Nothing: it is where things are bought and sold |

### Portals

The portal made at the workbench and the spawn portal are the same kind of bench: they offer the same recipes and climb the same tiers. Every portal is one base block with two portal blocks on it. The spawn portal's base is set into the ground, so two blocks of it stand above the surface; a made portal or a trade portal stands on the ground, base and all, three blocks tall. Each is one thing: placing it needs room for all three blocks, any of its blocks opens it, upgrading changes all of it, and removing any block takes the whole portal down and gives one item back. The spawn portal cannot be removed.

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

Anything with a fire in it - campfire, kiln, furnace - shows it: flames flickering while it has fuel, and a bed of blackened coals when it has none. The frames are the block's `texture_lit` in `tiles.json`.

## Chests

Clicking a placed chest offers "Open chest" and "Remove chest". Opening it (the player walks up first) shows its 16 slots in a 4×4 grid beside it. Items are dragged between the chest and the inventory bar exactly as they are within the bar, including picking up part of a stack by holding. While a chest is open, a click on a stack sends the whole stack across: from the inventory into the chest, or from the chest into the inventory. It tops up a matching stack first, then takes a free slot, and anything that does not fit stays where it was. (So with a chest open, a click moves a stack rather than selecting it; the number keys still select.) A basket put in a chest keeps what it holds.

Removing a chest gives a chest item that still holds everything that was in it, and placing that item puts it all back.

What an item holds is part of the item, and two stacks only merge if they hold the same things. So two chests with 5 wood in each stack together, and neither stacks with an empty chest. Taking one off such a stack gives it its own copy of the contents. Baskets never stack.
### Golden chests

Under a beach, now and then - about one column in thirty - a golden chest lies buried at the bottom of the sand, on the rock. It opens and is carried off like any chest. The first time it is opened or dug up it holds 1 to 5 gold coins, up to 3 gold ore, up to 3 iron ingots, perhaps a titanium ingot, and one gem: an amethyst most often, then sapphire, emerald, and a ruby least. What a block is found holding is its `"loot"` in `tiles.json`, and where it is buried is `"buried"` on a biome in `worldgen.json`; a golden chest the player puts down, or buys, holds only what they put in it.

## Layout

- `data/` — all game content (see Data above)
- `src/content.c` — loads the data files into the tables the engine reads
- `src/world.c` — the wrapping tile grid and terrain generation
- `src/tree.c` — tree shapes and growth
- `src/inventory.c` — stacks, baskets and the rules for moving them
- `src/craft.c` — crafting from the recipes
- `src/trade.c` — buying, selling and exchanging coins
- `src/mob.c` — creatures: spawning, wandering, eggs and hunting
- `src/light.c` — how well lit each tile is
- `src/game.c` — player, movement rules, path planning, action queue, item drops, crafting
- `src/save.c` — saving and loading a world
- `src/assets.c` — textures for blocks and items
- `src/hud.c` — the on-screen inventory bar
- `src/benchui.c` — the bench popup and crafting menu
- `src/tradeui.c` — the trade portal menu
- `src/playerui.c` — the player's button, bars and the sleep and meditate panel
- `src/menu.c` — main menu, pause menu and key bindings
- `src/view.h` — placing the wrapping world on screen
- `src/main.c` — window, input, camera and rendering
- `tests/test_game.c` — tests for the rules, with no window
- `third_party/cjson/` — the JSON parser (MIT licence)
- `AGENTS.md` — the standing rule for working on this project: keep everything data driven
