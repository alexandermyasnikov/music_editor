
all: ms

ms: main.cpp
	g++ -std=c++2a main.cpp -o music_spec -g0 -O3 -Wall -Wextra -pedantic -lstdc++fs

run:
	./music_spec

