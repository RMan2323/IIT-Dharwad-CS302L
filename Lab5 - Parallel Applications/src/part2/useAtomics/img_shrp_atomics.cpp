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

// Hashing function
uint32_t hsh(const uint8_t v[3]){
    uint32_t x = (v[0] << 16) | (v[1] << 8) | v[2];
    x ^= (x >> 13);
    x *= 0x85ebca6b;
    x ^= (x >> 16);
    return x;
}

// Represents a batch of processed rows
struct RowBatchChunk {
    int run_id;
    int start_row;
    int num_rows;
    int width;
    uint8_t*** batch_data;
    uint32_t batch_hash;
};

// Shared buffers and locks
queue<RowBatchChunk*> buff_1;
queue<RowBatchChunk*> buff_2;

std::atomic_flag lock_stream_1 = ATOMIC_FLAG_INIT;
std::atomic_flag lock_stream_2 = ATOMIC_FLAG_INIT;

// Configurable constants
const int CHUNK_ROWS_N = 10;
const int BUFFER_SIZE = 20;
const int NUM_RUNS = 1000;

image_t* g_input_image = nullptr;

std::atomic<bool> s1_done(false);
std::atomic<bool> s2_done(false);

// Hash for batch verification
uint32_t calculate_batch_hash(RowBatchChunk* chunk) {
    uint32_t combined_hash = 0;
    for (int i = 0; i < chunk->num_rows; i++) {
        for (int j = 0; j < chunk->width; j++) {
            combined_hash ^= hsh(chunk->batch_data[i][j]);
        }
    }
    return combined_hash;
}

// Memory management helpers
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
        }
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
        for (int j = 0; j < width; j++) {
            chunk->batch_data[i][j] = new uint8_t[3];
        }
    }
    return chunk;
}

void delete_batch_chunk(RowBatchChunk* chunk) {
    if (chunk == nullptr) return;
    for (int i = 0; i < chunk->num_rows; i++) {
        for (int j = 0; j < chunk->width; j++) {
            delete[] chunk->batch_data[i][j];
        }
        delete[] chunk->batch_data[i];
    }
    delete[] chunk->batch_data;
    delete chunk;
}

// Arguments for S3 thread
struct S3_args {
    string output_filename;
    image_t* final_image;
};

// Thread 1: S1
void *thread_S1(void *arg)
{
    cout << "Thread S1 started." << endl;
    int height = g_input_image->height;
    int width = g_input_image->width;

    for (int run = 0; run < NUM_RUNS; run++)
    {
        for (int i = 0; i < height; i += CHUNK_ROWS_N)
        {
            int current_num_rows = min(CHUNK_ROWS_N, height - i);
            RowBatchChunk* chunk = allocate_batch_chunk(run, i, current_num_rows, width);
            for (int k = 0; k < current_num_rows; k++) {
                int current_row = i + k;
                uint8_t** output_row_pixels = chunk->batch_data[k];
                if (current_row == 0 || current_row == height - 1) {
                    for (int j = 0; j < width; j++) {
                        memset(output_row_pixels[j], 0, 3);
                    }
                } else {
                    for (int j = 0; j < width; j++) {
                        if (j == 0 || j == width - 1) {
                            memset(output_row_pixels[j], 0, 3);
                        } else {
                            for (int c = 0; c < 3; c++) {
                                uint16_t sum = 0;
                                for (int l = -1; l < 2; l++) {
                                    for (int m = -1; m < 2; m++) {
                                        sum += g_input_image->image_pixels[current_row - l][j - m][c] / 9;
                                    }
                                }
                                output_row_pixels[j][c] = sum;
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

// Global counters for verification
long long s2_expected_run = 0;
long long s2_expected_row = 0;
long long s3_expected_run = 0;
long long s3_expected_row = 0;

// Thread 2: S2
void *thread_S2(void *arg)
{
    cout << "Thread S2 started." << endl;
    long long total_rows_to_process = (long long)NUM_RUNS * g_input_image->height;
    long long rows_processed = 0;
    int img_height = g_input_image->height;

    while (rows_processed < total_rows_to_process)
    {
        RowBatchChunk* s1_chunk = nullptr;
        bool popped = false;
        while (!popped) {
            while (lock_stream_1.test_and_set(std::memory_order_acquire)) {}
            if (!buff_1.empty()) {
                s1_chunk = buff_1.front();
                buff_1.pop();
                popped = true;
            }
            lock_stream_1.clear(std::memory_order_release);
            if (!popped) {
                if (s1_done.load(std::memory_order_acquire)) {
                    while (lock_stream_1.test_and_set(std::memory_order_acquire)) {}
                    if (buff_1.empty()) {
                        lock_stream_1.clear(std::memory_order_release);
                        goto s2_loop_end;
                    }
                    lock_stream_1.clear(std::memory_order_release);
                }
            }
        }

        if (s1_chunk->run_id != s2_expected_run || s1_chunk->start_row != s2_expected_row) {
            cerr << "[FATAL] S1->S2 ORDER MISMATCH!" << endl;
            cerr << "  Expected: run=" << s2_expected_run << " row=" << s2_expected_row << endl;
            cerr << "  Actual:   run=" << s1_chunk->run_id << " row=" << s1_chunk->start_row << endl;
            exit(EXIT_FAILURE);
        }
        s2_expected_row += s1_chunk->num_rows;
        if (s2_expected_row >= img_height) {
            s2_expected_row = 0;
            s2_expected_run++;
        }

        uint32_t received_hash = calculate_batch_hash(s1_chunk);
        if (received_hash != s1_chunk->batch_hash) {
            cerr << "[FATAL] S1->S2 HASH MISMATCH!" << endl;
            cerr << "  Chunk:  run=" << s1_chunk->run_id << " row=" << s1_chunk->start_row << endl;
            cerr << "  Sent:   " << s1_chunk->batch_hash << endl;
            cerr << "  Recv'd: " << received_hash << endl;
            exit(EXIT_FAILURE);
        }

        RowBatchChunk* s2_chunk = allocate_batch_chunk(s1_chunk->run_id, s1_chunk->start_row, s1_chunk->num_rows, s1_chunk->width);
        for (int k = 0; k < s1_chunk->num_rows; k++) {
            int current_row = s1_chunk->start_row + k;
            uint8_t** s1_input_row_pixels = s1_chunk->batch_data[k];
            uint8_t** s2_output_row_pixels = s2_chunk->batch_data[k];
            if (current_row == 0 || current_row == img_height - 1) {
                 for (int j = 0; j < s1_chunk->width; j++) memset(s2_output_row_pixels[j], 0, 3);
            } else {
                for (int j = 0; j < s1_chunk->width; j++) {
                    if (j == 0 || j == s1_chunk->width - 1) {
                        memset(s2_output_row_pixels[j], 0, 3);
                    } else {
                        for (int c = 0; c < 3; c++) {
                            short res = (short)g_input_image->image_pixels[current_row][j][c] - (short)s1_input_row_pixels[j][c];
                            s2_output_row_pixels[j][c] = (res < 0) ? 0 : (uint8_t)res;
                        }
                    }
                }
            }
        }

        s2_chunk->batch_hash = calculate_batch_hash(s2_chunk);
        delete_batch_chunk(s1_chunk);

        bool pushed = false;
        while (!pushed) {
            while (lock_stream_2.test_and_set(std::memory_order_acquire)) {}
            if (buff_2.size() < BUFFER_SIZE) {
                buff_2.push(s2_chunk);
                pushed = true;
            }
            lock_stream_2.clear(std::memory_order_release);
        }
        rows_processed += s2_chunk->num_rows;
    }

s2_loop_end:
    s2_done.store(true);
    cout << "Thread S2 finished. Processed " << rows_processed << " rows." << endl;
    return NULL;
}

// Thread 3: S3
void *thread_S3(void *arg)
{
    S3_args* args = (S3_args*)arg;
    image_t* final_image = args->final_image;
    cout << "Thread S3 started." << endl;
    long long total_rows_to_process = (long long)NUM_RUNS * g_input_image->height;
    long long rows_processed = 0;
    int img_height = g_input_image->height;

    while (rows_processed < total_rows_to_process)
    {
        RowBatchChunk* s2_chunk = nullptr;
        bool popped = false;
        while (!popped) {
            while (lock_stream_2.test_and_set(std::memory_order_acquire)) {}
            if (!buff_2.empty()) {
                s2_chunk = buff_2.front();
                buff_2.pop();
                popped = true;
            }
            lock_stream_2.clear(std::memory_order_release);
            if (!popped) {
                if (s2_done.load(std::memory_order_acquire)) {
                    while (lock_stream_2.test_and_set(std::memory_order_acquire)) {}
                    if (buff_2.empty()) {
                        lock_stream_2.clear(std::memory_order_release);
                        goto s3_loop_end;
                    }
                    lock_stream_2.clear(std::memory_order_release);
                }
            }
        }

        if (s2_chunk->run_id != s3_expected_run || s2_chunk->start_row != s3_expected_row) {
            cerr << "[FATAL] S2->S3 ORDER MISMATCH!" << endl;
            cerr << "  Expected: run=" << s3_expected_run << " row=" << s3_expected_row << endl;
            cerr << "  Actual:   run=" << s2_chunk->run_id << " row=" << s2_chunk->start_row << endl;
            exit(EXIT_FAILURE);
        }
        s3_expected_row += s2_chunk->num_rows;
        if (s3_expected_row >= img_height) {
            s3_expected_row = 0;
            s3_expected_run++;
        }

        uint32_t received_hash = calculate_batch_hash(s2_chunk);
        if (received_hash != s2_chunk->batch_hash) {
            cerr << "[FATAL] S2->S3 HASH MISMATCH!" << endl;
            cerr << "  Chunk:  run=" << s2_chunk->run_id << " row=" << s2_chunk->start_row << endl;
            cerr << "  Sent:   " << s2_chunk->batch_hash << endl;
            cerr << "  Recv'd: " << received_hash << endl;
            exit(EXIT_FAILURE);
        }

        if (s2_chunk->run_id == NUM_RUNS - 1) {
            for (int k = 0; k < s2_chunk->num_rows; k++) {
                int current_row = s2_chunk->start_row + k;
                uint8_t** s2_input_row_pixels = s2_chunk->batch_data[k];
                if (current_row == 0 || current_row == img_height - 1) {
                    for (int j = 0; j < s2_chunk->width; j++) memset(final_image->image_pixels[current_row][j], 0, 3);
                } else {
                    for (int j = 0; j < s2_chunk->width; j++) {
                        if (j == 0 || j == s2_chunk->width - 1) {
                            memset(final_image->image_pixels[current_row][j], 0, 3);
                        } else {
                            for (int c = 0; c < 3; c++) {
                                short res = (short)g_input_image->image_pixels[current_row][j][c] + (short)s2_input_row_pixels[j][c];
                                final_image->image_pixels[current_row][j][c] = (res > 255) ? 255 : (uint8_t)res;
                            }
                        }
                    }
                }
            }
        }

        delete_batch_chunk(s2_chunk);
        rows_processed += s2_chunk->num_rows;
    }

s3_loop_end:
    cout << "Thread S3 writing final image..." << endl;
    write_ppm_file(const_cast<char*>(args->output_filename.c_str()), final_image);
    cout << "Thread S3 finished. Processed " << rows_processed << " rows." << endl;
    return NULL;
}

// Main function
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        cout << "usage: ./part2_1 <path-to-original-image> <path-to-transformed-image>\n\n";
        return 1;
    }

    string input_path = argv[1];
    string output_path = argv[2];

    auto start_read = chrono::steady_clock::now();
    g_input_image = read_ppm_file(const_cast<char*>(input_path.c_str()));
    if (g_input_image == nullptr) {
        cerr << "Failed to read input image: " << input_path << endl;
        return 1;
    }
    auto stop_read = chrono::steady_clock::now();
    chrono::duration<double> read_time = stop_read - start_read;
    cout << "Image read time: " << fixed << setprecision(6) << read_time.count() << "s" << endl;

    image_t* final_output_image = create_empty_image(g_input_image->height, g_input_image->width);

    long long total_rows = (long long)NUM_RUNS * g_input_image->height;
    long long total_chunks = (long long)ceil((double)g_input_image->height / CHUNK_ROWS_N) * NUM_RUNS;
    cout << "Starting 3 threads for " << NUM_RUNS << " runs." << endl;
    cout << "Chunk size = " << CHUNK_ROWS_N << " rows. Total rows = " << total_rows << ". Total chunks = " << total_chunks << "." << endl;
    
    auto start_total = chrono::steady_clock::now();

    pthread_t t1, t2, t3;
    
    S3_args s3_args;
    s3_args.output_filename = output_path;
    s3_args.final_image = final_output_image;

    pthread_create(&t1, NULL, thread_S1, NULL);
    pthread_create(&t2, NULL, thread_S2, NULL);
    pthread_create(&t3, NULL, thread_S3, &s3_args);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    auto stop_total = chrono::steady_clock::now();
    chrono::duration<double> total_time = stop_total - start_total;

    cout << "All threads finished." << endl;

    delete_image(g_input_image);
    delete_image(final_output_image);

    cout << "\n--- Timing Report ---" << endl;
    cout << "Read time:     " << setw(12) << read_time.count() << " s" << endl;
    cout << "Parallel time: " << setw(12) << total_time.count() << " s (for " << NUM_RUNS << " runs)" << endl;
    cout << "Avg per run:   " << setw(12) << total_time.count() / NUM_RUNS * 1000 << " ms" << endl;

    cout << "\n--- Verification ---" << endl;
    if (s2_expected_run == NUM_RUNS && s3_expected_run == NUM_RUNS) {
        cout << "SUCCESS: Order check passed for all " << NUM_RUNS << " runs." << endl;
    } else {
        cout << "FAILURE: Order check incomplete. S2 saw " << s2_expected_run << " runs, S3 saw " << s3_expected_run << " runs." << endl;
    }

    return 0;
}