#include <iostream>
#include "libppm.h"
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

using namespace std;

#define SHM_NAME "/s1s2"
#define NUM_PIX 1
#define SEM_EMPTY "/sem_empty"
#define SEM_FULL "/sem_full"
#define SEM_MUTEX "/lock"

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
};

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		cout << "usage: ./a.out <path-to-original-image> <path-to-transformed-image>\n\n";
		exit(0);
	}

    //open shared memory file
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }

    //set size of the shared memory
    if (ftruncate(shm_fd, sizeof(struct Data)) == -1) {
        perror("ftruncate");
        return 1;
    }

    //map the shared memory
    struct Data *ptr = (struct Data*) mmap(NULL, sizeof(struct Data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED || ptr <= 0) {
        perror("mmap");
        return 1;
    }

    //open semaphores
    sem_t* sem_full = sem_open(SEM_FULL, O_CREAT, 0666, 0);
    sem_t* sem_empty = sem_open(SEM_EMPTY, O_CREAT, 0666, NUM_PIX);
    sem_t* sem_lock = sem_open(SEM_MUTEX, O_CREAT, 1);

	struct image_t *input_image = read_ppm_file(argv[1]);

    uint8_t pixel[3];
    uint32_t h;

    uint8_t result = 0;
	for(int i = 0; i < (input_image->height); i++)
	{
		for(int j = 0; j < (input_image->width); j++)
		{
			for(int k = 0; k < 3; k++)
			{
                if(i == 0 || i == input_image->height-1 || j == 0 || j == input_image->width-1){
					pixel[k] = input_image->image_pixels[i][j][k];
					continue;
				}

				for(int l = -1; l < 2; l++)
				{
					for(int m = -1; m < 2; m++)
					{
						result += (input_image->image_pixels[i-l][j-m][k])/9;
					}
				}
				pixel[k] = result;
			}
            h = hsh(pixel);

            sem_wait(sem_empty);  //wait for empty slot
            sem_wait(sem_lock);  //lock critical section
            memcpy(&ptr->h[ptr->writeIdx], &h, sizeof(uint32_t));
            memcpy(&ptr->pixel[ptr->writeIdx], pixel, sizeof(pixel));
            ptr->writeIdx = (ptr->writeIdx+1)%NUM_PIX;
            sem_post(sem_lock);  //unlock critical section
            sem_post(sem_full);  //indicate that data has been written

		}
	}

    sem_unlink(SEM_FULL);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_MUTEX);

    munmap(ptr, sizeof(struct Data));
    shm_unlink(SHM_NAME);
	
	return 0;
}