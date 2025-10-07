run-simulator: simulator.out
	@read -p "Enter Page Size: " ps; \
	read -p "Enter Number of Memory Frames: " n; \
	echo "Enter Replacement Policy:"; \
	echo "1) Optimal\n2) FIFO\n3) LRU\n4) Random\n"; \
	read -p "Choice: " rp; \
	echo "Enter Allocation Policy:";\
	echo "1) Global\n2) Local\n";\
	read -p "Choice: " ap; \
	read -p "Enter path to Memory Trace File: " f; \
	echo; \
	./simulator.out $$ps $$n $$rp $$ap ./$$f

quick-run: simulator.out
	./simulator.out 64 4 1 1 ../traces/combined_short.trace

simulator.out: simulator.cpp util.h fifo_local.cpp fifo_global.cpp lru_local.cpp lru_global.cpp random_global.cpp random_local.cpp
	g++ simulator.cpp fifo_local.cpp fifo_global.cpp lru_local.cpp lru_global.cpp random_global.cpp random_local.cpp -o simulator.out

clean:
	rm -f *.out
