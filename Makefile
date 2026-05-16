CC = gcc
TARGET = game

CFLAGS = -Wall -Wextra -O2
CPPFLAGS = -DMERGED_BUILD \
	-Isrc/main_menu \
	-Isrc/options \
	-Isrc/load \
	-Isrc/choose \
	-Isrc/top_scores \
	-Isrc/enigma \
	-Isrc/ui \
	-Isrc/debug \
	-Isrc/game \
	-Isrc/online
SDL_CFLAGS = $(shell sdl2-config --cflags)
LIBS = $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm -lz

SRC = \
	src/main_menu/main.c \
	src/main_menu/mainmenu_fonctions.c \
	src/main_menu/main_scene_helpers.c \
	src/options/options_scene.c \
	src/options/options_fonctions.c \
	src/options/options_settings_io.c \
	src/load/load_scene.c \
	src/choose/choose_scene.c \
	src/online/online_scene.c \
	src/online/online_client.c \
	src/top_scores/top_scores_scene.c \
	src/enigma/enigma_scene.c \
	src/debug/debug_log.c \
	src/game/final_cutscene.c \
	src/game/level_shared.c \
	src/game/game_progress.c \
	src/game/merged_levels.c \
	src/game/skin_registry.c \
	src/ui/ui_shared.c \
	lvls/shared/arcade_input.c \
	lvls/shared/session.c \
	lvls/level1-climb/main.c \
	lvls/level2-chase/main.c \
	lvls/level3-hell/main.c \
	lvls/level4-pool/main.c \
	lvls/newshoplvl1/game.c \
	lvls/newshoplvl1/arcade.c \
	lvls/newshoplvl1/player.c \
	lvls/newshoplvl1/map.c \
	lvls/newshoplvl1/billiards.c \
	lvls/newshoplvl1/barista.c \
	lvls/newshoplvl1/audio.c \
	lvls/newshoplvl1/input.c \
	lvls/newshoplvl1/admin.c

.PHONY: all clean run docs docs-clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

docs:
	@if ! command -v doxygen >/dev/null 2>&1; then \
		echo "doxygen is not installed. Install it, then run 'make docs' again."; \
		exit 1; \
	fi
	doxygen Doxyfile

docs-clean:
	rm -rf docs/doxygen
