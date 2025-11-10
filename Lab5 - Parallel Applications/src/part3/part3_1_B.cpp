#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <vector>
#include "libppm.h"

// Socket programming headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

using namespace std;

// --- Constants ---
const int CHUNK_ROWS_N = 10; // Must match server's
const int NUM_RUNS = 1000;

// --- Data Structures ---
struct PacketHeader {
    int run_id;
    int start_row;
    int num_rows;
    int width;
    uint32_t batch_hash;
};

// --- Global Image ---
image_t* g_input_image = nullptr;

// --- Hashing Function ---
uint32_t hsh(const uint8_t v[3]){
    uint32_t x = (v[0] << 16) | (v[1] << 8) | v[2];
    x ^= (x >> 13);
    x *= 0x85ebca6b;
    x ^= (x >> 16);
    return x;
}

// --- FIX: This function now skips borders, matching part2_1.cpp ---
uint32_t calculate_batch_hash(const uint8_t* buffer, int start_row, int num_rows, int width, int img_height) {
    uint32_t combined_hash = 0;
    for (int i = 0; i < num_rows; i++) {
        int current_row = start_row + i;
        if (current_row == 0 || current_row == img_height - 1) continue; // Skip border rows

        for (int j = 0; j < width; j++) {
            if (j == 0 || j == width - 1) continue; // Skip border cols

            combined_hash ^= hsh(&buffer[(i * width + j) * 3]);
        }
    }
    return combined_hash;
}

// --- Reliable Socket I/O ---
// Loops to receive the exact number of bytes
ssize_t recv_all(int fd, void *buf, size_t len) {
    char *p = (char*)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = recv(fd, p + total, len - total, 0);
        if (n == -1) { // Error
            perror("recv"); 
            return -1; 
        }
        if (n == 0) { // Connection closed
            return total; // Return bytes read so far, will likely be 0
        }
        total += n;
    }
    return total;
}

// --- Socket Setup ---
int setup_client_socket(const char *host, const char *port) {
    struct addrinfo hints, *res, *rp;
    int sfd, s;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; // TCP

    s = getaddrinfo(host, port, &hints, &res);
    if (s != 0) {
        cerr << "getaddrinfo: " << gai_strerror(s) << endl;
        return -1;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break; // Success

        close(sfd);
    }

    freeaddrinfo(res);

    if (rp == NULL) {
        cerr << "Client: Could not connect to " << host << ":" << port << endl;
        return -1;
    }

    cout << "Client: Connected to " << host << ":" << port << endl;
    return sfd;
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
        }
    }
    return img;
}

// --- Main Program ---
int main(int argc, char **argv) {
    if (argc != 5) {
        cout << "Usage: ./part3_1_B <server-ip> <port> <path-to-input-image> <path-to-output-image>\n";
        return 1;
    }

    const char *host = argv[1];
    const char *port = argv[2];
    string input_path = argv[3];
    string output_path = argv[4];

    // Read input image (needed for S3)
    g_input_image = read_ppm_file(const_cast<char*>(input_path.c_str()));
    if (g_input_image == nullptr) {
        cerr << "Failed to read input image: " << input_path << endl;
        return 1;
    }
    cout << "Client: Image read. " << g_input_image->width << "x" << g_input_image->height << endl;

    int height = g_input_image->height;
    int width = g_input_image->width;

    // Create final image to be filled
    image_t* final_image = create_empty_image(height, width);

    // Setup network connection
    int sfd = setup_client_socket(host, port);
    if (sfd == -1) {
        return 1;
    }

    // Allocate flat buffer for receiving
    int max_batch_pixels = CHUNK_ROWS_N * width;
    uint8_t* s2_flat_buffer = new uint8_t[max_batch_pixels * 3];

    PacketHeader header;
    long long s3_expected_run = 0;
    long long s3_expected_row = 0;
    long long total_batches_received = 0;
    long long total_batches_expected = (long long)ceil((double)height / CHUNK_ROWS_N) * NUM_RUNS;

    auto start_total = chrono::steady_clock::now();

    // --- FIX: Declare timing variables before goto-able scope ---
    auto stop_total = start_total;
    chrono::duration<double> total_time(0.0);

    while (total_batches_received < total_batches_expected) {
        // 1. Receive header
        if (recv_all(sfd, &header, sizeof(PacketHeader)) != sizeof(PacketHeader)) {
            cerr << "Client: Failed to receive header or server disconnected." << endl;
            break;
        }

        // 2. Receive pixel data
        size_t data_size = (size_t)header.num_rows * header.width * 3;
        if (recv_all(sfd, s2_flat_buffer, data_size) != data_size) {
            cerr << "Client: Failed to receive pixel data." << endl;
            break;
        }

        // --- Verification Step 1: Check Order ---
        if (header.run_id != s3_expected_run || header.start_row != s3_expected_row) {
            cerr << "[FATAL] S2->S3 ORDER MISMATCH!" << endl;
            cerr << "  Expected: run=" << s3_expected_run << " row=" << s3_expected_row << endl;
            cerr << "  Actual:   run=" << header.run_id << " row=" << header.start_row << endl;
            goto cleanup;
        }
        s3_expected_row += header.num_rows;
        if (s3_expected_row >= height) {
            s3_expected_row = 0;
            s3_expected_run++;
        }

        // --- Verification Step 2: Check Hash ---
        // --- FIX: Pass correct arguments to new hash function ---
        uint32_t received_hash = calculate_batch_hash(s2_flat_buffer, header.start_row, header.num_rows, header.width, height);
        if (received_hash != header.batch_hash) {
            cerr << "[FATAL] S2->S3 HASH MISMATCH!" << endl;
            cerr << "  Sent:   " << header.batch_hash << endl;
            cerr << "  Recv'd: " << received_hash << endl;
            goto cleanup;
        }

        // --- S3 (Sharpen) Logic ---
        if (header.run_id == NUM_RUNS - 1) { // Only write data for the final run
            for (int k = 0; k < header.num_rows; k++) {
                int current_row = header.start_row + k;
                for (int j = 0; j < width; j++) {
                    uint8_t* s2_pixel = &s2_flat_buffer[(k * width + j) * 3];
                    if (current_row > 0 && current_row < height - 1 && j > 0 && j < width - 1) {
                        for (int c = 0; c < 3; c++) {
                            short res = (short)g_input_image->image_pixels[current_row][j][c] + (short)s2_pixel[c];
                            final_image->image_pixels[current_row][j][c] = (res > 255) ? 255 : (uint8_t)res;
                        }
                    } else {
                        // FIX: Explicitly set border pixels to 0
                        memset(final_image->image_pixels[current_row][j], 0, 3);
                    }
                }
            }
        }
        total_batches_received++;
    }

    stop_total = chrono::steady_clock::now();
    total_time = stop_total - start_total;
    
    cout << "Client: All data received. Total time: " << total_time.count() << "s" << endl;

    // Write final image
    cout << "Client: Writing final image to " << output_path << endl;
    write_ppm_file(const_cast<char*>(output_path.c_str()), final_image);

    cout << "\n--- Verification ---" << endl;
    if (s3_expected_run == NUM_RUNS) {
        cout << "SUCCESS: Order check passed for all " << NUM_RUNS << " runs." << endl;
    } else {
        cout << "FAILURE: Order check incomplete. Saw " << s3_expected_run << " runs." << endl;
    }

cleanup:
    // Clean up
    close(sfd);
    delete_image(g_input_image);
    delete_image(final_image);
    delete[] s2_flat_buffer;
    return 0;
}