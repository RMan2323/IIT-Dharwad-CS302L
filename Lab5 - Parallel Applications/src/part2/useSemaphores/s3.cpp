#include <iostream>
#include "libppm.h"
#include <cstdint>
#include <chrono>
#include <iomanip>

using namespace std;

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
	return finalImage;
}

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		cout << "usage: ./a.out <path-to-original-image> <path-to-transformed-image>\n\n";
		exit(0);
	}

	struct image_t *input_image = read_ppm_file(argv[1]);

	// struct image_t *smoothened_image = S1_smoothen(input_image);

	// struct image_t *details_image = S2_find_details(input_image, smoothened_image);

	struct image_t *sharpened_image = S3_sharpen(input_image, details_image);

	write_ppm_file(argv[2], sharpened_image);
		
	return 0;
}