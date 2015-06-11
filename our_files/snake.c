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
	#define DEBUG_CODE(code) 	code
#else
	#define PRINT(...)
	#define PRINT_IF(...)
	#define DEBUG_CODE(code)
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

static ErrorCode Init(Matrix*); /* initialize the board. return false if the board is illegal (should not occur, affected by N, M parameters) */
static bool IsAvailable(Matrix*, Point);
static ErrorCode RandFoodLocation(Matrix*);
static bool IsMatrixFull(Matrix*);
static void Print(Matrix*, char*);

static int abs(int x) {
	return x<0 ? -x : x;
}

static ErrorCode Init(Matrix *matrix) {
	PRINT("IN INIT()\n");
	// Start by emptying everything
	int i,j;
	for (i=0; i<N; ++i)
		for (j=0; j<N; ++j)
			(*matrix)[i][j] = EMPTY;
	
	/* initialize the snakes location */
	for (i = 0; i < M; ++i) {
		(*matrix)[0][i] =   WHITE * (i + 1);
		(*matrix)[N - 1][i] = BLACK * (i + 1);
	}
	/* initialize the food location */
	if (RandFoodLocation(matrix) != ERR_OK) {
		PRINT("RETURNING FROM INIT() WITH ERR_BOARD_FULL\n");
		return ERR_BOARD_FULL;
	}
	
	PRINT("RETURNING FROM INIT() WITH ERR_OK\n");
	return ERR_OK;
}

static bool IsAvailable(Matrix *matrix, Point p) {
	PRINT("IN ISAVAILABLE() WITH P=(%d,%d)\n",p.y,p.x);
	return
		/* is out of bounds */
		!(p.x < 0 || p.x >(N - 1) ||
		p.y < 0 || p.y >(N - 1) ||
		/* is empty */
		((*matrix)[p.y][p.x] != EMPTY && (*matrix)[p.y][p.x] != FOOD));
}

#define ABS(x) if (x<0) x*=-1
static ErrorCode RandFoodLocation(Matrix *matrix) {
	PRINT("IN RANDFOODLOCATION()\n");
	Point p;
	do {
		get_random_bytes(&p.x,sizeof(int));
		get_random_bytes(&p.y,sizeof(int));
		p.x %= N;
		p.y %= N;
		p.x = abs(p.x);
		p.y = abs(p.y);
	} while (!(IsAvailable(matrix, p) || IsMatrixFull(matrix)));
	PRINT("OUT OF THE LOOP, POINT=(%d,%d)\n",p.y,p.x);
	
	if (IsMatrixFull(matrix)) {
		PRINT("RETURNING FROM RANDFOODLOCATION() WITH ERR_BOARD_FULL\n");
		return ERR_BOARD_FULL;
	}

	(*matrix)[p.y][p.x] = FOOD;
	PRINT("PUT FOOD IN (%d,%d), RETURNING FROM RANDFOODLOCATION WITH ERR_OK\n",p.y,p.x);
	return ERR_OK;
}

static bool IsMatrixFull(Matrix *matrix) {
	PRINT("IN ISMATRIXFULL()\n");
	Point p;
	for (p.x = 0; p.x < N; ++p.x) {
		for (p.y = 0; p.y < N; ++p.y) {
			if ((*matrix)[p.y][p.x] == EMPTY || (*matrix)[p.y][p.x] == FOOD) {
				PRINT("MATRIX ISN'T FULL, ROOM IN M[%d][%d]\n",p.y,p.x);
				return FALSE;
			}
		}
	}
	PRINT("MATRIX IS FULL!\n");
	return TRUE;
}

// Assume *buf is big enough
static void Print(Matrix *matrix, char* buf) {
	int i;
	int j=0;
	Point p;
	for (i = 0; i < N + 1; ++i) {				// (N+1)*3
		buf[j++] = '-';
		buf[j++] = '-';
		buf[j++] = '-';
	}
	buf[j++] = '\n';							// 1
	for (p.y = 0; p.y < N; ++p.y) {				// N*
		buf[j++] = '|';								// 1+
		for (p.x = 0; p.x < N; ++p.x) {				// N*
			switch ((*matrix)[p.y][p.x]) {				// 3
			case FOOD:
				buf[j++] = ' ';
				buf[j++] = ' ';
				buf[j++] = '*';
				break;
			case EMPTY:
				buf[j++] = ' ';
				buf[j++] = ' ';
				buf[j++] = '.';
				break;
			default:
				buf[j++] = ' ';
				buf[j++] = (*matrix)[p.y][p.x] < 0? '-' : ' ';
				buf[j++] = (char)('0'+abs((*matrix)[p.y][p.x]));
			}
		}
		buf[j++] = ' ';								// +3
		buf[j++] = '|';
		buf[j++] = '\n';
	}											// Sub total: (N+1)*3+1+N*(1+3*N+3)
	for (i = 0; i < N + 1; ++i) {				// (N+1)*3
		buf[j++] = '-';
		buf[j++] = '-';
		buf[j++] = '-';
	}
	buf[j++] = '\n';							// +1
	buf[j++] = '\0';							// +1
	
	// TOTAL BUFFER SIZE:
	// (N+1)*3+1+N*(1+3*N+3)+(N+1)*3+1+1
	//=3N+3+1+N+3NN+3N+3N+3+1+1
	//=3NN+10N+9
	
}


/*******************************************************************************************
 ===========================================================================================
 ===========================================================================================
                                   MODULE FUNCTIONS
 ===========================================================================================
 ===========================================================================================
 ******************************************************************************************/
/* ****************************
 TYPEDEFS
 *****************************/
// Simple typedef, for comfort
typedef struct semaphore Semaphore;

// Valid states a file object can be in
typedef enum {
	PRE_START,	// Before call to open()
	ACTIVE,		// After open() by two players, before release()
	W_WIN,		// Game over, white player won
	B_WIN,		// Game over, black player won
	TIE,		// Game over, ended in a tie
	DESTROYED	// After release()
} GameState;

// All game-related data should be stored here.
// This includes synchronization tools.
// Resources are numbered to prevent deadlocks - if i<j and
// resource #j is locked, DO NOT lock resource #i.
// Some resources do not need numbers:
// - *_player_join don't need numbers because they are locked
//   exactly once and never signalled. In addition, all lock
//   operations on these locks are non-blocking.
// - *_move are locked by one player and signalled by another,
//   so white_move is "always in locked state" as far as the
//   white player is concerned. No point in preventing the
//   locking of other locks. However, to simplify things,
//   give these locks the lowest numbers.
typedef struct game_t {
	int minor;					// File's minor number
	Matrix matrix;				// Main game grid
	Semaphore w_player_join;	// (NO RESOURCE #) Player must lock this successfully to join as the white player
	Semaphore b_player_join;	// (NO RESOURCE #) Player must lock this successfully to join as the black player
	Semaphore white_move;		// (RESOURCE #0) White player must lock this to move (signalled by black player)
	Semaphore black_move;		// (RESOURCE #1) Black player must lock this to move (signalled by white player)
	Semaphore state_lock;		// (RESOURCE #2) Protects the game_state field
	Semaphore grid_lock;		// (RESOURCE #3) Protects the matrix field
	GameState state;			// The state of the game
} Game;

/* ****************************
 GLOBALS & FORWARD DECLARATIONS
 *****************************/
// Forward declarations
int our_open(struct inode*, struct file*);
int our_release_W(struct inode*, struct file*);
int our_release_B(struct inode*, struct file*);
ssize_t our_read(struct file*, char*, size_t, loff_t*);
ssize_t our_write_W(struct file*, const char*, size_t, loff_t*);
ssize_t our_write_B(struct file*, const char*, size_t, loff_t*);
int our_ioctl_W(struct inode*, struct file*, unsigned int, unsigned long);
int our_ioctl_B(struct inode*, struct file*, unsigned int, unsigned long);
loff_t our_llseek(struct file*, loff_t, int);

// Module name
#define MODULE_NAME "snake"

// Major number, and total number of games allowed (given as input)
static int major = -1;
static int max_games = 0;
MODULE_PARM(max_games,"i");

// Games
static Game* games = NULL;

// Different file operations for white or black players
struct file_operations fops_W = {
	.open=		our_open,
	.release=	our_release_W,
/*	.read=		our_read,
	.write=		our_write_W,
	.llseek=	our_llseek,
	.ioctl=		our_ioctl_W, */
	.owner=		THIS_MODULE,
};
struct file_operations fops_B = {
	.open=		our_open,
	.release=	our_release_B,
/*	.read=		our_read,
	.write=		our_write_B,
	.llseek=	our_llseek,
	.ioctl=		our_ioctl_B, */
	.owner=		THIS_MODULE,
};

/* ****************************
 UTILITY FUNCTIONS
 *****************************/
// Checks to see if the game is in DESTROYED state
static int is_destroyed(int minor) {
	Game* game = games+minor;
	down(&game->state_lock);
	if (game->state == DESTROYED) {
		up(&game->state_lock);
		return 1;
	}
	up(&game->state_lock);
	return 0;
}

// Destroys the game (changes the state)
static void destroy_game(int minor) {
	Game* game = games+minor;
	down(&game->state_lock);
	game->state = DESTROYED;
	up(&game->state_lock);
}

// This macro return -10 from any function if the game identified by
// the minor number has been destroyed.
#define CHECK_DESTROYED(minor) do { \
		if (is_destroyed(minor)) \
			return -10; \
	} while(0)

// Macro for freeing all dynamically-allocated global variables
#define FREE_GLOBALS() do { \
		kfree(games); \
	} while(0)

// Use this to read the win state, as required for SNAKE_GET_WINNER
static int get_winner(int minor) {
	int ret;
	Game* game = games+minor;
	down(&game->state_lock);
	switch(game->state) {
		case PRE_START:
		case ACTIVE:
			ret = -1;
			break;
		case W_WIN:
			ret = 4;
			break;
		case B_WIN:
			ret = 2;
			break;
		case TIE:
			ret = 5;
			break;
		case DESTROYED:
		default:
			ret = -10;	// Return this error as general return value - see CHECK_DESTROYED
			break;
	}
	up(&game->state_lock);
	return ret;
}

// Returns the minimal buffer size (in chars) for printing the board.
// Make this a macro so the size is known at compile-time
#define GOOD_BUF_SIZE 3*N*N+10*N+9

// Use this to check if X - representing the number os chars in a buffer - 
// is big enough to print the board.
// See Print() for the calculation.
static bool big_enough_buf(int X) {
	return (X >= GOOD_BUF_SIZE);
}

/* ****************************
 FOPS AUXILLARY FUNCTIONS
 *****************************/
// Use this to simplify the release() functions
static int our_release_aux(struct inode* i, bool is_black) {
	
	// Get minor
	int minor = MINOR(i->i_rdev);
	
	// Destroy the game
	destroy_game(minor);
	
	// Signal the other player, who may be waiting to move.
	// If we don't do this, the other player may be trapped, forever
	// waiting for his turn which won't come...
	Game* game = games+minor;
	is_black ? up(&game->white_move) : up(&game->black_move);
	
	return 0;
	
}

/* ****************************
 FOPS FUNCTIONS
 *****************************/

/**
 * Open a game.
 *
 * First, if the game has been destroyed (released by another
 * player), return -10.
 * Next, make sure the game is in some joinable state. If not,
 * return -ENOSPC.
 *
 * After those initial checks, try to join the game. Using the
 * semaphores in the Game structure, become player 1 or player
 * 2 (or return with ENOSPC, if there's no more room). The first
 * player who joins needs to wait for a signal from the second
 * player, and the second player needs to not only signal the
 * first player but to change the game state.
 *
 * The f_ops field should be assigned differently for the black
 * player and for the white player.
 */
int our_open(struct inode* i, struct file* filp) {
	
	int minor = MINOR(i->i_rdev);
	
	// Check if the operation is valid
	CHECK_DESTROYED(minor);
	
	// Get the Game data
	Game* game = games+minor;
	
	// If the game isn't willing to accept new players, exit in error
	down(&game->state_lock);
	if (game->state != PRE_START) {
		up(&game->state_lock);
		return -ENOSPC;
	}
	up(&game->state_lock);
	
	// Try to join the game
	if (down_trylock(&game->w_player_join)) {		// If player 1 is already in-game
		if (down_trylock(&game->b_player_join)) {	// ...and so is player 2
			PRINT("PLAYER COULDN'T JOIN, BOTH LOCKS ARE LOCKED\n");
			// "No space left on the device". This should happen only if player 2 joined but
			// didn't lock game_state_locks[minor] yet.
			return -ENOSPC;							
		}
		// I am player 2
		else {
			filp->f_op = &fops_B;				// Switch the writing function (so it knows I'm player 2)
			PRINT("PLAYER 2 JOINED, CHANGING STATE\n");
			down(&game->state_lock);			// Update game state
			game->state = ACTIVE;
			up(&game->state_lock);
			PRINT("STATE CHANGED, SIGNALLING PLAYER 1 THAT PLAYER 2 HAS JOINED\n");
			up(&game->white_move);				// Tell player 1 we're good to go! He can move
		}
	}
	// Else: I am player 1
	else {
		filp->f_op = &fops_W;					// Switch the writing function (so it knows I'm player 1)
		game->minor = minor;					// Inform the Game structure which minor it is
		PRINT("PLAYER 1 JOINED, WAITING FOR PLAYER 2...\n");
		down(&game->white_move);				// Wait for player 2 (blocking operation)
		PRINT("PLAYER 1 DONE WAITING FOR PLAYER 2 TO JOIN\n");
		up(&game->white_move);					// Signal the fact that it's my turn
	}
	
	// Done with game-starting logic
	return 0;

}

int our_release_W(struct inode* i, struct file* filp) {
	PRINT("IN OUR_RELEASE_W\n");
	return our_release_aux(i,FALSE);
}
int our_release_B(struct inode* i, struct file* filp) {
	PRINT("IN OUR_RELEASE_B\n");
	return our_release_aux(i,TRUE);
}

/*

ssize_t our_read(struct file *filp, char *buf, size_t n, loff_t *f_pos) {
	1. Lock, check state, unlock (and maybe react)
	2. Lock the grid (for reading)
	3. Read the grid
	4. Unlock it
	5. Print the grid into the buffer
}

ssize_t our_write_W(struct file *filp, const char *buf, size_t n, loff_t *f_pos) {
	1. Wait for a signal that it's our turn
	2. Lock status, check if the game is destroyed, unlock status, react
	3. Wait for a signal that we can play
	4. Lock the grid (for state reading and then writing)
	5. Play
	6. Unlock the grid
	7. Signal the other player that he can play
}

ssize_t our_write_B(struct file *filp, const char *buf, size_t n, loff_t *f_pos) {
	1. Lock status, check if the game is alive, unlock status, react
	2. Find out which player we are
	3. Wait for a signal that we can play
	4. Lock the grid (for writing)
	5. Play
	6. Unlock the grid
	7. Signal the other player that he can play
}

*/

int our_ioctl_W(struct inode *i, struct file *filp, unsigned int cmd, unsigned long arg) {
	int minor = MINOR(i->i_rdev);	// Get minor
	CHECK_DESTROYED(minor);			// Make sure the game wasn't released
	switch(cmd) {
	case SNAKE_GET_WINNER:
		return get_winner(minor);
	case SNAKE_GET_COLOR:
		return 4;
	default:
		return -ENOTTY;
	}
}

int our_ioctl_B(struct inode *i, struct file *filp, unsigned int cmd, unsigned long arg) {
	int minor = MINOR(i->i_rdev);	// Get minor
	CHECK_DESTROYED(minor);			// Make sure the game wasn't released
	switch(cmd) {
	case SNAKE_GET_WINNER:
		return get_winner(minor);
	case SNAKE_GET_COLOR:
		return 2;
	default:
		return -ENOTTY;
	}
}

loff_t our_llseek(struct file *filp, loff_t x, int n) {
	return -ENOSYS;
}


int init_module(void) {
	
	PRINT("IN INIT_MODULE, MAX_GAMES=%d\n",max_games);
	
	// Allocate memory
	games = NULL;
	games = (Game*)kmalloc(sizeof(Game)*max_games,GFP_KERNEL);
	if (!games) {
		PRINT("KMALLOC FAILED!\n");
		FREE_GLOBALS();
		return -ENOMEM;
	}
	
	// Initial values, game setup and semaphore setup
	int i;
	DEBUG_CODE(char buf[GOOD_BUF_SIZE]);
	for (i=0; i<max_games; ++i) {
		
		// Initialize the board
		DEBUG_CODE(Print(&games[i].matrix,buf));
		PRINT_IF(i<=-1, "PRINTING MATRIX %d BEFORE INIT():\n%s\n",i,buf);
		if (Init(&games[i].matrix) != ERR_OK) {
			PRINT("COULDN'T INIT BOARD #%d\n",i);
			FREE_GLOBALS();
			return -EPERM;						// Should never happen...
		}
		DEBUG_CODE(Print(&games[i].matrix,buf));
		PRINT_IF(i<=-1, "PRINTING MATRIX %d AFTER INIT():\n%s\n",i,buf);
		
		// Initialize other fields
		games[i].state = PRE_START;				// No one has called open() yet
		games[i].minor = -1;					// No minor number yet
		sema_init(&games[i].state_lock, 1);		// We need locks for each game
		sema_init(&games[i].grid_lock, 1);		// Player must lock this successfully to r/w the game grid
		sema_init(&games[i].white_move, 0);		// White player must lock this to move (signalled by black player)
		sema_init(&games[i].black_move, 0);		// Black player must lock this to move (signalled by white player)
		sema_init(&games[i].w_player_join, 1);	// Player must lock this successfully to join as the white player
		sema_init(&games[i].b_player_join, 1);	// Player must lock this successfully to join as the black player
	}
	
	// Registration
	major = register_chrdev(0, MODULE_NAME, &fops_B);	// Make black the default. Down with racism!
	if (major < 0) {	// FAIL
		PRINT("REGISTRATION FAILED, RETURN VALUE=%d\n",major);
		FREE_GLOBALS();
		return major;
	}
	SET_MODULE_OWNER(&fops_B);
	
	PRINT("EXITING INIT_MODULE, MAJOR=%d\n",major);
	
	return 0;
	
}

void cleanup_module(void) {
	
	PRINT("IN CLEANUP_MODULE\n");
	
	// Un-registration
	if (unregister_chrdev(major, MODULE_NAME)<0)
		printk("FATAL ERROR: unregister_chrdev() failed\n");
	
	// Structure cleanup 
	FREE_GLOBALS();
	
}


