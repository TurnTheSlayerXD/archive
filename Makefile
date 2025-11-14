all: archive.exe


archive.exe: main.o
	gcc -o archive.exe main.o -lm

main.o: main.c arch_instance.h encoding_decoding.h hamming.h helper.h
	gcc -o main.o -c main.c -std=c2x -O2 -Wall -Wextra -Wpedantic -ggdb3 -g