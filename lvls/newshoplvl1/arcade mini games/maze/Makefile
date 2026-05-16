NAME = prot
CC = gcc
CFLAGS = -O2 -Wall -Wextra -Wpedantic -std=c11
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm

OBJ = main.o

.PHONY: all clean fclean re run

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(OBJ) -o $(NAME) $(SDL_LIBS)
	find . -name '*.o' -delete

main.o: main.c headers.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c main.c -o main.o

clean:
	find . -name '*.o' -delete

fclean: clean
	rm -f $(NAME)

re: fclean all

run: $(NAME)
	./$(NAME)
