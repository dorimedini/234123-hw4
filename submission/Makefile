KERNELDIR=/usr/src/linux-2.4.18-14custom
include $(KERNELDIR)/.config
CFLAGS=-D__KERNEL__ -DMODULE -I$(KERNELDIR)/include -O -Wall

all: snake.o

test: snake.o
	gcc -O -Wall -lpthread test_snake.c -o test_snake

sandbox: snake.o sandbox.c
	gcc -O -Wall sandbox.c -o sandbox

clean:
	rm -f snake.o test_snake sandbox
