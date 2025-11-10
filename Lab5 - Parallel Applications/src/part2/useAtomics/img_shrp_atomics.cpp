#include <iostream>
#include "libppm.h" // Assuming libppm.h contains image_t and read/write functions
#include <cstdint>  // For uint32_t, uint8_t
#include <chrono>   // For timing in main
#include <iomanip>  // For setw, setprecision
#include <atomic>   // For std::atomic_flag, std::atomic<bool>, std::memory_order
#include <thread>   // For std::this_thread
#include <queue>    // For std::queue
#include <string>   // For std::string
#include <pthread.h>
#include <cstring> // For memcpy, memset
#include <cmath>   // For ceil
#include <stdlib.h> // For exit, EXIT_FAILURE

using namespace std;

// --- Hashing Function (from pipe example) ---
uint32_t hsh(const uint8_t v[3]){
    uint32_t x = (v[0] << 16) | (v[1] << 8) | v[2];
    x ^= (x >> 13);
    x *= 0x85ebca6b;
    x ^= (x >> 16);
    return x;
}

// --- New Data Structure for N-Row Pipeline ---
struct RowBatchChunk {
    int run_id;     // Which run (0-999) this batch belongs to
    int start_row;  // The starting row index (0 to height-1)
    int num_rows;   // Number of rows in *this* batch (can be < CHUNK_ROWS_N for the last batch)
    int width;      // Width of the rows
    uint8_t*** batch_data; // The pixel data: [num_rows][width][3]
    uint32_t batch_hash;   // Hash of the data in batch_data
};


// --- Shared Resources ---
queue<RowBatchChunk*> buff_1; // S1 -> S2
queue<RowBatchChunk*> buff_2; // S2 -> S3

std::atomic_flag lock_stream_1 = ATOMIC_FLAG_INIT;
std::atomic_flag lock_stream_2 = ATOMIC_FLAG_INIT;

// --- Granularity Control ---
// N = 1:   Fine-grained (one row per chunk).
// N = 10:  Batches of 10 rows.
// N = g_input_image->height: Coarse-grained (one image per chunk).
const int CHUNK_ROWS_N = 10; // You can change this value

// --- Constants for the pipeline ---
const int BUFFER_SIZE = 20; // Max items in queue
const int NUM_RUNS = 1000;  // Number of times to run the pipeline

image_t* g_input_image = nullptr;

std::atomic<bool> s1_done(false);
std::atomic<bool> s2_done(false);


// --- Batch Hash Function ---
uint32_t calculate_batch_hash(RowBatchChunk* chunk) {
    uint32_t combined_hash = 0;
    // We only hash the *inner pixels* just like the serial code
    // The serial code never touches the border pixels, so we won't either.
    for (int i = 0; i < chunk->num_rows; i++) {
        int current_row = chunk->start_row + i;
        if (current_row == 0 || current_row == g_input_image->height - 1) continue;

        for (int j = 0; j < chunk->width; j++) {
            if (j == 0 || j == chunk->width - 1) continue;
            
            // Combine hashes. A simple XOR is good for this.
            combined_hash ^= hsh(chunk->batch_data[i][j]);
        }
    }
    return combined_hash;
}


// --- Memory Management ---
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
            // We leave the data uninitialized, just like the serial code
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
            // We leave the data uninitialized, just like the serial code
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


// --- Thread Functions ---

struct S3_args {
    string output_filename;
    image_t* final_image; // S3 will write its final result here
};

void *thread_S1(void *arg)
{
    cout << "Thread S1 started." << endl;
    int height = g_input_image->height;
    int width = g_input_image->width;

    for (int run = 0; run < NUM_RUNS; run++)
    {
        for (int i = 0; i < height; i += CHUNK_ROWS_N)
        {
            // 1. Determine batch size and create chunk
            int current_num_rows = min(CHUNK_ROWS_N, height - i);
            RowBatchChunk* chunk = allocate_batch_chunk(run, i, current_num_rows, width);

            // 2. Process all rows in this batch
            for (int k = 0; k < current_num_rows; k++) {
                int current_row = i + k; // The actual row index in the image
                uint8_t** output_row_pixels = chunk->batch_data[k]; // [width][3]
                
                // --- S1 (Smoothen) Logic for 'current_row' ---
                // Only process inner rows
                if (current_row > 0 && current_row < height - 1) {
                    // Inner row: Apply smoothing filter
                    for (int j = 1; j < width - 1; j++) {
                        // Inner pixel: Apply 3x3 kernel
                        for (int c = 0; c < 3; c++) { // Color channel
                            
                            // --- FIX: Reverted to original serial code logic ---
                            uint8_t result = 0;
                            for (int l = -1; l < 2; l++) { // Row offset
                                for (int m = -1; m < 2; m++) { // Col offset
                                    result += g_input_image->image_pixels[current_row - l][j - m][c] / 9;
                                }
                            }
                            output_row_pixels[j][c] = result;
                            // --- End Fix ---
                        }
                    }
                }
                // --- End S1 Logic ---
                // We do NOT touch border rows/pixels, leaving them as garbage
            } // end loop for rows *within* batch

            // 3. Calculate hash for the processed batch
            chunk->batch_hash = calculate_batch_hash(chunk);

            // 4. Produce (add batch chunk to queue)
            bool pushed = false;
            while (!pushed) {
                while (lock_stream_1.test_and_set(std::memory_order_acquire)) {} // Spin
                if (buff_1.size() < BUFFER_SIZE) {
                    buff_1.push(chunk);
                    pushed = true;
                }
                lock_stream_1.clear(std::memory_order_release);
                if (!pushed) {
                    // std::this_thread::sleep_for(std::chrono::microseconds(10)); // Optional: yield
                }
            }
        } // end batch loop
    } // end run loop

    s1_done.store(true);
    cout << "Thread S1 finished." << endl;
    return NULL;
}

// --- Order Checking Variables ---
long long s2_expected_run = 0;
long long s2_expected_row = 0;
long long s3_expected_run = 0;
long long s3_expected_row = 0;


void *thread_S2(void *arg)
{
    cout << "Thread S2 started." << endl;
    long long total_rows_to_process = (long long)NUM_RUNS * g_input_image->height;
    long long rows_processed = 0;
    int img_height = g_input_image->height; // Cache height
    int img_width = g_input_image->width; // Cache width

    while (rows_processed < total_rows_to_process)
    {
        RowBatchChunk* s1_chunk = nullptr;

        // 1. Consume (from buff_1)
        bool popped = false;
        while (!popped) {
            while (lock_stream_1.test_and_set(std::memory_order_acquire)) {} // Spin
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
                        goto s2_loop_end; // S1 is done and buffer is empty
                    }
                    lock_stream_1.clear(std::memory_order_release);
                }
                // std::this_thread::sleep_for(std::chrono::microseconds(10)); // Optional: yield
            }
        }

        // --- Verification Step 1: Check Order ---
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

        // --- Verification Step 2: Check Hash (Data Integrity) ---
        uint32_t received_hash = calculate_batch_hash(s1_chunk);
        if (received_hash != s1_chunk->batch_hash) {
            cerr << "[FATAL] S1->S2 HASH MISMATCH!" << endl;
            cerr << "  Chunk:  run=" << s1_chunk->run_id << " row=" << s1_chunk->start_row << endl;
            cerr << "  Sent:   " << s1_chunk->batch_hash << endl;
            cerr << "  Recv'd: " << received_hash << endl;
            exit(EXIT_FAILURE);
        }


        // 2. Process the batch (S2 Logic)
        RowBatchChunk* s2_chunk = allocate_batch_chunk(s1_chunk->run_id, s1_chunk->start_row, s1_chunk->num_rows, s1_chunk->width);

        for (int k = 0; k < s1_chunk->num_rows; k++) {
            int current_row = s1_chunk->start_row + k;
            uint8_t** s1_input_row_pixels = s1_chunk->batch_data[k]; // [width][3]
            uint8_t** s2_output_row_pixels = s2_chunk->batch_data[k]; // [width][3]

            // --- S2 (Details) Logic for 'current_row' ---
            if (current_row > 0 && current_row < img_height - 1) {
                for (int j = 1; j < img_width - 1; j++) {
                    for (int c = 0; c < 3; c++) {
                        short res = (short)g_input_image->image_pixels[current_row][j][c] - (short)s1_input_row_pixels[j][c];
                        s2_output_row_pixels[j][c] = (res < 0) ? 0 : (uint8_t)res;
                    }
                }
            }
            // --- End S2 Logic ---
        }

        // 3. Calculate hash for the *new* S2 data
        s2_chunk->batch_hash = calculate_batch_hash(s2_chunk);

        // *** CRITICAL: Free the batch chunk we consumed ***
        delete_batch_chunk(s1_chunk);

        // 4. Produce (to buff_2)
        bool pushed = false;
        while (!pushed) {
            while (lock_stream_2.test_and_set(std::memory_order_acquire)) {} // Spin
            if (buff_2.size() < BUFFER_SIZE) {
                buff_2.push(s2_chunk);
                pushed = true;
            }
            lock_stream_2.clear(std::memory_order_release);
            if (!pushed) {
                 // std::this_thread::sleep_for(std::chrono::microseconds(10)); // Optional: yield
            }
        }
        
        rows_processed += s2_chunk->num_rows;
    }

s2_loop_end:
    s2_done.store(true);
    cout << "Thread S2 finished. Processed " << rows_processed << " rows." << endl;
    return NULL;
}

void *thread_S3(void *arg)
{
    S3_args* args = (S3_args*)arg;
    image_t* final_image = args->final_image; // The pre-allocated empty image

    cout << "Thread S3 started." << endl;
    long long total_rows_to_process = (long long)NUM_RUNS * g_input_image->height;
    long long rows_processed = 0;
    int img_height = g_input_image->height; // Cache height
    int img_width = g_input_image->width; // Cache width

    while (rows_processed < total_rows_to_process)
    {
        RowBatchChunk* s2_chunk = nullptr;

        // 1. Consume (from buff_2)
        bool popped = false;
        while (!popped) {
            while (lock_stream_2.test_and_set(std::memory_order_acquire)) {} // Spin
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
                        goto s3_loop_end; // S2 is done and buffer is empty
                    }
                    lock_stream_2.clear(std::memory_order_release);
                }
                 // std::this_thread::sleep_for(std::chrono::microseconds(10)); // Optional: yield
            }
        }

        // --- Verification Step 1: Check Order ---
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

        // --- Verification Step 2: Check Hash (Data Integrity) ---
        uint32_t received_hash = calculate_batch_hash(s2_chunk);
        if (received_hash != s2_chunk->batch_hash) {
            cerr << "[FATAL] S2->S3 HASH MISMATCH!" << endl;
            cerr << "  Chunk:  run=" << s2_chunk->run_id << " row=" << s2_chunk->start_row << endl;
            cerr << "  Sent:   " << s2_chunk->batch_hash << endl;
            cerr << "  Recv'd: " << received_hash << endl;
            exit(EXIT_FAILURE);
        }


        // 2. Process the batch (S3 Logic)
        if (s2_chunk->run_id == NUM_RUNS - 1) {
            for (int k = 0; k < s2_chunk->num_rows; k++) {
                int current_row = s2_chunk->start_row + k;
                uint8_t** s2_input_row_pixels = s2_chunk->batch_data[k]; // [width][3]
                
                // --- S3 (Sharpen) Logic for 'current_row' ---
                if (current_row > 0 && current_row < img_height - 1) {
                    for (int j = 1; j < img_width - 1; j++) {
                        for (int c = 0; c < 3; c++) {
                            short res = (short)g_input_image->image_pixels[current_row][j][c] + (short)s2_input_row_pixels[j][c];
                            final_image->image_pixels[current_row][j][c] = (res > 255) ? 255 : (uint8_t)res;
                        }
                    }
                }
                // --- End S3 Logic ---
            }
        }

        // *** CRITICAL: Free the batch chunk we consumed ***
        delete_batch_chunk(s2_chunk);
        
        rows_processed += s2_chunk->num_rows;
    }

s3_loop_end:
    // 3. Write the final assembled image to file
    cout << "Thread S3 writing final image..." << endl;
    write_ppm_file(const_cast<char*>(args->output_filename.c_str()), final_image);

    cout << "Thread S3 finished. Processed " << rows_processed << " rows." << endl;
    return NULL;
}


// --- Main Function ---

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        cout << "usage: ./part2_1 <path-to-original-image> <path-to-transformed-image>\n\n";
        return 1;
    }

    string input_path = argv[1];
    string output_path = argv[2];

    // --- Read Image (Once) ---
    auto start_read = chrono::steady_clock::now();
    g_input_image = read_ppm_file(const_cast<char*>(input_path.c_str()));
    if (g_input_image == nullptr) {
        cerr << "Failed to read input image: " << input_path << endl;
        return 1;
    }
    auto stop_read = chrono::steady_clock::now();
    chrono::duration<double> read_time = stop_read - start_read;
    cout << "Image read time: " << fixed << setprecision(6) << read_time.count() << "s" << endl;

    // --- Prepare for S3 ---
    image_t* final_output_image = create_empty_image(g_input_image->height, g_input_image->width);

    // --- Start Parallel Processing ---
    long long total_rows = (long long)NUM_RUNS * g_input_image->height;
    long long total_chunks = (long long)ceil((double)g_input_image->height / CHUNK_ROWS_N) * NUM_RUNS;
    cout << "Starting 3 threads for " << NUM_RUNS << " runs." << endl;
    cout << "Chunk size = " << CHUNK_ROWS_N << " rows. Total rows = " << total_rows << ". Total chunks = " << total_chunks << "." << endl;
    
    auto start_total = chrono::steady_clock::now();

    pthread_t t1, t2, t3;
    
    S3_args s3_args;
    s3_args.output_filename = output_path;
    s3_args.final_image = final_output_image; // Give S3 the empty image

    pthread_create(&t1, NULL, thread_S1, NULL);
    pthread_create(&t2, NULL, thread_S2, NULL);
    pthread_create(&t3, NULL, thread_S3, &s3_args); // Pass args to S3

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    auto stop_total = chrono::steady_clock::now();
    chrono::duration<double> total_time = stop_total - start_total;
    // --- End Parallel Processing ---

    cout << "All threads finished." << endl;

    // Clean up global and final images
    delete_image(g_input_image);
    delete_image(final_output_image);

    // Print final timing report
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