RAYLIB_VERSION := 5.5
RAYLIB_DIR := vendor/raylib-$(RAYLIB_VERSION)
RAYLIB_LIB := $(RAYLIB_DIR)/src/libraylib.a

CC ?= cc
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra

ifeq ($(shell uname),Darwin)
LDLIBS := -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL
else
LDLIBS := -lGL -lm -lpthread -ldl -lrt -lX11
endif

ENGINE_SRC := third_party/cjson/cJSON.c src/content.c src/world.c src/water.c src/tree.c src/inventory.c src/craft.c src/game.c src/mob.c src/light.c src/trade.c src/train.c src/guide.c src/power.c src/industry.c src/signal.c src/digital.c src/save.c src/mods.c
HEADERS := $(wildcard src/*.h)
DATA := $(wildcard data/*.json)

.PHONY: all run test clean

all: build/blockgame

build/blockgame: src/main.c src/assets.c src/hud.c src/benchui.c src/tradeui.c src/trainui.c src/world3d.c src/playerui.c src/playermodel.c src/recipeui.c src/termui.c src/menu.c $(ENGINE_SRC) $(HEADERS) $(RAYLIB_LIB)
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(RAYLIB_DIR)/src src/main.c src/assets.c src/hud.c src/benchui.c src/tradeui.c src/trainui.c src/world3d.c src/playerui.c src/playermodel.c src/recipeui.c src/termui.c src/menu.c $(ENGINE_SRC) $(RAYLIB_LIB) $(LDLIBS) -o $@

# (Not inlined: each test keeps whole worlds on the stack, and all of them in one frame is too much.)
build/test_game: tests/test_game.c $(ENGINE_SRC) $(HEADERS)
	@mkdir -p build/mod
	$(CC) $(CFLAGS) -fno-inline tests/test_game.c $(ENGINE_SRC) -o $@

run: build/blockgame
	./build/blockgame

test: build/test_game $(DATA)
	./build/test_game

# raylib is downloaded and built on first use; it is not checked in.
$(RAYLIB_LIB):
	@mkdir -p vendor
	curl -sSL https://github.com/raysan5/raylib/archive/refs/tags/$(RAYLIB_VERSION).tar.gz | tar xz -C vendor
	$(MAKE) -C $(RAYLIB_DIR)/src PLATFORM=PLATFORM_DESKTOP

clean:
	$(RM) -r build
