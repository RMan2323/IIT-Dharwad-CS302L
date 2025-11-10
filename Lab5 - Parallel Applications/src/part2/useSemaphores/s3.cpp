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

// Hashing function
uint32_t hsh(const uint8_t v[3]){
    uint32_t x = (v[0] << 16) | (v[1] << 8) | v[2];
    x ^= (x >> 13);
    x *= 0x85ebca6b;
    x ^= (x >> 16);
    return x;
}

// Calculates hash for a flat buffer of pixel data
uint32_t calculate_batch_hash(const uint8_t* buffer, int num_rows, int width) {
    uint32_t combined_hash = 0;
    size_t total_pixels = (size_t)num_rows * width;
    for (size_t i = 0; i < total_pixels; i++) {
        combined_hash ^= hsh(&buffer[i * 3]);
    }
    return combined_hash;
}

// Memory management for image_t
void delete_image(image_t* img) {
    if (img == nullptr) return;
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            delete[] img->image_pixels[i][j];
        }
        delete[] img->image_pixels[i];
    }
    delete[] img->image_pixels;
    delete img;
}

image_t* create_empty_image(int height, int width) {
    image_t* img = new image_t;
    img->height = height;
    img->width = width;
    img->image_pixels = new uint8_t**[height];
    for (int i = 0; i < height; i++) {
        img->image_pixels[i] = new uint8_t*[width];
        for (int j = 0; j < width; j++) {
            img->image_pixels[i][j] = new uint8_t[3];
            memset(img->image_pixels[i][j], 0, 3); // Init to black
        }
    }
    return img;
}

int main(int argc, char **argv)
{
    if(argc != 3) {
        cout << "usage: ./part2_3_s3.out <path-to-original-image> <path-to-transformed-image>\n";
        exit(1);
    }

    // --- Input SHM/Sem (From S2) ---
    int shm_in_fd = shm_open(SHM_NAME_2, O_RDWR, 0666);
    if (shm_in_fd == -1) { perror("shm_open (in)"); exit(1); }
    struct SharedData *ptr_in = (struct SharedData*) mmap(NULL, sizeof(struct SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_in_fd, 0);
    if (ptr_in == MAP_FAILED) { perror("mmap (in)"); exit(1); }

    sem_t* sem_full_in = sem_open(SEM_FULL_2, 0);
    sem_t* sem_empty_in = sem_open(SEM_EMPTY_2, 0);
    sem_t* sem_lock_in = sem_open(SEM_MUTEX_2, 0);
    sem_t* sem_time_in = sem_open(SEM_TIME_2, 0);

    // --- Read Timer ---
    sem_wait(sem_time_in);
    auto start_time = chrono::steady_clock::time_point(chrono::nanoseconds(ptr_in->start_time_ns));
    sem_post(sem_time_in);

    // --- Read Image & Prep Output ---
    auto start_read = chrono::steady_clock::now();
    struct image_t *input_image = read_ppm_file(argv[1]);
    auto stop_read = chrono::steady_clock::now();
    chrono::duration<double> read_time = stop_read-start_read;
    cout << "Image read time: " << fixed << setprecision(6) << read_time.count() << "s" << endl;

    if (input_image == nullptr) {
        cerr << "S3: Failed to read image " << argv[1] << endl;
        exit(1);
    }
    int height = input_image->height;
    int width = input_image->width;

    struct image_t* final_image = create_empty_image(height, width);

    // --- Local Buffer ---
    size_t max_batch_bytes = (size_t)CHUNK_ROWS_N * width * 3;
    uint8_t* s2_flat_buffer = new uint8_t[max_batch_bytes];

    long long total_rows_to_process = (long long)NUM_RUNS * height;
    long long rows_processed = 0;
    long long s3_expected_run = 0;
    long long s3_expected_row = 0;

    cout << "S3: Started." << endl;

    // --- Main Loop ---
    while (rows_processed < total_rows_to_process) {
        // --- Consume from S2 ---
        sem_wait(sem_full_in);
        sem_wait(sem_lock_in);

        RowBatchChunk* in_chunk = &ptr_in->buffer[ptr_in->readIdx];
        int run_id = in_chunk->run_id;
        int start_row = in_chunk->start_row;
        int num_rows = in_chunk->num_rows;
        uint32_t batch_hash = in_chunk->batch_hash;
        size_t data_size = (size_t)num_rows * width * 3;
        memcpy(s2_flat_buffer, in_chunk->batch_data, data_size);
        
        ptr_in->readIdx = (ptr_in->readIdx + 1) % BUFFER_SLOTS;

        sem_post(sem_lock_in);
        sem_post(sem_empty_in);

        // --- Verification (S2 -> S3) ---
        if (run_id != s3_expected_run || start_row != s3_expected_row) {
            cerr << "[FATAL] S2->S3 ORDER MISMATCH!" << endl; exit(EXIT_FAILURE);
        }
        s3_expected_row += num_rows;
        if (s3_expected_row >= height) { s3_expected_row = 0; s3_expected_run++; }
        
        uint32_t received_hash = calculate_batch_hash(s2_flat_buffer, num_rows, width);
        if (received_hash != batch_hash) {
            cerr << "[FATAL] S2->S3 HASH MISMATCH!" << endl; exit(EXIT_FAILURE);
        }

        // --- S3 Logic ---
        if (run_id == NUM_RUNS - 1) { // Only write data for the final run
            for (int k = 0; k < num_rows; k++) {
                int current_row = start_row + k;
                if (current_row > 0 && current_row < height - 1) {
                    for (int j = 1; j < width - 1; j++) {
                        uint8_t* s2_pixel = &s2_flat_buffer[(k * width + j) * 3];
                        for (int c = 0; c < 3; c++) {
                            short res = (short)input_image->image_pixels[current_row][j][c] + (short)s2_pixel[c];
                            final_image->image_pixels[current_row][j][c] = (res > 255) ? 255 : (uint8_t)res;
                        }
                    }
                }
            }
        }
        rows_processed += num_rows;
    }

    auto stop_total = chrono::steady_clock::now();
    chrono::duration<double> total_time = stop_total - start_time;

    cout << "S3: Finished processing. Verification complete." << endl;

    // --- Write File & Report ---
    auto start_write = chrono::steady_clock::now();
    write_ppm_file(argv[2], final_image);
    auto stop_write = chrono::steady_clock::now();
    chrono::duration<double> write_time = stop_write-start_write;

    cout << "\n--- Timing Report (Semaphores) ---" << endl;
    cout << "Read time:     " << setw(12) << read_time.count() << " s" << endl;
    // --- FIX: Corrected the typo ---
    cout << "Parallel time: " << setw(12) << total_time.count() << " s (for " << NUM_RUNS << " runs)" << endl;
    cout << "Write time:    " << setw(12) << write_time.count() << " s" << endl;
    cout << "Avg per run:   " << setw(12) << total_time.count() / NUM_RUNS * 1000 << " ms" << endl;

    // Cleanup
    delete_image(input_image);
    delete_image(final_image);
    delete[] s2_flat_buffer;

    sem_close(sem_full_in);
    sem_close(sem_empty_in);
    sem_close(sem_lock_in);
    sem_close(sem_time_in);

    sem_unlink(SEM_FULL_2);
    sem_unlink(SEM_EMPTY_2);
    sem_unlink(SEM_MUTEX_2);
    sem_unlink(SEM_TIME_2);

    munmap(ptr_in, sizeof(struct SharedData));
    close(shm_in_fd);
    shm_unlink(SHM_NAME_2);

    return 0;
}