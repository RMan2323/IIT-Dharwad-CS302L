#include <iostream>
#include "libppm.h"
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <cstring> // For memcpy

using namespace std;

const int NUM_RUNS = 1000;

// Helper function to prevent memory leaks
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

struct image_t* S1_smoothen(struct image_t *input_image)
{
	struct image_t* smoothened_image = new struct image_t;
	
	smoothened_image->height = input_image->height;
	smoothened_image->width = input_image->width;

	smoothened_image->image_pixels = new uint8_t**[smoothened_image->height];
	for(int i = 0; i < smoothened_image->height; i++)
	{
		smoothened_image->image_pixels[i] = new uint8_t*[smoothened_image->width];

		for(int j = 0; j < smoothened_image->width; j++)
			smoothened_image->image_pixels[i][j] = new uint8_t[3];
	}
	
	// Process inner pixels
	for(int i = 1; i < (smoothened_image->height)-1; i++)
	{
		for(int j = 1; j < (smoothened_image->width)-1; j++)
		{
			for(int k = 0; k < 3; k++)
			{
				uint8_t result = 0;
				for(int l = -1; l < 2; l++)
				{
					for(int m = -1; m < 2; m++)
					{
						result += (input_image->image_pixels[i-l][j-m][k])/9;
					}
				}
				smoothened_image->image_pixels[i][j][k] = result;
			}
		}
	}	

	// Copy border pixels (matches original logic)
    int height = smoothened_image->height;
    int width = smoothened_image->width;

    // Top & Bottom rows
    for(int j = 0; j < width; j++) {
        memcpy(smoothened_image->image_pixels[0][j], input_image->image_pixels[0][j], 3);
        memcpy(smoothened_image->image_pixels[height-1][j], input_image->image_pixels[height-1][j], 3);
    }

    // Left & Right columns
    for(int i = 1; i < height - 1; i++) {
        memcpy(smoothened_image->image_pixels[i][0], input_image->image_pixels[i][0], 3);
        memcpy(smoothened_image->image_pixels[i][width-1], input_image->image_pixels[i][width-1], 3);
    }

	return smoothened_image;
}

struct image_t* S2_find_details(struct image_t *input_image, struct image_t *smoothened_image)
{
	struct image_t* detailed_image = new struct image_t;
	
	detailed_image->height = input_image->height;
	detailed_image->width = input_image->width;

	detailed_image->image_pixels = new uint8_t**[detailed_image->height];
	for(int i = 0; i < detailed_image->height; i++)
	{
		detailed_image->image_pixels[i] = new uint8_t*[detailed_image->width];

		for(int j = 0; j < detailed_image->width; j++)
			detailed_image->image_pixels[i][j] = new uint8_t[3];
	}
	
	// Process inner pixels
	for(int i = 1; i < (detailed_image->height)-1; i++)
	{
		for(int j = 1; j < (detailed_image->width)-1; j++)
		{
			for(int k = 0; k < 3; k++)
			{
				short res = input_image->image_pixels[i][j][k] - smoothened_image->image_pixels[i][j][k];
				if(res < 0) detailed_image->image_pixels[i][j][k] = 0;
				else detailed_image->image_pixels[i][j][k] = res;
			}
		}
	}
    // Note: Borders are left uninitialized (garbage), matching original logic
	return detailed_image;
}

struct image_t* S3_sharpen(struct image_t *input_image, struct image_t *details_image)
{
	struct image_t* finalImage = new struct image_t;
	
	finalImage->height = input_image->height;
	finalImage->width = input_image->width;

	finalImage->image_pixels = new uint8_t**[finalImage->height];
	for(int i = 0; i < finalImage->height; i++)
	{
		finalImage->image_pixels[i] = new uint8_t*[finalImage->width];

		for(int j = 0; j < finalImage->width; j++)
			finalImage->image_pixels[i][j] = new uint8_t[3];
	}
	
	// Process inner pixels
	for(int i = 1; i < (finalImage->height)-1; i++)
	{
		for(int j = 1; j < (finalImage->width)-1; j++)
		{
			for(int k = 0; k < 3; k++)
			{
				short res = input_image->image_pixels[i][j][k] + (details_image->image_pixels[i][j][k]);
				if(res > 255) finalImage->image_pixels[i][j][k] = 255;
				else finalImage->image_pixels[i][j][k] = res;
			}
		}
	}
    // Note: Borders are left uninitialized (garbage), matching original logic
	return finalImage;
}

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		cout << "usage: ./part1.out <path-to-original-image> <path-to-transformed-image>\n\n";
		exit(0);
	}

	auto start0 = chrono::steady_clock::now();
	struct image_t *input_image = read_ppm_file(argv[1]);
	auto stop0 = chrono::steady_clock::now();
	chrono::duration<double> read_time = stop0-start0;

    cout << "Image read time: " << fixed << setprecision(6) << read_time.count() << "s" << endl;
    cout << "Starting " << NUM_RUNS << " serial runs..." << endl;

    struct image_t *smoothened_image = nullptr;
    struct image_t *details_image = nullptr;
    struct image_t *sharpened_image = nullptr;

    auto start_total = chrono::steady_clock::now();

    for (int run = 0; run < NUM_RUNS; run++) {
        smoothened_image = S1_smoothen(input_image);
        details_image = S2_find_details(input_image, smoothened_image);
        sharpened_image = S3_sharpen(input_image, details_image);

        // Clean up intermediate images
        delete_image(smoothened_image);
        delete_image(details_image);

        // Only keep the *last* sharpened image
        if (run < NUM_RUNS - 1) {
            delete_image(sharpened_image);
        }
    }

    auto stop_total = chrono::steady_clock::now();
    chrono::duration<double> total_time = stop_total - start_total;

	auto start4 = chrono::steady_clock::now();
	write_ppm_file(argv[2], sharpened_image);
	auto stop4 = chrono::steady_clock::now();
	chrono::duration<double> write_time = stop4-start4;
	
    // Final cleanup
    delete_image(input_image);
    delete_image(sharpened_image);

	// Print final timing report
    cout << "\n--- Timing Report (Serial) ---" << endl;
    cout << "Read time:     " << setw(12) << read_time.count() << " s" << endl;
    cout << "Serial time:   " << setw(12) << total_time.count() << " s (for " << NUM_RUNS << " runs)" << endl;
    cout << "Write time:    " << setw(12) << write_time.count() << " s" << endl;
    cout << "Avg per run:   " << setw(12) << total_time.count() / NUM_RUNS * 1000 << " ms" << endl;
		
	return 0;
}