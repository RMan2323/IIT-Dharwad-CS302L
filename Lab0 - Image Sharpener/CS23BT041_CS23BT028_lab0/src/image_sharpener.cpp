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

	int i = 0;
	for(int j = 0; j < (smoothened_image->width); j++)
	{
		for(int k = 0; k < 3; k++)
		{
			smoothened_image->image_pixels[i][j][k] = (input_image->image_pixels[i][j][k]);
		}
	}
	i = (smoothened_image->height) - 1;
	for(int j = 0; j < (smoothened_image->width); j++)
	{
		for(int k = 0; k < 3; k++)
		{
			smoothened_image->image_pixels[i][j][k] = (input_image->image_pixels[i][j][k]);
		}
	}

	int j = 0;
	for(int i = 0; i < (smoothened_image->height); i++)
	{
		for(int k = 0; k < 3; k++)
		{
			smoothened_image->image_pixels[i][j][k] = (input_image->image_pixels[i][j][k]);
		}
	}
	j = (smoothened_image->width) - 1;
	for(int i = 0; i < (smoothened_image->height); i++)
	{
		for(int k = 0; k < 3; k++)
		{
			smoothened_image->image_pixels[i][j][k] = (input_image->image_pixels[i][j][k]);
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

	auto start0 = chrono::steady_clock::now();
	struct image_t *input_image = read_ppm_file(argv[1]);
	auto stop0 = chrono::steady_clock::now();
	// cout << "Image dimensions: " << input_image->width << "x" << input_image->height << "\n";
	chrono::duration<double> t0 = stop0-start0;

	auto start1 = chrono::steady_clock::now();
	struct image_t *smoothened_image = S1_smoothen(input_image);
	auto stop1 = chrono::steady_clock::now();

	chrono::duration<double> t1 = stop1-start1;
	
	auto start2 = chrono::steady_clock::now();
	struct image_t *details_image = S2_find_details(input_image, smoothened_image);
	auto stop2 = chrono::steady_clock::now();

	chrono::duration<double> t2 = stop2-start2;
	
	auto start3 = chrono::steady_clock::now();
	struct image_t *sharpened_image = S3_sharpen(input_image, details_image);
	auto stop3 = chrono::steady_clock::now();

	chrono::duration<double> t3 = stop3-start3;
	
	// write_ppm_file(argv[2], input_image);
	// write_ppm_file(argv[2], smoothened_image);
	// write_ppm_file(argv[2], details_image);

	auto start4 = chrono::steady_clock::now();
	write_ppm_file(argv[2], sharpened_image);
	auto stop4 = chrono::steady_clock::now();

	chrono::duration<double> t4 = stop4-start4;
	
	// cout << t0.count() << "\t" << t1.count() << "\t" << t2.count() << "\t" << t3.count() << "\t" << t4.count() << "\t" << t0.count()+t1.count()+t2.count()+t3.count()+t4.count() << "\n";

	cout << fixed << setprecision(6);
	cout
		<< setw(12) << t0.count()
    	<< setw(12) << t1.count()
    	<< setw(12) << t2.count()
    	<< setw(12) << t3.count()
    	<< setw(12) << t4.count()
    	<< setw(16) << (t0.count() + t1.count() + t2.count() + t3.count() + t4.count())
    	<< "\n";
		
	return 0;
}
