
all: ms

ms: main.cpp
	g++ -std=c++17 main.cpp -o music_spec -g0 -O3 -Wall -Wextra -pedantic

run:
	./music_spec

