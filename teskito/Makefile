CC = gcc
TARGET = teskito
CFLAGS = -Wall -Wextra -O2
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs) -lSDL2_image -lm

SRC = main.c fonctions.c

.PHONY: all run clean docs docs-clean

all: $(TARGET)

$(TARGET): $(SRC) headers.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $(SRC) -o $(TARGET) $(SDL_LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

docs:
	doxygen Doxyfile

docs-clean:
	rm -rf docs
