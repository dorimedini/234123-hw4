#include <stdio.h>
#include <stdlib.h>		// For exit() and srand()
#include <unistd.h>		// For fork() and close()
#include <sys/wait.h>	// For wait()
#include <errno.h>		// Guess
#include <pthread.h>	// Testing with threads
#include <semaphore.h>	// For use with threads
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>		// For time(), used in srand()
#include "snake.h"		// For the ioctl functions
#include "hw3q1.h"		// For some definitions
#include <sys/ioctl.h>

// Set this to 1 if you want to see the output of PRINT
#define HW4_TEST_DEBUG 0

// --------------------------------IMPORTANT------------------------------------------------
// These macros / functions need to be redefined for your code:
#define INSTALL_SCRIPT "/root/hw4/install.sh"
#define UNINSTALL_SCRIPT "/root/hw4/uninstall.sh"
char node_name[20];
char* get_node_name(int minor) {
	sprintf(node_name,"/dev/snake%d",minor);
	return node_name;
}
// I'm assuming the scripts are called like this:
//
//	./install.sh 6		// Does insmod and mknod * 6 (creates snake0,snake1,...,snake6 in /dev/)
//	./uninstall.sh		// Does rmmod and deletes created files	(rm -f /dev/snake*)
//
// Where '6' is the max number of games (see setup_snake()).
// To use this, write a script that behaves like the one above. Here is ours:
/* *********************************** install.sh ***************************************** 
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

# Install the module (no cleanup or building)
insmod ./snake.o max_games=$1

# Acquire the MAJOR number
major=`cat /proc/devices | grep snake | sed 's/ snake//'`

# Create the files for the games
i=0
while [ $i -lt $1 ]; do
	mknod /dev/snake$i c $major $i
	let i=i+1
done
*************************************** install.sh ****************************************/
/* *********************************** uninstall.sh ****************************************
#!/bin/bash

# Go to the correct directory
cd /root/hw4

# Do some cleanup
rm -f /dev/snake*
rmmod snake
************************************** uninstall.sh ***************************************/

/*******************************************************************************************
 ===========================================================================================
 ===========================================================================================
								GLOBALS & TYPEDEFS
 ===========================================================================================
 ===========================================================================================
 ******************************************************************************************/

// Error string output
char output[256];

// Color types
#define WHITE_COLOR 4
#define BLACK_COLOR 2

// Data passed to threads in test functions.
typedef struct {
	sem_t* sem_arr;	// Used for thread "communication"
} ThreadParam;

// Debugging the tester (I know. Shut up.)
#if HW4_TEST_DEBUG
	#define PRINT(...) printf(__VA_ARGS__)
	#define PRINT_IF(cond,...) if(cond) PRINT(__VA_ARGS__)
#else
	#define PRINT(...)
	#define PRINT_IF(...)
#endif

// This is set to TRUE if a process has forked using FORK() at the beginning of a test.
// That way, ASSERT()s know to call P_CLEANUP() before returning.
// Same goes for CLONE() and T_CLEANUP()
bool forked = FALSE;
bool cloned = FALSE;

// This is set to TRUE if the module has been installed with setup_snake().
// If you use ASSERT()s, this has to be set correctly!
// Just use setup_snake() and destroy_snake() for that,
// or better yet: SETUP_P() and DESTROY_P().
bool installed = FALSE;

// Use these for easier thread / process use. Used in macros.
// Array of thread IDs
pthread_t* tids=NULL;
// Array of process IDs
int* pids=NULL;
// Which child (process) am I?
int child_num=-1;
// How many threads are there (needed for cleanup function)?
int total_threads=-1;
// How many semaphores are in the ThreadParam sent as an argument to the thread functions?
int total_semaphores=-1;
// Global thread param
ThreadParam tp;

// Use these for test progress
int total_progress=-1;
int total_progress_indicators=-1;

/*******************************************************************************************
 ===========================================================================================
 ===========================================================================================
                            MACROS AND UTILITY FUNCTIONS
 ===========================================================================================
 ===========================================================================================
 ******************************************************************************************/
/* **************************
ASSERTS AND PRINTING
****************************/
#define PRINT_WIDTH 50

// ASSERT() uses P/T_CLEANUP() and destroy_snake(), so declare them here
void P_CLEANUP();
void T_CLEANUP();
void destroy_snake();

// If ASSERT() fails, it cleans up processes & threads and uninstalls the module
#define ASSERT(cond) do { \
		if (!(cond)) { \
			if (forked) { \
				P_CLEANUP(); \
				forked = FALSE; \
			} \
			if (cloned) { \
				T_CLEANUP(); \
				cloned = FALSE; \
			} \
			if (installed) \
				destroy_snake(); \
			sprintf(output, "FAILED, line %d: condition (" #cond ") is false",__LINE__); \
			return 0; \
		} \
	} while(0)

#define RUN_TEST(test) do { \
		int i,n = strlen(#test); \
		printf(#test); \
		for (i=0; i<PRINT_WIDTH-n-2; ++i) \
			printf("."); \
		if (HW4_TEST_DEBUG) \
			printf("\n"); \
		if (!(test())) \
			printf("%s",output); \
		else \
			printf("OK"); \
		printf(" \b\n"); \
	} while(0)
 
void START_TESTS() {
	int i;
	for(i=0; i<PRINT_WIDTH; ++i) printf("=");
	printf("\nSTART TESTS\n");
	for(i=0; i<PRINT_WIDTH; ++i) printf("=");
	printf("\n");
}
 
void END_TESTS() {
	int i;
	for(i=0; i<PRINT_WIDTH; ++i) printf("=");
	printf("\nDONE\n");
	for(i=0; i<PRINT_WIDTH; ++i) printf("=");
	printf("\n");
}

#define TEST_AREA(name) do { \
		int i; \
		for(i=0; i<PRINT_WIDTH; ++i) printf("-"); \
		printf("\nTesting "name" tests:\n"); \
		for(i=0; i<PRINT_WIDTH; ++i) printf("-"); \
		printf("\n"); \
	} while(0)

// Call this to update the percentage of completion of a test.
// Some tests may be long (stress testers) so this is useful.
// When we reach 100% I expect to output OK or FAILURE, so no
// need to output 100%... that's why I'm allowing 2 digits
void UPDATE_PROG(int percent) {
	printf("%2d%%\b\b\b",percent);
}

// Sets up a progress bar.
// The 'total' sent is the amount of calls to PROG_BUMP() needed to get from 0% to 100%
void PROG_SET(int total) {
	total_progress_indicators = total;
	total_progress = 0;
}

// Updates the progress percentage, using total_progress and total_progress_indicators
void PROG_BUMP() {
	UPDATE_PROG((total_progress++)*100/total_progress_indicators);
}

/* **************************************
SETUP
****************************************/

// These need P_WAIT(), declare here
void P_WAIT();

// Installs the module with max_games = n
void setup_snake(int n) {
	char n_char[4];
	sprintf(n_char, "%d", n);
	char *argv[] = { INSTALL_SCRIPT, n_char, '\0'};
	if (!fork()) {
		execv(INSTALL_SCRIPT, argv);
		exit(0);
	}
	else {
		P_WAIT();
		installed = TRUE;
	}
}

// Uninstalls the module
void destroy_snake() {
	if (!fork()) {
		execv(UNINSTALL_SCRIPT, '\0');
		exit(0);
	}
	else {
		P_WAIT();
		installed = FALSE;
	}
}

/* **************************************
PROCESS / THREAD HANDLING
****************************************/

// Creates n processes. Starts by creating an array to store the IDs
// and an integer to store which child you are.
void FORK(int n) {
	if (n>0) {
		pids = (int*)malloc(sizeof(int)*n);
		forked = TRUE;
	}
	child_num=0;
	int i;
	for (i=0; i<n; ++i) {
		pids[i] = fork();
		if (!pids[i]) {
			child_num = i+1;
			break;
		}
	}
}

// Creates n clones. IDs (NOT INTEGERS!) are stored in tids[], and the function + parameter
// sent to all threads is the same.
#define CLONE(n, func, arg) do { \
		if (!CLONE_AUX(n, func, arg)) return FALSE; \
	} while(0)

bool CLONE_AUX(int n, void* (*func)(void*), void* arg) {
	if (n>0) {
		tids = (pthread_t*)malloc(sizeof(pthread_t)*(n));
		cloned = TRUE;
	}
	total_threads = n;
	int i;
	for(i=0; i<total_threads; ++i)
		ASSERT(!pthread_create(tids + i, NULL, func, arg));
	return TRUE;
}

// Returns TRUE if the process is the father (uses child_num)
bool P_IS_FATHER() {
	return ((bool)(!child_num));
}

// Waits for all children
void P_WAIT() {
	int status;
	while(wait(&status) != -1);
}

// Stops all processes except the father, who waits for them.
void P_CLEANUP() {
	if (P_IS_FATHER())
		P_WAIT();
	else
		exit(0);
	free(pids);
	pids = NULL;
	child_num = -1;
}

// Used by the main thread, to join (wait) for the clones.
// Uses the tids[] array and the total_threads variable defined in CLONE()
void T_WAIT() {
	int i;
	for (i=0; i<total_threads; ++i)
		pthread_join(tids[i],NULL);
}
			
void T_CLEANUP() {
	int i;
	T_WAIT();
	for (i=0; i<total_semaphores; ++i)
		sem_destroy(tp.sem_arr + i);
	if (total_threads > 0)
		free(tids);
	if (total_semaphores > 0)
		free(tp.sem_arr);
	total_threads = -1;
	total_semaphores = -1;
	tids = NULL;
}

// Installs the module with 'games' games and forks 'procs' times.
// Including the father, there will be 'procs'+1 processes
void SETUP_P(int games, int procs) {
	setup_snake(games);
	FORK(procs);
}

void DESTROY_P() {
	P_CLEANUP();
	destroy_snake();
}

// Installs the module with 'games' games, creates 'threads' threads with the func function
// as the starting function. The argument sent is tp (ThreadParam), with 'total_sems' semaphores,
// where semaphore[i] is initialized to sem_values[i].
#define SETUP_T(games, threads, func, total_sems, sem_values) do { \
		if (!SETUP_T_AUX(games, threads, func, total_sems, sem_values)) return FALSE; \
	} while(0)

bool SETUP_T_AUX(int games, int threads, void* (*func)(void*), int total_sems, int* sem_values) {
	if (total_sems > 0)
		tp.sem_arr = (sem_t*)malloc(sizeof(sem_t)*(total_sems));
	total_semaphores = total_sems;
	int i;
	for(i=0; i<total_sems; ++i)
		sem_init(tp.sem_arr + i, 0, sem_values[i]);
	setup_snake(games);
	CLONE(threads,func,(void*)&tp);
	return TRUE;
}

void DESTROY_T() {
	T_CLEANUP();
	destroy_snake();
}


/* **************************************
GRID FUNCTIONS
****************************************/
// Returns TRUE if the string (NULL-terminated) sent is a valid printout of
// a grid after 0 moves.
// Assumes N=4
bool is_good_init_grid(char* str) {
	if (strlen(str) > GOOD_BUF_SIZE)
		return FALSE;
	// Generic grid strings:
	char grid_str[]="---------------\n"
					"|  1  2  3  . |\n"
					"|  .  .  .  . |\n"
					"|  .  .  .  . |\n"
					"| -1 -2 -3  . |\n"
					"---------------\n";
	// Indexes of grid_str[] where FOOD may be located
	int food_locations[] = {28,35,38,41,44,51,54,57,60,76};
	int i;
	for (i=0; i<10; ++i) {
		grid_str[food_locations[i]] = '*';
		if (!strcmp(grid_str,str)) return TRUE;
		grid_str[food_locations[i]] = '.';
	}
	return FALSE;
}

// Returns TRUE if the grid sent is a valid printout of a grid in ANY state.
bool is_good_grid(char* str) {
	if (strlen(str) > GOOD_BUF_SIZE)
		return FALSE;
	
	// First, parse the grid into a Matrix object.
	Matrix m;
	
	// A row of dashes:
	int i;
	for (i=0; i<(N+1)*3; ++i)
		if (str[i] != '-')
			return FALSE;
	if (str[i++] != '\n')
		return FALSE;
	
	// Each row starts with '|', then three characters defining the slot.
	// Parse them.
	int j,k;
	for (j=0; j<N; ++j) {
		if (str[i++] != '|')
			return FALSE;
		if (str[i++] != ' ')
			return FALSE;
		for (k=0; k<N; ++k) {
			if (str[i++] == '-') {	// Black snake segment
				if (str[i]-'0' < 0 || str[i]-'0' > N*N) return FALSE;	// Max snake size
				m[j][k] = -(str[i]-'0');
			}
			else if (str[i-1] != ' ')
				return FALSE;	// If it wasn't '-' it should have been ' '
			else if (str[i] == '.')
				m[j][k] = EMPTY;
			else if (str[i] == '*')
				m[j][k] = FOOD;
			else if (str[i]-'0' >= 0 && str[i]-'0' <= N*N)
				m[j][k] = str[i]-'0';	// White snake segment
			else
				return FALSE;	// No other options...
			if (str[++i] != ' ')
				return FALSE;	// Next slot should be a space character
		}
		if (str[++i] != '|' || str[++i] != '\n')
			return FALSE;
		++i;
	}
	
	// Done parsing. The rest should be '-' characters, and one trailing '\n' character
	while(i < GOOD_BUF_SIZE-1)
		if (str[i++] != '-')
			return FALSE;
	if (str[i] != '\n')
		return FALSE;
	
	// Make sure the grid makes sense.
	
	// Only one FOOD instance:
	int total_food = 0;
	for (i=0; i<N; ++i)
		for (j=0; j<N; ++j)
			if (m[i][j] == FOOD)
				total_food++;
	// If the board is full, it may have no food
	if (total_food != 1 || (total_food == 0 && !IsMatrixFull(&m)))
		return FALSE;
	
	// For each snake color, find the largest segment |X|.
	// make sure all segments x=1,2,...,|X| exist exactly once and next to each other.
	// Get the max segments:
	int max_w=0,max_b=0;
	for (i=0; i<N; ++i)
		for (j=0; j<N; ++j)
			if (m[i][j] != FOOD && m[i][j] != EMPTY) {
				max_w = m[i][j] > max_w ? m[i][j] : max_w;
				max_b = m[i][j] < max_b ? m[i][j] : max_b;
			}
	// Make sure they all exist exactly once, and next to each other:
	int w_segs[max_w], b_segs[max_b];
	for (i=0; i<max_w; ++i)
		w_segs[i]=0;
	for (i=0; i<max_b; ++i)
		b_segs[i]=0;
	for (i=1; i<=max_w; ++i)
		for (j=0; j<N; ++j)
			for (k=0; k<N; ++k)
				if (m[j][k] == i) {
					w_segs[i-1]++;
					if (w_segs[i-1] > 1)
						return FALSE;	// Too many segments
					if (i>1) {	// Previous segment must be around here somewhere...
						
					}
				}
	// Same thing for black player
	
	
	// That's it...
	return TRUE;
}


/* **************************************
GENERAL UTILITY
****************************************/

// Shuffles the contents of the given array.
void shuffle_arr(int* arr, int n) {
	int i;
	for (i=0; i<n; ++i) {
		int j = i + rand()%(n-i);
		int tmp = arr[i];
		arr[i]=arr[j];
		arr[j]=tmp;
	}
}

