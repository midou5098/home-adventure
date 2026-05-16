game: main.o fonction.o
	gcc main.o fonction.o -o game `sdl2-config --cflags --libs` \
	-I/usr/include/SDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf -lm

main.o: main.c header.h
	gcc -c main.c -o main.o -g

fonction.o: fonction.c header.h
	gcc -c fonction.c -o fonction.o -g

clean:
	rm -f *.o game
