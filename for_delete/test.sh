#!/bin/bash

# rm ./test.ham
# rm ./testdir/*

make && ./hamarc --create -f test_arch.ham && ./hamarc -f test_arch.ham -a ./Makefile && ./hamarc -f test_arch.ham -a ./Makefile ./main.c




