#!/bin/bash

# Go home
cd /root/hw4

# Copy
/mnt/hgfs/HW/234123-hw4/scripts/update_files.sh

# Compile and allow usage
make clean
make
gcc -Wall test_snake_roy.c -o test_snake_roy
chmod a+x ./test_snake_roy

# Run
rmmod snake
i=0
rm -f /dev/snake*
insmod ./snake.o max_games=50
let i=0
while [ $i -lt 50 ]; do
	mknod /dev/snake$i c 254 $i
	let i=i+1
done;
./test_snake_roy

