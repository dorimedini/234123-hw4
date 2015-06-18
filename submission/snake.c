#include "snake.h"
#include "hw3q1.h"
#include <linux/errno.h>
#include <linux/module.h>
#include <asm-i386/semaphore.h>
#include <linux/fs.h>
#include <asm-i386/uaccess.h>	// For copy_to/from_user
#include "hw3q1.h"				// For some definitions
#include <linux/random.h>		// For get_random_bytes()
MODULE_LICENSE("GPL");

/*******************************************************************************************
 ===========================================================================================
 ===========================================================================================
                                   DEBUG FUNCTIONS
 ===========================================================================================
 ===========================================================================================
 ******************************************************************************************/
// Set this to 1 for debug mode, or 0 for production
#define HW4_DEBUG 0

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
ErrorCode Init(Matrix *matrix) {
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
	if (RandFoodLocation(matrix) != ERR_OK)
		return ERR_BOARD_FULL;
	
	return ERR_OK;
}

bool OutOfBounds(Point p) {
	return (p.x < 0 || p.x >(N - 1) || p.y < 0 || p.y >(N - 1));
}

bool IsAvailable(Matrix *matrix, Point p) {
	return
		/* is out of bounds */
		!(OutOfBounds(p) ||
		/* is empty */
		((*matrix)[p.y][p.x] != EMPTY && (*matrix)[p.y][p.x] != FOOD));
}

ErrorCode RandFoodLocation(Matrix *matrix) {
	Point p;
	do {
		get_random_bytes(&p.x,sizeof(int));
		get_random_bytes(&p.y,sizeof(int));
		p.x = p.x < 0? -p.x : p.x;
		p.y = p.y < 0? -p.y : p.y;
		p.x %= N;
		p.y %= N;
	} while (!(IsAvailable(matrix, p) || IsMatrixFull(matrix)));
	
	if (IsMatrixFull(matrix))
		return ERR_BOARD_FULL;

	(*matrix)[p.y][p.x] = FOOD;
	return ERR_OK;
}

ErrorCode Update(Matrix *matrix, Player player, Direction dir, int* hunger_counter) {
	Point p;
	ErrorCode e = GetInputLoc(matrix, player, &p, dir);
	if(e != ERR_OK) return e;
	if (!CheckTarget(matrix, player, p)) {
		return ERR_ILLEGAL_MOVE;
	}
	e = CheckFoodAndMove(matrix, player, p, hunger_counter);
	if (e != ERR_OK) return e;							// Could also return ERR_BOARD_FULL. Also a tie.
	if (IsMatrixFull(matrix)) return ERR_BOARD_FULL;	// Tie

	return ERR_OK;
}

ErrorCode GetInputLoc(Matrix *matrix, Player player, Point* p, Direction dir) {
	if (dir != UP   && dir != DOWN && dir != LEFT && dir != RIGHT) {
		return ERR_INVALID_MOVE;
	}

	if (GetSegment(matrix, player, p) != ERR_OK)
		return ERR_SEGMENT_NOT_FOUND;

	switch (dir) {
		case UP:    --p->y; break;
		case DOWN:  ++p->y; break;
		case LEFT:  --p->x; break;
		case RIGHT: ++p->x; break;
	}
	return ERR_OK;
}

ErrorCode GetSegment(Matrix *matrix, int segment, Point* out_p) {
	Point p;
	/* just run through all the matrix */
	for (p.x = 0; p.x < N; ++p.x) {
		for (p.y = 0; p.y < N; ++p.y) {
			if ((*matrix)[p.y][p.x] == segment) {
				*out_p = p;
				return ERR_OK;
			}
		}
	}
	out_p->x = out_p->y = -1;
	return ERR_SEGMENT_NOT_FOUND;
}

bool CheckTarget(Matrix *matrix, Player player, Point p) {
	/* is empty or is the tail of the snake (so it will move the next
	to make place) */
	return (IsAvailable(matrix, p) || 
			/* If it's out of bounds, don't check for the tail! */
			(!OutOfBounds(p) && (*matrix)[p.y][p.x] == player * GetSize(matrix, player)));
}

int GetSize(Matrix *matrix, Player player) {
	/* check one by one the size */
	Point p, next_p;
	int segment = 0;
	while (TRUE) {
		if (GetSegment(matrix, segment += player, &next_p) == ERR_SEGMENT_NOT_FOUND)
			break;
		p = next_p;
	}

	return (*matrix)[p.y][p.x] * player;
}

ErrorCode CheckFoodAndMove(Matrix *matrix, Player player, Point p, int* hunger_counter) {
	/* if the player did come to the place where there is food */
	if ((*matrix)[p.y][p.x] == FOOD) {
		*hunger_counter = K;

		IncSizePlayer(matrix, player, p);

		if (RandFoodLocation(matrix) != ERR_OK)
			return ERR_BOARD_FULL;	// Tie
	}
	else { /* check hunger */
		if (--(*hunger_counter) == 0) {
			return ERR_SNAKE_IS_TOO_HUNGRY;
		}

		AdvancePlayer(matrix, player, p);
	}
	return ERR_OK;
}

void IncSizePlayer(Matrix *matrix, Player player, Point p) {
	/* go from last to first so the identifier is always unique */
	Point p_tmp;
	int segment = GetSize(matrix, player)*player;
	while (TRUE) {
		GetSegment(matrix, segment, &p_tmp);
		(*matrix)[p_tmp.y][p_tmp.x] += player;
		segment -= player;
		if (segment == 0)
			break;
	}
	(*matrix)[p.y][p.x] = player;
}

void AdvancePlayer(Matrix *matrix, Player player, Point p) {
	/* go from last to first so the identifier is always unique */
	Point p_tmp, p_tail;
	GetSegment(matrix, GetSize(matrix, player) * player, &p_tail);
	int segment = GetSize(matrix, player) * player;
	while (TRUE) {
		GetSegment(matrix, segment, &p_tmp);
		(*matrix)[p_tmp.y][p_tmp.x] += player;
		segment -= player;
		if (segment == 0)
			break;
	}
	(*matrix)[p_tail.y][p_tail.x] = EMPTY;
	(*matrix)[p.y][p.x] = player;
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
	Semaphore grid_lock;		// (RESOURCE #3) Protects the matrix field & hunger states of the players
	GameState state;			// The state of the game
	int white_hunger;			// These two are protected by grid_lock (used in the original snake game functions)
	int black_hunger;
} Game;

/* ****************************
 GLOBALS & FORWARD DECLARATIONS
 *****************************/
// Forward declarations
int our_open(struct inode*, struct file*);
int our_release(struct inode*, struct file*);
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
static Game games[256];

// Different file operations for white or black players
struct file_operations fops_W = {
	.open=		our_open,
	.release=	our_release,
	.read=		our_read,
	.write=		our_write_W,
	.llseek=	our_llseek,
	.ioctl=		our_ioctl_W,
	.owner=		THIS_MODULE,
};
struct file_operations fops_B = {
	.open=		our_open,
	.release=	our_release,
	.read=		our_read,
	.write=		our_write_B,
	.llseek=	our_llseek,
	.ioctl=		our_ioctl_B, 
	.owner=		THIS_MODULE,
};

/* ****************************
 UTILITY FUNCTIONS
 *****************************/
// Nicely formats game state
#if HW4_DEBUG
static char* stringify_state(GameState gs) {
	switch(gs) {
	case PRE_START: return "PRE_START";
	case ACTIVE: return "ACTIVE";
	case W_WIN: return "W_WIN";
	case B_WIN: return "B_WIN";
	case TIE: return "TIE";
	case DESTROYED: return "DESTROYED";
	}
	return "NO_SUCH_STATE";
}
#endif /* HW4 DEBUG */

// Checks to see if the game is in the state sent
static int in_state(int minor, GameState gs) {
	Game* game = games+minor;
	down_interruptible(&game->state_lock);
	if (game->state == gs) {
		up(&game->state_lock);
		return 1;
	}
	up(&game->state_lock);
	return 0;
}

// Checks to see if the game is in DESTROYED state
static int is_destroyed(int minor) {
	return in_state(minor, DESTROYED);
}

// Check to see if the game is playable (ACTIVE) state
static int is_active(int minor) {
	return in_state(minor, ACTIVE);
}

// Destroys the game (changes the state)
static void destroy_game(int minor) {
	Game* game = games+minor;
	down_interruptible(&game->state_lock);
	game->state = DESTROYED;
	up(&game->state_lock);
}

// This macro return -10 from any function if the game identified by
// the minor number has been destroyed.
#define CHECK_DESTROYED(minor) do { \
		if (is_destroyed(minor)) { \
			PRINT("GAME DESTROYED! %d RETURNING -10\n",current->pid); \
			Game* game = games+minor; \
			up(&game->black_move); \
			up(&game->white_move); \
			return -10; \
		} \
	} while(0)

// Like the above macro, but for any non-ACTIVE state.
// This is called via write_aux(), and we need to make sure all processes get the signal.
// For example: two processes playing as the black player try to move while it's
// the black player's turn. They both lock black_move. Then, the white player calls close().
// Thus, black_move is signalled, but only one black process picks up the signal! While one
// process exits in error from write_aux(), the other will be stuck...
// To prevent this, signal before returning.
#define ASSERT_ACTIVE(minor,moves) do { \
		if (!is_active(minor)) { \
			Game* game = games+minor; \
			up(&game->black_move); \
			up(&game->white_move); \
			return moves; \
		} \
	} while(0)

// Used to handle invalid input.
// If the game was active when the input was sent, make the player lose.
#define ASSERT_VALID_MOVE(minor,move,is_black) do { \
		if (!is_valid_move(move)) { \
			if (is_active(minor)) \
				set_state(minor, is_black? W_WIN : B_WIN); \
			Game* game = games+minor; \
			up(&game->black_move); \
			up(&game->white_move); \
			return -10; \
		} \
	} while(0)

// Invalid moves should always return error.
static bool is_valid_move(char move) {
	return (move == '2' || move == '4' || move == '6' || move == '8');
}

// Use this to read the win state, as required for SNAKE_GET_WINNER
static int get_winner(int minor) {
	int ret;
	Game* game = games+minor;
	down_interruptible(&game->state_lock);
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

// Gets the minor number from a given file pointer
static int get_minor(struct file *filp) {
	return *((int*)filp->private_data);
}

// Updates the state of the game
static void set_state(int minor, GameState state) {
	Game* game = games+minor;
	down_interruptible(&game->state_lock);
	game->state = state;
	up(&game->state_lock);
}

/* ****************************
 FOPS AUXILLARY FUNCTIONS
 *****************************/

// Use this to simplify the ioctl() functions
static int our_ioctl_aux(struct inode* i, bool is_black, int cmd) {
	int minor = MINOR(i->i_rdev);	// Get minor
	CHECK_DESTROYED(minor);			// Make sure the game wasn't released
	switch(cmd) {
	case SNAKE_GET_WINNER:
		return get_winner(minor);
	case SNAKE_GET_COLOR:
		return is_black ? 2 : 4;
	default:
		return -ENOTTY;
	}
}

/**
 * Use this to simplify the write() functions.
 *
 * RULE OF THUMB:
 * - If the input is a legal move BUT we can't move (game over / destroyed), return the number
 *   of moves successfully written (moved).
 * - If the input is an illegal character (even if it came after some legal ones) return in error.
 * - If the input is illegal AND the game is over/destroyed, return in error.
 */
static ssize_t our_write_aux(struct file *filp, const char *buf, size_t n, loff_t *f_pos, bool is_black) {
	
	PRINT("In write() with n=%d, %s player\n",n,is_black? "Black":"White");
	
	int minor = get_minor(filp);	// Get the minor number
	CHECK_DESTROYED(minor);			// Make sure the game wasn't released
	
	// If n=0, return 0. It's legal.
	if (!n) return 0;
	
	// If the buffer is NULL, return EFAULT (Piazza 429)
	if (!buf) return -EFAULT;
	
	// Get the moves.
	char moves[n];
	int ret = copy_from_user(moves,buf,n);
	
	// If NOTHING was copied successfully, return in error.
	if (ret == n) return -EFAULT;
	
	// Otherwise, perform (n-ret) moves (ret is the amount NOT copied, so n-ret WERE copied).
	int current_move;
	Game* game = games+minor;
	for (current_move=0; current_move<n-ret; ++current_move) {
		
		PRINT("In write with %s player (pid %d), move #%d is '%c'. Waiting for signal...\n",is_black? "Black":"White",current->pid,current_move+1,moves[current_move]);
		
		// Wait for our turn
		down_interruptible(is_black? &game->black_move : &game->white_move);
		
		PRINT("In write with %s player, move #%d, locked the move lock\n",is_black? "Black":"White",current_move+1);
		
		// Check again for destruction state!
		// This may have happened while we were waiting for our turn.
		// Either way, if a player tries to move in a non-active game,
		// return
		CHECK_DESTROYED(minor);
		
		PRINT("In write with %s player, move #%d, game is active\n",is_black? "Black":"White",current_move+1);
		
		// If this is an illegal move, return in error.
		// This should happen BEFORE testing if the game is active!
		ASSERT_VALID_MOVE(minor,moves[current_move],is_black);
		
		// If the game is over, return NOW with the number of written moves.
		ASSERT_ACTIVE(minor, current_move);
		
		// Lock the grid and try to move.
		// What happens next depends on the error code
		down_interruptible(&game->grid_lock);
		ErrorCode e = Update(
							&game->matrix,
							is_black ? BLACK : WHITE,
							(int)(moves[current_move]-'0'),
							is_black? &game->black_hunger : &game->white_hunger
						);
		up(&game->grid_lock);
		
		PRINT("In write with %s player, move #%d, update return value is %d\n",is_black? "Black":"White",current_move+1,e);
		
		switch(e) {
		// OK: Nothing to do
		case ERR_OK: break;
		// Board full: The move was legal and the board is now full! It's a tie
		case ERR_BOARD_FULL: 
			set_state(minor,TIE);
			break;
		// The rest of the states are loss states
		case ERR_SNAKE_IS_TOO_HUNGRY:
		case ERR_ILLEGAL_MOVE:
		case ERR_INVALID_MOVE:	// This shouldn't happen, we've checked...
		case ERR_SEGMENT_NOT_FOUND:
			set_state(minor, is_black ? W_WIN : B_WIN);
			break;
		}

		PRINT("%s player signalling...\n",is_black? "Black":"White");
		
		// Anyway, if the error code wasn't ERR_OK, the game is over so we can release both players.
		if (e != ERR_OK) {
			up(&game->black_move);
			up(&game->white_move);
		}
		else {
			// Signal the other player it's her turn
			up(is_black ? &game->white_move : &game->black_move);\
		}
		
	}
	
	// Done moving! Return the number of written bytes (current_move)
	return current_move;
	
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
	down_interruptible(&game->state_lock);
	if (game->state != PRE_START) {
		up(&game->state_lock);
		return -ENOSPC;
	}
	up(&game->state_lock);
	
	// Try to join the game
	if (down_trylock(&game->w_player_join)) {		// If player 1 is already in-game
		if (down_trylock(&game->b_player_join)) {	// ...and so is player 2
			// "No space left on the device". This should happen only if player 2 joined but
			// didn't lock game_state_locks[minor] yet.
			return -ENOSPC;							
		}
		// I am player 2
		else {
			filp->f_op = &fops_B;						// Switch the writing function (so it knows I'm player 2)
			filp->private_data = (void*)&game->minor;	// Save the minor number for later use
			down_interruptible(&game->state_lock);		// Update game state
			game->state = ACTIVE;
			up(&game->state_lock);
			up(&game->white_move);						// Tell player 1 we're good to go! He can move
		}
	}
	// Else: I am player 1
	else {
		filp->f_op = &fops_W;						// Switch the writing function (so it knows I'm player 1)
		game->minor = minor;						// Inform the Game structure which minor it is
		filp->private_data = (void*)&game->minor;	// Save the minor number for later use
		down_interruptible(&game->white_move);		// Wait for player 2 (blocking operation)
		up(&game->white_move);						// Signal the fact that it's my turn
	}
	
	// Done with game-starting logic
	return 0;

}

int our_release(struct inode* i, struct file* filp) {
	
	// Get minor
	int minor = MINOR(i->i_rdev);
	
	// Destroy the game
	destroy_game(minor);
	
	// Signal the other player, who may be waiting to move.
	// If we don't do this, the other player may be trapped, forever
	// waiting for his turn which won't come...
	// Also signal myself. Maybe there are two processes playing as
	// the same player, one releasing and one writing. If the writing
	// process is waiting for it's turn, we should signal it here
	// already...
	PRINT("Signalling both players...\n");
	Game* game = games+minor;
	up(&game->white_move);
	up(&game->black_move);
	
	return 0;
	
}

ssize_t our_read(struct file *filp, char *buf, size_t n, loff_t *f_pos) {
	
	// Get the minor
	int minor = get_minor(filp);
	
	// Check if the operation is valid
	CHECK_DESTROYED(minor);
	
	// If size=0, return 0 (successfully)
	if (!n) return 0;
	
	// Piazza 429:
	if (!buf) return -EFAULT;
	
	// Get the game and lock the grid, read the data, unlock
	Game* game = games+minor;
	down_interruptible(&game->grid_lock);
	char our_buf[n];
	Print(&game->matrix, our_buf, n);
	up(&game->grid_lock);
	
	// If the buffer is too large, leave trailing zeros
	if (n>GOOD_BUF_SIZE) {
		int i;
		for(i=GOOD_BUF_SIZE; i<n; ++i)
			our_buf[i] = 0;
	}
	
	// Copy the data to the user.
	// If it fails completely (n bytes not written), return EFAULT.
	// Otherwise, return the number of copied bytes.
	// As ret is the number of bytes NOT copied, return n-ret.
	int ret = copy_to_user(buf, our_buf, n);
	return ret == n ? -EFAULT : n-ret;

}

ssize_t our_write_W(struct file *filp, const char *buf, size_t n, loff_t *f_pos) {
	int ret = our_write_aux(filp, buf, n, f_pos, FALSE);
	PRINT("%d IN OUR_WRITE_W, RETURNED %d\n",current->pid,ret);
	return ret;
}

ssize_t our_write_B(struct file *filp, const char *buf, size_t n, loff_t *f_pos) {
	int ret = our_write_aux(filp, buf, n, f_pos, TRUE);
	PRINT("%d IN OUR_WRITE_B, RETURNED %d\n",current->pid,ret);
	return ret;
}


int our_ioctl_W(struct inode *i, struct file *filp, unsigned int cmd, unsigned long arg) {
	return our_ioctl_aux(i,FALSE,cmd);
}

int our_ioctl_B(struct inode *i, struct file *filp, unsigned int cmd, unsigned long arg) {
	return our_ioctl_aux(i,TRUE,cmd);
}

loff_t our_llseek(struct file *filp, loff_t x, int n) {
	return -ENOSYS;
}


int init_module(void) {
	
	// Initial values, game setup and semaphore setup
	int i;
	for (i=0; i<max_games; ++i) {
		
		// Initialize the board
		if (Init(&games[i].matrix) != ERR_OK)
			return -EPERM;						// Should never happen...
		
		// Initialize other fields
		games[i].state = PRE_START;				// No one has called open() yet
		games[i].minor = -1;					// No minor number yet
		games[i].white_hunger = K;				// Both snakes are healthy & happy
		games[i].black_hunger = K;				// ...BUT NOT FOR LONG
		sema_init(&games[i].state_lock, 1);		// We need locks for each game
		sema_init(&games[i].grid_lock, 1);		// Player must lock this successfully to r/w the game grid
		sema_init(&games[i].white_move, 0);		// White player must lock this to move (signalled by black player)
		sema_init(&games[i].black_move, 0);		// Black player must lock this to move (signalled by white player)
		sema_init(&games[i].w_player_join, 1);	// Player must lock this successfully to join as the white player
		sema_init(&games[i].b_player_join, 1);	// Player must lock this successfully to join as the black player
	}
	
	// Registration
	major = register_chrdev(0, MODULE_NAME, &fops_B);	// Make black the default. Down with racism!
	if (major < 0)	// FAIL
		return major;
	SET_MODULE_OWNER(&fops_B);
	
	return 0;
	
}

void cleanup_module(void) {
	
	// Un-registration
	if (unregister_chrdev(major, MODULE_NAME)<0)
		printk("FATAL ERROR: unregister_chrdev() failed\n");
	
}


