all: build-sharpen get-times

part1: ./part1.out
	./part1.out ./1.ppm ./output_part1.ppm

./part1.out: ./part1/image_sharpener.cpp ./part1/libppm.cpp
	g++ ./part1/image_sharpener.cpp ./part1/libppm.cpp -o part1.out

part2_1: ./part2_1.out
	./part2_1.out ./1.ppm ./output_part2_1.ppm

./part2_1.out: ./part2/usePipes/image_sharpener.cpp ./part2/usePipes/libppm.cpp
	g++ ./part2/usePipes/image_sharpener.cpp ./part2/usePipes/libppm.cpp -o part2_1.out

part2_2: ./part2_2.out
	./part2_2.out ./1.ppm ./output_part2_2.ppm

./part2_2.out: ./part2/useAtomics/image_sharpener.cpp ./part2/useAtomics/libppm.cpp
	g++ ./part2/useAtomics/image_sharpener.cpp ./part2/useAtomics/libppm.cpp -o part2_2.out

part2_3: ./part2_3.out
	./part2_3.out ./1.ppm ./output_part2_3.ppm

./part2_3.out: ./part2/useSemaphores/image_sharpener.cpp ./part2/useSemaphores/libppm.cpp
	g++ ./part2/useSemaphores/image_sharpener.cpp ./part2/useSemaphores/libppm.cpp -o part2_3.out

#TODO: Add part3 rules

get-times: ./getTimesOUT.sh
	./getTimesOUT.sh

clean:
	rm a.out
