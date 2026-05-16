CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
SDL_CFLAGS = $(shell pkg-config --cflags sdl2)
SDL_MIXER_CFLAGS = $(shell pkg-config --cflags SDL2_mixer)
SDL_LIBS = $(shell pkg-config --libs sdl2) $(shell pkg-config --libs SDL2_mixer) -lm

TARGET = tetris
OBJ = main.o

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(SDL_LIBS)
	find . -name '*.o' -delete

main.o: main.c
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $(SDL_MIXER_CFLAGS) -c main.c -o main.o

run: $(TARGET)
	./$(TARGET)

clean:
	find . -name '*.o' -delete
	rm -f $(TARGET)
