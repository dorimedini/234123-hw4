#!/bin/bash

# Go home
cd /root/hw4

# Copy
/mnt/hgfs/HW/234123-hw4/scripts/update_files.sh

# Compile and allow usage
make clean
make
gcc -Wall my_test.c -o my_test
chmod a+x ./my_test

# Run
insmod ./snake.o max_games=50
mknod ./3 c 254 3
./my_test
rm -f ./3
rmmod snake

