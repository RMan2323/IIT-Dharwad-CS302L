#include <iostream>
#include "libppm.h"
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <unistd.h>

using namespace std;

void S1_smoothen(struct image_t *input_image, int s1s2[])
{
	uint8_t pixel[3];
	
	//int count = 0;
	//while(count < 1000){
	//count++;
	for(int i = 0; i < (input_image->height)-1; i++)
	{
		for(int j = 0; j < (input_image->width)-1; j++)
		{
			for(int k = 0; k < 3; k++)
			{
				if(i == 0 || i == input_image->height-1 || j == 0 || j == input_image->width-1){
					pixel[k] = input_image->image_pixels[i][j][k];
					continue;
				}
				
				uint8_t result = 0;
				for(int l = -1; l < 2; l++)
				{
					for(int m = -1; m < 2; m++)
					{
						result += (input_image->image_pixels[i-l][j-m][k])/9;
					}
				}
				pixel[k] = result;
			}
			if(write(s1s2[1], pixel, 3*sizeof(uint8_t)) < 0){  //give the [i,j]th pixel value to next process
				perror("s1 pixel write");
				exit(EXIT_FAILURE);
			} else cout << "s1 wrote\n";
		}
	}
	//}

	
}

void S2_find_details(struct image_t *input_image, int s1s2[], int s2s3[])
{
	uint8_t incomingPixel[3], outgoingPixel[3];
	
	//int count = 0;
	//while(count < 1000){
	//count++;
	for(int i = 0; i < (input_image->height)-1; i++)
	{
		for(int j = 0; j < (input_image->width)-1; j++)
		{
			if(read(s1s2[0], incomingPixel, 3*sizeof(uint8_t)) < 0){  //take the [i,j]th pixel value from the prev. process
				perror("s2 pixel read");
				exit(EXIT_FAILURE);
			} else cout << "s2 read\n";
			for(int k = 0; k < 3; k++)
			{
				short res = input_image->image_pixels[i][j][k] - incomingPixel[3];
				if(res < 0) outgoingPixel[k] = 0;
				else outgoingPixel[k] = res;
			}
			if(write(s2s3[1], outgoingPixel, 3*sizeof(uint8_t)) < 0){  //give the [i,j]th pixel value to next process
				perror("s2 pixel write");
				exit(EXIT_FAILURE);
			} else cout << "s2 wrote\n";
		}
	}
	//}
}

struct image_t* S3_sharpen(struct image_t *input_image, int s2s3[])
{
	uint8_t pixel[3];
	
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
	
	//int count = 0;
	//while(count < 1000){
	//count++;
	for(int i = 0; i < (finalImage->height)-1; i++)
	{
		for(int j = 0; j < (finalImage->width)-1; j++)
		{
			if(read(s2s3[0], pixel, 3*sizeof(uint8_t)) < 0){  //take the [i,j]th pixel value from the prev. process
				perror("s3 pixel read");
				exit(EXIT_FAILURE);
			} else cout << "s3 read\n";
			
			for(int k = 0; k < 3; k++)
			{
				short res = input_image->image_pixels[i][j][k] + pixel[k];
				if(res > 255) finalImage->image_pixels[i][j][k] = 255;
				else finalImage->image_pixels[i][j][k] = res;
			}
		}
	}
	//}
	return finalImage;
}

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		cout << "usage: ./a.out <path-to-original-image> <path-to-transformed-image>\n";
		exit(0);
	}

	int s1s2[2], s2s3[2];
	if(pipe(s1s2) < 0){
		perror("piping s1s2");
		exit(EXIT_FAILURE);
	}
	if(pipe(s2s3) < 0){
		perror("piping s2s3");
		exit(EXIT_FAILURE);
	}
	
	struct image_t *input_image = read_ppm_file(argv[1]);  //read image

	pid_t s2pid = fork();
	if(s2pid < 0){
		perror("forking s2");
		exit(EXIT_FAILURE);
	}
	if(s2pid == 0){  //s1 runs here
		close(s1s2[0]);  //close unused read end of s1s2
		S1_smoothen(input_image, s1s2);  //smoothen
		close(s1s2[1]);  //close write end of s1s2
	} else{  //s2 and s3 come here
		pid_t s3pid = fork();
		if(s3pid < 0){
			perror("forking s3");
			exit(EXIT_FAILURE);
		}
		if(s3pid == 0){  //s2 runs here
			close(s1s2[1]);  //close unused write end of s1s2
			close(s2s3[0]);  //close unused read end of s2s3
			S2_find_details(input_image, s1s2, s2s3);  //get details
			close(s1s2[1]);  //close read end of s1s2
			close(s2s3[1]);  //close write end of s2s3
		} else{  //s3 runs here
			close(s2s3[1]);  //close unused write end of s2s3
			struct image_t *sharpened_image = S3_sharpen(input_image, s2s3);  //sharpen
			close(s2s3[0]);  //close read end of s2s3
			write_ppm_file(argv[2], sharpened_image);  //write file
		}
	}
}
