#include <iostream>
#include "libppm.h" // Handles image_t structure and read/write functions
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <thread>
#include <queue>
#include <string>
#include <pthread.h>
#include <cstring> // For memcpy

using namespace std;

// --- Data Structure for Passing Work Between Threads ---

/**
 * Each RowChunk represents one row of the image being processed.
 * We'll pass these between threads.
 */
struct RowChunk {
    int run_id;     // Run number (0–999)
    int row_index;  // Row index in the image
    int width;      // Width of this row
    uint8_t** row_data; // Pixel data for this row [width][3]
};


// --- Shared Queues for Communication Between Threads ---

queue<RowChunk*> buff_1; // From S1 → S2
queue<RowChunk*> buff_2; // From S2 → S3

// Simple spin locks for protecting queue access
std::atomic_flag lock_stream_1 = ATOMIC_FLAG_INIT;
std::atomic_flag lock_stream_2 = ATOMIC_FLAG_INIT;

// --- Control Granularity ---

/**
 * How many rows are processed at once.
 * CHUNK_ROWS_N = 1 → process one row at a time (fine-grained)
 * CHUNK_ROWS_N = 10 → 10 rows per chunk (medium)
 * CHUNK_ROWS_N = height → whole image (coarse-grained)
 */
const int CHUNK_ROWS_N = 1; // You can change this value

// Queue size limit
const int BUFFER_SIZE = 20;
// How many times to run the whole pipeline
const int NUM_RUNS = 1000;

// Input image shared by all threads
image_t* g_input_image = nullptr;

// Flags to signal when threads are done
std::atomic<bool> s1_done(false);
std::atomic<bool> s2_done(false);


// --- Data Structure for Multi-Row Processing ---

/**
 * A batch of N rows processed together.
 * This is what threads will send between each other.
 */
struct RowBatchChunk {
    int run_id;     // Run number (0–999)
    int start_row;  // First row index in this batch
    int num_rows;   // Number of rows in this batch
    int width;      // Width of each row
    uint8_t*** batch_data; // Pixel data [num_rows][width][3]
};


// --- Memory Management Helpers ---

/**
 * Free all memory used by an image_t.
 */
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

/**
 * Create an empty image_t that S3 will fill later.
 */
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

/**
 * Create a new batch chunk with allocated space for pixels.
 */
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

/**
 * Free memory used by a RowBatchChunk.
 */
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


// --- Thread Arguments ---

struct S3_args {
    string output_filename;
    image_t* final_image; // Where S3 will store its output
};


// --- Thread 1 (S1): Smoothen Image ---

/**
 * Thread S1:
 * - Reads the input image
 * - Applies smoothing filter
 * - Sends processed rows to S2
 * - Repeats NUM_RUNS times
 */
void *thread_S1(void *arg)
{
    cout << "Thread S1 started." << endl;
    int height = g_input_image->height;
    int width = g_input_image->width;

    for (int run = 0; run < NUM_RUNS; run++) {
        for (int i = 0; i < height; i += CHUNK_ROWS_N) {
            int current_num_rows = min(CHUNK_ROWS_N, height - i);
            RowBatchChunk* chunk = allocate_batch_chunk(run, i, current_num_rows, width);

            for (int k = 0; k < current_num_rows; k++) {
                int current_row = i + k;
                uint8_t** output_row_pixels = chunk->batch_data[k];

                if (current_row == 0 || current_row == height - 1) {
                    // Top/bottom rows: copy directly
                    for (int j = 0; j < width; j++) {
                        memcpy(output_row_pixels[j], g_input_image->image_pixels[current_row][j], 3);
                    }
                } else {
                    // Inner rows: apply smoothing
                    for (int j = 0; j < width; j++) {
                        if (j == 0 || j == width - 1) {
                            memcpy(output_row_pixels[j], g_input_image->image_pixels[current_row][j], 3);
                        } else {
                            for (int c = 0; c < 3; c++) {
                                uint16_t sum = 0;
                                for (int l = -1; l < 2; l++) {
                                    for (int m = -1; m < 2; m++) {
                                        sum += g_input_image->image_pixels[current_row - l][j - m][c];
                                    }
                                }
                                output_row_pixels[j][c] = sum / 9;
                            }
                        }
                    }
                }
            }
            // TODO: push this chunk to buff_1 (code not shown)
        }
    }

    s1_done.store(true);
    cout << "Thread S1 finished." << endl;
    return NULL;
}


// --- Thread 2 (S2): Find Details ---

/**
 * Thread S2:
 * - Takes rows from S1
 * - Finds details by subtracting smoothened pixels
 * - Sends results to S3
 */
void *thread_S2(void *arg)
{
    cout << "Thread S2 started." << endl;
    long long total_rows_to_process = (long long)NUM_RUNS * g_input_image->height;
    long long rows_processed = 0;

    while (rows_processed < total_rows_to_process)
    {
        RowBatchChunk* s1_chunk = nullptr;

        // Wait for data from S1
        bool popped = false;
        while (!popped) {
            while (lock_stream_1.test_and_set(std::memory_order_acquire)) {}
            if (!buff_1.empty()) {
                s1_chunk = buff_1.front();
                buff_1.pop();
                popped = true;
            }
            lock_stream_1.clear(std::memory_order_release);

            if (!popped && s1_done.load() && buff_1.empty()) goto s2_loop_end;
        }

        // Process the chunk
        RowBatchChunk* s2_chunk = allocate_batch_chunk(s1_chunk->run_id, s1_chunk->start_row, s1_chunk->num_rows, s1_chunk->width);
        for (int k = 0; k < s1_chunk->num_rows; k++) {
            int current_row = s1_chunk->start_row + k;
            uint8_t** s1_input_row = s1_chunk->batch_data[k];
            uint8_t** s2_output_row = s2_chunk->batch_data[k];

            for (int j = 0; j < s1_chunk->width; j++) {
                for (int c = 0; c < 3; c++) {
                    short res = (short)g_input_image->image_pixels[current_row][j][c] - (short)s1_input_row[j][c];
                    s2_output_row[j][c] = (res < 0) ? 0 : (uint8_t)res;
                }
            }
        }

        delete_batch_chunk(s1_chunk); // Free memory

        // Push to next buffer
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


// --- Thread 3 (S3): Sharpen Image and Save ---

/**
 * Thread S3:
 * - Takes rows from S2
 * - Sharpens them
 * - Writes final result to output image (on last run)
 */
void *thread_S3(void *arg)
{
    S3_args* args = (S3_args*)arg;
    image_t* final_image = args->final_image;

    cout << "Thread S3 started." << endl;
    long long total_rows_to_process = (long long)NUM_RUNS * g_input_image->height;
    long long rows_processed = 0;

    while (rows_processed < total_rows_to_process)
    {
        RowBatchChunk* s2_chunk = nullptr;

        // Wait for data from S2
        bool popped = false;
        while (!popped) {
            while (lock_stream_2.test_and_set(std::memory_order_acquire)) {}
            if (!buff_2.empty()) {
                s2_chunk = buff_2.front();
                buff_2.pop();
                popped = true;
            }
            lock_stream_2.clear(std::memory_order_release);

            if (!popped && s2_done.load() && buff_2.empty()) goto s3_loop_end;
        }

        // Process only the last run’s data
        if (s2_chunk->run_id == NUM_RUNS - 1) {
            for (int k = 0; k < s2_chunk->num_rows; k++) {
                int current_row = s2_chunk->start_row + k;
                uint8_t** s2_row = s2_chunk->batch_data[k];

                for (int j = 0; j < s2_chunk->width; j++) {
                    for (int c = 0; c < 3; c++) {
                        short res = (short)g_input_image->image_pixels[current_row][j][c] + (short)s2_row[j][c];
                        final_image->image_pixels[current_row][j][c] = (res > 255) ? 255 : (uint8_t)res;
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
    cout << "Thread S3 finished." << endl;
    return NULL;
}


// --- Main Function ---

int main(int argc, char **argv)
{
    if (argc != 3) {
        cout << "Usage: ./part2_1 <input_image> <output_image>\n";
        return 1;
    }

    string input_path = argv[1];
    string output_path = argv[2];

    // Read the input image
    auto start_read = chrono::steady_clock::now();
    g_input_image = read_ppm_file(const_cast<char*>(input_path.c_str()));
    if (g_input_image == nullptr) {
        cerr << "Error: could not read input image.\n";
        return 1;
    }
    auto stop_read = chrono::steady_clock::now();
    chrono::duration<double> read_time = stop_read - start_read;
    cout << "Image read time: " << fixed << setprecision(6) << read_time.count() << "s" << endl;

    // Prepare empty output image
    image_t* final_output_image = create_empty_image(g_input_image->height, g_input_image->width);

    // Start the three pipeline threads
    cout << "Running " << NUM_RUNS << " times with " << CHUNK_ROWS_N << " rows per chunk.\n";

    auto start_total = chrono::steady_clock::now();

    pthread_t t1, t2, t3;
    S3_args s3_args = { output_path, final_output_image };

    pthread_create(&t1, NULL, thread_S1, NULL);
    pthread_create(&t2, NULL, thread_S2, NULL);
    pthread_create(&t3, NULL, thread_S3, &s3_args);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    auto stop_total = chrono::steady_clock::now();
    chrono::duration<double> total_time = stop_total - start_total;

    cout << "All threads done.\n";

    // Free memory
    delete_image(g_input_image);
    delete_image(final_output_image);

    // Show timings
    cout << "\n--- Timing Report ---" << endl;
    cout << "Read time:     " << setw(10) << read_time.count() << " s" << endl;
    cout << "Parallel time: " << setw(10) << total_time.count() << " s (" << NUM_RUNS << " runs)" << endl;
    cout << "Avg per run:   " << setw(10) << total_time.count() / NUM_RUNS * 1000 << " ms" << endl;

    return 0;
}