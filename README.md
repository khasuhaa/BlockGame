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
| Drag | Pan the map |
| `Space` | Make the camera follow the player again |

Clicks queue up and run in order. A click that cannot be carried out is refused with a red cross on the tile. Pressing a movement key cancels the queue.

## Movement

Holding a direction, or a queued click, takes the player:

- up a one-block ledge;
- over a one-block gap with a short hop (hold `S` as well to go down into it instead);
- down a one-block drop with a plain step;
- over the edge of a drop of two or more blocks into a climbing pose on its wall, then down the wall;
- from one wall to a facing wall up to two columns away with a hop, so a pit two or three blocks wide is crossed near the top without going to the bottom;
- up any wall, pulling onto the top.

## Layout

- `src/world.c` — tile grid and world generation
- `src/game.c` — player, movement rules, path planning, action queue
- `src/main.c` — window, input, camera and rendering
- `tests/test_game.c` — tests for the rules, with no window
