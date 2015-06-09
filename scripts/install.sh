#!/bin/bash

# Go to the correct directory
cd /root/hw4

# Make sure the input is OK - we expect to get N (total number of games allowed)
if [ "$#" -ne 1 ]; then
	echo "Need to provide an argument. Usage: install.sh [TOTAL_GAMES]"; exit 1
fi
if ! [[ $1 != *[!0-9]* ]]; then
   echo "Error: argument not a number. Usage: install.sh [TOTAL_GAMES]"; exit 2
fi

# Do some cleanup, and then build and install the module
rm -f /dev/snake*
rmmod snake
make clean
make
insmod ./snake.o total_games=$1

# Acquire the MAJOR number
major=`cat /proc/devices | grep snake | sed 's/ snake//'`

# Create the files for the games
i=0
while [ $i -lt $1 ]; do
	mknod /dev/snake$i c $major $i
	let i=i+1
done 

