#include <stdio.h>
#include <stdlib.h>		// For exit()
#include <unistd.h>		// For fork() and close()
#include <sys/wait.h>	// For wait()
#include <errno.h>		// Guess
#include <pthread.h>	// Testing with threads
#include <semaphore.h>	// For use with threads
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// --------------------------------IMPORTANT------------------------------------------------
// These macros need to be redefined for your code:
#define INSTALL_SCRIPT "/root/hw4/install.sh"
#define UNINSTALL_SCRIPT "/root/hw4/uninstall.sh"
#define NODE_NAME(n) "snake"#n
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

// Boolean type
typedef char bool;
#define TRUE	((char)1)
#define FALSE	((char)0)

// Debugging the tester (I know. Shut up.)
#define HW4_TEST_DEBUG 0
#if HW4_TEST_DEBUG
	#define PRINT(...) printf(__VA_ARGS__)
#else
	#define PRINT(...)
#endif

// This is set to TRUE if a process has forked using FORK() at the beginning of a test.
// That way, ASSERT()s know to call P_CLEANUP() before returning.
bool forked = FALSE;

// This is set to TRUE if the module has been installed with setup_snake().
// If you use ASSERT()s, this has to be set correctly!
// Just use setup_snake() and destroy_snake() for that,
// or better yet: SETUP() and DESTROY().
bool installed = FALSE;

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
#define PRINT_WIDTH 40

#define ASSERT(cond) do { \
		if (!(cond)) { \
			if (forked) { \
				P_CLEANUP(); \
				forked = FALSE; \
			} \
			if (installed) \
				destroy_snake(); \
			sprintf(output, "FAILED, line %d: condition " #cond " is false",__LINE__); \
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
 
#define START_TESTS() do { \
		int i; \
		for(i=0; i<PRINT_WIDTH; ++i) printf("="); \
		printf("\nSTART TESTS\n"); \
		for(i=0; i<PRINT_WIDTH; ++i) printf("="); \
		printf("\n"); \
	} while(0)
 
#define END_TESTS() do { \
		int i; \
		for(i=0; i<PRINT_WIDTH; ++i) printf("="); \
		printf("\nDONE\n"); \
		for(i=0; i<PRINT_WIDTH; ++i) printf("="); \
		printf("\n"); \
	} while(0)

// Call this to update the percentage of completion of a test.
// Some tests may be long (stress testers) so this is useful.
// When we reach 100% I expect to output OK or FAILURE, so no
// need to output 100%... that's why I'm allowing 2 digits
#define UPDATE_PROG(percent) do { \
		printf("%2d%%\b\b\b",percent); \
	} while(0)

/* **************************************
PROCESS / THREAD HANDLING
****************************************/

// Creates n processes. Starts by creating an array to store the IDs
// and an integer to store which child you are.
#define FORK(n) \
	int pids[n], child_num=0; \
	if (n>0) forked = TRUE; \
	do { \
		int i; \
		for (i=0; i<n; ++i) { \
			pids[i] = fork(); \
			if (!pids[i]) { \
				child_num = i+1; \
				break; \
			} \
		} \
	} while(0)

// Returns TRUE if the process is the father (uses child_num)
#define P_IS_FATHER() ((bool)(!child_num))

// Waits for all children
#define WAIT() do { \
		int status; \
		while(wait(&status) != -1); \
	} while(0)

// Stops all processes except the father, who waits for them.
#define P_CLEANUP() do { \
		if (P_IS_FATHER()) WAIT(); \
		else exit(0); \
	} while(0)

/* **************************************
SETUP
****************************************/

// Installs the module with max_games = n
void setup_snake(int n) {
	PRINT("In setup_snake()\n");
	char n_char[4];
	sprintf(n_char, "%d", n);
	char *argv[] = { INSTALL_SCRIPT, n_char, '\0'};
	if (!fork()) {
		PRINT("Installing...\n");
		execv(INSTALL_SCRIPT, argv);
		PRINT("Why am I here?\n");
		exit(0);
	}
	else {
		WAIT();
		PRINT("Done installing\n");
		installed = TRUE;
	}
}

// Uninstalls the module
void destroy_snake() {
	PRINT("In destroy_snake()\n");
	if (!fork()) {
		PRINT("Uninstalling...\n");
		execv(UNINSTALL_SCRIPT, '\0');
		exit(0);
	}
	else {
		WAIT();
		PRINT("Done uninstalling\n");
		installed = FALSE;
	}
}

#define SETUP(games,procs) \
	int fd; \
	setup_snake(games); \
	FORK(procs)

#define DESTROY() \
	P_CLEANUP(); \
	destroy_snake();

/*******************************************************************************************
 ===========================================================================================
 ===========================================================================================
                                   TEST FUNCTIONS
 ===========================================================================================
 ===========================================================================================
 ******************************************************************************************/
/* ***************************
 OPEN/RELEASE TESTS
*****************************/

// Test simple open and release.
// Make sure two processes do it, otherwise one will be stuck forever...
// calling open() blocks
bool open_release_simple() {
	SETUP(1,1);
	switch(child_num) {
	case 0:
	case 1:
		fd = open("/dev/"NODE_NAME(0),O_RDWR);
		PRINT("Child #%d opened file, fd=%d\n",child_num,fd);
		ASSERT(fd >= 0);
		ASSERT(!close(fd));
		PRINT("Child #%d has opened and closed successfully\n",child_num);
	}
	DESTROY();
	return TRUE;
}

// Test failure of release before open
// CAN'T DO THAT, NO FILE DESCRIPTOR

// Test failure of two releases (even after one open)
bool two_releases() {
	SETUP(1,1);
	switch(child_num) {
	case 0:
	case 1:
		fd = open("/dev/"NODE_NAME(0),O_RDWR);
		ASSERT(fd >= 0);
		ASSERT(!close(fd));
		ASSERT(close(fd));
		fd = open("/dev/"NODE_NAME(0),O_RDWR);
	}
	DESTROY();
	return TRUE;
}

// Test failure of open-release-open (can't re-open a game!)
bool open_release_open() {
	SETUP(1,1);
	switch(child_num) {
	case 0:
	case 1:
		fd = open("/dev/"NODE_NAME(0),O_RDWR);
		ASSERT(fd >= 0);
		ASSERT(!close(fd));
		fd = open("/dev/"NODE_NAME(0),O_RDWR);
		ASSERT(fd < 0);
	}
	DESTROY();
	return TRUE;
}

// Test two different processes opening/releasing


// Test two threads opening/releasing


// Test race - create 10 threads to try to open the same game,
// and make sure only two succeed each time.
// Do that 1000 times (so if there is a deadlock situation, we might catch it).


// Test the same thing, only with processes (forks)
bool open_race_processes() {
	long i;
	for (i=0; i<1000; ++i) {
		UPDATE_PROG(i/10);
		SETUP(1,10);
		if (child_num) {	// All 10 children should try to open files
			fd = open("/dev/snake0",O_RDWR);
			fd < 0? exit(1) : exit(0);	// Send the parent success status
		}
		else {				// The parent should read the exit status
			int status;
			char opened, failed;
			opened = failed = 0;
			while(wait(&status) != -1) {
				WEXITSTATUS(status) ? ++failed : ++opened;
			}
			ASSERT(failed == 8);
			ASSERT(opened == 2);
		}
		DESTROY();
	}
	return TRUE;
}


/*******************************************************************************************
 ===========================================================================================
 ===========================================================================================
										MAIN
 ===========================================================================================
 ===========================================================================================
 ******************************************************************************************/

int main() {
	
	// Prevent output buffering, so we don't see weird shit when multiple processes are active
	setbuf(stdout, NULL);
	
	// Test!
	START_TESTS();
	RUN_TEST(open_release_simple);
	RUN_TEST(two_releases);
	RUN_TEST(open_release_open);
	RUN_TEST(open_race_processes);
	END_TESTS();
	
	// That's all folks
	return 0;
	
}

