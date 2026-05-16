CC = gcc
CFLAGS = -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -DMERGED_BUILD
CPPFLAGS = -I../src/game -I../src/online
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs) -lSDL2_ttf -lSDL2_image -lSDL2_mixer -lm -lz

TARGET = merged
OBJ = \
	launcher_main.o \
	game_progress_shared.o \
	online_client_shared.o \
	arcade_input_shared.o \
	session_shared.o \
	option_scene_menu.o \
	option_fonctions_menu.o \
	option_settings_io_menu.o \
	option_ui_shared_menu.o \
	level1_main.o \
	level2_main.o \
	level3_main.o \
	level4_main.o \
	newshop_game.o \
	newshop_arcade.o \
	newshop_player.o \
	newshop_map.o \
	newshop_billiards.o \
	newshop_barista.o \
	newshop_audio.o \
	newshop_input.o \
	newshop_admin.o

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(SDL_LIBS)
	find . -name '*.o' -delete

launcher_main.o: launcher/main.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c launcher/main.c -o launcher_main.o

game_progress_shared.o: ../src/game/game_progress.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c ../src/game/game_progress.c -o game_progress_shared.o

online_client_shared.o: ../src/online/online_client.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c ../src/online/online_client.c -o online_client_shared.o

arcade_input_shared.o: shared/arcade_input.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c shared/arcade_input.c -o arcade_input_shared.o

session_shared.o: shared/session.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c shared/session.c -o session_shared.o

option_scene_menu.o: menu/option\ in\ game/src/options_scene.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c "menu/option in game/src/options_scene.c" -o option_scene_menu.o

option_fonctions_menu.o: menu/option\ in\ game/src/options_fonctions.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c "menu/option in game/src/options_fonctions.c" -o option_fonctions_menu.o

option_settings_io_menu.o: menu/option\ in\ game/src/options_settings_io.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c "menu/option in game/src/options_settings_io.c" -o option_settings_io_menu.o

option_ui_shared_menu.o: menu/option\ in\ game/src/ui_shared.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c "menu/option in game/src/ui_shared.c" -o option_ui_shared_menu.o

level1_main.o: level1-climb/main.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c level1-climb/main.c -o level1_main.o

level2_main.o: level2-chase/main.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c level2-chase/main.c -o level2_main.o

level3_main.o: level3-hell/main.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c level3-hell/main.c -o level3_main.o

level4_main.o: level4-pool/main.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c level4-pool/main.c -o level4_main.o

newshop_game.o: newshoplvl1/game.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c newshoplvl1/game.c -o newshop_game.o

newshop_arcade.o: newshoplvl1/arcade.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c newshoplvl1/arcade.c -o newshop_arcade.o

newshop_player.o: newshoplvl1/player.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c newshoplvl1/player.c -o newshop_player.o

newshop_map.o: newshoplvl1/map.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c newshoplvl1/map.c -o newshop_map.o

newshop_billiards.o: newshoplvl1/billiards.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c newshoplvl1/billiards.c -o newshop_billiards.o

newshop_barista.o: newshoplvl1/barista.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c newshoplvl1/barista.c -o newshop_barista.o

newshop_audio.o: newshoplvl1/audio.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c newshoplvl1/audio.c -o newshop_audio.o

newshop_input.o: newshoplvl1/input.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c newshoplvl1/input.c -o newshop_input.o

newshop_admin.o: newshoplvl1/admin.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c newshoplvl1/admin.c -o newshop_admin.o

run: $(TARGET)
	./$(TARGET)

clean:
	find . -name '*.o' -delete
	rm -f $(TARGET)
