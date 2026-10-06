# BlockGame

A small 2D block-based game engine in C, using [raylib](https://www.raylib.com) for the window, input and drawing.

## Build and run

```sh
make run          # build and start the game (random world)
./build/blockgame 42   # start with a fixed seed
make test         # headless tests of the game rules
```

The first build downloads raylib 5.5 into `vendor/` and compiles it.

On an Apple Silicon Mac, if the shell is running under Rosetta the compiler may fail with an `xcrun` error; use `arch -arm64 make run` instead.

## Controls

| Input | Effect |
| --- | --- |
| `A` / `D` | Move left / right over whatever is in the way (see below) |
| `W` / `S` | Climb up / down a wall; `W` on the ground jumps and catches a wall if one is beside you |
| Click an air tile | Queue a move that puts the player's feet or head on that tile |
| Click a ground tile | Queue walking next to that tile and mining it |
| Click air beside a block, holding a block | Queue walking next to that tile and placing the block |
| Click a bench | Open it, or choose between opening and removing it |
| Drag | Pan the map |
| `Space` | Make the camera follow the player again |
| `1`–`8` | Select an inventory slot |

Clicks queue up and run in order. A click that cannot be carried out is refused with a red cross on the tile. Pressing a movement key cancels the queue.

## Movement

Holding a direction, or a queued click, takes the player:

- up a one-block ledge;
- over a one-block gap with a short hop (hold `S` as well to go down into it instead);
- down a one-block drop with a plain step;
- over the edge of a drop of two or more blocks into a climbing pose on its wall, then down the wall;
- from one wall to a facing wall up to two columns away with a hop, so a pit two or three blocks wide is crossed near the top without going to the bottom;
- up any wall, pulling onto the top.

## Inventory

The bar along the bottom has a head button followed by 8 slots. Stacks hold up to 99. The player starts with one basket (item ID 1).

- Tap a slot to select it. Tapping a basket also opens or closes its 4 slots, which extend upward from it.
- Tap an item inside a basket to select it; the closed basket then shows that item. Tapping an empty basket slot selects the basket.
- Tap the head button to open the 4 armor slots (head, chest, legs, boots) and every basket at once.
- Drag an item onto another slot to move it: equal stacks merge, anything else swaps.
- A basket can be put inside another basket and keeps its contents, but only baskets sitting in the bar can be opened.
- Clicking or dragging the world, or pressing a movement key, closes everything.

A mined block drops as an item where it was. It is collected when the player is on or next to its tile, a moment after it drops, as long as there is room in the bar or in a basket in the bar.

## Building and benches

With a block selected (dirt or a workbench), clicking an air tile that touches a block queues placing it there, marked with a green outline and a faded copy of the block. Other air tiles still queue a move. Select a slot that is not a block to walk onto tiles next to the ground.

The spawn portal stands where the player spawns and is a workbench that cannot be removed, so clicking it opens it straight away. Clicking a placed workbench shows "Open bench" and "Remove bench". Opening is a queued action: the player walks up to the bench first.

An open bench shows a menu beside it in the world:

- a row of the items it can make, which scrolls sideways (wheel or drag) only when there are more than fit;
- for the selected item, its recipe and output, a slider from 1 to 99 that stops at what the inventory can pay for, and a Craft button or "Not enough items".

Each item takes 100 ms to craft. Output that does not fit in the inventory is dropped at the player's feet. The bench closes when the world is clicked, a movement key is pressed, or the player moves out of reach, and that ends any crafting still running.

| Bench | Makes | From |
| --- | --- | --- |
| Spawn portal | Workbench | 1 dirt |
| Workbench | Basket | 1 dirt |

## Layout

- `src/world.c` — tile grid and world generation
- `src/inventory.c` — items, stacks, baskets and the rules for moving them
- `src/craft.c` — recipes for each kind of bench
- `src/game.c` — player, movement rules, path planning, action queue, item drops, crafting
- `src/hud.c` — the on-screen inventory bar
- `src/benchui.c` — the bench popup and crafting menu
- `src/main.c` — window, input, camera and rendering
- `tests/test_game.c` — tests for the rules, with no window
