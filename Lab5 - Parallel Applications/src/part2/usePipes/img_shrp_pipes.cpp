#include <iostream>
#include "libppm.h"
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <unistd.h>
#include <sys/wait.h> // For waitpid
#include <cstring>    // For memset
#include <cmath>      // For ceil
#include <stdlib.h>   // For exit, EXIT_FAILURE

using namespace std;

// --- Configurable Constants ---
const int CHUNK_ROWS_N = 10; // Number of rows per batch
const int NUM_RUNS = 1000;

// --- Data Structures ---
// Header sent over the pipe before each data buffer
struct RowBatchHeader {
    int run_id;
    int start_row;
    int num_rows;
    int width;
    uint32_t batch_hash;
};

// --- Hashing ---
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

// --- Reliable Pipe I/O ---
// Loops to write the exact number of bytes
ssize_t write_all(int fd, const void *buf, size_t len) {
    const char *p = (const char*)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fd, p + total, len - total);
        if (n == -1) { 
            perror("write"); 
            return -1; 
        }
        total += n;
    }
    return total;
}

// Loops to read the exact number of bytes
ssize_t read_all(int fd, void *buf, size_t len) {
    char *p = (char*)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, p + total, len - total);
        if (n == -1) { 
            perror("read"); 
            return -1; 
        }
        if (n == 0) return 0; // Connection closed
        total += n;
    }
    return total;
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
            // Initialize to 0 (black) to match serial behavior
            memset(img->image_pixels[i][j], 0, 3);
        }
    }
    return img;
}

// --- Process Functions ---

// S1: Smoothen
void S1_process(const struct image_t *input_image, const int s1s2[])
{
    int height = input_image->height;
    int width = input_image->width;
    size_t max_batch_bytes = (size_t)CHUNK_ROWS_N * width * 3;
    uint8_t* s1_flat_buffer = new uint8_t[max_batch_bytes];

    RowBatchHeader header;

    for (int run = 0; run < NUM_RUNS; run++) {
        for (int i = 0; i < height; i += CHUNK_ROWS_N) {
            int num_rows_in_batch = min(CHUNK_ROWS_N, height - i);

            // S1 Logic (produces black borders)
            for (int k = 0; k < num_rows_in_batch; k++) {
                int current_row = i + k;
                for (int j = 0; j < width; j++) {
                    uint8_t* s1_pixel = &s1_flat_buffer[(k * width + j) * 3];
                    
                    if (current_row == 0 || current_row == height - 1 || j == 0 || j == width - 1) {
                        memset(s1_pixel, 0, 3);
                    } else {
                        for (int c = 0; c < 3; c++) {
                            uint8_t result = 0; // Use 'buggy' serial logic
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

            // Prep header
            header.run_id = run;
            header.start_row = i;
            header.num_rows = num_rows_in_batch;
            header.width = width;
            header.batch_hash = calculate_batch_hash(s1_flat_buffer, num_rows_in_batch, width);

            // Send header and data
            if (write_all(s1s2[1], &header, sizeof(RowBatchHeader)) == -1) exit(EXIT_FAILURE);
            if (write_all(s1s2[1], s1_flat_buffer, num_rows_in_batch * width * 3) == -1) exit(EXIT_FAILURE);
        }
    }
    delete[] s1_flat_buffer;
}

// S2: Find Details
void S2_process(const struct image_t *input_image, const int s1s2[], const int s2s3[])
{
    int height = input_image->height;
    int width = input_image->width;
    size_t max_batch_bytes = (size_t)CHUNK_ROWS_N * width * 3;
    uint8_t* s1_flat_buffer = new uint8_t[max_batch_bytes];
    uint8_t* s2_flat_buffer = new uint8_t[max_batch_bytes];

    RowBatchHeader header;
    long long total_rows_to_process = (long long)NUM_RUNS * height;
    long long rows_processed = 0;
    long long s2_expected_run = 0;
    long long s2_expected_row = 0;

    while (rows_processed < total_rows_to_process) {
        // Read header
        if (read_all(s1s2[0], &header, sizeof(RowBatchHeader)) <= 0) break;
        
        // Read data
        size_t data_size = (size_t)header.num_rows * header.width * 3;
        if (read_all(s1s2[0], s1_flat_buffer, data_size) <= 0) break;

        // Verify order
        if (header.run_id != s2_expected_run || header.start_row != s2_expected_row) {
            cerr << "[FATAL] S1->S2 ORDER MISMATCH!" << endl; exit(EXIT_FAILURE);
        }
        s2_expected_row += header.num_rows;
        if (s2_expected_row >= height) { s2_expected_row = 0; s2_expected_run++; }

        // Verify hash
        uint32_t received_hash = calculate_batch_hash(s1_flat_buffer, header.num_rows, header.width);
        if (received_hash != header.batch_hash) {
            cerr << "[FATAL] S1->S2 HASH MISMATCH!" << endl; exit(EXIT_FAILURE);
        }

        // S2 Logic (produces black borders)
        for (int k = 0; k < header.num_rows; k++) {
            int current_row = header.start_row + k;
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

        // Update header for S3
        header.batch_hash = calculate_batch_hash(s2_flat_buffer, header.num_rows, header.width);
        
        // Send to S3
        if (write_all(s2s3[1], &header, sizeof(RowBatchHeader)) == -1) exit(EXIT_FAILURE);
        if (write_all(s2s3[1], s2_flat_buffer, data_size) == -1) exit(EXIT_FAILURE);

        rows_processed += header.num_rows;
    }

    delete[] s1_flat_buffer;
    delete[] s2_flat_buffer;
}

// S3: Sharpen
void S3_process(struct image_t *input_image, struct image_t *final_image, const int s2s3[])
{
    int height = input_image->height;
    int width = input_image->width;
    size_t max_batch_bytes = (size_t)CHUNK_ROWS_N * width * 3;
    uint8_t* s2_flat_buffer = new uint8_t[max_batch_bytes];

    RowBatchHeader header;
    long long total_rows_to_process = (long long)NUM_RUNS * height;
    long long rows_processed = 0;
    long long s3_expected_run = 0;
    long long s3_expected_row = 0;

    while (rows_processed < total_rows_to_process) {
        // Read header
        if (read_all(s2s3[0], &header, sizeof(RowBatchHeader)) <= 0) break;
        
        // Read data
        size_t data_size = (size_t)header.num_rows * header.width * 3;
        if (read_all(s2s3[0], s2_flat_buffer, data_size) <= 0) break;

        // Verify order
        if (header.run_id != s3_expected_run || header.start_row != s3_expected_row) {
            cerr << "[FATAL] S2->S3 ORDER MISMATCH!" << endl; exit(EXIT_FAILURE);
        }
        s3_expected_row += header.num_rows;
        if (s3_expected_row >= height) { s3_expected_row = 0; s3_expected_run++; }

        // Verify hash
        uint32_t received_hash = calculate_batch_hash(s2_flat_buffer, header.num_rows, header.width);
        if (received_hash != header.batch_hash) {
            cerr << "[FATAL] S2->S3 HASH MISMATCH!" << endl; exit(EXIT_FAILURE);
        }

        // S3 Logic
        if (header.run_id == NUM_RUNS - 1) { // Only write data for the final run
            for (int k = 0; k < header.num_rows; k++) {
                int current_row = header.start_row + k;
                
                // Only process inner pixels (borders are already 0 in final_image)
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
        rows_processed += header.num_rows;
    }
    cout << "S3: Verification complete. " << s3_expected_run << " runs processed." << endl;
    delete[] s2_flat_buffer;
}

// --- Main ---
int main(int argc, char **argv)
{
    if(argc != 3)
    {
        cout << "usage: ./part2_1.out <path-to-original-image> <path-to-transformed-image>\n";
        exit(0);
    }

    // Read Image
    auto start_read = chrono::steady_clock::now();
    struct image_t *input_image = read_ppm_file(argv[1]);
    auto stop_read = chrono::steady_clock::now();
    chrono::duration<double> read_time = stop_read-start_read;
    cout << "Image read time: " << fixed << setprecision(6) << read_time.count() << "s" << endl;
    cout << "Starting " << NUM_RUNS << " serial runs..." << endl;

    struct image_t* final_image = create_empty_image(input_image->height, input_image->width);
    
    int s1s2[2], s2s3[2];
    if(pipe(s1s2) < 0) { perror("pipe s1s2"); exit(EXIT_FAILURE); }
    if(pipe(s2s3) < 0) { perror("pipe s2s3"); exit(EXIT_FAILURE); }
    
    auto start_total = chrono::steady_clock::now();

    pid_t s1_pid = fork();
    if(s1_pid < 0) { perror("fork s1"); exit(EXIT_FAILURE); }
    
    if(s1_pid == 0){ 
        // S1 Process
        close(s1s2[0]); // Close read end
        close(s2s3[0]); // Not used
        close(s2s3[1]); // Not used
        S1_process(input_image, s1s2);
        close(s1s2[1]); // Close write end
        exit(0);
    } 
        
    pid_t s2_pid = fork();
    if(s2_pid < 0) { perror("fork s2"); exit(EXIT_FAILURE); }

    if(s2_pid == 0){ 
        // S2 Process
        close(s1s2[1]); // Close write end
        close(s2s3[0]); // Close read end
        S2_process(input_image, s1s2, s2s3);
        close(s1s2[0]); // Close read end
        close(s2s3[1]); // Close write end
        exit(0);
    } 
        
    // S3 Process (Parent)
    close(s1s2[0]); // Not used
    close(s1s2[1]); // Not used
    close(s2s3[1]); // Close write end
    S3_process(input_image, final_image, s2s3);
    close(s2s3[0]); // Close read end

    // Wait for children to finish
    waitpid(s1_pid, NULL, 0);
    waitpid(s2_pid, NULL, 0);

    auto stop_total = chrono::steady_clock::now();
    chrono::duration<double> total_time = stop_total - start_total;

    // Write file
    auto start_write = chrono::steady_clock::now();
    write_ppm_file(argv[2], final_image);
    auto stop_write = chrono::steady_clock::now();
    chrono::duration<double> write_time = stop_write-start_write;
    
    // Cleanup
    delete_image(input_image);
    delete_image(final_image);

    // Report
    cout << "\n--- Timing Report (Pipes) ---" << endl;
    cout << "Read time:     " << setw(12) << read_time.count() << " s" << endl;
    cout << "Parallel time: " << setw(12) << total_time.count() << " s (for " << NUM_RUNS << " runs)" << endl;
    cout << "Write time:    " << setw(12) << write_time.count() << " s" << endl;
    cout << "Avg per run:   " << setw(12) << total_time.count() / NUM_RUNS * 1000 << " ms" << endl;

    return 0;
}