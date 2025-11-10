#include <iostream>
#include "libppm.h"
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <thread>
#include <queue>
#include <string>
#include <pthread.h>
#include <cstring>
#include <cmath>
#include <stdlib.h>
using namespace std;

// --- Hash Function ---
uint32_t hsh(const uint8_t v[3]) {
    uint32_t x = (v[0] << 16) | (v[1] << 8) | v[2];
    x ^= (x >> 13);
    x *= 0x85ebca6b;
    x ^= (x >> 16);
    return x;
}

// --- Batch Structure ---
struct RowBatchChunk {
    int run_id;
    int start_row;
    int num_rows;
    int width;
    uint8_t*** batch_data;
    uint32_t batch_hash;
};

// --- Shared Resources ---
queue<RowBatchChunk*> buff_1;
queue<RowBatchChunk*> buff_2;
std::atomic_flag lock_stream_1 = ATOMIC_FLAG_INIT;
std::atomic_flag lock_stream_2 = ATOMIC_FLAG_INIT;
const int CHUNK_ROWS_N = 10;
const int BUFFER_SIZE = 20;
const int NUM_RUNS = 1000;
image_t* g_input_image = nullptr;
std::atomic<bool> s1_done(false);
std::atomic<bool> s2_done(false);

// --- Helpers ---
uint32_t calculate_batch_hash(RowBatchChunk* chunk) {
    uint32_t combined_hash = 0;
    for (int i = 0; i < chunk->num_rows; i++)
        for (int j = 0; j < chunk->width; j++)
            combined_hash ^= hsh(chunk->batch_data[i][j]);
    return combined_hash;
}

void delete_image(image_t* img) {
    if (!img) return;
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++)
            delete[] img->image_pixels[i][j];
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
        for (int j = 0; j < width; j++)
            img->image_pixels[i][j] = new uint8_t[3];
    }
    return img;
}

RowBatchChunk* allocate_batch_chunk(int run_id, int start_row, int num_rows, int width) {
    RowBatchChunk* chunk = new RowBatchChunk;
    chunk->run_id = run_id;
    chunk->start_row = start_row;
    chunk->num_rows = num_rows;
    chunk->width = width;
    chunk->batch_data = new uint8_t**[num_rows];
    for (int i = 0; i < num_rows; i++) {
        chunk->batch_data[i] = new uint8_t*[width];
        for (int j = 0; j < width; j++)
            chunk->batch_data[i][j] = new uint8_t[3];
    }
    return chunk;
}

void delete_batch_chunk(RowBatchChunk* chunk) {
    if (!chunk) return;
    for (int i = 0; i < chunk->num_rows; i++) {
        for (int j = 0; j < chunk->width; j++)
            delete[] chunk->batch_data[i][j];
        delete[] chunk->batch_data[i];
    }
    delete[] chunk->batch_data;
    delete chunk;
}

// --- Thread Args ---
struct S3_args {
    string output_filename;
    image_t* final_image;
};

// --- Thread S1 ---
void* thread_S1(void* arg) {
    cout << "Thread S1 started." << endl;
    int height = g_input_image->height, width = g_input_image->width;

    for (int run = 0; run < NUM_RUNS; run++) {
        for (int i = 0; i < height; i += CHUNK_ROWS_N) {
            int current_num_rows = min(CHUNK_ROWS_N, height - i);
            RowBatchChunk* chunk = allocate_batch_chunk(run, i, current_num_rows, width);

            for (int k = 0; k < current_num_rows; k++) {
                int r = i + k;
                uint8_t** row = chunk->batch_data[k];
                if (r == 0 || r == height - 1) {
                    for (int j = 0; j < width; j++) memset(row[j], 0, 3);
                } else {
                    for (int j = 0; j < width; j++) {
                        if (j == 0 || j == width - 1) memset(row[j], 0, 3);
                        else {
                            for (int c = 0; c < 3; c++) {
                                uint16_t sum = 0;
                                for (int l = -1; l < 2; l++)
                                    for (int m = -1; m < 2; m++)
                                        sum += g_input_image->image_pixels[r - l][j - m][c] / 9;
                                row[j][c] = sum;
                            }
                        }
                    }
                }
            }

            chunk->batch_hash = calculate_batch_hash(chunk);
            bool pushed = false;
            while (!pushed) {
                while (lock_stream_1.test_and_set(std::memory_order_acquire)) {}
                if (buff_1.size() < BUFFER_SIZE) {
                    buff_1.push(chunk);
                    pushed = true;
                }
                lock_stream_1.clear(std::memory_order_release);
            }
        }
    }
    s1_done.store(true);
    cout << "Thread S1 finished." << endl;
    return NULL;
}

// --- Order Checking ---
long long s2_expected_run = 0, s2_expected_row = 0;
long long s3_expected_run = 0, s3_expected_row = 0;

// --- Thread S2 ---
void* thread_S2(void* arg) {
    cout << "Thread S2 started." << endl;
    long long total_rows = (long long)NUM_RUNS * g_input_image->height, processed = 0;
    int height = g_input_image->height;

    while (processed < total_rows) {
        RowBatchChunk* s1_chunk = nullptr;
        bool popped = false;
        while (!popped) {
            while (lock_stream_1.test_and_set(std::memory_order_acquire)) {}
            if (!buff_1.empty()) {
                s1_chunk = buff_1.front(); buff_1.pop(); popped = true;
            }
            lock_stream_1.clear(std::memory_order_release);
            if (!popped && s1_done.load() && buff_1.empty()) goto end;
        }

        if (s1_chunk->run_id != s2_expected_run || s1_chunk->start_row != s2_expected_row) exit(EXIT_FAILURE);
        s2_expected_row += s1_chunk->num_rows;
        if (s2_expected_row >= height) { s2_expected_row = 0; s2_expected_run++; }
        if (calculate_batch_hash(s1_chunk) != s1_chunk->batch_hash) exit(EXIT_FAILURE);

        RowBatchChunk* s2_chunk = allocate_batch_chunk(s1_chunk->run_id, s1_chunk->start_row, s1_chunk->num_rows, s1_chunk->width);
        for (int k = 0; k < s1_chunk->num_rows; k++) {
            int r = s1_chunk->start_row + k;
            uint8_t** in = s1_chunk->batch_data[k];
            uint8_t** out = s2_chunk->batch_data[k];
            if (r == 0 || r == height - 1)
                for (int j = 0; j < s1_chunk->width; j++) memset(out[j], 0, 3);
            else
                for (int j = 0; j < s1_chunk->width; j++)
                    if (j == 0 || j == s1_chunk->width - 1) memset(out[j], 0, 3);
                    else for (int c = 0; c < 3; c++) {
                        short res = (short)g_input_image->image_pixels[r][j][c] - (short)in[j][c];
                        out[j][c] = (res < 0) ? 0 : (uint8_t)res;
                    }
        }

        s2_chunk->batch_hash = calculate_batch_hash(s2_chunk);
        delete_batch_chunk(s1_chunk);

        bool pushed = false;
        while (!pushed) {
            while (lock_stream_2.test_and_set(std::memory_order_acquire)) {}
            if (buff_2.size() < BUFFER_SIZE) { buff_2.push(s2_chunk); pushed = true; }
            lock_stream_2.clear(std::memory_order_release);
        }
        processed += s2_chunk->num_rows;
    }
end:
    s2_done.store(true);
    cout << "Thread S2 finished." << endl;
    return NULL;
}

// --- Thread S3 ---
void* thread_S3(void* arg) {
    S3_args* args = (S3_args*)arg;
    image_t* final_image = args->final_image;
    cout << "Thread S3 started." << endl;
    long long total_rows = (long long)NUM_RUNS * g_input_image->height, processed = 0;
    int height = g_input_image->height;

    while (processed < total_rows) {
        RowBatchChunk* s2_chunk = nullptr;
        bool popped = false;
        while (!popped) {
            while (lock_stream_2.test_and_set(std::memory_order_acquire)) {}
            if (!buff_2.empty()) {
                s2_chunk = buff_2.front(); buff_2.pop(); popped = true;
            }
            lock_stream_2.clear(std::memory_order_release);
            if (!popped && s2_done.load() && buff_2.empty()) goto end3;
        }

        if (s2_chunk->run_id != s3_expected_run || s2_chunk->start_row != s3_expected_row) exit(EXIT_FAILURE);
        s3_expected_row += s2_chunk->num_rows;
        if (s3_expected_row >= height) { s3_expected_row = 0; s3_expected_run++; }
        if (calculate_batch_hash(s2_chunk) != s2_chunk->batch_hash) exit(EXIT_FAILURE);

        if (s2_chunk->run_id == NUM_RUNS - 1) {
            for (int k = 0; k < s2_chunk->num_rows; k++) {
                int r = s2_chunk->start_row + k;
                uint8_t** in = s2_chunk->batch_data[k];
                if (r == 0 || r == height - 1)
                    for (int j = 0; j < s2_chunk->width; j++) memset(final_image->image_pixels[r][j], 0, 3);
                else
                    for (int j = 0; j < s2_chunk->width; j++)
                        if (j == 0 || j == s2_chunk->width - 1) memset(final_image->image_pixels[r][j], 0, 3);
                        else for (int c = 0; c < 3; c++) {
                            short res = (short)g_input_image->image_pixels[r][j][c] + (short)in[j][c];
                            final_image->image_pixels[r][j][c] = (res > 255) ? 255 : (uint8_t)res;
                        }
            }
        }
        delete_batch_chunk(s2_chunk);
        processed += s2_chunk->num_rows;
    }
end3:
    write_ppm_file(const_cast<char*>(args->output_filename.c_str()), final_image);
    cout << "Thread S3 finished." << endl;
    return NULL;
}

// --- Main ---
int main(int argc, char **argv) {
    if (argc != 3) { cout << "usage: ./part2_1 <input> <output>\n"; return 1; }

    string input_path = argv[1], output_path = argv[2];
    auto start_read = chrono::steady_clock::now();
    g_input_image = read_ppm_file(const_cast<char*>(input_path.c_str()));
    if (!g_input_image) { cerr << "Failed to read input\n"; return 1; }
    auto stop_read = chrono::steady_clock::now();
    cout << "Image read time: " << chrono::duration<double>(stop_read - start_read).count() << "s\n";

    image_t* final_output = create_empty_image(g_input_image->height, g_input_image->width);
    auto start_total = chrono::steady_clock::now();

    pthread_t t1, t2, t3;
    S3_args args{output_path, final_output};
    pthread_create(&t1, NULL, thread_S1, NULL);
    pthread_create(&t2, NULL, thread_S2, NULL);
    pthread_create(&t3, NULL, thread_S3, &args);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    auto stop_total = chrono::steady_clock::now();
    double total_time = chrono::duration<double>(stop_total - start_total).count();

    delete_image(g_input_image);
    delete_image(final_output);

    cout << "\n--- Timing Report ---\n";
    cout << "Parallel time: " << total_time << " s (" << NUM_RUNS << " runs)\n";
    cout << "Avg per run: " << total_time / NUM_RUNS * 1000 << " ms\n";
    cout << "\n--- Verification ---\n";
    if (s2_expected_run == NUM_RUNS && s3_expected_run == NUM_RUNS)
        cout << "SUCCESS\n";
    else
        cout << "FAILURE\n";
    return 0;
}