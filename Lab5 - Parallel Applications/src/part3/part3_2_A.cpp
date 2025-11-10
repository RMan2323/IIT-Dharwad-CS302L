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
ssize_t send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char*)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(fd, p + total, len - total, 0);
        if (n == -1) { 
            perror("send"); 
            return -1; 
        }
        total += n;
    }
    return total;
}

// --- Socket Setup ---
int setup_server_socket(const char *port) {
    struct addrinfo hints, *res, *rp;
    int sfd, connfd, s;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP socket
    hints.ai_flags = AI_PASSIVE;     // For wildcard IP address

    s = getaddrinfo(NULL, port, &hints, &res);
    if (s != 0) {
        cerr << "getaddrinfo: " << gai_strerror(s) << endl;
        return -1;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;

        int yes = 1;
        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            perror("setsockopt");
            close(sfd);
            continue;
        }

        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break; // Success

        close(sfd);
    }

    freeaddrinfo(res);

    if (rp == NULL) {
        cerr << "Could not bind to any address" << endl;
        return -1;
    }

    if (listen(sfd, 1) == -1) {
        perror("listen");
        return -1;
    }

    cout << "Server: waiting for connection on port " << port << "..." << endl;
    connfd = accept(sfd, NULL, NULL);
    if (connfd == -1) {
        perror("accept");
        return -1;
    }
    
    close(sfd); // We only accept one client
    cout << "Server: client connected." << endl;
    return connfd;
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

// --- Main Program ---
int main(int argc, char **argv) {
    if (argc != 3) {
        cout << "Usage: ./part3_2_A <path-to-input-image> <port>\n";
        return 1;
    }

    string input_path = argv[1];
    const char *port = argv[2];

    // --- Declarations moved up ---
    auto start_read = chrono::steady_clock::now();
    chrono::duration<double> read_time{0};
    auto start_total = chrono::steady_clock::now();
    chrono::steady_clock::time_point stop_total;
    chrono::duration<double> total_time{0};

    // Read input image
    start_read = chrono::steady_clock::now();
    g_input_image = read_ppm_file(const_cast<char*>(input_path.c_str()));
    auto stop_read = chrono::steady_clock::now();
    read_time = stop_read - start_read;

    if (g_input_image == nullptr) {
        cerr << "Failed to read input image: " << input_path << endl;
        return 1;
    }
    cout << "Server: Image read. " << g_input_image->width << "x" << g_input_image->height << endl;

    int height = g_input_image->height;
    int width = g_input_image->width;

    // Setup network connection
    int connfd = setup_server_socket(port);
    if (connfd == -1) {
        cerr << "Server: Failed to setup socket." << endl;
        return 1;
    }

    // Allocate flat buffer
    int max_batch_pixels = CHUNK_ROWS_N * width;
    uint8_t* s1_flat_buffer = new uint8_t[max_batch_pixels * 3];

    PacketHeader header;
    start_total = chrono::steady_clock::now();

    for (int run = 0; run < NUM_RUNS; run++) {
        for (int i = 0; i < height; i += CHUNK_ROWS_N) {
            int num_rows_in_batch = min(CHUNK_ROWS_N, height - i);

            // --- S1 (Smoothen) Logic ---
            for (int k = 0; k < num_rows_in_batch; k++) {
                int current_row = i + k;
                for (int j = 0; j < width; j++) {
                    uint8_t* s1_pixel = &s1_flat_buffer[(k * width + j) * 3];
                    
                    // --- FIX: Set borders to 0 ---
                    if (current_row == 0 || current_row == height - 1 || j == 0 || j == width - 1) {
                        memset(s1_pixel, 0, 3);
                    } else {
                        // Inner pixel logic
                        for (int c = 0; c < 3; c++) {
                            uint8_t result = 0;
                            for (int l = -1; l < 2; l++) {
                                for (int m = -1; m < 2; m++) {
                                    result += g_input_image->image_pixels[current_row - l][j - m][c] / 9;
                                }
                            }
                            s1_pixel[c] = result;
                        }
                    }
                }
            }

            // --- Send S1 Data ---
            header.run_id = run;
            header.start_row = i;
            header.num_rows = num_rows_in_batch;
            header.width = width;
            header.batch_hash = calculate_batch_hash(s1_flat_buffer, i, num_rows_in_batch, width, height);

            // 1. Send header
            if (send_all(connfd, &header, sizeof(PacketHeader)) == -1) {
                cerr << "Server: Failed to send header" << endl;
                goto cleanup;
            }

            // 2. Send pixel data
            size_t data_size = (size_t)num_rows_in_batch * width * 3;
            if (send_all(connfd, s1_flat_buffer, data_size) == -1) {
                cerr << "Server: Failed to send pixel data" << endl;
                goto cleanup;
            }
        } // end batch loop
    } // end run loop

    stop_total = chrono::steady_clock::now();
    total_time = stop_total - start_total;
    
    cout << "\n--- Timing Report (Server) ---" << endl;
    cout << "Read time:     " << setw(12) << read_time.count() << " s" << endl;
    cout << "Server time:   " << setw(12) << total_time.count() << " s (for " << NUM_RUNS << " runs)" << endl;
    cout << "Avg per run:   " << setw(12) << total_time.count() / NUM_RUNS * 1000 << " ms" << endl;

cleanup:
    // Clean up
    close(connfd);
    delete_image(g_input_image);
    delete[] s1_flat_buffer;
    return 0;
}