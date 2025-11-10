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

    //open shared memory file of incoming pixels
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
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
    sem_t* sem_full = sem_open(SEM_FULL, 0);
    sem_t* sem_empty = sem_open(SEM_EMPTY, 0);
    sem_t* sem_lock = sem_open(SEM_MUTEX, 0);

    //remove the shm if it exists
    shm_unlink(SHM_NAME_2);

    //open shared memory file for sending pixels
    int shm2_fd = shm_open(SHM_NAME_2, O_CREAT | O_RDWR, 0666);
    if (shm2_fd == -1) {
        perror("shm2_open");
        return 1;
    }

    //set size of the shared memory for sending pixels
    if (ftruncate(shm2_fd, sizeof(struct Data)) == -1) {
        perror("ftruncate");
        return 1;
    }

    //map the shared memory for sending pixels
    struct Data *ptr2 = (struct Data*) mmap(NULL, sizeof(struct Data), PROT_READ | PROT_WRITE, MAP_SHARED, shm2_fd, 0);
    if (ptr2 == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    //remove the semaphores if they exist
    sem_unlink(SEM_FULL_2);
    sem_unlink(SEM_MUTEX_2);
    sem_unlink(SEM_EMPTY_2);
    sem_unlink(SEM_TIME_2);

    //open semaphores for sending pixels
    sem_t* sem2_full = sem_open(SEM_FULL_2, O_CREAT, 0666, 0);
    sem_t* sem2_empty = sem_open(SEM_EMPTY_2, O_CREAT, 0666, NUM_PIX);
    sem_t* sem2_lock = sem_open(SEM_MUTEX_2, O_CREAT, 0666, 1);
    sem_t* sem2_time = sem_open(SEM_TIME_2, O_CREAT, 0666, 0);

    sem_t* sem_time = sem_open(SEM_TIME, 0);
    if (sem_time == SEM_FAILED) {
        perror("sem_open(SEM_TIME) in s2");
        return 1;
    }

    // wait for s1 to post the time
    if (sem_wait(sem_time) != 0) {
        perror("sem_wait(SEM_TIME) in s2");
        sem_close(sem_time);
        return 1;
    }

    //copy timestamp from incoming shared memory to outgoing shared memory
    ptr2->start_time_ns = ptr->start_time_ns;

    //allow the s3 to proceed
    if (sem_post(sem2_time) != 0) {
        perror("sem_post(SEM_TIME) in s2");
    }
    sem_close(sem_time);

	struct image_t *input_image = read_ppm_file(argv[1]);

    uint8_t incomingPixel[3], outgoingPixel[3];
    uint32_t incomingH, outgoingH;

    uint8_t result = 0;

    int count = 0;
    while(count < NUM_REPEATS){
    count++;
	for(int i = 0; i < (input_image->height); i++)
	{
		for(int j = 0; j < (input_image->width); j++)
		{
            sem_wait(sem_full);  //wait a write
            sem_wait(sem_lock);  //lock critical section
            memcpy(&incomingH, &ptr->h[ptr->readIdx], sizeof(uint32_t));
            memcpy(incomingPixel, &ptr->pixel[ptr->readIdx], sizeof(incomingPixel));
            ptr->readIdx = (ptr->readIdx+1)%NUM_PIX;
            sem_post(sem_lock);  //unlock critical section
            sem_post(sem_empty);  //indicate that data has been read

            if(hsh(incomingPixel) != incomingH){
				perror("s1s2 hash mismatch");
				//cout << hsh(incomingPixel) << " " << incomingH << "\n";
				exit(EXIT_FAILURE);
			}

            // cout << "S2: Received pixel [" << i << ", " << j << "]: " << static_cast<int>(incomingPixel[0]) << " " << static_cast<int>(incomingPixel[1]) << " " << static_cast<int>(incomingPixel[2]) << "\n";

			for(int k = 0; k < 3; k++)
			{
                if(i == 0 || i == input_image->height-1 || j == 0 || j == input_image->width-1){
					outgoingPixel[k] = 0;
					continue;
				}

				short res = input_image->image_pixels[i][j][k] - incomingPixel[k];
				if(res < 0) outgoingPixel[k] = 0;
				else outgoingPixel[k] = res;
			}
            outgoingH = hsh(outgoingPixel);

            sem_wait(sem2_empty);  //wait for empty slot
            sem_wait(sem2_lock);  //lock critical section
            memcpy(&ptr2->h[ptr2->writeIdx], &outgoingH, sizeof(uint32_t));
            memcpy(&ptr2->pixel[ptr2->writeIdx], outgoingPixel, sizeof(outgoingPixel));
            ptr2->writeIdx = (ptr2->writeIdx+1)%NUM_PIX;
            sem_post(sem2_lock);  //unlock critical section
            sem_post(sem2_full);  //indicate that data has been written

            // cout << "S2: Sent pixel [" << i << ", " << j <<"]: " << static_cast<int>(outgoingPixel[0]) << " " << static_cast<int>(outgoingPixel[1]) << " " << static_cast<int>(outgoingPixel[2]) << "\n";
		}
	}
    }

    sem_unlink(SEM_FULL);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_MUTEX);

    munmap(ptr, sizeof(struct Data));
    shm_unlink(SHM_NAME);

    sem_unlink(SEM_FULL_2);
    sem_unlink(SEM_EMPTY_2);
    sem_unlink(SEM_MUTEX_2);

    munmap(ptr2, sizeof(struct Data));
    shm_unlink(SHM_NAME_2);
	
	return 0;
}