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
	if(argc != 2)
	{
		cout << "usage: ./a.out <path-to-original-image>\n\n";
		exit(0);
	}

    //remove the shm if it exists
    shm_unlink(SHM_NAME);

    //open shared memory file
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }
    // cout << "S1: Shm opened\n";

    //set size of the shared memory
    if (ftruncate(shm_fd, sizeof(struct Data)) == -1) {
        perror("ftruncate");
        return 1;
    }
    // cout << "S1: Shm truncated\n";

    //map the shared memory
    struct Data *ptr = (struct Data*) mmap(NULL, sizeof(struct Data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // cout << "S1: Shm mapped\n";

    //remove the semaphores if they exist
    sem_unlink(SEM_FULL);
    sem_unlink(SEM_MUTEX);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_TIME);

    //open semaphores
    sem_t* sem_full = sem_open(SEM_FULL, O_CREAT | O_EXCL, 0666, 0);
    sem_t* sem_empty = sem_open(SEM_EMPTY, O_CREAT | O_EXCL, 0666, NUM_PIX);
    sem_t* sem_lock = sem_open(SEM_MUTEX, O_CREAT | O_EXCL, 0666, 1);
    sem_t* sem_time = sem_open(SEM_TIME, O_CREAT | O_EXCL, 0666, 0);

    // cout << "S1: Sems opened\n";

	struct image_t *input_image = read_ppm_file(argv[1]);

    uint8_t pixel[3];
    uint32_t h;

    auto start = chrono::steady_clock::now();
    ptr->start_time_ns = chrono::duration_cast<chrono::nanoseconds>(start.time_since_epoch()).count();

    sem_post(sem_time);  //signal that time has been posted

    uint8_t result = 0;
    int count = 0;
    while(count < NUM_REPEATS){
    count++;
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
                result = 0;
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
            // cout << "here\n";
            sem_wait(sem_empty);  //wait for empty slot
            // cout << "here2\n";
            sem_wait(sem_lock);  //lock critical section
            // cout << "here3\n";
            memcpy(&ptr->h[ptr->writeIdx], &h, sizeof(uint32_t));
            // cout << "here4\n";
            memcpy(&ptr->pixel[ptr->writeIdx], pixel, sizeof(uint8_t)*3);
            // cout << "here5\n";
            ptr->writeIdx = (ptr->writeIdx+1)%NUM_PIX;
            // cout << "here6\n";
            sem_post(sem_lock);  //unlock critical section
            // cout << "here7\n";
            sem_post(sem_full);  //indicate that data has been written
            // cout << "here8\n";

            // cout << "S1: Sent pixel [" << i << ", " << j << "]: " << static_cast<int>(pixel[0]) << " " << static_cast<int>(pixel[1]) << " " << static_cast<int>(pixel[2]) << "\n";
		}
	}
    }

    sem_unlink(SEM_FULL);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_MUTEX);
    sem_unlink(SEM_TIME);

    munmap(ptr, sizeof(struct Data));
    shm_unlink(SHM_NAME);
	
	return 0;
}