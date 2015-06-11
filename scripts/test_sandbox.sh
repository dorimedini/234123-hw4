#!/bin/bash

# Go home
cd /root/hw4

# Copy
/mnt/hgfs/HW/234123-hw4/scripts/update_files.sh

# Compile
make clean
make sandbox

# Allow usage
chmod a+x ./sandbox

# Run
./sandbox

