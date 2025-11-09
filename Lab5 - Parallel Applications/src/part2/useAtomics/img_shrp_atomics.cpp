#include <iostream> 
#include "libppm.h" // image_t, read/write functions
#include <pthread.h>
#include <cstring>
#include <cmath>

using namespace std;

// Hash a single RGB pixel
uint32_t hsh(const uint8_t v[3]){
    uint32_t x = (v[0] << 16) | (v[1] << 8) | v[2];
    x ^= (x >> 13);
    x *= 0x85ebca6b;
    x ^= (x >> 16);
    return x;
}

// Buffers between pipeline stages
queue<struct RowBatchChunk*> buff_1; // S1 -> S2
queue<struct RowBatchChunk*> buff_2; // S2 -> S3

image_t* g_input_image = nullptr;

// Batch of rows
struct RowBatchChunk {
    int run_id;
    int start_row;
    int num_rows;
    int width;
    uint8_t*** batch_data;
    uint32_t batch_hash;
};

// Hash for an entire batch
uint32_t calculate_batch_hash(RowBatchChunk* chunk) {
    uint32_t combined_hash = 0;
    for (int i = 0; i < chunk->num_rows; i++)
        for (int j = 0; j < chunk->width; j++)
            combined_hash ^= hsh(chunk->batch_data[i][j]);
    return combined_hash;
}

// Thread S1: read/process input, send batches to S2
void *thread_S1(void *arg)
{
    for (int run = 0; run < NUM_RUNS; run++) {
        for (int i = 0; i < g_input_image->height; i += CHUNK_ROWS_N) {
            // Process rows (S1 logic)
            chunk->batch_hash = calculate_batch_hash(chunk);

            // Push to buff_1
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
}

// Thread S2: consume from S1, process, send to S3
void *thread_S2(void *arg)
{
    int img_height = g_input_image->height;
    while (rows_processed < total_rows_to_process) {
        RowBatchChunk* s1_chunk = nullptr;
        // Consume from buff_1

        // Check order
        if (s1_chunk->run_id != s2_expected_run || s1_chunk->start_row != s2_expected_row) exit(EXIT_FAILURE);
        s2_expected_row += s1_chunk->num_rows;
        if (s2_expected_row >= img_height) {
            s2_expected_row = 0;
            s2_expected_run++;
        }

        // Check hash
        if (calculate_batch_hash(s1_chunk) != s1_chunk->batch_hash) exit(EXIT_FAILURE);

        // Process batch (S2 logic)
        s2_chunk->batch_hash = calculate_batch_hash(s2_chunk);

        delete_batch_chunk(s1_chunk);

        // Push to buff_2
        rows_processed += s2_chunk->num_rows;
    }
}

// Thread S3: consume from S2, process, write final image
void *thread_S3(void *arg)
{
    int img_height = g_input_image->height;
    while (rows_processed < total_rows_to_process) {
        RowBatchChunk* s2_chunk = nullptr;
        // Consume from buff_2

        // Check order
        if (s2_chunk->run_id != s3_expected_run || s2_chunk->start_row != s3_expected_row) exit(EXIT_FAILURE);
        s3_expected_row += s2_chunk->num_rows;
        if (s3_expected_row >= img_height) {
            s3_expected_row = 0;
            s3_expected_run++;
        }

        // Check hash
        if (calculate_batch_hash(s2_chunk) != s2_chunk->batch_hash) exit(EXIT_FAILURE);

        // Process batch (S3 logic)
        delete_batch_chunk(s2_chunk);
        rows_processed += s2_chunk->num_rows;
    }
}

// Main function
int main(int argc, char **argv)
{
    // Run threads and process image

    cout << "\n--- Verification ---" << endl;
    if (s2_expected_run == NUM_RUNS && s3_expected_run == NUM_RUNS)
        cout << "SUCCESS: Order check passed for all " << NUM_RUNS << " runs." << endl;
    else
        cout << "FAILURE: Order check incomplete. S2 saw " << s2_expected_run << " runs, S3 saw " << s3_expected_run << " runs." << endl;

    return 0;
}