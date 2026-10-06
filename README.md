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
| `A` / `D` | Walk left / right, climbing ledges one block high |
| `W` | Jump one block |
| Click an air tile | Queue a move that puts the player's feet or head on that tile |
| Click a ground tile | Queue walking next to that tile and mining it |
| Drag | Pan the map |
| `Space` | Make the camera follow the player again |

Clicks queue up and run in order. A click that cannot be carried out is refused with a red cross on the tile. Pressing a movement key cancels the queue.

## Layout

- `src/world.c` — tile grid and world generation
- `src/game.c` — player, movement rules, path planning, action queue
- `src/main.c` — window, input, camera and rendering
- `tests/test_game.c` — tests for the rules, with no window
