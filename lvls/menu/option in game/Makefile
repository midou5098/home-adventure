CC = gcc
CFLAGS = -Wall -Wextra -O2
CPPFLAGS = -Isrc
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm

TARGET = option_menu
OBJ = main.o options_scene.o options_fonctions.o options_settings_io.o ui_shared.o

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(SDL_LIBS)
	find . -name '*.o' -delete

main.o: src/main.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c src/main.c -o main.o

options_scene.o: src/options_scene.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c src/options_scene.c -o options_scene.o

options_fonctions.o: src/options_fonctions.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c src/options_fonctions.c -o options_fonctions.o

options_settings_io.o: src/options_settings_io.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c src/options_settings_io.c -o options_settings_io.o

ui_shared.o: src/ui_shared.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -c src/ui_shared.c -o ui_shared.o

run: $(TARGET)
	./$(TARGET)

clean:
	find . -name '*.o' -delete
	rm -f $(TARGET)
