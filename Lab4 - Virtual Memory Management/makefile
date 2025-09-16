run-simulator: virt_mem_manager.out
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
	./virt_mem_manager.out $$ps $$n $$rp $$ap ./$$f

virt_mem_manager.out:
	g++ virt_mem_manager.cpp -o virt_mem_manager.out
clean:
	rm *.out


