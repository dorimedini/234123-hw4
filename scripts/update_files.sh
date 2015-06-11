#!/bin/bash

# Go home
cd /root/hw4

# Copy
cp -vr /mnt/hgfs/HW/234123-hw4/our_files/* ./
cp -vr /mnt/hgfs/HW/234123-hw4/course_files/snake.h ./
cp -vr /mnt/hgfs/HW/234123-hw4/scripts/*install.sh ./
cp -vr /mnt/hgfs/HW/234123-hw4/scripts/test*.sh ./

# Unixify
dos2unix ./*

# Allow script usage
chmod a+x ./*.sh

