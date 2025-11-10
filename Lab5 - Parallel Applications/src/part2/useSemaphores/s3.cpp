#include <iostream>
#include "libppm.h"
#include "config.h"
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

using namespace std;

uint32_t hsh(const uint8_t v[3]){
	uint32_t x = (v[0] << 16) | (v[1] << 8) | v[2];
	x ^= (x >> 13);
	x *= 0x85ebca6b;
	x ^= (x >> 16);
	
	return x;
}

struct Data{
    uint32_t h[NUM_PIX];
    uint8_t pixel[NUM_PIX][3];
    int writeIdx = 0, readIdx = 0;
	int64_t start_time_ns;
};

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		cout << "usage: ./a.out <path-to-original-image> <path-to-transformed-image>\n\n";
		exit(0);
	}

	//open shared memory file of incoming pixels
    int shm_fd = shm_open(SHM_NAME_2, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }

	//map the shared memory of incoming pixels
    struct Data *ptr = (struct Data*) mmap(NULL, sizeof(struct Data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

	//open semaphores of incoming pixels
    sem_t* sem_full = sem_open(SEM_FULL_2, 0);
    sem_t* sem_empty = sem_open(SEM_EMPTY_2, 0);
    sem_t* sem_lock = sem_open(SEM_MUTEX_2, 0);
	sem_t* sem_time = sem_open(SEM_TIME_2, 0);

	sem_wait(sem_time);
	auto start_time = chrono::steady_clock::time_point(chrono::nanoseconds(ptr->start_time_ns));
	sem_post(sem_time);

	sem_unlink(SEM_TIME_2);

	struct image_t *input_image = read_ppm_file(argv[1]);

	struct image_t* finalImage = new struct image_t;
	
	finalImage->height = input_image->height;
	finalImage->width = input_image->width;

	finalImage->image_pixels = new uint8_t**[finalImage->height];
	for(int i = 0; i < finalImage->height; i++)
	{
		finalImage->image_pixels[i] = new uint8_t*[finalImage->width];

		for(int j = 0; j < finalImage->width; j++)
			finalImage->image_pixels[i][j] = new uint8_t[3];
	}

	uint8_t incomingPixel[3];
    uint32_t incomingH;

	int count = 0;
    while(count < NUM_REPEATS){
	count++;
	for(int i = 0; i < (finalImage->height); i++)
	{
		for(int j = 0; j < (finalImage->width); j++)
		{
			sem_wait(sem_full);  //wait a write
            sem_wait(sem_lock);  //lock critical section
            memcpy(&incomingH, &ptr->h[ptr->readIdx], sizeof(uint32_t));
            memcpy(incomingPixel, &ptr->pixel[ptr->readIdx], sizeof(incomingPixel));
            ptr->readIdx = (ptr->readIdx+1)%NUM_PIX;
            sem_post(sem_lock);  //unlock critical section
            sem_post(sem_empty);  //indicate that data has been read

			if(hsh(incomingPixel) != incomingH){
				perror("s2s3 hash mismatch");
				//cout << hsh(incomingPixel) << " " << incomingH << "\n";
				exit(EXIT_FAILURE);
			}

			// cout << "S3: Received pixel [" << i << ", " << j << "]: " << static_cast<int>(incomingPixel[0]) << " " << static_cast<int>(incomingPixel[1]) << " " << static_cast<int>(incomingPixel[2]) << "\n";

			for(int k = 0; k < 3; k++)
			{
				if(i == 0 || i == input_image->height-1 || j == 0 || j == input_image->width-1){
					finalImage->image_pixels[i][j][k] = 0;
					continue;
				}

				short res = input_image->image_pixels[i][j][k] + incomingPixel[k];
				if(res > 255) finalImage->image_pixels[i][j][k] = 255;
				else finalImage->image_pixels[i][j][k] = res;
			}
		}
	}
	// cout << "Repeat " << count << " done\n";
	}

	auto end = chrono::steady_clock::now();
	auto timeTotal = end - start_time;
	int timeSec = std::chrono::duration_cast<std::chrono::seconds>(timeTotal).count();

	cout << "Time taken for " << NUM_REPEATS << " iterations is " << timeSec << "\n";

	sem_unlink(SEM_FULL_2);
    sem_unlink(SEM_EMPTY_2);
    sem_unlink(SEM_MUTEX_2);

    munmap(ptr, sizeof(struct Data));
    shm_unlink(SHM_NAME_2);

	write_ppm_file(argv[2], finalImage);
		
	return 0;
}