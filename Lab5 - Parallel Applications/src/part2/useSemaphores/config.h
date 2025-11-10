#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

// --- Configurable Constants ---
const int NUM_RUNS = 1000;
const int CHUNK_ROWS_N = 10;  // Batch size: 10 rows
const int BUFFER_SLOTS = 20;  // Number of slots in the shared memory buffer
const int MAX_WIDTH = 1000; // Max image width, for sizing batch_data

// --- Shared Memory & Semaphore Names ---
#define SHM_NAME "/shm_s1s2"
#define SEM_FULL "/sem_full_s1s2"
#define SEM_EMPTY "/sem_empty_s1s2"
#define SEM_MUTEX "/sem_mutex_s1s2"
#define SEM_TIME "/sem_time_s1s2"

#define SHM_NAME_2 "/shm_s2s3"
#define SEM_FULL_2 "/sem_full_s2s3"
#define SEM_EMPTY_2 "/sem_empty_s2s3"
#define SEM_MUTEX_2 "/sem_mutex_s2s3"
#define SEM_TIME_2 "/sem_time_s2s3"

// --- Data Structures ---

// This is the "unit of work"
struct RowBatchChunk {
    int run_id;
    int start_row;
    int num_rows;
    int width;
    uint32_t batch_hash;
    // Flat data buffer for the batch
    uint8_t batch_data[CHUNK_ROWS_N * MAX_WIDTH * 3]; 
};

// This is the layout of the shared memory
struct SharedData {
    int64_t start_time_ns;
    int writeIdx;
    int readIdx;
    RowBatchChunk buffer[BUFFER_SLOTS]; // The circular buffer of batches
};

#endif // CONFIG_H