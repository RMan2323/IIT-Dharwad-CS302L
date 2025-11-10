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
        cout << "usage: ./part2_3_s2.out <path-to-original-image>\n";
        exit(1);
    }

    // --- Input SHM/Sem (From S1) ---
    int shm_in_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_in_fd == -1) { perror("shm_open (in)"); exit(1); }
    struct SharedData *ptr_in = (struct SharedData*) mmap(NULL, sizeof(struct SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_in_fd, 0);
    if (ptr_in == MAP_FAILED) { perror("mmap (in)"); exit(1); }

    sem_t* sem_full_in = sem_open(SEM_FULL, 0);
    sem_t* sem_empty_in = sem_open(SEM_EMPTY, 0);
    sem_t* sem_lock_in = sem_open(SEM_MUTEX, 0);
    sem_t* sem_time_in = sem_open(SEM_TIME, 0);

    // --- Output SHM/Sem (To S3) ---
    shm_unlink(SHM_NAME_2);
    sem_unlink(SEM_FULL_2);
    sem_unlink(SEM_MUTEX_2);
    sem_unlink(SEM_EMPTY_2);
    sem_unlink(SEM_TIME_2);

    int shm_out_fd = shm_open(SHM_NAME_2, O_CREAT | O_RDWR, 0666);
    if (shm_out_fd == -1) { perror("shm_open (out)"); exit(1); }
    if (ftruncate(shm_out_fd, sizeof(struct SharedData)) == -1) { perror("ftruncate (out)"); exit(1); }
    struct SharedData *ptr_out = (struct SharedData*) mmap(NULL, sizeof(struct SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_out_fd, 0);
    if (ptr_out == MAP_FAILED) { perror("mmap (out)"); exit(1); }

    sem_t* sem_full_out = sem_open(SEM_FULL_2, O_CREAT, 0666, 0);
    sem_t* sem_empty_out = sem_open(SEM_EMPTY_2, O_CREAT, 0666, BUFFER_SLOTS);
    sem_t* sem_lock_out = sem_open(SEM_MUTEX_2, O_CREAT, 0666, 1);
    sem_t* sem_time_out = sem_open(SEM_TIME_2, O_CREAT, 0666, 0);

    // --- Pass Timer ---
    sem_wait(sem_time_in);
    ptr_out->start_time_ns = ptr_in->start_time_ns;
    sem_post(sem_time_in); // Post again in case S3 is slow
    sem_post(sem_time_out);
    
    ptr_out->writeIdx = 0;
    ptr_out->readIdx = 0;

    // --- Read Image ---
    struct image_t *input_image = read_ppm_file(argv[1]);
    if (input_image == nullptr) {
        cerr << "S2: Failed to read image " << argv[1] << endl;
        exit(1);
    }
    int height = input_image->height;
    int width = input_image->width;

    // --- Local Buffers ---
    size_t max_batch_bytes = (size_t)CHUNK_ROWS_N * width * 3;
    uint8_t* s1_flat_buffer = new uint8_t[max_batch_bytes];
    uint8_t* s2_flat_buffer = new uint8_t[max_batch_bytes];

    long long total_rows_to_process = (long long)NUM_RUNS * height;
    long long rows_processed = 0;
    long long s2_expected_run = 0;
    long long s2_expected_row = 0;

    cout << "S2: Started." << endl;

    // --- Main Loop ---
    while (rows_processed < total_rows_to_process) {
        // --- Consume from S1 ---
        sem_wait(sem_full_in);
        sem_wait(sem_lock_in);

        // Copy chunk from SHM
        RowBatchChunk* in_chunk = &ptr_in->buffer[ptr_in->readIdx];
        int run_id = in_chunk->run_id;
        int start_row = in_chunk->start_row;
        int num_rows = in_chunk->num_rows;
        uint32_t batch_hash = in_chunk->batch_hash;
        size_t data_size = (size_t)num_rows * width * 3;
        memcpy(s1_flat_buffer, in_chunk->batch_data, data_size);
        
        ptr_in->readIdx = (ptr_in->readIdx + 1) % BUFFER_SLOTS;

        sem_post(sem_lock_in);
        sem_post(sem_empty_in);

        // --- Verification (S1 -> S2) ---
        if (run_id != s2_expected_run || start_row != s2_expected_row) {
            cerr << "[FATAL] S1->S2 ORDER MISMATCH!" << endl; exit(EXIT_FAILURE);
        }
        s2_expected_row += num_rows;
        if (s2_expected_row >= height) { s2_expected_row = 0; s2_expected_run++; }
        
        uint32_t received_hash = calculate_batch_hash(s1_flat_buffer, num_rows, width);
        if (received_hash != batch_hash) {
            cerr << "[FATAL] S1->S2 HASH MISMATCH!" << endl; exit(EXIT_FAILURE);
        }

        // --- S2 Logic ---
        for (int k = 0; k < num_rows; k++) {
            int current_row = start_row + k;
            for (int j = 0; j < width; j++) {
                uint8_t* s1_pixel = &s1_flat_buffer[(k * width + j) * 3];
                uint8_t* s2_pixel = &s2_flat_buffer[(k * width + j) * 3];

                if (current_row == 0 || current_row == height - 1 || j == 0 || j == width - 1) {
                    memset(s2_pixel, 0, 3);
                } else {
                    for (int c = 0; c < 3; c++) {
                        short res = (short)input_image->image_pixels[current_row][j][c] - (short)s1_pixel[c];
                        s2_pixel[c] = (res < 0) ? 0 : (uint8_t)res;
                    }
                }
            }
        }

        // --- Produce to S3 ---
        sem_wait(sem_empty_out);
        sem_wait(sem_lock_out);

        RowBatchChunk* out_chunk = &ptr_out->buffer[ptr_out->writeIdx];
        out_chunk->run_id = run_id;
        out_chunk->start_row = start_row;
        out_chunk->num_rows = num_rows;
        out_chunk->width = width;
        out_chunk->batch_hash = calculate_batch_hash(s2_flat_buffer, num_rows, width);
        memcpy(out_chunk->batch_data, s2_flat_buffer, data_size);

        ptr_out->writeIdx = (ptr_out->writeIdx + 1) % BUFFER_SLOTS;

        sem_post(sem_lock_out);
        sem_post(sem_full_out);

        rows_processed += num_rows;
    }

    cout << "S2: Finished processing." << endl;

    // Cleanup
    delete[] s1_flat_buffer;
    delete[] s2_flat_buffer;
    sem_close(sem_full_in);
    sem_close(sem_empty_in);
    sem_close(sem_lock_in);
    sem_close(sem_time_in);
    sem_close(sem_full_out);
    sem_close(sem_empty_out);
    sem_close(sem_lock_out);
    sem_close(sem_time_out);
    munmap(ptr_in, sizeof(struct SharedData));
    munmap(ptr_out, sizeof(struct SharedData));
    close(shm_in_fd);
    close(shm_out_fd);
    
    return 0;
}