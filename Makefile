all: bitmap.o convertisseur.exe

bitmap.o: bitmap.c bitmap.h
	gcc -c bitmap.c -o bitmap.o

convertisseur.exe: main.c bitmap.o
	gcc main.c bitmap.o -o convertisseur.exe -pthread -O2 -ftree-vectorize -fopt-info -mavx2 -fopt-info-vec-all

c:
	rm *.exe *.o
	rm -r out

r:
	./convertisseur.exe ./bmps ./out sharpen