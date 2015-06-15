#ifndef _HW3Q1_H
#define _HW3Q1_H

/*=========================================================================
Constants and definitions:
==========================================================================*/
#define N (4) /* the size of the board */
#define M (3)  /* the initial size of the snake */
#define K (5)  /* the number of turns a snake can survive without eating */

typedef char Player;
/* PAY ATTENTION! i will use the fact that white is positive one and black is negative
one to describe the segments of the snake. for example, if the white snake is 2 segments
long and the black snake is 3 segments long
white snake is  1   2
black snake is -1  -2  -3 */
#define WHITE ( 1) /* id to the white player */
#define BLACK (-1) /* id to the black player */
#define EMPTY ( 0) /* to describe an empty point */
/* to describe a point with food. having the value this big guarantees that there will be no
overlapping between the snake segments' numbers and the food id */
#define FOOD  (N*N)

typedef char bool;
#define FALSE (0)
#define TRUE  (1)

typedef int Direction;
#define DOWN  (2)
#define LEFT  (4)
#define RIGHT (6)
#define UP    (8)

/* a point in 2d space */
typedef struct {
	int x, y;
} Point;


typedef int Matrix[N][N];

// ERR_ILLEGAL_MOVE means the move was valid input but illegal in the game (loss)
typedef enum {
	ERR_OK,
	ERR_BOARD_FULL,
	ERR_SNAKE_IS_TOO_HUNGRY,
	ERR_ILLEGAL_MOVE,
	ERR_INVALID_MOVE,
	ERR_SEGMENT_NOT_FOUND,
} ErrorCode;

// The minimal buffer size (in chars) for printing the board, without NULL termination.
// Make this a macro so the size is known at compile-time.
#define GOOD_BUF_SIZE (3*N*N+10*N+8)

ErrorCode Init(Matrix*);
bool IsAvailable(Matrix*, Point);
ErrorCode RandFoodLocation(Matrix*);
bool IsMatrixFull(Matrix*);
void Print(Matrix*, char*, int);
ErrorCode Update(Matrix*, Player, Direction);
ErrorCode GetInputLoc(Matrix*, Player, Point*, Direction);
ErrorCode GetSegment(Matrix*, int, Point*);
bool CheckTarget(Matrix*, Player, Point);
int GetSize(Matrix*, Player);
ErrorCode CheckFoodAndMove(Matrix*, Player, Point);
void IncSizePlayer(Matrix*, Player, Point);
void AdvancePlayer(Matrix*, Player, Point);

#endif /* _HW3Q1_H */

