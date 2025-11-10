all: part1 part2_1 part2_2 part2_3

# --- Part 1 ---
part1: ./part1.out
	./part1.out ./1.ppm ./output_part1.ppm

./part1.out: ./part1/image_sharpener.cpp ./part1/libppm.cpp
	g++ ./part1/image_sharpener.cpp ./part1/libppm.cpp -o part1.out

# --- Part 2 (Pipes) ---
part2_1: ./part2_1.out
	./part2_1.out ./1.ppm ./output_part2_1.ppm

./part2_1.out: ./part2/usePipes/img_shrp_pipes.cpp ./part2/usePipes/libppm.cpp
	g++ ./part2/usePipes/img_shrp_pipes.cpp ./part2/usePipes/libppm.cpp -o part2_1.out

# --- Part 2 (Atomics/Threads) ---
part2_2: ./part2_2.out
	./part2_2.out ./1.ppm ./output_part2_2.ppm

./part2_2.out: ./part2/useAtomics/img_shrp_atomics.cpp ./part2/useAtomics/libppm.cpp
	# Added -pthread, which is required for img_shrp_atomics.cpp
	g++ ./part2/useAtomics/img_shrp_atomics.cpp ./part2/useAtomics/libppm.cpp -o part2_2.out -pthread

part2_3: ./part2_3_s1.out ./part2_3_s2.out ./part2_3_s3.out
	./part2_3_s1.out ./1.ppm &
	@sleep 0.1
	./part2_3_s2.out ./1.ppm &
	@sleep 0.1
	./part2_3_s3.out ./1.ppm ./output_part2_3.ppm

./part2_3_s1.out: ./part2/useSemaphores/s1.cpp ./part2/useSemaphores/libppm.cpp ./part2/useSemaphores/libppm.h ./part2/useSemaphores/config.h
	g++ ./part2/useSemaphores/s1.cpp ./part2/useSemaphores/libppm.cpp -o part2_3_s1.out

./part2_3_s2.out: ./part2/useSemaphores/s2.cpp ./part2/useSemaphores/libppm.cpp ./part2/useSemaphores/libppm.h ./part2/useSemaphores/config.h
	g++ ./part2/useSemaphores/s2.cpp ./part2/useSemaphores/libppm.cpp -o part2_3_s2.out

./part2_3_s3.out: ./part2/useSemaphores/s3.cpp ./part2/useSemaphores/libppm.cpp ./part2/useSemaphores/libppm.h ./part2/useSemaphores/config.h
	g++ ./part2/useSemaphores/s3.cpp ./part2/useSemaphores/libppm.cpp -o part2_3_s3.out

# --- Part 3 ---
# Scenario 1 (A: S1+S2, B: S3)
part3_1_A: ./part3_1_A.out
	@echo "--- Running Part 3 Server (S1+S2) ---"
	@echo "Run 'make part3_1_B' in another terminal to connect."
	./part3_1_A.out ./1.ppm 55555

./part3_1_A.out: ./part3/part3_1_A.cpp ./part3/libppm.cpp
	g++ ./part3/part3_1_A.cpp ./part3/libppm.cpp -o part3_1_A.out

part3_1_B: ./part3_1_B.out
	@echo "--- Running Part 3 Client (S3) ---"
	@echo "Connecting to localhost:55555..."
	./part3_1_B.out 127.0.0.1 55555 ./1.ppm ./output_part3_1.ppm
	@echo "Client finished. Output saved to output_part3_1.ppm"

./part3_1_B.out: ./part3/part3_1_B.cpp ./part3/libppm.cpp
	g++ ./part3/part3_1_B.cpp ./part3/libppm.cpp -o part3_1_B.out

# Scenario 2 (A: S1, B: S2+S3)
part3_2_A: ./part3_2_A.out
	@echo "--- Running Part 3 Server (S1) ---"
	@echo "Run 'make part3_2_B' in another terminal to connect."
	./part3_2_A.out ./1.ppm 55555

./part3_2_A.out: ./part3/part3_2_A.cpp ./part3/libppm.cpp
	g++ ./part3/part3_2_A.cpp ./part3/libppm.cpp -o part3_2_A.out

part3_2_B: ./part3_2_B.out
	@echo "--- Running Part 3 Client (S2+S3) ---"
	@echo "Connecting to localhost:55555..."
	./part3_2_B.out 127.0.0.1 55555 ./1.ppm ./output_part3_2.ppm
	@echo "Client finished. Output saved to output_part3_2.ppm"

./part3_2_B.out: ./part3/part3_2_B.cpp ./part3/libppm.cpp
	g++ ./part3/part3_2_B.cpp ./part3/libppm.cpp -o part3_2_B.out

# --- Other Rules ---
get-times: ./getTimesOUT.sh
	./getTimesOUT.sh

clean:
	rm -f part1.out part2_1.out part2_2.out part2_3.out
	rm -f part3_1_A.out part3_1_B.out part3_2_A.out part3_2_B.out
	rm -f output_part1.ppm output_part2_1.ppm output_part2_2.ppm output_part2_3.ppm
	rm -f output_part3_1.ppm output_part3_2.ppm

# Mark run targets as phony so make always runs them
.PHONY: part1 part2_1 part2_2 part2_3 part3_1_A part3_1_B part3_2_A part3_2_B get-times clean all
