#!/bin/bash

# rm ./test.ham
# rm ./testdir/*

rm -rf ./_temp
a=$(make && ./hamarc --create -f arch.ham main.c && ./hamarc --create -f barch.ham Makefile && ./hamarc -f carch.ham -A arch.ham barch.ham)





