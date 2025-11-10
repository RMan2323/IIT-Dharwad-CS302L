#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include "libppm.h" // Must be in the same directory

// Socket programming headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h> // For exit, EXIT_FAILURE

using namespace std;

// --- Constants ---
const int CHUNK_ROWS_N = 10; // Batch size (N rows)
const int NUM_RUNS = 1000;

// --- Data Structures ---
// This header is received before each batch of pixel data
struct PacketHeader {
    int run_id;
    int start_row;
    int num_rows;
    int width;
    uint32_t batch_hash;
};

// --- Global Image ---
image_t* g_input_image = nullptr; // Original image, needed for S3

// --- Hashing Function ---
uint32_t hsh(const uint8_t v[3]){
    uint32_t x = (v[0] << 16) | (v[1] << 8) | v[2];
    x ^= (x >> 13);
    x *= 0x85ebca6b;
    x ^= (x >> 16);
    return x;
}

// Calculates hash for a flat buffer of pixel data
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
        if (n == -1) { 
            perror("recv"); 
            return -1; 
        }
        if (n == 0) return 0; // Connection closed
        total += n;
    }
    return total;
}

// --- Socket Setup ---
int setup_client_socket(const char *server_ip, const char *port) {
    struct addrinfo hints, *res, *rp;
    int sfd, s;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP socket

    s = getaddrinfo(server_ip, port, &hints, &res);
    if (s != 0) {
        cerr << "getaddrinfo: " << gai_strerror(s) << endl;
        return -1;
    }

    // Try each address until we successfully connect
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break; // Success

        close(sfd);
    }

    freeaddrinfo(res); // No longer needed

    if (rp == NULL) {
        cerr << "Client: Could not connect to " << server_ip << ":" << port << endl;
        return -1;
    }

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
            // --- FIX: Initialize all pixels to 0 (black) ---
            memset(img->image_pixels[i][j], 0, 3);
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

    string server_ip = argv[1];
    const char *port = argv[2];
    string input_path = argv[3];
    string output_path = argv[4];

    // --- Declarations moved up ---
    auto start_read = chrono::steady_clock::now();
    chrono::duration<double> read_time{0};
    auto start_total = chrono::steady_clock::now();
    chrono::steady_clock::time_point stop_total; 
    chrono::duration<double> total_time{0};
    chrono::steady_clock::time_point start_write, stop_write;
    chrono::duration<double> write_time{0};

    // Read input image (needed for S3)
    start_read = chrono::steady_clock::now();
    g_input_image = read_ppm_file(const_cast<char*>(input_path.c_str()));
    auto stop_read = chrono::steady_clock::now();
    read_time = stop_read - start_read;

    if (g_input_image == nullptr) {
        cerr << "Failed to read input image: " << input_path << endl;
        return 1;
    }
    cout << "Client: Image read. " << g_input_image->width << "x" << g_input_image->height << endl;

    int height = g_input_image->height;
    int width = g_input_image->width;

    // Setup network connection
    int sfd = setup_client_socket(server_ip.c_str(), port);
    if (sfd == -1) {
        cerr << "Client: Failed to connect." << endl;
        return 1;
    }

    // Allocate final image and receive buffer
    image_t* final_image = create_empty_image(height, width); // Borders are now 0
    int max_batch_pixels = CHUNK_ROWS_N * width;
    uint8_t* s2_flat_buffer = new uint8_t[max_batch_pixels * 3];

    PacketHeader header;
    long long total_rows_to_process = (long long)NUM_RUNS * height;
    long long rows_processed = 0;
    long long s3_expected_run = 0;
    long long s3_expected_row = 0;

    cout << "Client: Connected to " << server_ip << ":" << port << endl;
    start_total = chrono::steady_clock::now(); // Start timer

    while (rows_processed < total_rows_to_process) {
        // 1. Receive header
        ssize_t n = recv_all(sfd, &header, sizeof(PacketHeader));
        if (n == 0) {
            cout << "Client: Server closed connection." << endl;
            break;
        }
        if (n == -1) {
            cerr << "Client: Failed to receive header" << endl;
            goto cleanup;
        }

        // 2. Receive pixel data
        size_t data_size = (size_t)header.num_rows * header.width * 3;
        n = recv_all(sfd, s2_flat_buffer, data_size);
        if (n == 0) {
            cout << "Client: Server closed connection." << endl;
            break;
        }
        if (n == -1) {
            cerr << "Client: Failed to receive pixel data" << endl;
            goto cleanup;
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

        // --- Verification Step 2: Check Hash (Data Integrity) ---
        uint32_t received_hash = calculate_batch_hash(s2_flat_buffer, header.start_row, header.num_rows, header.width, height);
        if (received_hash != header.batch_hash) {
            cerr << "[FATAL] S2->S3 HASH MISMATCH!" << endl;
            cerr << "  Chunk:  run=" << header.run_id << " row=" << header.start_row << endl;
            cerr << "  Sent:   " << header.batch_hash << endl;
            cerr << "  Recv'd: " << received_hash << endl;
            goto cleanup;
        }

        // --- S3 (Sharpen) Logic ---
        if (header.run_id == NUM_RUNS - 1) { // Only write data for the final run
            for (int k = 0; k < header.num_rows; k++) {
                int current_row = header.start_row + k;
                
                // --- FIX: Only process inner pixels ---
                // (We only need to write inner pixels, since final_image borders are already 0)
                if (current_row > 0 && current_row < height - 1) {
                    for (int j = 1; j < width - 1; j++) { // Skip border cols
                        uint8_t* s2_pixel = &s2_flat_buffer[(k * width + j) * 3];
                        for (int c = 0; c < 3; c++) {
                            short res = (short)g_input_image->image_pixels[current_row][j][c] + (short)s2_pixel[c];
                            final_image->image_pixels[current_row][j][c] = (res > 255) ? 255 : (uint8_t)res;
                        }
                    }
                }
            }
        }
        
        rows_processed += header.num_rows;
    }

    stop_total = chrono::steady_clock::now(); 
    total_time = stop_total - start_total;
    
    cout << "Client: All data received." << endl;

    // Write final image
    cout << "Client: Writing final image to " << output_path << endl;
    
    start_write = chrono::steady_clock::now();
    write_ppm_file(const_cast<char*>(output_path.c_str()), final_image);
    stop_write = chrono::steady_clock::now();
    write_time = stop_write - start_write;

    cout << "\n--- Timing Report (Client) ---" << endl;
    cout << "Read time:     " << setw(12) << read_time.count() << " s" << endl;
    cout << "Client time:   " << setw(12) << total_time.count() << " s (for " << NUM_RUNS << " runs)" << endl;
    cout << "Write time:    " << setw(12) << write_time.count() << " s" << endl;
    cout << "Avg per run:   " << setw(12) << total_time.count() / NUM_RUNS * 1000 << " ms" << endl;

    cout << "\n--- Verification ---" << endl;
    if (s3_expected_run == NUM_RUNS) {
        cout << "SUCCESS: Order check passed for all " << NUM_RUNS << " runs." << endl;
    } else {
        cout << "FAILURE: Order check incomplete. Client saw " << s3_expected_run << " runs." << endl;
    }

cleanup:
    // Clean up
    close(sfd);
    delete_image(g_input_image);
    delete_image(final_image);
    delete[] s2_flat_buffer;
    return 0;
}