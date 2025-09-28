# ./a.exe 64 4 1 1 ../traces/combined_short.trace

run-simulator: simulator.out
	@read -p "Enter Page Size:" ps; \
# 
	read -p "Enter Number of Memory Frames:" n; \
# 
	echo "Enter Replacement Policy:"; \
	echo "1) Optimal\n2) FIFO\n3) LRU\n4) Random\n"; \
	read -p "Choice: " rp; \
# 
	echo "Enter Allocation Policy:";\
	echo "1) Global\n2) Local\n";\
	read -p "Choice: " ap; \
# 
	read -p "Enter path to Memory Trace File: " f; \
	echo; \
	./mem_manager.out $$ps $$n $$rp $$ap ./$$f

quick-run: simulator.out
	./simulator.out 64 4 1 1 ./traces/combined_short.trace

simulator.out:
	g++ simulator.cpp -o simulator.out

clean:
	rm -f simulator.out
