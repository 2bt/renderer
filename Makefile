all:
	g++ -Wall -std=c++14 -O3 -g src/*.cpp -o render -lSDL2 -lSDL2_image
