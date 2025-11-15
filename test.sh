#!/bin/bash

# rm ./test.ham
# rm ./testdir/*

make -B && ./archive.exe --create --file=./archive.ham ./testdir/0_test.txt ./testdir/1_main.c ./testdir/2_test.c




