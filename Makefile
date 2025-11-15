all: ham.exe

INCLUDE=./include/

ham.exe: main.o
	gcc -o ham.exe main.o -lm

main.o: main.c $(INCLUDE)arch_instance.h $(INCLUDE)encoding_decoding.h $(INCLUDE)hamming.h $(INCLUDE)helper.h
	gcc -o main.o -c main.c -std=c2x -O2 -Wall -Wextra -Wpedantic -ggdb3 -g -I $(INCLUDE)