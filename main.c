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
#include <sys/stat.h>

#define OFFSET 3 /2

typedef struct ImageToConvert {
	Image data;
	char* destination;
} ImageToConvert;

typedef struct stack_t {
	ImageToConvert **images;
	int count;
	int max;
	pthread_mutex_t lock;
	pthread_cond_t can_consume;
	pthread_cond_t can_produce;
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
	pthread_cond_init(&stack.can_produce, NULL);
	pthread_cond_init(&stack.can_consume, NULL);
	pthread_mutex_init(&stack.lock, NULL);
	stack.max = 10;
	stack.count = 0;
	stack.images = malloc(sizeof(ImageToConvert) * 10);
	srand(time(NULL));
}

char* source;
char* destination;
char* effect;

void* producer(void* file);
void* producer(void* file) {
	
	//while(1) {
		pthread_mutex_lock(&stack.lock);
		char* sourcePath = malloc(sizeof(char) * 100);
		strcpy(sourcePath, source);
		strcat(sourcePath, "/");
		strcat(sourcePath, file);

		Image img = open_bitmap(sourcePath);
		Image new;

		if (strcmp(effect, "edge") == 0) {
			edge(&img, &new);
		} else if (strcmp(effect, "boxblur") == 0) {
			boxblur(&img, &new);
		} else if (strcmp(effect, "sharpen") == 0) {
			sharpen(&img, &new);
		}
		
		int size = strlen(file) + strlen(effect) + strlen(source) + 2;
		char* d = malloc(sizeof(char) * size);
		strcpy(d, destination);
		strcat(d, "/");
		strcat(d, effect);
		strcat(d, "_");
		strcat(d, file);
		
		stack.images[stack.count] = malloc(sizeof(ImageToConvert));
		stack.images[stack.count]->data = new;
		stack.images[stack.count]->destination = d;
		stack.count++;
	//}

	pthread_mutex_unlock(&stack.lock);

	return NULL;
}

void* consumer(void* arg);
void* consumer(void* arg) {
	while(1) {
		pthread_mutex_lock(&stack.lock);
		for (int i = 0; i < stack.count; i++) {
			save_bitmap(stack.images[i]->data, stack.images[i]->destination);
		}
		pthread_cond_signal(&stack.can_produce);
		pthread_mutex_unlock(&stack.lock);

		break;
	}
	
	return NULL;
}

int main(int argc, char** argv) {
	argc--;	

	if (argc == 0) {
		printf("%s \"./in/\" \"./out/\" boxblur\n", argv[0]); 
		return 0;
	}

	if (!opendir(argv[1]) || !argv[2] || !argv[3]) {
		printf("%s \"./in/\" \"./out/\" boxblur\n", argv[0]); 
		return 0;
	}

	source = argv[1];
	destination = argv[2];
	effect = argv[3];

	if (strcmp(effect, "edge") != 0 && strcmp(effect, "boxblur") != 0 && strcmp(effect, "sharpen") != 0) {
		printf("Effets disponibles :\n- edge\n- boxblur\n- sharpen");
		return 0;
	}

	

	DIR *test = opendir(destination);

	if (!test) {
		mkdir(destination, "777");
	} else {
		struct dirent *dir;

		while ((dir = readdir(test)) != NULL) {
			//if (strstr(dir->d_name, ".bmp")) {
				char dest_filename[50];
				sprintf(dest_filename, "%s/%s", destination, dir->d_name);
				printf("%s", dest_filename);
				remove(dest_filename);
			//}
		}
	}

	/*if (exist == -1) {
		printf("1");
		mkdir(destination);
	} else {
		printf("2");
		
	}*/

	DIR *d = opendir(source);
	char* files[100];

	int count = 0;
	
	if (d) {
		struct dirent *dir;

		while ((dir = readdir(d)) != NULL) {
			char* f = malloc(sizeof(char) * (strlen(dir->d_name) + 1));
			strcpy(f, dir->d_name);

			if (strstr(dir->d_name, ".bmp")) {
				files[count] = f;
				count++;
			}
    	}
		
		closedir(d);
	}

	stack_init();	

	pthread_t threads[count + 1];

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	for(int i = 0; i < count; i++) {
		pthread_create(&threads[i], NULL, producer, files[i]);
	}

	pthread_join(threads[count - 1], NULL);
	pthread_create(&threads[count], NULL, consumer, NULL);
	pthread_join(threads[count], NULL);

	return 0;
}