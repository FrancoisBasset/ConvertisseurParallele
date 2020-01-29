all: bitmap.o convertisseur.exe

bitmap.o: bitmap/bitmap.c bitmap/bitmap.h
	gcc -c bitmap/bitmap.c -o bitmap/bitmap.o

convertisseur.exe: main.c bitmap/bitmap.o
	gcc main.c bitmap/bitmap.o -o convertisseur.exe -pthread -O2 -ftree-vectorize -fopt-info -mavx2 -fopt-info-vec-all

c:
	rm *.exe
	rm bitmap/bitmap.o
	rm -r out

edge:
	./convertisseur.exe bmps out edge

boxblur:
	./convertisseur.exe bmps out boxblur

sharpen:
	./convertisseur.exe bmps out sharpen