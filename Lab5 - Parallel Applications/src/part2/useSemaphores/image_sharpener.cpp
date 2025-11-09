#include <iostream>
#include "libppm.h"
#include <cstdint>
#include <chrono>
#include <iomanip>

using namespace std;

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

	struct image_t *smoothened_image = S1_smoothen(input_image);

	struct image_t *details_image = S2_find_details(input_image, smoothened_image);

	struct image_t *sharpened_image = S3_sharpen(input_image, details_image);

	write_ppm_file(argv[2], sharpened_image);
		
	return 0;
}
