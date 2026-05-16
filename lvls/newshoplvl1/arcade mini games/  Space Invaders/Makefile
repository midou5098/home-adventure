CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -pedantic
SDL_CFLAGS = $(shell pkg-config --cflags sdl2)
SDL_LIBS = $(shell pkg-config --libs sdl2)

TARGET = space_invaders
OBJ = main.o

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(SDL_LIBS)
	find . -name '*.o' -delete

main.o: main.c
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c main.c -o main.o

run: $(TARGET)
	./$(TARGET)

clean:
	find . -name '*.o' -delete
	rm -f $(TARGET)
