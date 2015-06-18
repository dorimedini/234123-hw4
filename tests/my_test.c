

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include "snake.h"
#include <limits.h>
#include <math.h>
#include <stdbool.h>

#define DOWN  '2'
#define LEFT  '4'
#define RIGHT '6'
#define UP    '8' 
#define BOARD_SIZE 97

#define RUN_TEST(test) do { \
    printf("Running "#test"... \n"); \
    if (test()) { \
        printf("[OK]\n");\
    } else { \
            printf("[Failed]\n"); \
    } \
} while(0)

bool joinAndCrash() {
	int pid = fork();
	int fd = open("1",O_RDWR);
	printf("Open file: %d\n", fd);
	char buffer[BOARD_SIZE];
	int player = ioctl(fd, SNAKE_GET_COLOR);
	printf("%d\n",player);
	char move = (player == 2) ? DOWN : UP;
	while (write(fd,&move,1) > 0){
		read(fd,buffer,BOARD_SIZE);
		printf("%s\n", buffer);
	}
	if (pid) {
		wait(pid);
	} else {
		exit(0);
	}
	return 1;
}

bool joinAndMeet() {
	int pid = fork();
	int fd = open("2",O_RDWR);
	printf("Open file: %d\n", fd);
	char buffer[BOARD_SIZE];
	int player = ioctl(fd, SNAKE_GET_COLOR);
	char move = (player == 4) ? DOWN : UP;
	while (write(fd,&move,1) > 0){
		read(fd,buffer,BOARD_SIZE);
		printf("%s\n", buffer);
	}
	
	if (pid) {
		wait(pid);
	} else {
		exit(0);
	}
	return 1;
}

bool walkInCircles	() {
	int pid = fork();
	int fd = open("3",O_RDWR);
	printf("Open file: %d\n", fd);
	char buffer[BOARD_SIZE];
	char whiteMoves[8] = {DOWN,RIGHT,RIGHT,RIGHT,UP,LEFT,LEFT,LEFT};
	char blackMoves[8] = {UP,RIGHT,RIGHT,RIGHT,DOWN,LEFT,LEFT,LEFT};
	int player = ioctl(fd, SNAKE_GET_COLOR);
	char* moves = (player == 4) ? whiteMoves : blackMoves ;
	int i=0;
	while (write(fd,moves + i,1) > 0){
		read(fd,buffer,BOARD_SIZE);
		printf("%s\n", buffer);
		int winner = ioctl(fd, SNAKE_GET_WINNER);
		printf("Current winner: %d\n", winner);
		i++;
		i%=8;
	}
	
	if (pid) {
		wait(pid);
	} else {
		exit(0);
	}
	return 1;
}

int main(){
	setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
//	RUN_TEST(joinAndCrash);
//	RUN_TEST(joinAndMeet);
	RUN_TEST(walkInCircles);
	return 0;
}

