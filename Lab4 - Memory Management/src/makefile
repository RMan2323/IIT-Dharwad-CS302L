# ./a.exe 64 4 1 1 ../traces/combined_short.trace

all: simulator.out

simulator.out: simulator.cpp
	g++ -o simulator.out simulator.cpp

run: simulator.out
	./simulator.out 64 4 1 1 ./traces/combined_short.trace

clean:
	rm -f simulator.out
