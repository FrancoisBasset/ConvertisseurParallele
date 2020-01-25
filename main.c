/*
	//gcc edge-detect.c bitmap.c -O2 -ftree-vectorize -fopt-info -mavx2 -fopt-info-vec-all
	//UTILISER UNIQUEMENT DES BMP 24bits
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "bitmap.h"
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <dirent.h>

#define OFFSET 3 /2

typedef struct stack_t {
	Image data[10];
	char *filename[10];
	int count;
	/*int max;
	pthread_mutex_t lock;
	pthread_cond_t can_consume;
	pthread_cond_t can_produce;*/
} Stack;

static Stack stack;

typedef struct Color_t {
	float Red;
	float Green;
	float Blue;
} Color_e;

void inline apply_convolution(Color_e* c, int a, int b, int x, int y, Image* img, const float KERNEL[3][3]) __attribute__((always_inline));
void apply_convolution(Color_e* restrict c, int a, int b, int x, int y, Image* restrict img, const float KERNEL[3][3]) {
	int xn = x + a - OFFSET;
	int yn = y + b - OFFSET;

	Pixel* p = &img->pixel_data[yn][xn];

	c->Red += ((float) p->r) * KERNEL[a][b];
	c->Green += ((float) p->g) * KERNEL[a][b];
	c->Blue += ((float) p->b) * KERNEL[a][b];
}

void apply_effect(Image* original, Image* new_i, const float KERNEL[3][3]);
void apply_effect(Image* original, Image* new_i, const float KERNEL[3][3]) {

	int w = original->bmp_header.width;
	int h = original->bmp_header.height;

	*new_i = new_image(w, h, original->bmp_header.bit_per_pixel, original->bmp_header.color_planes);

	for (int y = OFFSET; y < h - OFFSET; y++) {
		for (int x = OFFSET; x < w - OFFSET; x++) {
			Color_e c = { .Red = 0, .Green = 0, .Blue = 0};

			apply_convolution(&c, 0, 0, x, y, original, KERNEL);
			apply_convolution(&c, 0, 1, x, y, original, KERNEL);
			apply_convolution(&c, 0, 2, x, y, original, KERNEL);

			apply_convolution(&c, 1, 0, x, y, original, KERNEL);
			apply_convolution(&c, 1, 1, x, y, original, KERNEL);
			apply_convolution(&c, 1, 2, x, y, original, KERNEL);

			apply_convolution(&c, 2, 0, x, y, original, KERNEL);
			apply_convolution(&c, 2, 1, x, y, original, KERNEL);
			apply_convolution(&c, 2, 2, x, y, original, KERNEL);

			Pixel* dest = &new_i->pixel_data[y][x];
			dest->r = (uint8_t)  (c.Red <= 0 ? 0 : c.Red >= 255 ? 255 : c.Red);
			dest->g = (uint8_t) (c.Green <= 0 ? 0 : c.Green >= 255 ? 255 : c.Green);
			dest->b = (uint8_t) (c.Blue <= 0 ? 0 : c.Blue >= 255 ? 255 : c.Blue);
		}
	}
}

void edge(Image* original, Image* new_i);
void edge(Image* original, Image* new_i) {
	const float mat[3][3] = {{-1, -1, -1},
								 {-1,  8, -1},
								 {-1, -1, -1}};

	apply_effect(original, new_i, mat);
}

void boxblur(Image* original, Image* new_i);
void boxblur(Image* original, Image* new_i) {
	const float mat[3][3] = {{0.1111111111, 0.1111111111, 0.1111111111},
								 {0.1111111111, 0.1111111111, 0.1111111111},
								 {0.1111111111, 0.1111111111, 0.1111111111}};

	apply_effect(original, new_i, mat);
}

void sharpen(Image* original, Image* new_i);
void sharpen(Image* original, Image* new_i) {
	const float mat[3][3] = {{ 0, -1,  0},
								 {-1,  5, -1},
								 { 0, -1,  0}};

	apply_effect(original, new_i, mat);
}

void stack_init() {
	//pthread_cond_init(&stack.can_produce, NULL);
	//pthread_cond_init(&stack.can_consume, NULL);
	//pthread_mutex_init(&stack.lock, NULL);
	//stack.max = STACK_MAX;
	stack.count = 0;
	srand(time(NULL));
}

char* effect;

void* producer(void* file);
void* producer(void* file) {
	Image img = open_bitmap(file);
	Image new;

	if (strcmp(effect, "edge") == 0) {
		edge(&img, &new);
	} else if (strcmp(effect, "boxblur") == 0) {
		boxblur(&img, &new);
	} else if (strcmp(effect, "sharpen") == 0) {
		sharpen(&img, &new);
	}
	
	char* filename = malloc(sizeof(char) * (strlen(file) + 6));
	strcpy(filename, effect);
	strcat(filename, "_");
	strcat(filename, file);
	
	stack.filename[stack.count] = filename;
	stack.data[stack.count] = new;
	stack.count++;

	return NULL;
}

void* consumer(void* arg);
void* consumer(void* arg) {
	for (int i = 0; i < stack.count; i++) {
		save_bitmap(stack.data[i], stack.filename[i]);
	}
	
	return NULL;
}

int main(int argc, char** argv) {
	argc--;

	if (argc == 0) {
		return 0;
	}

	DIR *d = opendir(".");
	
	if (d) {
		struct dirent *dir;
		while ((dir = readdir(d)) != NULL) {
			char* f = malloc(sizeof(char) * (strlen(dir->d_name) + 1));
			strcpy(f, dir->d_name);

			if (strstr(dir->d_name, ".bmp")) {
				printf("%s\n", f);
			}
    	}
		
		closedir(d);
	}

	effect = argv[1];	

	stack_init();

	char* files[3] = { "perroquet.bmp", "tiger.bmp", "tank.bmp" };

	pthread_t threads[4];

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	for(int i = 0; i < 3; i++) {
		pthread_create(&threads[i], NULL, producer, files[i]);
	}

	pthread_join(threads[2], NULL);
	pthread_create(&threads[3], NULL, consumer, NULL);
	pthread_join(threads[3], NULL);

	return 0;
}