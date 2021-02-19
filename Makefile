all:
	g++ -Wall -std=c++14 -Ofast -g src/*.cpp -o render -lSDL2 -lSDL2_image
