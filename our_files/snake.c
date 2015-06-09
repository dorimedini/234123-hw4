#include "snake.h"
#include <linux/errno.h>
#include <linux/module.h>
#include <asm-i386/semaphore.h>
#include <linux/fs.h>
#include <linux/slab.h>		// For kmalloc() and kfree()
#include <linux/random.h>	// For get_random_bytes()
//#include <stdio.h>
MODULE_LICENSE("GPL");

/*******************************************************************************************
 ===========================================================================================
 ===========================================================================================
                                   DEBUG FUNCTIONS
 ===========================================================================================
 ===========================================================================================
 ******************************************************************************************/
// Set this to 1 for debug mode, or 0 for production
#define HW4_DEBUG 1

// Conditional debug printing.
// Important note: don't do PRINT("%d",++i) if you want i to be incremented in the production
// version! If HW4_DEBUG is 0 then the command won't exist and i won't be incremented.
#if HW4_DEBUG
	#define PRINT(...)			printk(__VA_ARGS__);
	#define PRINT_IF(cond,...)	if(cond) PRINT(__VA_ARGS__);
#else
	#define PRINT(...)
	#define PRINT_IF(...)
#endif


/*******************************************************************************************
 ===========================================================================================
 ===========================================================================================
                                   SNAKE FUNCTIONS
 ===========================================================================================
 ===========================================================================================
 ******************************************************************************************/
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

typedef int ErrorCode;
#define ERR_OK      			((ErrorCode) 0)
#define ERR_BOARD_FULL			((ErrorCode)-1)
#define ERR_SNAKE_IS_TOO_HUNGRY ((ErrorCode)-2)

bool Init(Matrix*); /* initialize the board. return false if the board is illegal (should not occur, affected by N, M parameters) */

bool Update(Matrix*, Player);/* handle all updating to this player. returns whether to continue or not. */

void Print(Matrix*);/* prints the state of the board */

Point GetInputLoc(Matrix*, Player);/* calculates the location that the player wants to go to */

bool CheckTarget(Matrix*, Player, Point);/* checks if the player can move to the specified location */

Point GetSegment(Matrix*, int);/* gets the location of a segment which is numbered by the value */

bool IsAvailable(Matrix*, Point);/* returns if the point wanted is in bounds and not occupied by any snake */

ErrorCode CheckFoodAndMove(Matrix*, Player, Point);/* handle food and advance the snake accordingly */

ErrorCode RandFoodLocation(Matrix*);/* randomize a location for food. return ERR_BOARD_FULL if the board is full */

void AdvancePlayer(Matrix*, Player, Point);/* advance the snake */

void IncSizePlayer(Matrix*, Player, Point);/* advance the snake and increase it's size */

bool IsMatrixFull(Matrix*);/* check if the matrix is full */

int GetSize(Matrix*, Player);/* gets the size of the snake */



/*-------------------------------------------------------------------------
The main program. The program implements a snake game
-------------------------------------------------------------------------*/
int old_main() {
	Player player = WHITE;
	Matrix matrix = { { EMPTY } };

	if (!Init(&matrix))
	{
		//printf("Illegal M, N parameters.");
		return -1;
	}
	while (Update(&matrix, player))
	{
		Print(&matrix);
		/* switch turns */
		player = -player;
	}

	return 0;
}

bool Init(Matrix *matrix) {
	int i;
	/* initialize the snakes location */
	for (i = 0; i < M; ++i)
	{
		(*matrix)[0][i] =   WHITE * (i + 1);
		(*matrix)[N - 1][i] = BLACK * (i + 1);
	}
	/* initialize the food location */
	PRINT("OLD CODE: srand(time(0))\n");
	if (RandFoodLocation(matrix) != ERR_OK)
		return FALSE;
	//printf("instructions: white player is represented by positive numbers, \nblack player is represented by negative numbers\n");
	Print(matrix);

	return TRUE;
}

bool Update(Matrix *matrix, Player player) {
	ErrorCode e;
	Point p = GetInputLoc(matrix, player);

	if (!CheckTarget(matrix, player, p))
	{
		//printf("% d lost.", player);
		return FALSE;
	}
	e = CheckFoodAndMove(matrix, player, p);
	if (e == ERR_BOARD_FULL)
	{
		//printf("the board is full, tie");
		return FALSE;
	}
	if (e == ERR_SNAKE_IS_TOO_HUNGRY)
	{
		//printf("% d lost. the snake is too hungry", player);
		return FALSE;
	}
	// only option is that e == ERR_OK
	if (IsMatrixFull(matrix))
	{
		//printf("the board is full, tie");
		return FALSE;
	}

	return TRUE;
}

Point GetInputLoc(Matrix *matrix, Player player) {
	Direction dir;
	Point p;

	//printf("% d, please enter your move(DOWN2, LEFT4, RIGHT6, UP8):\n", player);
	do
	{
		/** NEW CODE START */
		dir = UP;
		/** NEW CODE END */
		/** OLD CODE:
		if (scanf("%d", &dir) < 0)
		{
			printf("an error occurred, the program will now exit.\n");
			exit(1);
		}*/
		if (dir != UP   && dir != DOWN && dir != LEFT && dir != RIGHT)
		{
			//printf("invalid input, please try again\n");
		}
		else
		{
			break;
		}
	} while (TRUE);

	p = GetSegment(matrix, player);

	switch (dir)
	{
	case UP:    --p.y; break;
	case DOWN:  ++p.y; break;
	case LEFT:  --p.x; break;
	case RIGHT: ++p.x; break;
	}
	return p;
}

Point GetSegment(Matrix *matrix, int segment) {
	/* just run through all the matrix */
	Point p;
	for (p.x = 0; p.x < N; ++p.x)
	{
		for (p.y = 0; p.y < N; ++p.y)
		{
			if ((*matrix)[p.y][p.x] == segment)
				return p;
		}
	}
	p.x = p.y = -1;
	return p;
}

int GetSize(Matrix *matrix, Player player) {
	/* check one by one the size */
	Point p, next_p;
	int segment = 0;
	while (TRUE)
	{
		next_p = GetSegment(matrix, segment += player);
		if (next_p.x == -1)
			break;

		p = next_p;
	}

	return (*matrix)[p.y][p.x] * player;
}

bool CheckTarget(Matrix *matrix, Player player, Point p) {
	/* is empty or is the tail of the snake (so it will move the next
	to make place) */
	return IsAvailable(matrix, p) || ((*matrix)[p.y][p.x] == player * GetSize(matrix, player));
}

bool IsAvailable(Matrix *matrix, Point p) {
	return
		/* is out of bounds */
		!(p.x < 0 || p.x >(N - 1) ||
		p.y < 0 || p.y >(N - 1) ||
		/* is empty */
		((*matrix)[p.y][p.x] != EMPTY && (*matrix)[p.y][p.x] != FOOD));
}

ErrorCode CheckFoodAndMove(Matrix *matrix, Player player, Point p) {
	static int white_counter = K;
	static int black_counter = K;
	/* if the player did come to the place where there is food */
	if ((*matrix)[p.y][p.x] == FOOD)
	{
		if (player == BLACK) black_counter = K;
		if (player == WHITE) white_counter = K;

		IncSizePlayer(matrix, player, p);

		if (RandFoodLocation(matrix) != ERR_OK)
			return ERR_BOARD_FULL;
	}
	else /* check hunger */
	{
		if (player == BLACK && --black_counter == 0)
			return ERR_SNAKE_IS_TOO_HUNGRY;
		if (player == WHITE && --white_counter == 0)
			return ERR_SNAKE_IS_TOO_HUNGRY;

		AdvancePlayer(matrix, player, p);
	}
	return ERR_OK;
}

void AdvancePlayer(Matrix *matrix, Player player, Point p) {
	/* go from last to first so the identifier is always unique */
	Point p_tmp, p_tail = GetSegment(matrix, GetSize(matrix, player) * player);
	int segment = GetSize(matrix, player) * player;
	while (TRUE)
	{
		p_tmp = GetSegment(matrix, segment);
		(*matrix)[p_tmp.y][p_tmp.x] += player;
		segment -= player;
		if (segment == 0)
			break;
	}
	(*matrix)[p_tail.y][p_tail.x] = EMPTY;
	(*matrix)[p.y][p.x] = player;
}

void IncSizePlayer(Matrix *matrix, Player player, Point p) {
	/* go from last to first so the identifier is always unique */
	Point p_tmp;
	int segment = GetSize(matrix, player)*player;
	while (TRUE)
	{
		p_tmp = GetSegment(matrix, segment);
		(*matrix)[p_tmp.y][p_tmp.x] += player;
		segment -= player;
		if (segment == 0)
			break;
	}
	(*matrix)[p.y][p.x] = player;
}

ErrorCode RandFoodLocation(Matrix *matrix) {
	Point p;
	do
	{
		get_random_bytes(&p.x,sizeof(int));
		get_random_bytes(&p.y,sizeof(int));
		p.x %= N;
		p.y %= N;
		PRINT("OLD CODE: p.x = rand() %% N;\n");
		PRINT("OLD CODE: p.y = rand() %% N;\n");
	} while (!IsAvailable(matrix, p) || IsMatrixFull(matrix));

	if (IsMatrixFull(matrix))
		return ERR_BOARD_FULL;

	(*matrix)[p.y][p.x] = FOOD;
	return ERR_OK;
}

bool IsMatrixFull(Matrix *matrix) {
	Point p;
	for (p.x = 0; p.x < N; ++p.x)
	{
		for (p.y = 0; p.y < N; ++p.y)
		{
			if ((*matrix)[p.y][p.x] == EMPTY || (*matrix)[p.y][p.x] == FOOD)
				return FALSE;
		}
	}
	return TRUE;
}

void Print(Matrix *matrix) {
	int i;
	Point p;
	for (i = 0; i < N + 1; ++i)
		;//printf("---");
	//printf("\n");
	for (p.y = 0; p.y < N; ++p.y)
	{
		//printf("|");
		for (p.x = 0; p.x < N; ++p.x)
		{
			switch ((*matrix)[p.y][p.x])
			{
			case FOOD:  //printf("  *");
				break;
			case EMPTY: //printf("  .");
				break;
			default:    //printf("% 3d", (*matrix)[p.y][p.x]);
				break;
			}
		}
		//printf(" |\n");
	}
	for (i = 0; i < N + 1; ++i)
		;//printf("---");
	//printf("\n");
}


/*******************************************************************************************
 ===========================================================================================
 ===========================================================================================
                                   MODULE FUNCTIONS
 ===========================================================================================
 ===========================================================================================
 ******************************************************************************************/
/* *************
 GLOBALS
 **************/
// Module name
#define MODULE_NAME "snake"

// Simple typedef, for comfort
typedef struct semaphore Semaphore;

// Major number, and total number of games allowed (given as input)
int major = -1;
int total_games = 0;
MODULE_PARM(total_games,"i");

// Global lock, to protect the private file data during open().
// Uses files_initialized[total_games] to keep track of which Game structures
// have already been initialized, after which the per-game semaphores may be
// used
Semaphore file_lock;
int* files_initialized = NULL;

// Game states, to be stored as private data per game
typedef enum {
	PRE_START,		// Game hasn't started yet (no player has joined)
	WAIT_FOR_B,		// Player 1 has joined, waiting for player 2
	IN_PROGRESS,	// Game is being played
	RAGE_QUIT,		// Game stopped: one of the players quit (called release(), for example)
	W_WIN,			// Game over, white player won
	B_WIN,			// Game over, black player won
} GameState;

// All game-related data should be stored here.
// This includes synchronization tools.
typedef struct game_t {
	Matrix matrix;				// Main game grid
	Semaphore white_move;		// White player must lock this to move (signalled by black player)
	Semaphore black_move;		// Black player must lock this to move (signalled by white player)
	Semaphore w_player_join;	// Player must lock this successfully to join as the white player
	Semaphore b_player_join;	// Player must lock this successfully to join as the black player
} Game;

/* *************
 FOPS FUNCTIONS
 **************/
/*

int our_open(struct inode* i, struct file* filp) {
	1. Get minor. Say minor == N
	2. Setup the file object with the minor, semaphores, game state, locks, etc. (DYNAMIC ALLOC)
	3. Try to enter game #N as the white player.
	4. If failed, try to be the black player
	5. If failed, return in error
	6. If I am the black player, signal the white player's move-semaphore
}

int our_release(struct inode* i, struct file* filp) {
	1. Lock stuff (after this, functions should start returning errors instead of working)
	2. Change game state to indicate that the player did a rage quit
	3. Unlock stuff (no reason to keep things locked for memory freeing)
	4. Free memory from the private data
}

ssize_t our_read(struct file *filp, char *buf, size_t n, loff_t *f_pos) {
	1. Lock, check state, unlock (and maybe react)
	2. Lock the grid (for reading)
	3. Read the grid
	4. Unlock it
	5. Print the grid into the buffer
}

ssize_t our_write(struct file *filp, const char *buf, size_t n, loff_t *f_pos) {
	1. Lock status, check if the game is alive, unlock status, react
	2. Find out which player we are
	3. Wait for a signal that we can play
	4. Lock the grid (for writing)
	5. Play
	6. Unlock the grid
	7. Signal the other player that he can play
}

int our_ioctl(struct inode *i, struct file *filp, unsigned int cmd, unsigned long arg) {
	switch(cmd) {
	case SNAKE_GET_WINNER:
		
		break;
	case SNAKE_GET_COLOR:
		
		break;
	default:
		return -ENOTTY;
	}
}

loff_t our_llseek(struct file *filp, loff_t x, int n) {
	return -ENOSYS;
}

*/

struct file_operations fops = {
/*	.open=		our_open,
	.release=	our_release,
	.read=		our_read,
	.write=		our_write,
	.llseek=	our_llseek,
	.ioctl=		our_ioctl, */
};


int init_module(void) {
	
	PRINT("IN INIT_MODULE\n");
	
	// Setup variables: files_initialized[] array and the file_lock semaphore
	files_initialized = (int*)kmalloc(sizeof(int)*total_games,GFP_KERNEL);
	if (!files_initialized) {
		PRINT("KMALLOC FAILED!\n");
		return -ENOMEM;
	}
	int i;
	for (i=0; i<total_games; ++i)
		files_initialized[i] = 0;	// No one has called open() yet
	sema_init(&file_lock, 1);
	
	// Registration
	major = register_chrdev(0, MODULE_NAME, &fops);
	if (major < 0) {	// FAIL
		PRINT("REGISTRATION FAILED, RETURN VALUE=%d\n",major);
		kfree(files_initialized);
		return major;
	}
	SET_MODULE_OWNER(&fops);
	PRINT("EXITING INIT_MODULE, MAJOR=%d\n",major);
	
	return 0;
	
}

void cleanup_module(void) {
	
	PRINT("IN CLEANUP_MODULE\n");
	
	// Un-registration
	int ret = unregister_chrdev(major, MODULE_NAME);
	if (ret<0)
		printk("FATAL ERROR: unregister_chrdev() failed\n");
	
	// Structure cleanup 
	kfree(files_initialized);
	
}


