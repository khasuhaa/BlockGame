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

- **Recipes** — the recipe book. On the left is every item in the game in a grid; type to narrow it down by name. Click an item to see every recipe that makes it: the bench and tier, what goes in and how many, how long it takes, and any crystals. Click anything in a recipe - an ingredient, or the bench - to look that up in turn, and so on down to the things that are not made at all, where the book says how they are got instead: which block to mine (and where it lies, and which tool is quicker), which creature leaves it and how often, which tree bears it and in which biome, which chest holds it, whether meditating brings it. Under that is everything the item is used to make. **Back** (or `Esc`) steps back the way you came, and the trail of items along the top jumps straight to one. The book is worked out from the data files as they are loaded (`src/guide.c`), so a mod's items and recipes are in it without anything being written for them.
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
- **Ocean** is everything below Y 0. Its bed is sand, kelp and coral grow on it, and it is filled with water up to Y 0. Valleys reach below Y 0, so every valley is a sea.
- **Beach** is the land within nine columns of a sea: sand, more of it the nearer the water.

One biome gives way to another gradually: toward its edge its own ground thins to a skin over the ordinary soil, so the ground changes over a stretch rather than at a line.

**Water** is real, and there is always the same amount of it. Every square of the world holds so many eighths of water, from none to 8. It is never made or lost by moving:

- it **falls** into any open square beneath it;
- on anything that holds it up - ground, or a square already full - it **levels out**: an eighth at a time, a few times a tick, water runs from a square to the nearest one standing two or more eighths lower, nearest first. It only feels a difference within 32 squares. So a dip fills in from round about it, and a bucketful emptied on level ground spreads into a puddle one eighth deep and eight squares wide round where it landed, and no further;
- where the floor stops, it **runs over the edge**;
- a block put down in it **pushes it aside**, upward first.

So breaking a block out of the sea bed makes the water above dip into the hole and then flatten out again, leaving the block's worth of water missing as one eighth from each of eight squares of the surface over the hole. There is no pressure: water does not rise up the far side of a U-bend. A new world's seas start full and still, having already run into whatever caves open on them. Only the columns where something has just changed are worked on, so water at rest costs nothing; a world saved before there was real water gets its seas filled in the same way when it is first opened. How much a stretch moves each tick is `flow` under `water` in `rules.json`.

A square at least half full counts as under water: kelp and coral need that, creatures keep out of it, and sunlight fades with the depth of water overhead.

**Swimming.** In water the player is afloat: it holds them up wherever they are in it, and the movement keys or a click take them up, down and sideways through it, as through the crown of a tree. With ground under their feet they walk the bottom as before. At the surface they can pull out onto the side. A swim goes straight to where it is going - on the slant, the rise spread evenly along the way - rather than along and then up, and round whatever is in the way.

**Buckets.** A bucket (3 iron ingots at a tier 2 metalwork bench) holds up to 8 eighths of water, shown as a blue bar under it. With one selected, clicking a square that has water in it scoops that water up: only as much as is there, and only as much as the bucket has room for, so a bucket that has taken a 5/8 square is 5/8 full, and taking another 5/8 square fills it and leaves 2/8 behind. With no room in the bucket, or no water in the square, a click empties it there instead: everything it holds goes into that square - standing above it if it is more than the square can take - and is left to fall and find its own level. Water only ever changes places.

**Oxygen** is a fourth bar, blue, in the player's panel, and shows under the player's button whenever it is not full. It runs down while the player's head is under water - a full bar lasts 300 ticks - and comes back quickly in the air. With none left, health runs down instead, and a sleeper wakes. Eating kelp gives half a bar back, and can be done for that even on a full stomach.

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

Veins of the same material never touch, so each stays within its size. Mining flint gives flint and mining any ore gives that ore; **clay** lies in the dirt in veins of its own, by exactly the rules flint does (4 to 10 blocks, 1 to 6 down, never touching the open air), and is dug the same way; the harder ores take longer to mine.

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
- **Hunting.** Clicking a creature queues going after it, marked with a red ring; the player follows it as it moves and, once within reach, hits it once (see Fighting). Each click is one blow; to hit it again, click it again. Clicking it again calls that off. A grown dodo leaves 1 or 2 dodo meat and 1 or 2 feathers; a chick leaves nothing. A campfire cooks dodo meat.
- **No stopping at each block.** Any creature that is going somewhere goes on from one block into the next without a pause between, and is drawn gliding after its position, so a run, a climb or a swim is one flowing movement.
- **How they move.** A dodo strides through its walking frames and bobs as it goes, setting off and stopping gently. Up a step it hops up and then forward, and down one it walks off and then drops, so it goes round the corner of a block and never through it. The frames and the height of the bob are in `mobs.json`.

**The dodo's family tree.** A dodo does not always lay its own egg. Three times in four it does; the fourth time it lays the egg of one of the kinds that come next after it, each of those as likely as the other. Every kind has an egg of its own colour that hatches into that kind, and only the ordinary dodo ever turns up in the wild: the rest are bred.

| Kind | Breeds on into | Leaves, besides meat and feathers | How often |
|---|---|---|---|
| Dodo | Stone dodo or Dirt dodo | — | — |
| Dirt dodo | Wood dodo or Flint dodo | 1 dirt | always |
| Wood dodo | — | 1 wood | 50% |
| Flint dodo | — | 1 flint | 50% |
| Stone dodo | Amethyst dodo or Copper dodo | 1 stone | always |
| Copper dodo | Tin dodo | 1 copper ore | 50% |
| Tin dodo | Iron dodo | 1 tin ore | 50% |
| Iron dodo | Gold dodo | 1 iron ore | 33% |
| Gold dodo | Titanium dodo | 1 gold ore | 33% |
| Titanium dodo | Platinum dodo | 1 titanium ore | 20% |
| Platinum dodo | — | 1 platinum ore | 10% |
| Amethyst dodo | Sapphire dodo | 1 amethyst | 50% |
| Sapphire dodo | Emerald dodo | 1 sapphire | 33% |
| Emerald dodo | Ruby dodo | 1 emerald | 25% |
| Ruby dodo | Diamond dodo | 1 ruby | 20% |
| Diamond dodo | — | 1 diamond | 10% |

A kind at the end of its line only ever lays its own egg. All of this is in `mobs.json`: what a kind breeds on into is `"next"` under its `"lays"`, and how often it leaves a thing is the `"chance"` on that drop.

**Fighting.** Every creature has health, and each blow takes some off. A click on a creature is one blow, which takes 0.35 seconds to land once the player is within reach; it is not kept up. A creature that has been hurt shows a small red bar. What a blow does depends on what is in the player's hand:

| In hand | A blow does | Made at the tool bench from |
|---|---|---|
| Anything else, or nothing | 2 | — |
| Flint spear | 3 | 2 flint, 1 stick (tier 1) |
| Bronze sword | 5 | 5 bronze ingots, 1 stick (tier 3) |
| Iron sword | 10 | 5 iron ingots, 1 stick (tier 4) |
| Titanium sword | 20 | 5 titanium ingots, 1 stick (tier 5) |

**Bows** hit from a distance. With one in hand, a click on a creature up to 10 blocks away looses an arrow from where the player stands, as long as nothing solid is in the way; further off than that, or with a wall between, the player walks until the shot is clear and then takes it. The **copper bow** does 2. The **gold bow** does 5 and poisons what it hits: 5 more damage, 1 every 2 ticks over the next 10. Poison from a second arrow adds to what is still to come. A bow is an item with a `"range"`, and poison is its `"poison": { "damage", "every" }`.

Dodos of every kind have 5 health, fish 5, dropbears 10 and sharks 25. A weapon is an item with a `"damage"`; a creature's `"health"` is in `mobs.json`.

Creatures of the water are counted apart from those of the land: each has a cap of 10 near the player, so fish do not keep dodos from turning up, nor dodos fish.

**Fish** turn up in the water of an ocean, up to eight near the player, and swim about in it without stopping, one square running on into the next; they never leave the water. One leaves a fish, which can be eaten raw or cooked at a campfire.

**Sharks** turn up there too, far less often, and no more than three near the player. A shark is 1 or 2 blocks long, and is drawn that long. It takes no notice of a player on land or on the sea bed's shore, but one who is in the water within 24 blocks it goes for: it swims straight at them, bites (a fifth of the health bar), makes off for a moment, and comes round again. Killed, it leaves 3 or 4 fish and its **head**.

A **shark head** is a trophy the size of the shark it came from. It can be hung up like any background block, and is drawn bigger the bigger the shark was. (Heads can be any of eight sizes; sharks now only come in the first two.) Heads stack whatever their sizes; the one on top of the stack is the one shown, at its size, and the one that is put up. Taken down, a head comes back the size it was.

**Dropbears** live at the top of trees. A random event on the trunk of a tree near the player can bring one, at most three near the player at once and one to a tree.

- **Ambush.** When the player comes within two columns of its tree, below it, it lets go and drops on them - the whole height of the tree in one fall, gathering speed, landing beside them - and bites - 15% of the health bar, and enough to wake a sleeper - then runs back to the tree and up it in one unbroken climb. It then leaves the player alone for 150 ticks. If the player gets away for long enough it gives up and goes home.
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
- A stack has an order: things are taken off the **top** of it, and things added go on top. For most items that makes no difference. For items with a **meter** - a bucket's water, or a tool's wear if it is given a `"durability"` in `items.json` (none of the shipped tools is) - each one keeps its own reading, a stack of them holds them whatever they read, and the one on top is the one that is shown and used. So taking half a stack off and putting the rest back on top of it leaves what was the top one in the middle.
- Tools stack.
- Press and hold on a stack to pick items up from it, from the top down: 1 after a moment, then 2, 3 … 10, then 15, 20, 25 … up to one fewer than the stack holds. The count stops rising once the cursor leaves the slot. Release over an empty slot or an equal stack to put them there; release anywhere else to put them back.
- Drag a stack away without waiting to move all of it: equal stacks merge, anything else swaps.
- Drag a stack off the inventory and let go over the world to drop it there - as many as were picked up, or the whole stack. It drops right where the player is, and stays there while they stand over it: it is only picked up again once they have walked out of reach of it and come back, or by tapping it to queue picking it up. A basket or chest is dropped with what is in it. Letting go over a menu drops nothing.
- A basket can be put inside another basket and keeps its contents, but only baskets sitting in the bar can be opened. Baskets never stack.
- Each basket in the bar remembers which item inside it was last chosen. Closed, it shows that item; choosing its bar slot (a tap, or its number key) chooses that item.
- **Picking up a basket with no room for it** still works as long as some bar slot holds something that is not a basket: the basket takes that slot (the first such), and what was there goes inside the basket as its chosen item. So that slot still shows, and still gives, the same thing. If it was the selected slot, that item stays selected; if it was not, the selection does not change.
- Clicking or dragging the world, or pressing a movement key, closes everything.
- An item's picture is centred in its slot by what is actually drawn in it, whatever empty space its image has round it, and sits a little inside the slot.
- A selected food has an "Eat" button above it (see Health, hunger and energy).

Anything lying in the world - mined, dropped by the player, spilled from a bench, left by a creature, fallen from a tree - is gone 10 minutes after it landed, and blinks for its last 10 seconds. Fruit still on its tree does not count as lying anywhere. The time is `lifetime` under `drops` in `rules.json`.

Items float: one dropped in water rises to the top of it, and one that falls onto water stops there, riding up and down with the water. (`float_speed` under `drops` in `rules.json`; 0 makes them sink.)

A mined block drops as an item where it was. It is collected when the player is on or next to its tile, a moment after it drops, as long as there is room in the bar or in a basket in the bar.

## Making a character

A world does not come with a character. On joining a new one there is a panel at the top of the screen: type a name and press Enter (or the button) and the character is made at the spawn portal, with what a new player starts with. Until then there is nothing to do but look about - drag, zoom, the map.

The same panel comes back if every character has died and been removed. A character can be made at any portal the player has uncovered, not only the spawn portal: click the portal, or zoom out to the map and click its mark there, and the panel says which one it will be. A character made later this way starts with nothing: a world's starting items are given once.

## Dying

Losing the whole health bar is not death. With health gone, whatever hurts next comes off a second bar, red on black, that shows in health's place ("Dying" in the panel). It is as long as the health bar. Mending - by itself, or from food - fills the death bar back up before it does anything for health. Only when the death bar is gone as well is the character dead. Bites and drowning can do it; hunger alone still stops short.

A dead character lies where they fell, grey, and does nothing; their queue is dropped, creatures ignore them, and if they were the one being played and somebody else is alive, that one is played instead. They come back by themselves after 1800 ticks, at the spawn portal, whole, with everything they were carrying.

Over a dead character in the world, and beside their button on the left when it is clicked, is a menu with how long is left and two buttons:

- **Respawn now**, for crystals: 100 just after dying, and less the more of the wait is done - 50 half-way through, 1 at the very end.
- **Remove character**, which has to be pressed twice. They are gone for good, everything they carried is left lying where they died, and their place is free for a new character to be made.

The length of the death bar, the wait and the price are `death` in `rules.json`.

## More than one character

A portal makes a **new player**: another character to play, for 100 crystals and 5 fruit. The fruit is apples, oranges and cherries in a mix that is each world's own - 1 apple and 4 cherries in one, 3 oranges and 2 apples in another - and the recipe shows which. The new character appears standing at the portal with nothing, and is called "Player 2". For now a world can have one of them: two characters in all (`players` in `rules.json`).

Each character has a body, a bar of items, a queue of actions and health, hunger, energy and oxygen of their own. Each has a button down the left of the screen, with their name beside it and their bars under it; the one being played is edged in gold and has a small arrow over their head. **Clicking another character's button plays them**: the view, the inventory bar and the menus go over to them. Clicking the button of the one already being played opens their panel, as before.

Whoever is not being played carries on with whatever they were told to do - walking, mining, crafting, sleeping, meditating - so both can be kept busy at once: queue something for one, switch, queue something for the other. Things they walk over go into their own bar, and their needs run down like anyone's. Creatures treat whichever character is nearest them as the player: they turn up and go away by that one's distance, and a dropbear or a shark goes for that one. Time only runs fast when *every* character is asleep or meditating. Something one character drops stays where it is until nobody is standing over it.

## Health, hunger and energy

The button in the top left corner, under the readout, is the player. Under it are two small bars, health and hunger. Clicking it flies a panel out beside it with those two and a third, energy, and two buttons: **Sleep** and **Meditate**. Bars are green on dark red.

- **Hunger** runs down all the time: a full bar lasts 3600 ticks. Selecting a food in the inventory puts an **Eat** button above it; eating one fills some of the bar, a cooked meal much more than a berry. Eating to full and then eating again turns the bar golden: nothing more can be eaten (the button reads "Full"), and hunger holds off until that wears off - longer the bigger the meal.
- **Health** mends by itself, slowly, and a little with everything eaten - much less than the food does for hunger. With hunger at nothing it runs down instead, though never below a tenth.
- **Energy** runs down while awake: a full bar lasts 1800 ticks. It has no golden state.
- **Sleeping** lays the player on the ground and brings energy back; they wake by themselves when it is full. The button is greyed out while energy is full.
- **Meditating** sits the player down, with a progress bar over their head. Every 900 ticks it reaches 100% and something drops out right on top of them, and is picked up like anything else at their feet: a gem (amethyst most often, diamond least), a fruit, or one of the six ores found in stone. (Coffee beans count as a fruit.)
- While meditating neither hunger nor energy runs down.
- Being attacked wakes the player from either.
- Either needs the player to be standing on the ground, and ends as soon as they are told to do anything else - a click on the world, a movement key - or the button is pressed again.
- While the player sleeps or meditates the ticks go three times as fast (30 a second instead of 10), and arrows at the top of the screen say so.

All the numbers, what each food is worth (`"food"` in `items.json`) and the list of what meditating brings are data.

## The readout

The top left corner shows, a line each: frames per second; the player's X and Y; the biome they are standing in; how many creatures are within range against the cap, for the land and for the water separately; and the time of day with the tick count and the rate ticks are going at. `F3` hides it and brings it back. The crystals are in the top right corner.

## Fog of war and the world map

A world starts blacked out. The player uncovers everything within 20 blocks of themselves as they move, and what has been uncovered stays uncovered: it is saved with the world. The edge is not a line; the outer 8 blocks of the circle fade into the black. Fog of war can be turned off and on in the pause menu's world settings - turning it off shows everything, and turning it back on brings back exactly what had been explored. A save from before the fog existed starts with only the area round the player uncovered. The radius, the fade and whether new worlds start fogged are in `rules.json`.

The mouse wheel zooms the view in and out in steps, about the cursor: whatever is under the mouse stays under it. Zooming lets the camera go of the player, as dragging does; `Space` makes it follow again. Zoomed out past a threshold, the view fades into the world map: the whole world at a few pixels a block, each block drawn as the one average colour of its texture, with a marker where each character is, ringed for the one being played. The map can be dragged about like the world, and wraps round like it. Every portal the player has uncovered is marked on it with a small purple doorway, and that is the one thing on the map that can be clicked: the view goes in on that portal and its menu comes up, Teleport button and all. Nothing else on the map answers a click. The fog of war covers the map too. Zooming back in returns to the world.

## Reach

The player can mine, place, plant, open and pick up within the ten tiles around their body, or the eight around them while crawling. A tile at a corner of that range cannot be reached when the two tiles between it and the body are both blocks: there is no reaching diagonally through a closed corner. The player goes somewhere else to do it if there is anywhere, and otherwise the action is refused.

## Building and benches

With a block selected (dirt or a workbench), clicking an air tile that touches a block queues placing it there, marked with a green outline and a faded copy of the block. Other air tiles still queue a move. Select a slot that is not a block to walk onto tiles next to the ground.

The spawn portal stands where the player spawns: two portal blocks on a base block set into the ground. It is a workbench that cannot be removed, so clicking it opens it straight away. Clicking a placed workbench shows "Open bench" and "Remove bench". Opening a bench or a portal needs no walking: its menu comes up from wherever the player is. **Doing** anything at it does: crafting, upgrading, adding fuel, and buying, selling or exchanging at a trade portal are each done at once if the player is standing at it, and otherwise are queued - the player walks there, and it happens on arriving. (From a distance the Craft button reads "Go and craft".) A chest is not a bench: it still has to be walked to before it opens.

An open bench shows a menu beside it in the world:

- a row of the items it can make, lowest tier first, which scrolls sideways (wheel or drag) only when there are more than fit;
- for the selected item, its recipe and output, a slider from 1 to 99 that stops at what the inventory can pay for, and a Craft button or "Not enough items".

Each recipe says how long one takes to craft. Output that does not fit in the inventory is dropped at the player's feet. The menu closes when the world is clicked or a movement key is pressed, and that ends any crafting still running; so does the player moving out of reach, though the menu itself then stays up.

A bench can have tiers. Its menu shows its tier and, if it can go higher, an Upgrade button with the cost; upgrading happens in place and keeps the menu open. A recipe that needs a higher tier is still shown in the row, darkened with a padlock and the tier it needs, and cannot be selected. A bench that is removed comes back at the tier it had reached.

Every recipe is in `data/recipes.json`, written out from the list in `recipes.txt`; `obtainment.txt` is the reckoning of what that list leaves without a source or without a use. This table is drawn from the data:

| Bench | Made at the | Further tiers cost | Recipes |
| --- | --- | --- | --- |
| Campfire | Workbench: 10 stick, 20s | - | 6 |
| Craft bench | Workbench: 1 dirt, 1 stick, 20s | 2: 1 limestone; 3: 1 marble (1m 20s); 4: 5 carbon fiber sheet | 18 |
| Tool bench | Workbench: 1 dirt, 1 flint, 20s | 2: 5 stone; 3: 5 bronze ingot; 4: 5 iron ingot; 5: 5 steel ingot; 6: 5 gold ingot; 7: 5 titanium ingot | 25 |
| Woodwork bench | Workbench: 1 wood, 20s | - | 12 |
| Builder's bench | Workbench tier 2: 1 stone, 1 limestone, 1 marble, 1m 20s | - | 55 |
| Compost bin | Workbench tier 2: 1 wood, 1 dirt, 5 worms, 1m 20s | - | 24 |
| Easel | Workbench tier 2: 1 wood, 1m 20s | - | none yet |
| Furnace | Workbench tier 2: 1 stone, 10 charcoal, 1m 20s | 2: 3 iron ore; 3: 3 gold nugget (1m 20s) | 11 |
| Kiln | Workbench tier 2: 1 dirt, 1 clay, 10 stick, 1m 20s | - | 8 |
| Press | Workbench tier 2: 1 stone, 1 wood, 1m 20s | 2: 1 bronze ingot; 3: 1 steel ingot, 1 diamond (1m 20s) | 29 |
| Tailor's bench | Workbench tier 2: 1 stone, 5 flax, 1m 20s | 2: 1 leather | 12 |
| Armor bench | Workbench tier 3: 1 stone, 5 tin ingot, 2m | 2: 5 iron ingot; 3: 5 titanium ingot | 16 |
| Dye bench | Workbench tier 3: 1 stone, 1 bucket of water, 1 linen, 2m | - | none yet |
| Metalwork bench | Workbench tier 3: 5 bronze ingot, 2 coal, 2m | 2: 1 iron block | 27 |
| Mixing bench | Workbench tier 3: 1 stone, 1 tin bucket, 5 oil, 2m | - | 93 |
| Pizza oven | Workbench tier 3: 5 clay, 1 sand, 1 red brick, 1 plaster, 2m | - | 2 |
| Train yard | Workbench tier 3: 1 stone, 5 steel ingot, 2m | - | none yet |
| Egg extractor | Workbench tier 4: 1 steel block, 1 electric motor, 1 dodo egg, 10 copper wire, 3m | - | none yet |
| Electric furnace | Workbench tier 4: 1 steel block, 10 copper wire, 1 iron ingot, 3m | - | none yet |
| Electric kiln | Workbench tier 4: 1 steel block, 1 red brick, 10 copper wire, 3m | - | none yet |
| Electric metalwork bench | Workbench tier 4: 1 steel block, 10 copper wire, 1 steel ingot, 3m | - | none yet |
| Electric press | Workbench tier 4: 1 steel block, 10 copper wire, 1 copper block, 3m | - | none yet |
| Electric sluice | Workbench tier 4: 1 steel block, 10 copper wire, 1 bucket of water, 1 electric motor, 3m | - | none yet |
| Electric stove | Workbench tier 4: 1 steel block, 1 iron pot, 10 copper wire, 3m | - | none yet |
| Flywheel | Workbench tier 4: 1 steel block, 1 copper block, 1 electric motor, 10 copper wire, 3m | - | none yet |
| Refinery | Workbench tier 4: 1 electric furnace, 1 copper boiler, 10 copper wire, 10 oil, 3m | - | none yet |
| Solar panel | Workbench tier 4: 1 steel block, 3 silicon wafer, 10 copper wire, 3m | - | none yet |
| Steam generator | Workbench tier 4: 1 steel block, 1 steam engine, 1 electric motor, 3m | - | none yet |
| Workbench | nothing makes it yet | 2: 5 stone; 3: 5 bronze ingot; 4: 5 steel ingot | 30 |

Some things about recipes:

- **Each takes time**: from 5 seconds for a basic torch to 5 minutes for a jetpack. The menu shows it beside the name, and the Craft button fills as the one being made comes along. Some upgrades take time too: the button fills, and the cost is paid when it is done, the player still being there. Shutting the menu part-way costs nothing.
- **A bucket of water** as an ingredient is a tin bucket that is full, 8/8. It is emptied by the recipe and handed back, not used up. (A recipe that asks for a tin bucket uses the bucket itself up.)
- **One thing, several recipes**: where a bench makes the same thing more than one way - a gold ingot from nuggets, from coins or from a golden chest - each recipe's picture has what it is made from, small, in its top right corner.
- **Paint** is one item in many colours. The mixing bench makes it from a tin bucket, 5 oil and one, two or three of the eight pigments - 92 mixes - and each paint carries the colour of its mix, shown as a band on its picture. Like buckets, paints stack whatever colour they are.
- **Things with "block" in their name are blocks**, and can be put down. Everything else that is new - beds, doors, ladders, columns, stairs, lanterns - is an item to carry, for now.
- The clothes and armour go in the four armour slots. Wearing them does nothing yet.

### Teleporting

A portal's menu - the spawn portal's or one the player made, from Tier 1 - has a **Teleport** button on top of it, with what it costs in crystals. Since the menu opens from anywhere, a portal that is in view can be clicked and gone to: the player is put at its foot at once, whatever they were doing is dropped, and the camera goes with them. It costs 10 crystals for anything up to 100 blocks away, and 1 more for every 10 blocks beyond that. Standing at the portal already, the button reads "You are here". Which kind of bench offers it and the three numbers are `teleport` in `rules.json`.

### Portals

The portal made at the workbench and the spawn portal are the same kind of bench: they offer the same recipes and climb the same tiers. Every portal is one base block with two portal blocks on it. The spawn portal's base is set into the ground, so two blocks of it stand above the surface; a made portal or a trade portal stands on the ground, base and all, three blocks tall. Each is one thing: placing it needs room for all three blocks, any of its blocks opens it, upgrading changes all of it, and removing any block takes the whole portal down and gives one item back. The spawn portal cannot be removed.

Every portal makes the same things, at once and with no waiting: a **workbench** from 1 dirt, a **Shop** (trade portal) from a portal and a gold coin, a **gold coin** from a golden chest, the gem pickaxes (3 of the gem and a stick, each at its tier), the portal chest and a new player. It has six tiers, each bought with one gem:

| Portal tier | Costs | Unlocks | Ore multiplier |
| --- | --- | --- | --- |
| 2 | 1 amethyst | Amethyst pickaxe | 2× |
| 3 | 1 sapphire | Sapphire pickaxe | 3× |
| 4 | 1 emerald | Emerald pickaxe | 4× |
| 5 | 1 ruby | Ruby pickaxe | 5× |
| 6 | 1 diamond | Diamond pickaxe, portal chest | 6× |

A gem pickaxe makes every ore block drop that many times over: the metal ores, coal, and time crystals. Like any tool it only has to be carried, and the best one counts.

The **portal chest** (1 golden chest and 100 crystals, at a tier 6 portal) opens onto one store of 16 slots that belongs to the player: every portal chest, in every world, shows the same contents. It is kept with the crystals in `saves/profile.txt`. It takes only plain items - a basket or chest with something in it is refused - and taking the block down leaves the contents where they are.

### Money

**Crystals** belong to the player, not to a world: the same number is there in every world. They are shown in a box at the top right, kept in `saves/profile.txt`, and written as soon as they change. A new player starts with 1000. They are spent on the portal and portal chest recipes, and gained by mining time crystals in caves.

**Coins** are ordinary items, in stacks of up to 99: 100 copper coins are worth 1 gold coin, and 100 gold coins 1 platinum coin. The furnace makes and melts them: at tier 2, 1 copper ingot makes 10 copper coins and 10 copper coins 1 copper ingot; at tier 3, 1 gold ingot makes 1 gold coin and 10 gold coins 1 gold ingot. (Nothing makes a platinum coin now. The table below is how it was before, at the metalwork bench.)

| Tier | Recipes |
| --- | --- |
| 1 | 1 copper ingot → 10 copper coins; 1 gold coin → 100 copper coins |
| 2 (5 copper ingots) | 1 gold ingot → 1 gold coin; 100 copper coins → 1 gold coin; 1 platinum coin → 100 gold coins |
| 3 (5 gold ingots) | 5 platinum ingots → 1 platinum coin; 100 gold coins → 1 platinum coin |

### The Shop

The trade portal is called the **Shop** now, and is made at a tier 2 workbench from 1 wood and 1 gold coin.

For trying things out, a new world starts the player with a trade portal and 1 gold coin as well as the basket, These are in the data (`rules.json`) and are meant to come out again.

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
| Axe | Tree trunks, wood, cactus, chest, workbench, woodwork bench, tool bench, craft bench |
| Machete | Leaves of every tree, living or decayed; carrots, flax, coral and kelp |

| Tool bench tier | Makes | From | Takes | It is a |
| --- | --- | --- | --- | --- |
| 1 | Flint spade | 1 stick, 2 flint | 40s | shovel, 2× |
| 1 | Flint machete | 1 stick, 2 flint | 40s | machete, 2× |
| 1 | Flint axe | 1 stick, 3 flint | 40s | axe, 2× |
| 1 | Flint pickaxe | 1 stick, 5 flint | 40s | pickaxe, 2× |
| 1 | Flint spear | 1 stick, 1 flint | 40s | weapon, 3 a blow |
| 2 | Stone axe | 1 stick, 3 stone | 1m | axe, 3× |
| 2 | Stone pickaxe | 1 stick, 5 stone | 1m | pickaxe, 3× |
| 2 | Stone spade | 1 stick, 2 stone | 1m | shovel, 3× |
| 2 | Copper bow & arrows | 50 stick, 5 copper ingot, 5 dodo feather, 1 string | 1m | bow, 2 a shot from 10 blocks |
| 3 | Bronze pickaxe | 1 stick, 5 bronze ingot | 1m 10s | pickaxe, 4× |
| 3 | Bronze machete | 1 stick, 2 bronze ingot | 1m 10s | machete, 4× |
| 3 | Bronze sword | 1 stick, 3 bronze ingot | 1m 10s | weapon, 5 a blow |
| 3 | Tin spade | 1 stick, 2 tin ingot | 1m 10s | shovel, 4× |
| 4 | Iron pickaxe | 1 stick, 5 iron ingot | 1m 20s | pickaxe, 5.5× |
| 4 | Iron machete | 1 stick, 2 iron ingot | 1m 20s | machete, 5.5× |
| 4 | Iron sword | 1 stick, 3 iron ingot | 1m 20s | weapon, 10 a blow |
| 4 | Iron axe | 1 stick, 3 iron ingot | 1m 20s | axe, 5.5× |
| 5 | Steel pickaxe | 1 stick, 5 steel ingot | 1m 40s | pickaxe, 6.75× |
| 5 | Crowbar | 2 steel ingot | 1m 40s | nothing yet |
| 5 | Razor | 1 yak horn, 1 steel ingot | 1m 40s | nothing yet |
| 6 | Gold spade | 1 stick, 2 gold ingot | 2m | shovel, 5.5× |
| 6 | Gold pickaxe | 1 stick, 5 gold ingot | 2m | pickaxe, 5.5× |
| 6 | Gold bow & poison arrows | 50 stick, 5 gold ingot, 5 dodo feather, 5 poison | 2m | bow, 5 a shot from 10 blocks, and 5 more by poison |
| 7 | Titanium sword | 1 stick, 3 titanium ingot | 3m | weapon, 20 a blow |
| 7 | Titanium pickaxe | 1 stick, 5 titanium ingot | 3m | pickaxe, 8× |

Without the right tool a block takes its ordinary time. Tools stack; none of them wears out. (A spade is a shovel.)

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

Under a beach, quite often - about one column in six, a dozen or more to a world - a golden chest lies buried at the bottom of the sand. A golden chest is what a portal chest is made from (with 100 crystals, at a tier 6 portal), and a tier 2 metalwork bench melts one down into a gold coin. It opens and is carried off like any chest. The first time it is opened or dug up it holds 1 to 5 gold coins, up to 3 gold ore, up to 3 iron ingots, perhaps a titanium ingot, and one gem: an amethyst most often, then sapphire, emerald, and a ruby least. What a block is found holding is its `"loot"` in `tiles.json`, and where it is buried is `"buried"` on a biome in `worldgen.json`; a golden chest the player puts down, or buys, holds only what they put in it.

## Layout

- `data/` — all game content (see Data above)
- `src/content.c` — loads the data files into the tables the engine reads
- `src/world.c` — the wrapping tile grid and terrain generation
- `src/water.c` — the water in each square and how it moves
- `src/tree.c` — tree shapes and growth
- `src/inventory.c` — stacks, baskets and the rules for moving them
- `src/craft.c` — crafting from the recipes
- `src/trade.c` — buying, selling and exchanging coins
- `src/guide.c` — what the content says about where each item comes from, for the recipe book
- `src/mob.c` — creatures: spawning, wandering, eggs and hunting
- `src/light.c` — how well lit each tile is
- `src/game.c` — player, movement rules, path planning, action queue, item drops, crafting
- `src/save.c` — saving and loading a world
- `src/assets.c` — textures for blocks and items
- `src/hud.c` — the on-screen inventory bar
- `src/benchui.c` — the bench popup and crafting menu
- `src/tradeui.c` — the trade portal menu
- `src/playerui.c` — the player's button, bars and the sleep and meditate panel
- `src/recipeui.c` — the recipe book
- `src/menu.c` — main menu, pause menu and key bindings
- `src/view.h` — placing the wrapping world on screen
- `src/main.c` — window, input, camera and rendering
- `tests/test_game.c` — tests for the rules, with no window
- `third_party/cjson/` — the JSON parser (MIT licence)
- `AGENTS.md` — the standing rule for working on this project: keep everything data driven
