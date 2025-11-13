all: archive.exe


archive.exe: main.o
	gcc -o archive.exe main.o -lm

main.o: main.c 
	gcc -o main.o -c main.c -std=c17 -O2 -Wall -Wextra -Wpedantic -ggdb3 -g