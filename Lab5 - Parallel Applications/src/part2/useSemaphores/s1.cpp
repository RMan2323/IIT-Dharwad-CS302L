#include <iostream>
#include <cstring>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "libppm.h"
#include "config.h"

using namespace std;

uint32_t hsh(const uint8_t v[3]){
    uint32_t x = (v[0] << 16) | (v[1] << 8) | v[2];
    x ^= (x >> 13);
    x *= 0x85ebca6b;
    x ^= (x >> 16);
    return x;
}

uint32_t calculate_batch_hash(const uint8_t* buffer, int num_rows, int width) {
    uint32_t combined_hash = 0;
    size_t total_pixels = (size_t)num_rows * width;
    for (size_t i = 0; i < total_pixels; i++) {
        combined_hash ^= hsh(&buffer[i * 3]);
    }
    return combined_hash;
}

int main(int argc, char **argv)
{
    if(argc != 2) {
        cout << "usage: ./part2_3_s1.out <path-to-original-image>\n";
        exit(1);
    }

    // Clean up old SHM/Sems
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_FULL);
    sem_unlink(SEM_MUTEX);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_TIME);

    // Open shared memory
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) { perror("shm_open"); exit(1); }
    if (ftruncate(shm_fd, sizeof(struct SharedData)) == -1) { perror("ftruncate"); exit(1); }
    struct SharedData *ptr = (struct SharedData*) mmap(NULL, sizeof(struct SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) { perror("mmap"); exit(1); }

    // Open semaphores
    sem_t* sem_full = sem_open(SEM_FULL, O_CREAT, 0666, 0); // Start empty
    sem_t* sem_empty = sem_open(SEM_EMPTY, O_CREAT, 0666, BUFFER_SLOTS); // Start full
    sem_t* sem_lock = sem_open(SEM_MUTEX, O_CREAT, 0666, 1); // Mutex
    sem_t* sem_time = sem_open(SEM_TIME, O_CREAT, 0666, 0); // Timer sync

    struct image_t *input_image = read_ppm_file(argv[1]);
    if (input_image == nullptr) {
        cerr << "S1: Failed to read image " << argv[1] << endl;
        exit(1);
    }
    
    int height = input_image->height;
    int width = input_image->width;
    if (width > MAX_WIDTH) {
        cerr << "S1: Image width " << width << " exceeds MAX_WIDTH " << MAX_WIDTH << endl;
        exit(1);
    }

    // Temp buffer for processing
    size_t max_batch_bytes = (size_t)CHUNK_ROWS_N * width * 3;
    uint8_t* s1_flat_buffer = new uint8_t[max_batch_bytes];

    // Set start time
    auto start = chrono::steady_clock::now();
    ptr->start_time_ns = chrono::duration_cast<chrono::nanoseconds>(start.time_since_epoch()).count();
    ptr->writeIdx = 0;
    ptr->readIdx = 0;
    sem_post(sem_time); // Signal that time is set

    // Main processing loop
    for (int run = 0; run < NUM_RUNS; run++) {
        for (int i = 0; i < height; i += CHUNK_ROWS_N) {
            int num_rows_in_batch = min(CHUNK_ROWS_N, height - i);

            // S1 Logic
            for (int k = 0; k < num_rows_in_batch; k++) {
                int current_row = i + k;
                for (int j = 0; j < width; j++) {
                    uint8_t* s1_pixel = &s1_flat_buffer[(k * width + j) * 3];
                    if (current_row == 0 || current_row == height - 1 || j == 0 || j == width - 1) {
                        memset(s1_pixel, 0, 3);
                    } else {
                        for (int c = 0; c < 3; c++) {
                            uint8_t result = 0; // Serial logic
                            for (int l = -1; l < 2; l++) {
                                for (int m = -1; m < 2; m++) {
                                    result += input_image->image_pixels[current_row - l][j - m][c] / 9;
                                }
                            }
                            s1_pixel[c] = result;
                        }
                    }
                }
            }

            // --- Produce to Shared Memory ---
            sem_wait(sem_empty); // Wait for an empty slot
            sem_wait(sem_lock);  // Lock critical section

            // Get the slot
            RowBatchChunk* chunk = &ptr->buffer[ptr->writeIdx];
            chunk->run_id = run;
            chunk->start_row = i;
            chunk->num_rows = num_rows_in_batch;
            chunk->width = width;
            chunk->batch_hash = calculate_batch_hash(s1_flat_buffer, num_rows_in_batch, width);
            memcpy(chunk->batch_data, s1_flat_buffer, num_rows_in_batch * width * 3);

            ptr->writeIdx = (ptr->writeIdx + 1) % BUFFER_SLOTS;

            sem_post(sem_lock);  // Unlock critical section
            sem_post(sem_full);  // Signal a full slot
        }
    }

    cout << "S1: Finished processing." << endl;

    // Cleanup
    delete[] s1_flat_buffer;
    sem_close(sem_full);
    sem_close(sem_empty);
    sem_close(sem_lock);
    sem_close(sem_time);
    munmap(ptr, sizeof(struct SharedData));
    close(shm_fd);
    
    return 0;
}