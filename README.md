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

**Playsets** opens the list of playsets. A playset is a name, a set of mods, and worlds of its own: it is made and named by the player, so that a world is found by what its playset is called rather than by remembering which mods it was made with.

- The list shows each playset's name and its mods as icons; the one in use is marked, and a click on another puts that one in use. **No mods** is always there: the game by itself.
- **New** makes one, and opens it to be named and filled. **Edit** opens the one in use: type to change its name, and each mod has a switch, On or Off, that puts it in or takes it out. **Remove** takes the one in use off the list; its worlds stay on the disk.
- **Done** goes back; if anything was changed, the game's data and textures are loaded afresh with the mods of the playset now in use. Its name, and its mods as icons, show in the top right corner of the main menu.
- Worlds made before playsets had names are not lost: the first time, a playset is made for each set of mods that had worlds, named for those mods.

**Worlds belong to a playset.** Each playset has worlds of its own - and its own crystals and portal chest. The worlds of No mods are the ones in `saves/`; every other playset has a directory under `saves/playsets/`. Changing playset puts the other's worlds out of sight, not out of existence. The playsets, and which is in use, are kept in `saves/playsets.txt`. A mod that will not load is turned off, and the menu says which and why. **Two mods that cannot be on together** - each defines a block, an item, a creature or a tree under a name the other defines too - are not loaded at all: asked for together, every mod is turned off, the game goes back to its own worlds, and the menu says which two and over what. (Magic and Mayhem and Industry are such a pair: each makes a fifth tier of the workbench, of different things.)

**New world** asks for a name and four things about the world: its **size** (Small, the usual 2048 columns; Medium, 3072; Large, 4096; Extra large, 6144), and how big its **mountains**, its **valleys and seas** and its **ore veins** are - Small (0.6 times the usual), Normal, Large (1.5) or Huge (2). The sizes and scales offered are `"options"` in `worldgen.json`. **Delete** removes the world in the middle of the row, after asking; a deleted world cannot be got back.

Those are on the **World** tab of that screen. The **Advanced** tab has the world's **type**: Normal, or **Debug**. A debug world is level dirt with the spawn portal near its left end and, set out along the ground to the right of it, one of every block in the game, each in every way it can be put up: as it is; anything that goes up against another block - a torch, a lantern, a sign - on a backwall, on top of a block, under one, and on the left and the right of one, with a block of dirt as the thing it is put against; steps in a flight rising each way; a door as itself and between two blocks with their look; and anything that joins to its like as a block of nine and as a cross. It is as wide as it needs to be for all of that (about 1,600 columns now) whatever size was chosen, nothing in it grows, rots or freezes, and it is saved and played like any other world. It is made from the data, so a block added to the game is in the next debug world made.

Worlds are saved in `saves/<name>.world`, next to where the game is run from. A save records its blocks and items by name, so it still loads after the data files change; anything that no longer exists is removed from it. Saves made by earlier versions of the game still load. A world is saved when it is created, every two minutes while playing, on "Save and quit to menu", and when the window is closed. Actions still queued at that moment are not kept.

`Esc` opens the pause menu, which stops the world and offers:

- **Recipes** — the recipe book. On the left is every item in the game in a grid, in order - by category, as the dev portal has them (blocks, nature, tools and so on), and by name within each; type to narrow it down by name. Click an item to see every recipe that makes it: the bench and tier, what goes in and how many, how long it takes, and any crystals. Click anything in a recipe - an ingredient, or the bench - to look that up in turn, and so on down to the things that are not made at all, where the book says how they are got instead: which block to mine (and where it lies, and which tool is quicker), which creature leaves it and how often, which tree bears it and in which biome, which chest holds it, whether meditating brings it. Under that is everything the item is used to make. **Back** (or `Esc`) steps back the way you came, and the trail of items along the top jumps straight to one. The book is worked out from the data files as they are loaded (`src/guide.c`), so a mod's items and recipes are in it without anything being written for them.
- **World settings** — the tick rate, from 1 to 300 ticks a second (10 by default); the random ticks, from 0 to 40 to a chunk (10 by default), which set how fast water freezes and thaws and snow lies and melts, and nothing else; whether the fog of war is on; and whether crafting takes its time or a single tick. All are saved with the world.
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

**The queue waits for work and for menus.** Something being made at a bench, or an upgrade under way, is seen through before anything queued after it is started: the character stays at the bench, its menu stays up showing how the work is coming along, and neither a click elsewhere nor the movement keys take them from it - the click is queued for afterwards. The **Stop** button on the bar gives the work up. And a chest - or any menu the character was sent to open - holds the queue until it is closed: what was queued after it starts when the menu is shut, which a click elsewhere does. A bench looked at from where the character stands holds nothing up.

## Movement

Holding a direction, or a queued click, takes the player:

- up a one-block ledge;
- over a one-block gap with a short hop (hold `S` as well to go down into it instead);
- down a one-block drop with a plain step;
- over the edge of a drop of two or more blocks into a climbing pose on its wall, then down the wall;
- from one wall to a facing wall up to two columns away with a hop, so a pit two or three blocks wide is crossed near the top without going to the bottom;
- up any wall, pulling onto the top;
- through a gap one block high, crawling; the player is one block tall while under the low ceiling and stands up again on the far side;
- into a gap one block high wherever it is: one at head height is got into head first, with no need of room to rise in, and one further up a wall is climbed to and gone into from the wall - or straight in from beside it, off a tree or anything else being climbed;
- along the underside of a row of blocks, hand over hand like monkey bars.

To get onto a row of blocks overhead, press `W` under it: the player jumps and catches it if there is one clear block between their head and it. Holding a direction past the end of the row does not let go: nobody lets themselves fall (see below). Pressing `W` at the end instead swings round onto the side of the last block, from where it is an ordinary climb onto the top; `S` on the side of a block swings back in under it rather than dropping off. A queued click uses all of these to find its way.

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
- Each kind of tree keeps to its own country, so like grows with like: a **cherry grove** where it is cold, an **orchard** of apple trees where it is mild and damp, and the **tropics** - coffee, orange and mango trees - where it is hot and damp. All three are grass over dirt like the plains. Which biome a column is is recorded with the world; a world saved before that has it worked out from its climate and the lie of the land, so there the name may not match the trees that were generated.
- A **pine forest** stands where it is cold and dry, thick with tall pines, and a **maple forest** where it is mild and dry. Coconut palms grow on the sand of **beaches**.
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

**Light.** Everything is lit from 0 to 15. The open sky is as bright as the sun by day (15) and the moon by night (5): it beams straight down every column until a block stops it, and fills any open space that has no backwall. Chandeliers (15), steel lanterns (14), oil lanterns (13), torches (12), a burning campfire (12), gems (6) and time crystals (4) give light of their own, and all of them are put up in the background. A **steel uplight** or **downlight** throws its light one way instead: a quarter circle of it (45 degrees either side of straight up, or straight down) as bright as a chandelier at the lamp and a level dimmer with each row, stopped by blocks, with only a faint glow of its own to the other sides (`"beam": { "direction", "light" }`). From there light spreads, losing 1 with every tile of air and 2 with every block, so the ground is dark a few blocks down and a cave is dark until something lights it. A torch or a gem can be placed against a block or on a backwall. Something that is giving light is itself shown brighter than what it lights: it is not dimmed for being in the background, and the thing itself - not the square it is in, nor anything round it - is drawn as if lit by its light and `"glow"` more (8), so a lamp stands out in the dark, and a torch or anything brighter is shown at full brightness. A fire that is out, or a lamp hidden behind a block, does not glow. **The ends are extreme.** With no light at all a square is all but black (`"darkness"`, 0.98 of the way to it; `"minimum"` is the least light anything is given, and is 0): a cave with no lamp in it cannot be seen into. The dark closes in faster than the light falls off (`"contrast"`, 1.5; 1 would be evenly), so the edge of a lamp's light is sharp. And what gives light is drawn brighter than its true colours (`"bloom"`, a quarter as bright again at the full), which nothing merely lit ever is; the light round it falls off evenly from there, with no patch of extra brightness about the lamp.

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

**Poles.** Every world made from now on has four poles, a quarter of the way round the world from one to the next: going right from where the world begins, the **east pole**, the **south pole**, the **west pole** and the **north pole** - so that the world begins half way between the north pole, to the left, and the east pole, to the right. A pole is a post ten blocks tall, standing on the ground in the background (in the sea it stands on the bottom and rises ten blocks above the water); it cannot be taken down. North and south are candy canes, red and white and blue and white; east and west are carved tropical posts. Floating on top of each is a piece of **pole armour**, taken like any block - which means getting up there:

- the **north hat**, from the north pole: to be worn, and nothing more, for now;
- the **grass skirt**, from the east pole: wearing it, the weather cannot touch you - the environment score stays full whatever else is or is not on (it no longer does anything for energy);
- the **pole boots**, from the south pole: wearing them, walking, crawling and hopping go twice as fast;
- the **cornucopia hat**, from the west pole: wearing it, hunger never runs down.

Where the poles are, how tall, what they are made of and what is on top is `"poles"` in `data/worldgen.json`; what the armour does is `"walk_speed"`, `"warmth"` and `"sated"` on the items.

**Islands in the sky.** High over the land hang islands, none any lower than Y 80: each 15 to 30 blocks long and 10 to 15 deep or a little more, a ragged wedge of marble under dirt and a grass top. Nothing about one is a straight line: its top rises and falls by up to three blocks along its length, the dirt runs one to four blocks deep and the line where it meets the marble wavers with it, and the underside bulges, pinches and hangs down in spurs (`"jag"`). And none is solid: a **cave** one or two blocks high winds through the marble of every island from end to end, under a roof of it and over a floor of it, with a hole down through the floor somewhere along it to get in by from underneath (`"cave"`). They have no backwall: there is only sky behind them. About half carry a **gem tree**, which is cut for ordinary wood and sticks but bears shards of its gem among its leaves in place of fruit, and cannot be planted. An amethyst tree may be on any island; sapphire only above Y 105, emerald above 125, ruby above 145 and diamond above 165, each rarer than the last - so the rarer gems are found further up as they are further down. (`"islands"` in `worldgen.json`.)

**Basalt** lies in patches deep in the rock - 14 to 40 blocks to a patch, from 110 blocks down - dark and slow to mine.

**Scars.** Four other kinds of stone run through the rock in great gashes, tens of blocks long and several thick, on a wandering slant and tapering to nothing at their ends. They are laid down in order - **limestone**, then **marble**, then **lapis lazuli**, then **red marble** - and each cuts through whatever of the earlier ones it crosses, so a broad seam of limestone may have marble running through it, lapis through that, and red marble through all three. Limestone is the commonest and shallowest, red marble the scarcest and deepest. A scar is a vein with `"shape": "scar"`: its `"size"` is its length, its `"thickness"` how thick it is at the middle, and `"in"` the blocks it may replace; veins are placed in the order the file lists them.

| Stone | Replaces | Long | Thick | Down |
| --- | --- | --- | --- | --- |
| Limestone | Stone | 50–140 | 4–8 | 8–170 |
| Marble | Stone, limestone | 40–110 | 3–6 | 20–180 |
| Lapis lazuli | Stone, limestone, marble | 30–80 | 2–4 | 40–190 |
| Red marble | Stone and all three | 30–90 | 2–5 | 60–195 |

**Sandstone** lies under deserts, beaches and the sea bed in place of stone - but only where the sand goes all the way down. At the edge of sandy country, where the sand is a skin over dirt, there is ordinary stone under the dirt; from the point where the sand has deepened to the full depth of the ground, sandstone comes in beneath it, thin at first and thickening inward to 10 blocks, never more than a couple of blocks deeper than in the column beside it. So the wedge that sand makes over dirt above is carried on by sandstone over stone below. It is solid for the top half of its depth and broken up more and more by stone under that. The lowest layer of sand, lying on the sandstone, is half **black sand**, scattered through it; black sand is dug with a shovel like sand. This is a biome's `"under": { "tile", "depth", "seam", "seam_share" }`.

Columns are grass over dirt over stone. The dirt is 4 or 5 blocks deep on the plains and thins out gradually up a mountain or down a valley, so peaks and valley floors are bare stone. Stone takes twice as long to mine as dirt. Grass drops dirt. One time in ten, digging **dirt** turns up **worms** as well (a drop can have a `"chance"` of its own in a block's `"drops"`).

**Vines** hang in caves: from the roof, straight down, 1 to 5 blocks long, anywhere from just under the surface to 120 blocks down. They can be climbed. Cutting one takes everything below the cut with it, each block giving a vine. A cave decoration with `"hang": [shortest, longest]` hangs like this.

## Time

World time runs in ticks, 10 a second unless the world's tick rate is changed. A day is 1000 ticks and a night 800, with the light fading over 80 ticks at dusk and dawn. Movement, mining and crafting run in real time and are not affected by the tick rate.

Every tick, ten random blocks in each 16-column chunk get a random event (the number is a setting of the world). The weather goes by all ten. What grows and what turns up - trees, fruit, plants, creatures, eggs, rot - go by one in ten of them, exactly as when there was one event to a chunk, so their pace is the same whatever the number is set to: any one block is hit, for them, about once every 6400 ticks. (`"random_events_per_chunk"` and `"random_event_base"` in `rules.json`.)

**Seasons.** A year is four seasons of 10 days each - spring, summer, autumn, winter - 40 days in all. The leaves of the trees that shed follow it: oak and apple are pale green in spring (the apple in blossom), full in summer, gold and russet in autumn and bare twigs in winter; the cherry is in blossom in spring and green in summer; the maple is green until autumn turns it; pines keep their needles and carry snow in winter; the trees of the tropics do not change. A block's `"texture_seasons"` lists its pictures season by season.

**Happiness** is a sixth bar, green, in the player's panel, and shows under the player's button whenever it is not exactly full. It is how a character is doing, all told, and it is the one thing that changes how fast they go.

- **What it is.** The average of three bars - energy, hunger and the environment score - and never more than health: at 80% health, happiness is at most 80%. So a character rested, fed and warm but hurt is as happy as they are whole; one whole and warm and rested but starving is two thirds happy.
- **What it does.** The less happy, the slower a character walks, climbs, crawls, swims, flies, digs, builds and strikes: half as fast with none at all, three quarters as fast at half. Nothing else slows them.
- **Past full.** Each of the three bars can be kept past full, and shows gold while it is: **energy** by sleeping in a soft bed (or coffee); **hunger** by eating cooked food on a full stomach; the **environment** score by wearing the grass skirt. With all three gold and health full, happiness is past full too - gold - and the character does everything **20% faster**: all of the above, and work at a bench as well.
- **Cooked food** is any food a recipe makes - cooked meat and fish, bread, flatbread, pizza, mozzarella, the stews, coffee, rainbow cake - unless its data says otherwise with `"cooked"`. Fruit, eggs, milk and raw meat are not: they fill the bar, and on a full stomach cannot be eaten at all.
- **With a bar empty.** With no energy a character cannot make anything at a bench - the button says so - and work under way waits until they have rested. With no hunger left, or no environment score, health runs down.
- It is not kept: it is worked out each moment from the others. `"slow"` and `"boost"` under `"happiness"` in `rules.json` are how much speed it costs and gives; an item that keeps the environment score past full has `"shelters": true`.

**Environment** is a fifth bar, blue, in the player's panel, and shows under the player's button whenever it is not full. It is how well a character is standing up to the weather.

- **What wears it down.** Being out of doors while it rains (or snows), or in the cold: all through winter, and at any time above the snow line. Out of doors is anywhere with no solid block overhead - a roof, a cave or a tree house all do. It takes 1,200 ticks to run from full to nothing, and comes back four times as fast under cover or when the weather turns.
- **What it does.** It is half of a character's happiness (below), which is what slows them. With none left **and nothing on**, health runs down as well, a full bar in 1,200 ticks, and a sleeper wakes.
- **Clothes** set how low it can fall. With nothing on that is nothing. With anything on at all it is at least 10%, so the weather can slow a clothed character but never hurt one. Each piece adds its share: linen and the tin foil hat 5% a piece; a leather jacket or fur coat 30%, leather trousers and leather or fur boots 25% each, so a full set of leather and fur keeps 80%; tin armour 15% a piece (60% for the suit); iron 20% a piece (80%); titanium, carbon fiber and every other armour 25% a piece (100%). The north hat, cornucopia hat, pole boots and jetpack are 25% each, and the grass skirt is 100% by itself.
- **In the data.** The rates are `"environment"` in `rules.json` (`"fall_ticks"`, `"rise_ticks"`, `"harm_ticks"`, and `"clothed"` for the least anything worn keeps); a piece's share is its `"warmth"` in `items.json`; a season is cold if it `"snows"` or says `"cold"`, and a biome may say `"cold"` too.

**Weather.** Time is cut into spells of 600 ticks, and about three in ten of them are wet: it rains, coming on and easing off at the ends of a spell. Rain is only to look at - it puts no water anywhere. In winter what falls is snow, and it lies: a layer an eighth of a block deep at a time on the top of every column open to the sky, up to four layers (half a block), and melts away again layer by layer once winter is over. It is only to look at too: nothing stands on it. Ground more than 50 blocks up has snow on it the year round.

**Ice.** In winter a random event on open water - a square of it with the air over it - can freeze it (one chance in two) into a block of ice, which remembers how much water it was. Only the top freezes: water under ice stays water. In spring a random event on ice can thaw it, back into the water it was (a whole square, for ice that was never water). Ice is solid, can be walked on, and is dug with a pickaxe for the ice item.

## Trees

Trees are generated on the plains at every stage of growth, and keep growing:

- A random event on a **trunk** block makes its tree grow one step: either one new leaf, added outward from the trunk, or one more block of trunk. When the trunk extends, all the leaves move up a row with it; a leaf with a block in its way is lost.
- A random event on a **leaf** hangs a fruit on it: an acorn on an oak, a cherry on a cherry tree, a cone on a pine, a coconut on a palm, a winged seed on a maple - and each of those, planted, grows its tree. Fruit hovers on its leaf, never falls off by itself, and disappears after 1800 ticks. It is not collected by walking past; click it to queue picking it. If its leaf is broken it falls and can be collected like any other item.

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

**Scorpions** live in the desert, on sand, up to four near the player. One that sees a player within 6 blocks makes for them, and stings when it is beside them: 40 ticks of **poison**, each taking half a percent of the health bar (a fifth of it in all), while the health bar shows purple. A second sting while the first is at work does not add to it, only starts the 40 ticks again; a sting wakes a sleeper. Having stung, a scorpion leaves off for 80 ticks. It has 6 health, and killed leaves 1 to 3 poison. Stinging is a creature's `"sting"` in `mobs.json`; what poison does to a player is `"poison_damage"` in `rules.json`.

**Yaks** graze on grass in the forests - plains, cherry grove, orchard, pine forest and maple forest, not the tropics - up to three near the player. They do nothing to anyone. Killed (15 health), a yak leaves one yak horn, 1 or 2 leather, and half the time a fur. A click on a yak with an **empty bucket** in hand milks it: one milk drops beside it, the bucket is kept, and the yak has no more for 600 ticks (a bucket with water in it will not do). Milking is a creature's `"milk"` and an item's `"milks"`, and has nothing to do with whether the yak is shorn. With a **razor** in hand, a click on a yak shears it instead of hitting it: 1 to 3 yak shavings drop beside it, it is none the worse, and it goes about pale and shorn for 3000 ticks until it has grown back and can be shorn again. Shearing is a creature's `"shear"` and a tool's `"shears"`. Where two kinds of creature live on the same ground, as yaks and dodos do, each has its own chance to be the one that turns up.

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

- **Ambush.** When the player comes within two columns of its tree, below it, it lets go and drops on them - the whole height of the tree in one fall, gathering speed, landing beside them - and bites - 15% of the health bar, and enough to wake a sleeper - then runs back to the tree and up it in one unbroken climb. Back at the top it waits a second - 10 ticks - and, if the player is still under it, comes down for another go, and again for as long as they stay. If the player gets away for long enough it gives up and goes home.
- **Homeless.** If its tree is cut down it drops to the ground and runs for the nearest tree within 24 columns, and goes up that. With none in sight it runs about until one is.
- Hunted like a dodo, it leaves 1 or 2 leather, and half the time a fur as well. Health never goes below nothing, and nothing more happens when it is at nothing.

## Plants

Carrots, flax, chilli and coral are single blocks that grow through four stages, one stage for each random event that lands on them. They can be pulled up at any stage, and give more the further they have grown.

| Plant | Grows | Planted with | On | Gives, by stage |
| --- | --- | --- | --- | --- |
| Carrot | Plains, orchard | A carrot | Dirt or grass | 1, 1, 2, 4 carrots |
| Flax | Plains, cherry grove, pine forest | Flax seeds | Dirt or grass | 1 seed; 1 seed; 1 seed and 1 flax; 2 seeds and 3 flax |
| Chilli | Tropics | A chilli | Dirt or grass | 1, 1, 2, 4 chillies |
| Coral | Ocean | Coral | Sand, under water | 1, 1, 2, 3 coral |

**Tall plants** - sunflowers, tomatoes, wheat and corn - grow through three stages one block high and then a last one two blocks high: a foot with a top on it. The last step needs the block above free, and waits until it is. Taking the **top** gives three of what the plant bears and puts the foot back to the third stage, to grow its top again; taking the **foot** takes the whole plant, top and all. They are sown like any plant, and grow wild at every stage.

| Plant | Grows | Planted with | A stage or the foot gives | The top gives |
| --- | --- | --- | --- | --- |
| Sunflower | Plains, maple forest | A sunflower seed | 1 seed | 3 seeds |
| Tomato | Orchard, tropics | A tomato | 1 tomato | 3 tomatoes |
| Wheat | Plains, maple forest | Wheat | 1 wheat | 3 wheat |
| Corn | Plains, tropics | Corn | 1 corn | 3 corn |

In data such a plant is three fields: the third stage's `"grows_top"` (the block put on top when it grows), the foot's `"topped_by"`, and the top's `"cut_to"` (what the block under it becomes when the top alone is taken).

**Tulips** are one block high and each has two colours of its own: its **body** and its **tips**. Wild ones - on the plains, in cherry groves and maple forests - are one colour all over: red, blue, yellow or green. Picking a tulip gives a **bulb** and a **seed**, both of the flower's two colours, and either can be planted to grow that flower again (a shoot first, then the flower at the next random event).

- **Crossing.** Plant a seed where a bulb has just been planted, or a bulb where a seed has - click the shoot itself or the soil under it - and the two become one crossed shoot. One of its two features, body or tips, is a mix of its parents' there: halfway between the two colours. The other is taken whole from one parent or the other. Red crossed with blue gives, say, a purple body with red tips; cross that again and the colours wander further. There are 216 colours a part can be.
- **Stacking.** Bulbs and seeds stack only with others of exactly the same two colours; ones that differ take a slot each. A stack dropped on one that differs changes places with it.
- Bought at a trade portal they are plain red.

A two-tone block has `"tones"` in `tiles.json` (the pictures drawn over it in its two colours, and the `"natural"` colours wild ones come in); a two-tone item has `"tones"` in `items.json`, and `"crosses": { "with", "into" }` says which shoot it crosses with and what the pair becomes. The colours are carried on the item as its meter and kept with the block in the world.

Coral only grows while it is under water. Plants stand in the background layer: they are walked past, and clicking one pulls it up.

## Inventory

The bar along the bottom has a head button followed by 8 slots. Stacks hold up to 99. The player starts with one basket (item ID 1).

- Tap a slot to select it. Tapping a basket also opens or closes its 4 slots, which extend upward from it. The name of the selected item shows above its slot for a couple of seconds whenever the selection changes, and the name of anything being dragged shows under the cursor.
- Tap an item inside a basket to select it; the closed basket then shows that item. Tapping an empty basket slot selects the basket.
- Tap the head button to open the 4 armor slots (head, chest, legs, boots) and every basket at once. It is also a slot that can be chosen, as the others are: with it chosen the character has nothing in hand but their hands. Nothing is put down or eaten, and a block is dug by hand - at the pace of no tool at all - whatever tools are in the bar. Choosing any other slot goes back to using the best tool carried.
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

- **Hunger** runs down all the time: a full bar lasts 36,000 ticks. Selecting a food in the inventory puts an **Eat** button above it; eating one fills some of the bar, a cooked meal much more than a berry. Eating to full and then eating cooked food again turns the bar golden: nothing more can be eaten (the button reads "Full"), and hunger holds off until that wears off - longer the bigger the meal. Only cooked food does it; anything else cannot be eaten on a full stomach.
- **Health** mends by itself, slowly, and a little with everything eaten - much less than the food does for hunger. With hunger at nothing it runs down instead, though never below a tenth.
- **Energy** runs down while awake: a full bar lasts 1800 ticks. It has no golden state.
- **Sleeping** lays the player on the ground and brings energy back; they wake by themselves when it is full. The button is greyed out while energy is full.
- **Meditating** sits the player down, with a progress bar over their head. Every 900 ticks it reaches 100% and something drops out right on top of them, and is picked up like anything else at their feet: a shard of a gem (amethyst most often, diamond least; three shards are pressed into the gem at a tier 3 press), a fruit, or one of the six ores found in stone. (Coffee beans count as a fruit.)
- While meditating neither hunger nor energy runs down.
- Being attacked wakes the player from either.
- Either needs the player to be standing on the ground, and ends as soon as they are told to do anything else - a click on the world, a movement key - or the button is pressed again.
- While the player sleeps or meditates the ticks go three times as fast (30 a second instead of 10), and arrows at the top of the screen say so.

All the numbers, what each food is worth (`"food"` in `items.json`) and the list of what meditating brings are data.

## The readout

The top left corner shows, a line each: frames per second; the player's X and Y; the biome they are standing in; how many creatures are within range against the cap, for the land and for the water separately; and the date - the season, then the time passed as year, day of the year and tick of the day, whether that is day or night and how far through, and "rain" or "snow" while it is doing either. The rate ticks are going at is on the first line. `F3` hides it and brings it back. The crystals are in the top right corner.

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

Each recipe says how long one takes to craft, and under its name sets out what it takes - each thing with how many are held against how many are needed, red where there are too few - four to a row, in as many rows as the recipe needs: the menu grows taller for a big one. Output that does not fit in the inventory is dropped at the player's feet. The menu closes when the world is clicked or a movement key is pressed, and that ends any crafting still running; so does the player moving out of reach, though the menu itself then stays up.

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
| Train yard | Workbench tier 3: 1 stone, 5 steel ingot, 2m | - | 8 |
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

Every portal makes the same things, at once and with no waiting: a **workbench** from 1 dirt, a **trade portal** from a portal and a gold coin, a **gold coin** from a golden chest, the gem pickaxes (3 of the gem and a stick, each at its tier), the portal chest and a new player. It has six tiers, each bought with one gem:

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

### Trade portals

A **trade portal** is made at a tier 2 workbench from 1 wood and 1 gold coin. Clicking a placed one offers to open or remove it. Open, it shows the coins held along the top, the categories of item as a list down the left - each with its whole name, and only the ones the portal has anything under - and the items of the chosen category in a grid.

- **What it deals in.** Seven categories - Blocks, Nature, Tools, Clothing, Ores, Crafted and Energy - each in six tiers, set out in `TradeTiers.txt`. Everything else is in four more categories that only the dev portal has - **Benches**, **Portals**, **Creatures** (caged ones and the eggs) and **Special** (the gem pickaxes, the pole clothing and the like): no trade portal buys or sells those, nor anything that takes crystals to make.
- **Mods.** What a mod adds is in the same categories as everything else, by what it is - its blocks with the blocks, its food with the rest of Nature - at whatever tier it gives or its price earns; its benches, portals and creatures are with the others at the dev portal. A mod may still add a category of its own for things that fit none of the game's, as Discovery Road does for relics.
- **Prices** are shown in the biggest coins that make them up: 100 copper coins are 1 gold and 100 gold are 1 platinum, so a thing that costs 15,500 copper is shown as 1 platinum and 55 gold.
- **Tiers.** A trade portal starts at tier I and deals only in tier I things. The **Upgrade** tab raises it: to II for 1 amethyst, 10 gold coins and 10 crystals; III for 1 sapphire, 20 gold coins and 20 crystals; IV for 1 emerald, 50 gold coins and 50 crystals; V for 1 ruby, 1 platinum coin and 100 crystals; VI for 1 diamond, 10 platinum coins and 200 crystals. The stone in its keystone shows the tier, and a raised portal picked up is still raised when put down again. A category lists its things tier by tier, in the order the document gives them. Every place in the grid has its tier's numeral in the corner, found or not - a `?` as well - in gold where the portal has yet to be raised that far. Something found but of a higher tier than the portal shows dimmed, and cannot be clicked until the portal is raised.
- **In the data.** An item's `"category"` and `"tier"` are in `items.json` (put there from the document by `tools/trade_tiers.py`); a trade block's `"trade_level"` and what its `"upgrade"` costs - items and `"crystals"` - are in `tiles.json`. A category marked `"free_only"` in `trade.json` is one only a block that gives everything away has. An item with no tier - a mod's - takes the highest tier whose price in `"tier_prices"` it reaches, and a mod that names one of the old categories (materials, food, gems, building, benches, animals) is put in the new one that `"also"` goes by that name.
- **Finding things first.** A trade portal deals only in what somebody in that world has had in their hands - dug, picked, made, picked up or been given: anything that has been in a character's bar, armour or basket. Every item a trade portal could deal in has a place of its own in its category, in a fixed order. Until it has been found that place shows a **?** and cannot be clicked; once found it shows the item, in the same place, and can be bought and sold. Nothing moves when something is found. What has been found is kept with the world.
- **How portals look.** Every portal is an arch of stone with a spiral turning in it, stood on a stepped foot: blue in grey stone for the ones a player makes, violet for the one the world begins at, green in gold for the trade portal and red in blue for the dev portal. The stone in the keystone tells a portal's tier.
- **Prices.** Every item has a buying price of its own, in copper coins: from 10 for dirt, sticks and seeds, through 40 for copper ore, 80 for iron ore and 300 for an amethyst, to tens of thousands for a diamond chandelier. Raw things are priced by how hard they are to come by; made things by what goes into them, a little over, and the time they take. **Selling gives nine tenths** of the buying price (`"sell_share"` in `trade.json`; an item's `"price"` in `items.json`).
- Pick an item to see both prices and how many are held, then Buy 1, Buy 10, Sell 1 or Sell 10.
- **Buying** spends the smallest coins first, and breaks a bigger coin into change when they do not cover the price.
- **Selling** pays in copper coins and never exchanges them upward.
- The **Convert** entry exchanges between neighbouring coins in either direction, such as 100 copper for 1 gold.

Anything bought or given as change that does not fit in the inventory is dropped at the player's feet.

### The dev portal

A new world starts its first character with a basket and a **dev portal**, for trying things out. It is a trade portal that gives everything away: every item there is - benches, other portals, the portal chest and the coins included - whether or not it has been found, for nothing. It needs no raising: it has every tier from the start, and the Benches, Portals, Creatures and Special categories besides. Handing a thing back to it gives nothing either. It cannot be made or bought. (What a character starts with is `"start_items"` in `rules.json`; a block that trades like this has `"trade_free": true`.)

### Crafting times

Everything made at a bench takes the time its recipe gives. There are two ways round that:

- **Hurry it for crystals.** While something is being made, the craft button has a **Now** button at its end with a price in crystals: one crystal for every 10 seconds the work still has to go, rounded up, for the whole batch. Paying finishes all of it at once. (An electric bench still has to have its electricity first.)
- **Turn the times off.** In the pause menu's world settings, "Crafting times" can be switched off for the world: everything then takes one tick to make. It is saved with the world.

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

### Electricity

Electricity is made, stored, carried along wires and used. Nothing shows it as numbers: a flywheel has a bar over it for how full it is, a bench says when it is waiting, and what is on its way along a wire shows as sparks running down it.

- **Generators.** A **solar panel** makes up to 10 units a tick, by day and only with the sun straight on it - nothing solid anywhere above it - and less toward dusk and dawn. A **steam generator** makes 10 a tick for as long as it has fuel burning, which is put in from its menu like a furnace's. What a generator makes in a tick and nobody takes is gone.
- **The flywheel** stores it: up to 10,000 units, 1000 ticks' worth of a generator. It is filled by the generators: one that has made something nothing has asked for looks along the wires for the nearest flywheel with room and sends it there, and when that one is full, to the next. Several generators on the same wire all send what they make. A flywheel keeps what it holds when the world is saved.
- **Copper wire** carries it, one block a tick. It goes anywhere: on a block - in the very square the block is in, drawn over the face of it - against a backwall, or in thin air. A click on a block with wire in hand puts the wire on it rather than digging the block out; a click where there is wire already takes the wire off. A block can as well be put where wire already is. Off a block the wire comes first, before the block. It joins to wire beside it and to anything electric beside it or in the same square. A thing touching another directly needs no wire.
- **Asking for it.** Anything that wants electricity looks along the wires for the nearest thing that has some - a flywheel with a charge, or a generator with output to spare - and it comes from there. It arrives no sooner than the wire is long. A flywheel sends what is asked for all at once; a generator can only send its 10 a tick. If the wire is cut, what was in it is lost.
- **Electric benches** have recipes of their own, and burn no fuel. The **electric furnace** smelts with two ore to the ingot where the furnace takes three, makes pig iron, silicon, titanium and platinum, and turns ingots into coins and back; the **electric stove** cooks, roasts coffee and makes coffee, curry, stew and cake; the **electric press** does what the press does, quicker, and crushes stone to gravel or sand; the **electric sluice** washes an ore out of gravel or sand - which ore is chance, the common ones likelier; the **egg extractor** gets from a dodo's egg the thing that kind of dodo is made of; the **refinery** makes fuel, resin, epoxy and carbon fiber from oil and coal; and the **electric kiln** and **electric metalwork bench** make what the fuelled ones make in three quarters of the time. Starting something at any of them asks for the electricity the work will take - 100 units for each second of crafting, for the whole batch, and no more than that - and the work does not begin until all of it has reached the bench: the craft button reads "Waiting for power" and fills as it comes. So a flywheel close by a bench starts it at once, and a generator at the end of a long wire keeps it waiting. (A recipe whose `"output"` is a `"one_of"` list makes one of those things, by weight.)
- **Lifts.** An **electric elevator motor** with **elevator shaft** above and below it, as much as is wanted, is a lift. The motor takes 10 units every tick, all the time, and the lift runs while it gets them. Shaft and motor go in the background, so blocks can be built in front of them, floors included. A character standing in the shaft of a running lift can ride to any other floor of it - any square of the shaft with room for them and something to stand on - straight through whatever blocks are between, and fast: 25 blocks a second. Routes use lifts by themselves; with the keys, up and down in a lift go to the next floor that way; and a click on a floor of a running lift is a ride there. A door in front of the shaft is no bar to getting in. (With the power off a click on the shaft takes it down, like any block.)

In data: `"generator": { "power", "needs": "sun" | "fuel" }`, `"battery"`, `"wire"`, `"electric": { "per_second" }`, `"motor": { "power" }` and `"shaft"` on a block; an item's `"places_on_block"` is the block it becomes instead when put down on top of one; `"floats"` lets a block be put up with nothing to hold it.

### Doors and trapdoors

A **door** or **trapdoor** (wooden or iron) is a solid block to everything except a character: water, light and creatures are stopped by it and it can be stood on, but a character walks straight through it. A trapdoor can also be climbed: standing on one, the down key lets the character down through it, and from below they climb up through it and out onto its top.

- **A door is two blocks tall**, put up and taken down as one thing; it needs both squares free. A trapdoor is one block.
- **They take on the look of the wall they are put in, and keep it.** A door put up with the same block above it and below it is drawn as that block from then on; a trapdoor, which lies flat, goes by the blocks to its left and right. Other doors and trapdoors in between are looked past. With different blocks at the two ends, or nothing at one, a door looks like a door. The look is settled once, as the door goes up: changing the blocks round it afterwards changes nothing. To give a door another look, take it down and put it up again.
- `"passable": true` makes a block a door; `"mimics": "upright"` or `"flat"` says which way it looks for the block to copy; `"height": 2` makes it two blocks tall.
- **The lowest block of a portal is solid** - a portal, a trade portal or a dev portal: it is walked over and stood on, not through, and nothing else can be put where it is. The rest of the portal stands behind the character as before. (`"solid_foot"` on a tall background block.)

### Walls that run themselves

- A block put up within 10 blocks above the next block below it has backwall of itself run down through the open squares between the two - so roofing a room over its floor closes in the back of it. Squares that already have backwall keep it.
- Taking a block away when there is no backwall behind the square above it - the top of such a wall - takes away the backwall behind the block itself and the backwall under it, down to the next block or 10 squares, whichever comes first. A block with backwall above it leaves the wall alone, its own included. (So digging down from open ground leaves no backwall behind, and the hole is open to the sky.)
- How far is `"wall_span"` in `rules.json`.

### Armour

Clothes and armour worn in the four slots each stop a share of every **attack** - a dropbear's or shark's bite, and how long a scorpion's poison lasts - and the shares add up. Linen stops 2% a piece, fur 4%, leather 6%, tin 9%, iron 13%, titanium and carbon fiber 17% each, so a full suit of iron stops 52% and of titanium 68%. It does nothing for hunger, drowning or poison already in. An item's share is its `"armor"`.

### How the character looks

The character is made of six boxes - a cube for the head, and a block each for the body, the two arms and the two legs - seen dead level, never from above. They wear an undershirt and a pair of shorts.

Stood doing nothing, they face straight out of the screen. To walk, climb, crawl, swim, dig, build or strike they turn square to the side they are going or working toward, so that they face the way they walk, and turn back to the front a moment after they stop. The arms and legs swing from the shoulder and the hip, and every change - of pose, and of which way they are turned - is eased into rather than jumped to. The stride keeps time of its own, a little under two a second at a walk, whatever the pace over the ground; digging is a quick chop, the arm drawn back and brought down between two and three times a second.

The **jetpack** is worn on the body, in the slot a breastplate goes in: straps over the shoulders, the tanks and burners on the back. Wearing it lets them fly (see Pace, and flying).

Whatever is in the four armour slots is drawn on them, each piece a shell a little larger than the part it covers, so a coat goes over the undershirt and a helmet over the hair: a helmet leaves the face open, a breastplate has a guard on each shoulder, a coat has sleeves to the wrist, leggings stop at the shin and boots cover the foot.

The skin is one picture, 64 by 32, with each box opened out flat: the head's faces start at the top left corner (8 by 8 by 8), a leg at (0, 16), the body at (16, 16) and an arm at (40, 16), each laid out as right side, front, left side, back, with the top and the underside in the row above. The other arm and leg use the same part of the picture, turned about. Which picture is the skin is `"skin"` under `"players"` in `rules.json`; what an item looks like worn is its `"worn"` in `items.json`, a picture of the same shape with nothing drawn where the thing does not cover. A mod's clothes carry their own.

### The view

The world is drawn in depth. The game itself is still a grid seen from the side - where things are, and what can be done to them, has not changed - but it is looked at through a camera whose eye is in front of the middle of the screen, so everything has perspective: what is left of the middle shows its right side, what is below it shows its top, and as the view moves across the world the sides of things swing round with it.

- **Material is material.** Earth, stone, wood, ore - anything solid that is stuff rather than a thing - is a block two squares deep, with its face where it always was and its sides and top showing wherever nothing is against them; the top of a block is coloured as the top edge of its picture is, so grass is green on top.
- **The world has a front half and a back half.** Characters, creatures, things lying on the ground and whatever runs on rails are in the front half. The background layer - trees, benches, chests, ladders - is the back half: a tree stands behind the character on the same ground. The backwall closes the back, so a cave or a room is a space with a floor, a roof, sides and a far wall.
- **What is built and put down to be used is modelled**, from its own picture: benches, whatever holds things, beds, lamps, columns and stairs, and what runs on rails.
  - a **box** - a chest, a safe, a furnace - is one block, its sides plain, the colour the picture is along that edge (not the front of it over again). Chests and safes are of the background layer, like benches: walked past, not round, and nothing stands on them;
  - a **built thing** - a bench, a lantern, a locomotive - is put together from the parts of its picture: every stretch across it becomes a box as deep as it is wide, so a table's top is a slab and a post is square, and a part much thinner than the whole, like a leg or a wheel, stands once at the front and once at the back. A table has four legs; a carriage has wheels on both sides and can be seen through at its windows.
- **Everything else is its picture, one pixel deep**: creatures, a portal's arch, eggs, gems and crystals, torches, vines, windows, signs, and whatever is lying on the ground. They stand just behind whoever walks past them (what is climbed is against the back; creatures are in among the characters).
- **Plants** - anything sown, or that grows, or is the top of something that does - are their picture twice, crossed.
- **Whatever is stood on is as deep as the ground**: a square that stops a character and holds them up is drawn from the front of the world to the back of it, whatever it is part of. The foot of a portal is such a square: it fills the depth, and the arch stands on it.
  What kind a block is is worked out from what it does and what its picture is like; `"model"` in `tiles.json` says it outright where that would be wrong (`"block"`, `"box"`, `"frame"`, `"body"`, `"panel"` or `"cross"`). The character is the six boxes they always were, now standing in the scene.
- **Clicking** goes by what is under the cursor in depth: the face of a block; failing that the square the characters are in; failing that what stands in the background behind it.
- Light and darkness, the fog of war, water and the marks for what is queued are laid over the front of the scene, as before.

### Not falling, and being hit

**Nobody lets themselves drop.** A step that would leave a character falling more than one block - off the end of a row they are hanging from, down from the underside of a block, off a ledge into the air - is not one they will take, by the keys or on the way to a click: they go over the side and climb down, or stay where they are. One block down is an ordinary step. They still fall when what they stand on or hold is taken away, and when a jetpack is taken off in the air. (How far they will let themselves drop is `"max_drop"` under `"player"` in `rules.json`; 0 is any distance.)

**They face what they work on.** Digging, building or striking, a character turns to the block or creature it is being done to, whichever way they last walked - and does so even while holding on to a wall on their other side. Something straight above or below turns them neither way.

**How they move** is drawn all the way through: going up onto a block they lean to it and bring the leading knee up and over; coming down off one, or falling, the arms go out and up; a hop tucks the knees; and lying down to sleep, sitting to meditate, and dying are gone into and come out of, not jumped to.

**Attacked**, a character throws their arms about for a little over half a second (`"flail_time"`): a bite or a sting does it.

**A creature that attacks, hit and not killed, goes for whoever hit it**, there and then: a dropbear comes down out of its tree for them from however far along they hit it from, and a scorpion or a shark that was resting from its last attack has done resting.

**A click on something lying on the ground** picks up all of that thing that is lying in the square - the whole pile, or as much of it as there is room for. Where there is room for none of it, the click is not on it at all: it goes to whatever is behind - the block, the bench, the square.

### Pace, and flying

A step on foot takes 0.24 seconds - about four blocks a second. Climbing and crawling go at half that: 0.48 seconds a block. (Hopping, falling and swimming are as they were.) These are `"walk_time"`, `"climb_time"` and `"crawl_time"` under `"player"` in `rules.json`.

With a **jetpack** on - in the chest slot, not just carried - the air holds a character up as water holds a swimmer. A click anywhere in the open sky sends them there, straight, on the slant if that is the way; they hover where they stop, and go on from there to anywhere else or back to the ground. The movement keys fly them too. It is two and a half times as fast as walking (`"fly_speed"`) - 0.096 seconds a block - and that is of the plain walking pace: boots that quicken walking do nothing for it. It burns nothing. Taken off in the air, they fall. An item that does this has `"lifts": true`.

Nothing in the **fog of war** can be clicked: not a square that has never been uncovered, nor anything in one. To get into it, walk or fly with the keys, or click as far as has been seen.

### The railway

Everything for it is made at the **train yard**: 10 rail from 1 wood and 2 steel ingots; a crowbar from 2 steel ingots (1m 40s); a train station from 3 wood and 3 rail; a train wheel from 3 steel ingots; a rail handcar from 4 train wheels and 2 wood; a steam locomotive from 6 train wheels, a steam engine, an iron block and 10 coal; a freight car from 4 train wheels, 1 wood and a chest; a passenger car from 4 train wheels, 1 wood and an iron block. Each takes 1m 20s but the crowbar.

- **Rail** goes down on top of a block, and nowhere else. A piece joins the piece beside it, and also the piece beside it and one block up or down, so track climbs a staircase of blocks a block at a time; a climbing piece is drawn as a ramp on a trestle. A click on rail only takes it up with a **crowbar** in hand: without one the click is somewhere to walk to. Rail with something standing on it does not come up at all.
- **A train station** is three blocks wide and two tall: three pieces of rail with a board over them. It is put up by a click on the square above where its middle rail is to go; it lays the three pieces itself (or uses rail already there), and they come up with it when it is taken down. A click on the board writes its name on it, as on a sign.
- **Putting things on the track.** With a handcar, a locomotive or a car in hand, a click on a piece of rail within four blocks (or on the square over it) puts it there. Each stands on three pieces and is two blocks tall. One put down within two squares of another is slid up against it. Things that touch end to end are **coupled**: whatever moves one moves them all, so a car is attached by putting it down behind a locomotive or another car.
- **A click on anything on the rails**, from within reach or from a seat anywhere on the same train, brings up its menu. The last choice is always to remove it, which leaves it, and whatever it held, lying where it stood, and puts out whoever was in it.
- **A rail handcar** goes by being worked, and by its own weight. *Get in* stands the character at its left end, at the pump. Holding `A` or `D` works it that way: it gathers speed slowly - 2 squares a second, each second, on the level - up to the 9 a second it can be worked to, and no faster by working. Let go and it rolls on, losing half a square a second each second, until it stops. `W` or `S`, or working it the other way, holds it back and stops it within a second or so. A slope takes 5 off going up and adds 5 going down, and the pump is weaker on a climb, so even worked all the way a handcar loses 2 a second, each second, going up: from a standing start at the foot of a hill it does not climb at all, and one that runs out of speed part way up runs back down. A hill wants a run at it - at full speed it will carry up about twenty blocks. Coming down, the hill does the work. It stops dead at the end of the track. *Get out* to leave; left to itself it rolls till it stops.
- **A steam locomotive**: *Move left* and *Move right* send it to the end of the track that way - or, where there is a station that way, the choice reads *Move to* and the station's name, and it stops with its middle at the station. While it is going the choice is *Stop*. *Fuel* opens its one slot: it burns what is in it, one thing at a time, each for as long as it burns in a furnace (coal 1,200 ticks, fuel 3,600), and only while it is moving. With nothing to burn it does not go, and stops where it runs out. *Get in* puts a character in the cab.
- **A passenger car** has two seats, at its two windows: *Sit* takes the one clicked on. Seated, a click on the car offers *Get out*.
- **A freight car** is a chest on wheels: *Open* shows its 16 slots.
- Whoever is riding goes where the train goes, and can do nothing else until they get out; from their seat they can still work the menu of any part of their train. Dying puts a rider out.
- A train stops where the track ends. It does not look for blocks in its way beyond the square ahead of it.

In the data, track is a block with `"rail": true` (and `"texture_ramp"` for the climbing piece); `"needs_tool"` makes a block come up only with its tool in hand, and `"on_floor"` makes it go only on a block. A station is `"station": true`, `"written": true` and `"lays"` naming the track it lays. A vehicle is an item with `"vehicle"`: its `"width"` (1 or 3), whether it is an `"engine"` or worked by `"hand"`, its `"seats"`, `"cargo_slots"`, `"speed"` in squares a second, and its `"texture"`; one worked by hand also has `"accel"`, `"climb"`, `"gravity"`, `"drag"` and `"brake"`, each in squares a second, each second.

### Putting things up without a backwall

Lamps, torches, gems and the like go on a backwall where there is one, and look as they always have. Where there is none, a thing is fixed to a block instead - the one under it if there is one, failing that the one over it, failing that the one to its left, then its right - and is drawn to suit: standing on a foot, hanging from a chain, or held out on a bracket; a gem or a crystal grows out of whichever it is on. With no backwall and no block beside it, it cannot be put up.

- A **column** under it counts as a block, for anything that can stand on one: a lantern or a bench can be stood on top of a column.
- **Chandeliers** hang, and only hang: one can be put up under a block and nowhere else, backwall or no.
- **Stairs** are the exception already described: a flight can be put a step up or down on the slant from another with nothing else by it.

How a thing looks fixed each way is its `"texture_mount": { "floor", "ceiling", "side" }`; `"hangs": true` makes it a thing that only hangs, and `"supports": true` lets a background block be stood on.

### Gates, signs, compost and other things to put down

- A **wooden gate** is a block that a character walks through and a creature cannot: a way in that keeps the animals in.
- A **sign** is two blocks wide, and goes up against a block - standing on one, hanging under one, or fixed to the side of one, by either of its halves, and looks the part - but not on a backwall alone. It needs both of its squares free, so one cannot go where it would overlap another; two can stand end to end. A click on either half opens it to be written on: one line of up to 14 letters, typed; Done keeps it, Cancel does not, and Take down takes the sign down. The board is drawn centred on the block that holds it, where that is under it or over it: on the block at its left half - the square it was put in - or, if only its right half has a block, on that one; so it stands half a block out to either side of its block, though the two squares it takes up are the same as ever. (One fixed to the side of a block stays hard against it, and one with a block in the way stays in its squares.) A click anywhere on the board as drawn opens it. The line is written across the middle of the board, and smaller if it would not otherwise fit, so it never runs off the sign. A sign taken down - by either half - keeps what it says, says it again when it is put back up, and stacks with signs that say other things. Writing on one sign never changes another.
- **Compost** is ground. A plant growing on compost grows three times as fast: of the random events that land on it, it gets three times its usual share. (`"fertile"` on the block.)
- **Luminous plaster** is a white block that glows faintly and is not quite opaque: the backwall behind it shows through a little.
- A **window** or a **black window** goes on a backwall and is a hole in it: the wall is not drawn there, so the sky or whatever is behind shows through the glass. A clear window lets the daylight in as an open square would; a black one shows the view and keeps the light out. Neither can be put up where there is no backwall. (`"window": {}`, or `{ "dark": true }`.)
- A **reinforced platform** is a block to build with like any other, except that it brings no backwall: none behind itself, and none run down from it to the block below.
- An **ember** in hand, a click on anything made of wood - a wood block, a trunk, a chest, a bed, a door, a bench - burns it down in a moment and drops it, and uses the ember up. (`"burns"` on the block, `"ignites"` on the item.)
- A **boat** is put down on the top of water. A character in the boat's square who goes left or right across the surface takes the boat along, at twice the speed of swimming. (`"afloat"` on the block; `"boat_speed"` in `rules.json`.)

### Furniture

- A **safe** is a chest of iron: 16 slots, opened and carried off the same way.
- A **shelf** and a **display cabinet** hold 4 things each and go in the background; what is on one is drawn on it, small, so it can be seen from across the room. Taken down, they keep what they hold. (`"shows_contents": true`.)
- A **bed** is two blocks wide: it needs two squares side by side with level ground under both, and comes up as one bed from either end. A click on one brings up the little menu a bench has, with **Sleep** - the character goes to the bed and lies down in it, if they are tired enough to - and **Remove**. Sleeping while standing at a bed is quicker - twice as quick in a **wooden bed**, three times in a **soft bed** - and a soft bed goes on past a full bar: up to half a bar over, which shows gold and then runs down like the rest, so the next sleep is that much further off. **Coffee cherries** take energy past full in the same way. (A bed is a block with `"pair"`, the block that is its other half, and `"bed": { "speed", "over" }`; how far past full is `"energy_over"` in `rules.json`.)

### Cages

A **cage** in hand, a click on any creature of the land takes it up alive: the cage becomes a "Caged dodo", "Caged dropbear", "Caged scorpion", "Caged yak" or whichever it was. With that in hand, a click on open ground lets the creature out there - grown, and the player's own from then on, so it never wanders off for good - and gives the empty cage back. Creatures of the water cannot be caged. (A creature's `"caged"` names the item it becomes.)

### Paint

**Paint** in hand, a click on a block colours it the paint's colour; on a square with no block but a backwall, it colours the backwall. **Paint stripper** takes the colour off again. Each can is good for 100 uses - the dark part of a paint's swatch, and a stripper's bar, show how much is gone - and painting something the colour it already is, or stripping what is bare, uses none. Paint is kept with the world, and goes when the block or backwall it is on does.

### Columns and stairs

Every **column** and every flight of **stairs** from the builder's bench is put down in the background layer, in the 27 materials they are made in. Neither is solid: both are walked straight past.

- **Stairs can be walked up.** Steps hold up whoever is on top of them, so from beside a flight a step takes the player up onto it, and from there onto the next one a block higher, and so on; coming down is the same in reverse. Clicking somewhere up a staircase walks the player up it. With the keys, walking into stairs goes up them where they lead on to something - more stairs, or a floor at the top - and with up held as well, always; stairs standing by themselves in the way are walked past, as they are by a route that only passes them. Holding down walks through any of them along the ground. Something that falls onto stairs lands on them. They are drawn rising toward the next flight up, or failing that toward a block beside them.
- **Putting them up.** Like anything in the background they can go beside a block or against a backwall; besides that, a column can be put on or beside another column or stairs, and stairs can be put a step up or down (on the slant) from other stairs, so a staircase or a colonnade can be built out into the open air.
- Taken down, with a pickaxe for speed, each gives itself back.

Stairs are a block with `"steps": true`; a building piece that others can hold on to has `"attaches": true`.

Benches are placed in the background layer, like trees: the player walks in front of them, and an ordinary block can be placed in front of one. Chests are still solid blocks.

### Fuel

The kiln and the furnace burn fuel. A gauge on the side of their menu shows how much is left; it runs down all the time, by the tick, whether or not anything is being made. With none left the Craft button is replaced by "Needs fuel", and a crafting run in progress stops.

Clicking the gauge opens a list of the fuel in the inventory, each with how much of the gauge one of it fills; clicking an item burns one. Sticks, wood and coal are fuel, coal much the best. A bench that is removed loses the fuel in it.

Anything with a fire in it - campfire, kiln, furnace - shows it: flames flickering while it has fuel, and a bed of blackened coals when it has none. The frames are the block's `texture_lit` in `tiles.json`.

## Chests

Clicking a placed chest offers "Open chest" and "Remove chest". Opening it (the player walks up first) shows its 16 slots in a 4×4 grid beside it. Items are dragged between the chest and the inventory bar exactly as they are within the bar, including picking up part of a stack by holding. While a chest is open, a click on a stack sends the whole stack across: from the inventory into the chest, or from the chest into the inventory. It tops up a matching stack first, then takes a free slot, and anything that does not fit stays where it was. (So with a chest open, a click moves a stack rather than selecting it; the number keys still select.) A basket put in a chest keeps what it holds.

Removing a chest gives a chest item that still holds everything that was in it, and placing that item puts it all back.

What an item holds is part of the item, and two stacks only merge if they hold the same things. So two chests with 5 wood in each stack together, and neither stacks with an empty chest. Taking one off such a stack gives it its own copy of the contents. Baskets never stack.
### Mods, and Magic and Mayhem

A mod is a directory under `mods/` with a `mod.json` (`"label"`, and an `"icon"` file) beside data files and a `textures/` directory laid out like the game's own. It is loaded on top of `data/`: what it defines is added, and anything it defines under a name the game already uses replaces that. Its textures are looked for first in the mod, then in the game. The game comes with one mod, **Magic and Mayhem** (`mods/magic_mayhem`), written out from `ModDescriptions/MagicMayhem.txt`:

- **Benches.** A **lectern** (workbench: 2 wood, 1 dodo feather) with tiers II (1 elemental essence) and III (1 sundial essence); a **magic orb** (workbench III: 3 glass, 1 magic gem) with the same two tiers at the same costs; a fifth tier of the **workbench** (5 obsidian); and at that, the **magic core** (5 magic gems, 5 obsidian).
- **Paper, books and scrolls**, at the lectern: 3 paper from a sugarcane; a spellbook from 2 leather and 2 paper; a master's spellbook (tier II) from a spellbook and 2 magic gems; a grandmaster's (tier III) from a master's and a soulstone. A scroll is a book and a catalyst.
- **Catalysts and essences**, at the magic orb: a glass jar; flame, water, air and earth catalysts, each from the spirit's essence and something of its element; elemental essence from two of each catalyst and a jar; at tier II the sun and moon catalysts and sundial essence; at tier III the life catalyst.
- **At the magic core:** a magic conduit, soulstones, and the **magic array** - put up in the background, it makes work at any bench within 5 blocks of it go twice as fast. (The plan's wording is that it "increases crafting time by 2x"; it is taken to mean that work is quicker, not slower.)
- **Scrolls** are used by a click with one in hand, and each has so many uses before it is spent:
  - **Fireball** (10): at a creature up to 10 blocks off; 10 damage to it and to any within a block of it, and what they leave that can be cooked is left cooked.
  - **Water** (50): a bucket's worth of water in the square clicked.
  - **Shield** (10): a shield round the character that takes one health bar's worth of harm before it is gone; it shows as a pale line along the top of their health bar, and is saved with the world.
  - **Miner's** (1): breaks every plain block within 5 blocks of the one clicked, each leaving what it leaves. Benches, chests, doors, signs and anything else of more than a plain block are left standing.
  - **Sunrise** and **Moonrise** (1 each): time goes on to the next sunrise, or the next moonrise.
  - The **life scroll** is not cast: with 100 crystals it makes a **third character** at a portal, once there are two. (The second is made as ever.)
- **Spirits.** Four creatures that blows do nothing to: each is hurt only by the magic of one element, and one cast of that is the end of it. A **flame spirit** drifts about the sky by day and is ended by a water scroll; an **air spirit** does the same by night and is ended by a miner's scroll; a **water spirit** swims in the ocean by night and is ended by a fireball; an **earth spirit** walks the caves by day and is ended by a shield scroll. (A click on a spirit with the right scroll in hand casts it at the spirit.) Each leaves its essence about half the time.
- **In the world:** **obsidian** in veins near the bottom; **magic gems** in the caves, about as often as emeralds; **sugarcane**, a crop two blocks tall, on beaches; and a **magic tree**, bearing magic gem shards, on some of the islands in the sky above Y 110.

For someone writing a mod, what the engine was taught for this one is all in the data: an item's `"cast"` (`"element"`, `"damage"`, `"bane"`, `"splash"`, `"cooks"`, `"water"`, `"shield"`, `"dig"`, `"time"`) with `"uses"` and `"range"`; `"cooks_into"` on an item; a creature's `"flies"`, `"immune"` and `"weak_to"`, and `"sky"`, `"cave"` and `"time"` in its `"spawn"`; a block's `"quickens"` (`"speed"`, `"radius"`); a recipe's `"characters"`; and in `worldgen.json`, `"biome_plants"` and `"island_trees"`, which add to the biomes and islands that are already there without saying them over again.

### Industry

`mods/industry`, written out from `ModDescriptions/Industry.txt`. It cannot be on with Magic and Mayhem.

- **Lead**, in veins like iron's, smelted like iron. **Source ores** - of copper, tin and coal (one ore block in 100 of a vein), iron, lead and gold (one in 200), titanium and platinum (one in 300) - look like the ore with a gold frame round it, are mined like it with the same tools, and are still there afterwards: mined again and again, an ore each time.
- **Alloys**, at the **alloy furnace** (workbench IV; tiers II for a lead block and III for a royal bronze block): bronze and steel three at a time in ten seconds; **heavy bronze** (lead and bronze) and **royal bronze** (gold and bronze) at tier II; **dreadsteel** (titanium and steel) and **imperial** (platinum, lead and gold) at tier III. Each of the five new metals has its block, column and stairs.
- **The workbench** goes on to tier V (5 heavy bronze ingots) and VI (5 dreadsteel ingots).
- **Benches that run on electricity:** the alloy furnace and the **industrial bench** (conveyors and extractors; tier II for 2 imperial ingots) at 10 units a tick while they work; the **industrial armory** and **smithery** (dreadsteel and imperial armour, pickaxes, axes and machetes, the dreadsteel sword - 50 damage - and the imperial longbow - 20) at 50; the **industrial processor** (processing chips, industrial crates) at 100.
- **The electric miner** stands beside a source ore - on any of its four sides - and, for 10 units a tick, takes an ore from it every 20 ticks into the four slots it has.
- **Conveyors and extractors.** A **conveyor extractor** takes things out of whatever holds them beside it and sends them along the conveyors beside it to the nearest thing along them that will have them. If nothing will, nothing is taken. One item of a kind put in the extractor's own slot makes that the only kind it takes. A run of conveyor carries as much each tick as its slowest stretch: 1 for a conveyor, 2 for a gilded one, 5 for an imperial one. Out of a machine an extractor takes only what the machine has made; into one a conveyor puts only what the machine can use.
- **The industrial furnace** makes what an electric furnace makes, in half the time, by itself, for 50 units a tick: it has one slot for what goes in and one for what comes out, and starts whenever what is in the first can be made into something there is room for in the second.
- **The factory** is put beside a bench - on any of its four sides - and told one of that bench's recipes, with the arrows under its slots: any recipe of up to four kinds of thing that takes nothing but plain items. Then it makes it, at the bench's own pace, for 50 units a tick, from what is put in its four slots, into its fifth. Until it is told, a conveyor puts nothing in it; told, only what the recipe takes, each kind in its own slot. A **processing chip** upgrades it to six slots and twice the speed. Take the bench away and it forgets what it was told.
- **The industrial crate** holds 16 stacks and sorts: if anything is in its first four slots, conveyors put in it only those kinds of thing.
- **The power plant** makes 100 units a tick while it burns, and burns only dreadsteel ingots (1,000 ticks each) and imperial ingots (1,800).

(Two things the engine learnt for Explorer's Delight, for someone writing a mod: a biome that is `"below_sea"` and has a `"heat"` or `"dryness"` is that sea only where the climate is right; and `"cave_biomes"` in `worldgen.json`, each with a `"depth"`, a `"share"` of the columns, a `"wavelength"`, a `"lining"` and lists of what is on its `"floor"` and hangs from its `"roof"`.)

For someone writing a mod: `"endless"` on a block; `"conveyor"` (items a tick) and `"extractor"`; `"draw"` (units a tick while working) with `"miner"` (ticks an ore) or `"automat"` (`"bench"`, `"inputs"`, `"speed"`); `"sorts"` on a store; `"fuels"` on a generator; and `"vein_rares"` in `worldgen.json`. The code is `src/industry.c`.

### Digital Storage

`mods/digital_storage`: storage kept as numbers in cells, on a network of cable, after the manner of Applied Energistics 2. It goes with either of the other mods.

- **Certus quartz** is in the rock, as often as iron. The **inscriber** (workbench IV: 1 steel block, 10 copper wire, 4 certus quartz; 10 units of electricity a tick while it works) makes everything else: **fluix crystal** (quartz, a copper ingot and coal), the three **processors** (logic from gold, calculation from quartz, engineering from diamond - each with a silicon wafer and a fluix crystal), and every part below.
- **A network** is whatever is joined by **network cable** to a **controller**, which takes 10 units of electricity a tick; with none, the network is there and does nothing. One controller to a network.
- **Storage cells** - 1k, 4k, 16k and 64k - go in the four slots of a **storage drive**. A cell holds up to 63 kinds of thing; each kind costs it a hundred-and-twenty-eighth of its bytes - 8 bytes of a 1k cell, 32 of a 4k and so on - and every eight things cost one byte more. So a 1k cell holds 8,128 of one thing, or 63 things of 63 kinds and 4,096 more. A cell keeps what is in it when it is taken out of the drive, carried about, and put in another. Only plain things are stored: nothing with something inside it, or a meter - no buckets, tools that wear, chests with things in, or cells.
- **The storage terminal** is clicked from within a few blocks (from further off the character walks to it; hold Shift to dig it up instead). Its screen has three tabs:
  - **Stored**: everything in the network, most of first, with a search. A click takes a stack out into what the character carries; the other mouse button takes one. The character's own bar is underneath: a click on a stack there puts it in, the other button puts in one, and **Store all** puts in everything it can.
  - **Make**: everything the network can make (see below). Pick one, set how many, and **Make**. If it cannot be done, it says what is missing.
  - **Jobs**: what is being made, how far along, whether it is waiting for something, and a button to cancel it.
- **Buses** stand beside whatever holds things - a chest, a crate, a machine. An **import bus** takes what is in there into the network, four things a tick; an **export bus** puts the network's things out into it. One item in a bus's own slot is the kind it deals in: an import bus with none takes everything, an export bus with none sends nothing. A **storage bus** makes the chest beside it part of what the network stores: counted, drawn on, and filled when the cells are full.
- **Making things.** An **assembler** beside a bench - on any of its four sides - teaches the network every recipe of that bench that takes nothing but plain things. A **crafting CPU** runs one job at a time; each **co-processor** on the network makes every job that much faster (one halves the time, two cut it to a third). Asked for something, the network uses what it has and makes what it is short of, and what that takes in its turn, eight deep; what it makes goes into store.
- **A wireless access point** lets the network be reached from 32 blocks away with a **wireless terminal** in hand: a click anywhere brings up the same screen.
- **A quantum bridge** is joined to every other quantum bridge in the world, however far apart: cable at one is on the same network as cable at another.
- **Planes.** An **annihilation plane** breaks the plain blocks beside it, one every ten ticks, into the network. A **formation plane** puts the kind of block in its slot out of the network into the open squares beside it.

What Applied Energistics 2 has that this does not: channels and the different weights of cable (a network here is as big as its cable); encoded patterns and interfaces (an assembler beside a bench stands in for both); the security terminal, spatial storage, P2P tunnels, level emitters, the matter condenser, the IO port and cell workbench, fluid storage, facades, and its tools and weapons. Jobs that are running when a world is saved are not kept; what they had taken out of store for the item in hand is lost with them.

For someone writing a mod: `"network"` on a block (`cable`, `controller`, `drive`, `terminal`, `import`, `export`, `storage_bus`, `access` with `"network_range"`, `cpu`, `coprocessor`, `assembler`, `bridge`, `break`, `place`); `"cell"` (bytes) and `"remote"` on an item. The code is `src/digital.c`, and the screen `src/termui.c`.

### Culinary Delights

`mods/culinary_delights`: things to grow, places they grow in, and a kitchen to cook them in. It goes with every other mod.

- **Thirty crops.** Twenty-two that grow one block tall - strawberry, blueberry, raspberry, garlic, onion, potato, cabbage, lettuce, bell pepper, pumpkin, melon, peanut, ginger, basil, mint, soybean, cucumber, eggplant, beetroot, spinach, sugar beet and mushroom - and eight that grow two tall and are cut back to grow again: rice, grapes, hops, barley, sesame, green beans, peas and tea. Each is sown from what it bears, in dirt, grass, compost or sand.
- **Twelve trees**, each grown from its fruit: lemon, lime, peach, pear, plum, banana, olive, cocoa, cinnamon, avocado, walnut and fig.
- **Six kinds of country**, each with its own crops and trees wild in it: **berry meadows** (berries, plums, pears), **citrus groves** (lemons, limes, basil, melons), the **spice jungle** (bananas, cocoa, cinnamon, ginger, sesame, peanuts), **vineyard hills** (grapes, olives, figs, barley, hops), **rice terraces** (rice, soy, tea, cabbage) and **harvest fields** (pumpkins, potatoes, beets, beans, peas). They take their places among the old ones by heat and dryness, so only worlds made with the mod on have them. A little grows in the old country too: potatoes and lettuce on the plains, mushrooms in the forests.
- **The kitchen**, all made at a workbench: the **mortar** (sugar, cocoa, spices, pastes, flours and oils), the **fermenting barrel** (butter, cream, cheese, yogurt, tofu, soy sauce, vinegar, wine, beer, pickles, kimchi), the **prep table** (salads, dips, doughs, sandwiches, sushi, drinks), the **cooking pot** (sauces, jams, chocolate, soups, curries, noodles and rice dishes, tea), the **baking oven** (breads, pies, tarts, cakes, bakes and roasts) and the **chef's table**.
- **Where it leads.** A dish is made of prepared things, which are made of ground, fermented or cooked things, which are made of what was grown. At the chef's table four **platters** put dishes together (breakfast platter, picnic basket, cheese board, mezze). A picnic basket upgrades the table to tier II, where six **feasts** are each five dishes: Italian, Eastern, harvest, garden and spice feasts, and the dessert trolley. A harvest feast upgrades it to tier III, where the **royal banquet** is five feasts, and **ambrosia** - the banquet, the dessert trolley, a cheese board, a mezze and a rainbow cake - is the end of it: six makings deep, and it fills hunger, health and energy to the top and beyond.
- 106 recipes in all. What a thing costs at the trade portal is worked out from what goes into it.

### Explorer's Delight

`mods/explorers_delight`: ten more kinds of country, three kinds of sea and six kinds of cave, each made of ground and rock of its own, and a great many things to build with out of what is found there. It goes with every other mod.

- **Ten biomes**, placed among the others by heat and dryness (so only worlds made with the mod on have them):
  - **Red mesa** - red sand over terracotta, with red sandstone at its heart; baobabs and dead bushes.
  - **Salt flats** - a white crust over rock salt.
  - **Volcanic wastes** - ash over dark red scoria, with pumice at its heart.
  - **Frozen tundra** - packed snow over glacial ice, with permafrost deep in.
  - **Slate highlands** - coarse highland grass over slate; heather, and a birch here and there.
  - **Misty marsh** - mud over peat; willows and reeds.
  - **Savanna** - dry grass; acacias, and the odd baobab.
  - **Birch woods** - thick with birch, over granite at its heart.
  - **Mossy hollow** - moss underfoot, mossy stone below, and glowcaps: mushrooms the size of trees, whose caps give light.
  - **Bamboo grove** - bamboo, close together.
- **Three seas**, by how warm the water is (where none of them is, the sea is the one the game has):
  - **Coral reef**, in warm water - coral sand over pink coral stone, with red, blue and yellow coral growing on it, sea grass, and now and then a clam with a pearl in it.
  - **Frozen sea**, in cold water - sea gravel over glacial ice.
  - **Seagrass meadow**, in between - sand thick with sea grass, and more clams.
- **Six kinds of cave**, by how deep. A stretch of the underground is one kind or none, settled by the world's seed; in it the rock round the caves, and the wall behind them, is that kind's own, and its own things grow there:
  - **Ice caves** (near the surface) - glacial ice, hung with icicles.
  - **Lush caves** (near the surface) - moss, ferns, and glow vines hanging from the roof, which give light and bear glow berries.
  - **Dripstone caves** (further down) - dripstone, with stalactites above and stalagmites below.
  - **Mushroom caves** (further down) - mycelium, and glowshrooms, which give light and can be eaten.
  - **Crystal caverns** (deep) - dark geode, set with crystal clusters that give a strong light and leave prism shards.
  - **Magma caves** (deepest) - magma rock, which glows a little, and ember blooms, which leave embers.
- A biome's own ground is a block deep at its edge and deepens toward its middle. Granite, mossy stone, pumice and red sandstone, which lie only under the heart of their country, are in pockets in the rock everywhere as well.
- **Thirteen stones** - terracotta, red sandstone, rock salt, scoria, pumice, slate, granite, mossy stone, glacial ice, and from the sea and the caves coral stone, dripstone, geode and magma rock - each a block to build with as it is, and each with **bricks**, a **polished** block, **tiles**, a **column** and **stairs**, made at the builder's bench: 78 blocks. **Mud bricks** (mud and dry grass), with their column and stairs, besides.
- **Six woods** - acacia, baobab, birch, willow, bamboo and glowcap - each a block of its own colour with a column and stairs, and each good wherever wood is wanted (one of any makes one wood at a workbench). Each tree is grown again from what it bears: a seed, a catkin, a spore, a baobab fruit or a bamboo shoot, the last two of which can be eaten.
- **Ground to build with too**: red sand, salt crust, ash, packed snow, permafrost, mud, peat, coral sand, sea gravel, mycelium, highland grass, dry grass and moss are all blocks that are dug up and put down.
- **From the sea and the caves**, at the builder's bench: **coral blocks** in red, blue and yellow (four pieces of coral each), the **pearl block** (four pearls), and the **prism block** (four prism shards), which gives light. Coral and sea grass are grown again under water from what they leave.
- Odds and ends: rock salt and salt crust give salt; four packed snow make glacial ice; four red sand make red sandstone; stone and moss make mossy stone; reeds make sticks.

### Discovery Road

A mod (`mods/discovery_road`, written out by `tools/mod_discovery_road.py`) about finding things. It adds **relics**: thirty-six small old things, each of which does a little for whoever carries it.

**Relic slots.** With the mod on, the head button opens a second column beside the armour: four relic slots. They take relics and nothing else, one to a slot and no two the same. A relic does nothing in the bar or a basket - only in a relic slot. Relics are kept with the character and the world.

**What relics do.** Each has one or two of these: faster on foot; faster at digging; faster at a bench; more damage to every blow; a share more of every attack stopped; a share more of the environment score kept; mending faster; a longer breath; hunger or tiredness coming on more slowly; luck - a chance that a block dug leaves one more of what it leaves; and thorns - whatever bites or stings the character is hurt for it. They add up with each other and with armour.

**Sets.** The relics come in nine sets of four, and a set gives more as more of it is carried: one bonus with two of its pieces in the slots, and a second, on top, with all four.

| Set | Its relics | With 2 | With all 4 |
| --- | --- | --- | --- |
| Wayfarer's | Worn compass (8% faster on foot), Wayfarer's flask (hunger 15% slower), Trail marker (tiredness 15% slower), Lucky horseshoe (6% luck) | 5% faster on foot | 10% more, and tiredness 10% slower |
| Grove | Blossom charm (mends half again as fast), Amber acorn (tiredness 20% slower), Carver's knot (12% faster at a bench), Pinecone idol (15% of the environment score kept) | mends 25% faster | 15% faster at a bench, 10% more kept |
| Dune | Scarab brooch (6% of attacks stopped), Sun disc (20% kept), Thorn ring (1 thorns), Nomad's waterskin (hunger 25% slower) | 4% more stopped | 1 more thorns, hunger 15% slower |
| Tide | Pearl of breath (twice the breath), Coral crown (mends 30% faster), Drowned coin (8% luck), Sailor's knot (2 more damage) | half again the breath | 2 more damage, 5% luck |
| Jungle | Jade idol (3 more damage), Feathered charm (10% faster on foot), Monkey's paw (10% luck), Vine bracelet (mends 40% faster) | 1 more damage | 10% faster on foot, 2 more damage |
| Deepdelver | Ember lantern (12% faster digging), Prospector's pin (18%), Geode heart (8% stopped), Canary feather (60% more breath) | 10% faster digging | 20% more, and 8% luck |
| Abyssal | Obsidian skull (5 more damage), Magma core (40% kept), Void shard (3 thorns), Heart of the mountain (mends twice as fast, 8% stopped) | 6% more stopped | 3 more damage, 2 more thorns |
| Skyborne | Cloud feather (18% faster on foot), Star fragment (14% luck), Wind chime (tiredness 30% slower), Sunstone (30% kept, mends 20% faster) | 6% faster on foot | 12% more, and 8% luck |
| Artisan's | Gilded hammer (25% faster at a bench), Tinker's gear (15%), Old coin (5% luck), Hearthstone (15% kept, hunger 10% slower) | 10% faster at a bench | 20% more, and 10% faster digging |

**Relic chests** are a new kind of chest, of dark wood bound in green bronze, that the world comes with: in caves at every depth, and now and then out on the ground. Each holds one relic, sometimes two, with a little relic dust and perhaps a gold coin - and **which relics depends on where the chest stands**. Every place has its own, and a chest holds what every place it is in has, taken together:

| Where | What is found there |
| --- | --- |
| Anywhere | the Artisan's set, and now and then the Wayfarer's |
| The plains | the Wayfarer's set, a little of the Grove |
| The woods (cherry grove, orchard, pine and maple forest) | the Grove set, some of the Wayfarer's |
| The desert | the Dune set |
| The shore and the sea | the Tide set, a little of the Dune |
| The tropics | the Jungle set, a little of the Tide |
| The cold country (pine forest) | the relics that keep the weather off: Pinecone idol, Hearthstone, and more rarely Sun disc, Sunstone, Magma core |
| Just under the ground | Wayfarer's, Deepdelver and Artisan's together |
| The depths (26 to 110 below) | the Deepdelver set |
| The very bottom of the world (below 110) | the Abyssal set, some of the Deepdelver |
| The heights and the sky (above 60) | the Skyborne set |

So a chest far down under a desert may hold a Dune relic's neighbour from the depths, but never a Skyborne one; and the Abyssal and Skyborne sets are only found at the bottom and the top of the world. With Explorer's Delight on as well, its biomes join these places (the red mesa is desert, the coral reef is shore, and the volcanic wastes have the Abyssal set at the surface); nothing depends on it. The recipe book says, for each relic, where it is found. A relic chest holds 8 stacks, can be picked up and put down again, and one put down by hand is empty.

**Ancient urns** lie about in caves, and in deserts and on beaches. Broken, an urn leaves relic dust - one, sometimes two - and now and then a gold coin.

**The reliquary** is a bench (workbench tier 2: 6 stone, 2 glass, 1 gold ingot). At it any relic is **broken down** into 3 relic dust, and 12 relic dust with an amethyst are **restored** into a relic - which, there is no saying: any of the thirty-six, the everyday ones likelier and the Abyssal and Skyborne rarer. That is what to do with the third Worn compass. It also makes a relic chest of your own from a chest, 4 relic dust and 2 bronze ingots.

**The relic identifier** is a lens (made at the reliquary: 2 glass, 1 gold ingot, 4 relic dust). With one anywhere in the bar or a basket, putting the cursor over a relic - in a relic slot, the bar, a basket or a chest - shows what it does: its own effects; its set, and how many of it are in the slots; what the set gives at two pieces and at four, in green once it is being given; and the set's four relics, the ones carried marked. It reads armour the same way. Without one a relic says nothing of itself.

Relics have a category of their own at the trade portal, **Relics**, all at tier VI: found first, as everything is, they can then be bought and sold.

In the data, a relic is an item with `"relic": true` and `"boons"`; `"set"` names its set, and `"sets"` in `items.json` says what each set gives for how many `"pieces"`. A chest's loot is `{"area": true}` for something from the place it stands in, and `"loot_pools"` in `worldgen.json` are the places: each with its `"biomes"` (any, if none), the heights `"y"` it runs between, and its `"loot"` by weight. Armour can have boons and a set as well.

### Alchemie

A mod (`mods/alchemie`, written out by `tools/mod_alchemie.py`) of benches that turn things into other things, ending in one that makes money out of nothing.

- **The alembic** (workbench tier 2: 4 glass, 2 copper ingots, 2 clay) draws the six **essences** out of ordinary stuff, 20 seconds each: *terra* from 16 dirt, 12 stone, 12 sand or 8 gravel; *vita* from 8 wheat, 8 flax, 6 apples, 6 kelp or 4 compost; *ignis* from 4 coal, 6 charcoal or 2 oil; *aqua* from 4 ice or a bucket of water (the bucket is kept); *metallum* from 2 copper ore, 2 tin ore or 1 iron ore; *aether* from 2 amethyst shards, or 2 of it from an amethyst.
- **The athanor** (workbench tier 3: 8 red brick, 4 iron ingots, 2 ignis, 2 terra) turns one thing into another with an essence to do it.
  - Metals, up the ladder: 2 copper ingots and a metallum make 2 tin (and back); 3 copper, a metallum and an ignis make an iron ingot; **3 iron, 2 metallum and an aether make a gold ingot**; 2 gold, 2 metallum and an ignis make titanium; 2 titanium, a metallum and 2 aether make platinum.
  - Stone, four at a time: stone to gravel to sand and round again (terra); stone to limestone (aqua) or basalt (ignis); sand to sandstone (terra and ignis), glass (ignis) or black sand (metallum); limestone to marble (ignis); marble to red marble (metallum) or lapis (aether); and 4 ice from 2 aqua.
  - And 4 dirt into clay (aqua) or compost (vita), 4 charcoal into 2 coal (terra), 2 coal into 2 oil (aqua).
- **The alchemist's desk** (workbench tier 3: 6 wood, 2 gold ingots, 4 aether, 2 vita) does gems and the great works. Each gem is made of two of the one before with aether: amethyst to sapphire (1), sapphire to emerald (2), emerald to ruby (2), ruby to diamond (3). **Quicksilver** is 2 metallum and an aqua. The **philosopher's stone** is a diamond, 4 quicksilver and 2 of every essence, and takes ten minutes.
- **The Tome of Knowledge** (alchemist's desk: 8 linen, 4 aether, 4 vita, a gold ingot) knows everything there is. With it in the bar, or in a basket in the bar, every item counts as found in that world - for good, whether the tome is kept or not - so every `?` at a trade portal becomes the thing itself. (The portal's tier still has to be reached, and what it never deals in it still does not.)
- **The Transmutation Table** (alchemist's desk: the philosopher's stone, 2 gold blocks, 8 marble, 8 aether) makes things **out of nothing at all**: no ingredients, only time. A **platinum coin** takes fifteen minutes; an aether essence, two. As with any work at a bench it goes on while the character does other things nearby, can be queued up to the usual number, and can be hurried for crystals.

### Dimensions

A mod that gives a world two more places, and a third of your own. Each is as wide as the world and lies over the same columns: go through a portal at column 1,200 and you come out at column 1,200.

- **The Aether** - nothing but islands in open sky, with banks of cloud along the bottom. Eight kinds of country (Cloudmeadow, Skywood, Stormreach, Crystal spires, Sunpetal gardens, Frostveil - which is cold - the ruins and the Cloud shoals), six stones, eight trees, eight crops, four metals (skysteel, galvanite, aerite, moonsilver) and two gems (**zephyrite** and **heliodor**). Whoever comes to the bottom of it falls out, into the sky over the same column of the world below: have a glider on, or a jetpack, or water under you.
- **The Deep** - all rock, with caverns many times the size of the world's own, lakes in some of them and magma along the bottom. It has no sky: the only light is what glows, or what you bring. Nine kinds of country (Fungal forest, Glowmoss grotto, Crystal cathedral, Bone hollows, Sunken lake, Obsidian flats, Magma fields, the Roots, the abyss), six stones, eight giant fungi for trees, eight crops, three metals found (deepiron, cinderite, adamant) and one made (umbral steel), and three gems (lumite, **umbrite** and **emberheart**). The Magma fields and the abyss are **hot**: the environment score runs down there indoors and out, warm clothes are no help, and only what cools - a heat suit, and the armour of the Deep's metals a little - keeps it. **Magma** takes health from whoever stands on it or against it, and is fuel.
- **A world of your own** - what the World Seed leads to: 128 columns of empty sky with one island in the middle and a diamond tree on it. Every World Seed in a world leads to the same one, and you come back out by whichever you went in at.

A place is made the first time somebody goes there, and saved with the world from then on. Everything goes on in all of them at once: crops grow, machines run and creatures wander in the Aether while you are in the Deep. Only the place the view is in is drawn.

**The ways through.** The **Aether portal** only works set down 80 or more above where the world began - on one of the islands in the sky, or on something built up to there; the **Deep portal** only within 20 of the bottom of the world. Click one and choose Enter: the character goes to it and through. Where they come out a way back is put up beside them (and something to stand on, if there was nothing); going again from near the same place uses the same one. In the Aether, the Deep and the place of your own a portal works wherever it stands.

**Getting there.** Each of the four great makings is a tree ten and more makings deep:

- *Aether portal* (12): featherweight alloy (electric furnace) - featherweight plate - resonant coil - storm capacitor - charged capacitor (electric press) - sky lens - gem prism (one of each of the five gems) - sky compass - keystone core - Aether keystone (Portal VI) - Aether frame - the portal, at Portal VI. All from what the world has to begin with. The same alloy makes the **featherweight pickaxe** and the **glider**.
- *Deep portal* (15): from the Aether's metals and creatures - aerite lattice, lift cell, inverted cell, gravity coil, charged gravity coil, gravity core, storm heart (which takes a storm feather: the **Thunderbird** of Stormreach drops it), deep compass, deep keystone core, deep keystone (Portal VIII), deep frame - the portal, at Portal VIII.
- *World Seed* (11), at **Portal IX**: rich loam to sleeping seed, out of what the Deep grows and what lives there (it takes a golem core: the **Crystal golem** of the Crystal cathedral).
- *Third blockhead* (15), at **Portal X**: an adamant frame, jointed, strung, given an engine, an eye, a lattice to think with and a spark - a third character, where a world could only ever have two. It can be made only in a world that has two already.

**More levels of what the game has.** The portal goes on to **VII** (1 zephyrite), **VIII** (1 heliodor), **IX** (1 umbrite) and **X** (1 emberheart), each making its gem's pickaxe; the trade portal to VII-X for the same gem, platinum coins and crystals, and everything in the mod has a tier from VI to X, in the trade's usual categories. The workbench goes on to the **Aether workbench** (5 skysteel ingots and a zephyrite) and the **Deep workbench** (5 deepiron ingots and a golem core) - Workbench V and VI, when no other mod has put levels of its own on it; with Industry on they come after Industry's. The tool bench has five more (skysteel, moonsilver, deepiron, umbral, adamant), the armor bench four, the furnace two, and the metalwork bench, craft bench, press, tailor's bench, woodwork bench, builder's bench, kiln, mixing bench and campfire two each (a sky one and a deep one). New benches: the skyforge, cloud loom and stargazer's table in the Aether; the magma forge, crystal lathe, fungal vat and bone carver in the Deep.

**Hardness.** Some blocks are too hard for a lesser pick: clicking them with one does nothing. A featherweight pickaxe digs the Aether's first ores; skysteel digs moonsilver and the Aether's gems; moonsilver digs the Deep's first ores; deepiron digs umbrite; umbral steel digs adamant; adamant digs emberheart.

**Other things.** A **glider** (chest) makes a drop of any height a step like any other. **Wind turbines** and **lightning rods** make electricity under open sky, by day and by night, more the higher they stand; a **geothermal generator** makes it beside magma or a steam vent, for ever. Galvanite wire, a storm battery, cloudweave clothes (warm), heat clothes (cool), wind boots and moleskin boots (quicker on foot), a cloud bed, lamps that need no fuel, doors, trapdoors, shelves and lanterns in every wood and metal, and about forty things to eat and drink.

**Creatures.** In the Aether: fleecelings (shorn for cloud wool), moas, cloud rays, sky jellies, storm wisps, star moths (by night), gale hawks that drop out of the trees as dropbears do, ruin sentinels, frost mites, zephyrite and heliodor dodos, and the Thunderbird. In the Deep: glow beetles, cave crawlers, deep bats, blind cavefish and lantern eels in the lakes, magma slugs, spore puffers, tunnel moles (shorn), rock worms, fungus striders, umbral stalkers, bone hounds, umbrite and emberheart dodos, and the Crystal golem.

**What was chosen, where the plan left it open.** Both places run at once. Magma is a block that stands still, not a second liquid. The Thunderbird and the Crystal golem are ordinary creatures with a great deal of health, not a new kind of fight. All the art is the game's own, recoloured by `tools/mod_dimensions.py`. **Not built** from the plan: wind and lightning as weather, floating-block and bouncing rules beyond aerite blocks needing no support, falling rock and gas, creatures to ride or tame, the airship and the drill rig, drinks whose effects last a while, sky anchors and recall stones, fish in the Aether, and built ruins (the ruins are a kind of country, not buildings).

**In the data.** `"dimensions"` in `worldgen.json`: a `"generator"` (`"islands"`, `"caverns"` or `"void"`), a `"height"`, its `"rock"` and `"edge"`, and what the generator wants. A biome, a vein or a cave decoration with a `"dimension"` belongs to that place and no other. A block with `"leads_to"` is a way there (`"opens_above"` / `"opens_below"` say where it works). `"extends"` on a bench or trade block hangs it on the end of that kind's levels, and a recipe made `"at"` a named block wants that block's level. `"hardness"` on a block and `"power"` on a tool; `"harm"` on a block; `"hot"` on a biome and `"cooling"` on clothes; `"glides"` on something worn; generators that need `"sky"` or `"heat"`.

### Golden chests

Under a beach, quite often - about one column in six, a dozen or more to a world - a golden chest lies buried at the bottom of the sand: behind the sand, in the background, so it is not seen or opened until the sand in front of it is dug away. (Golden chests and portal chests are things of the background: they are walked past, not into.) A golden chest is what a portal chest is made from (with 100 crystals, at a tier 6 portal), and a tier 2 metalwork bench melts one down into a gold coin. It opens and is carried off like any chest. The first time it is opened or dug up it holds 1 to 5 gold coins, up to 3 gold ore, up to 3 iron ingots, perhaps a titanium ingot, and one gem shard: of amethyst most often, then sapphire, emerald, and ruby least. What a block is found holding is its `"loot"` in `tiles.json`, and where it is buried is `"buried"` on a biome in `worldgen.json`; a golden chest the player puts down, or buys, holds only what they put in it.

## Layout

- `data/` — all game content (see Data above)
- `src/content.c` — loads the data files into the tables the engine reads
- `src/world.c` — the wrapping tile grid and terrain generation
- `src/water.c` — the water in each square and how it moves
- `src/tree.c` — tree shapes and growth
- `src/inventory.c` — stacks, baskets and the rules for moving them
- `src/craft.c` — crafting from the recipes
- `src/trade.c` — buying, selling and exchanging coins
- `src/power.c` — electricity: generators, flywheels, wires, and what draws on them
- `src/guide.c` — what the content says about where each item comes from, for the recipe book
- `src/mob.c` — creatures: spawning, wandering, eggs and hunting
- `src/light.c` — how well lit each tile is
- `src/game.c` — player, movement rules, path planning, action queue, item drops, crafting
- `src/save.c` — saving and loading a world
- `src/assets.c` — textures for blocks and items
- `src/hud.c` — the on-screen inventory bar
- `src/benchui.c` — the bench popup and crafting menu
- `src/tradeui.c` — the trade portal menu
- `src/train.c` — the railway: track, and what runs on it
- `src/trainui.c` — the menu over something on the rails, and its slots
- `src/world3d.c` — the depth of the view: the camera, blocks as blocks, pictures given thickness
- `src/playerui.c` — the player's button, bars and the sleep and meditate panel
- `src/playermodel.c` — the character: six boxes with a skin on them, and what is worn over them
- `src/recipeui.c` — the recipe book
- `src/menu.c` — main menu, pause menu and key bindings
- `src/view.h` — placing the wrapping world on screen
- `src/main.c` — window, input, camera and rendering
- `tests/test_game.c` — tests for the rules, with no window
- `third_party/cjson/` — the JSON parser (MIT licence)
- `AGENTS.md` — the standing rule for working on this project: keep everything data driven
