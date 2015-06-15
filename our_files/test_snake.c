#include "test_snake.h"

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
// calling open() blocks the process
bool open_release_simple() {
	int i, tries = 30;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		SETUP_P(1,1);
		int fd;
		switch(child_num) {
		case 0:
		case 1:
			fd = open(get_node_name(0),O_RDWR);
			ASSERT(fd >= 0);
			ASSERT(!close(fd));
		}
		DESTROY_P();
	}
	return TRUE;
}

// Test failure of two releases (even after one open)
bool two_releases_processes() {
	int i, tries = 30;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		SETUP_P(1,1);
		int fd;
		switch(child_num) {
		case 0:
		case 1:
			fd = open(get_node_name(0),O_RDWR);
			ASSERT(fd >= 0);
			ASSERT(!close(fd));
			ASSERT(close(fd));
		}
		DESTROY_P();
	}
	return TRUE;
}

// Same thing, with threads
void* two_releases_func(void* arg) {
	ThreadParam *tp = (ThreadParam*)arg;
	int fd = open(get_node_name(0),O_RDWR);
	int val;
	if(fd < 0) {
		sem_post(tp->sem_arr);			// Did I fail to open?
		sem_getvalue(tp->sem_arr,&val);
		PRINT("Couldn't open, val=%d\n",val);
	}
	else {
		if(close(fd)) {
			sem_post(tp->sem_arr+1);		// If not, did I fail to close?
			sem_getvalue(tp->sem_arr+1,&val);
			PRINT("Couldn't close, val=%d\n",val);
		}
		if(!close(fd)) {
			sem_post(tp->sem_arr+2);		// If not, can I close again?
			sem_getvalue(tp->sem_arr+2,&val);
			PRINT("Closed again... val=%d\n",val);
		}
	}
	return NULL;
}
bool two_releases_threads() {
	// sem1 - how many threads failed to open?
	// sem2 - how many threads failed to close once?
	// sem3 - how many threads succeeded in closing twice?
	int values[] = {0,0,0};
	int i, tries = 30;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		SETUP_T(1,2,two_releases_func,3,values);
		T_WAIT();
		int val;
		sem_getvalue(tp.sem_arr,&val);
		ASSERT(val == 0);
		sem_getvalue(tp.sem_arr+1,&val);
		ASSERT(val == 0);
		sem_getvalue(tp.sem_arr+2,&val);
		ASSERT(val == 0);
		DESTROY_T();
	}
	return TRUE;
}

// Test failure of open-release-open (can't re-open a game!)
bool open_release_open() {
	int i, tries = 30;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		SETUP_P(1,1);
		int fd;
		switch(child_num) {
		case 0:
		case 1:
			fd = open(get_node_name(0),O_RDWR);
			ASSERT(fd >= 0);
			ASSERT(!close(fd));
			fd = open(get_node_name(0),O_RDWR);
			ASSERT(fd < 0);
		}
		DESTROY_P();
	}
	return TRUE;
}

// Make sure the first one is the white player
bool first_open_is_white() {
	
	// This may sometimes succeed, so try this many times
	int i, fd, trials = 50;
	for (i=0; i<trials; ++i) {
		
		UPDATE_PROG(i*100/trials);
		
		SETUP_P(1,1);
		switch(child_num) {
		case 0:
			fd = open(get_node_name(0),O_RDWR);
			ASSERT(fd >= 0);
			ASSERT(ioctl(fd,SNAKE_GET_COLOR,NULL) == WHITE_COLOR);
			close(fd);
			break;
		case 1:
			usleep(10000);	// 10ms, should be enough for father to be first to open
			fd = open(get_node_name(0),O_RDWR);
			ASSERT(fd >= 0);
			usleep(10000);	// To prevent closing of a game before the white player can read the color
			close(fd);
			break;
		}
		DESTROY_P();
	}
	
	return TRUE;
}

// Test race - create 10 threads to try to open the same game, and make sure only two
// succeed each time.
// Do that T_OPEN_RACE_TRIES times (so if there is a deadlock situation, we might catch it).
// Uses open_race_thread_func and a ThreadParam passed to it to keep track of what's
// going on.
#define T_OPEN_RACE_TRIES 200
void* open_race_thread_func(void* arg) {
	ThreadParam *tp = (ThreadParam*)arg;
	int file_d = open(get_node_name(0),O_RDWR);
	if (file_d >= 0)
		sem_wait(tp->sem_arr);
	sem_post(tp->sem_arr+1);	// Signal father
	sem_wait(tp->sem_arr+2);	// Wait for father
	if (file_d >= 0)			// If I've open()ed, now I need to close()
		close(file_d);
	return NULL;
}
bool open_race_threads() {
	int i;
	for (i=0; i<T_OPEN_RACE_TRIES; ++i) {
		UPDATE_PROG(i*100/T_OPEN_RACE_TRIES);
		
		// Let 10 threads race (try to open() the same node)
		int threads = 10;
		int values[] = {threads,0,0};
		
		// Each thread that successfully open()s should lock sem1.
		// The father thread should lock sem2 #threads times, and each thread
		// Should signal this once. That way the father thread has an indication
		// of when it should keep running.
		// The father thread should signal sem3 #threads times to tell the clones
		// they can continue.
		// sem2 and sem3 are like a barrier.
		SETUP_T(1,threads,open_race_thread_func,3,values);
		
		int i;
		for (i=0; i<threads; ++i)			// Wait for clones
			sem_wait(tp.sem_arr+1);
		for (i=0; i<threads; ++i)			// Signal clones
			sem_post(tp.sem_arr+2);
		
		// Make sure exactly two succeeded
		int val;
		sem_getvalue(tp.sem_arr,&val);
		ASSERT(val == threads-2);		// Only 2 out of all threads should succeed in locking
		DESTROY_T();
	}
	return TRUE;
}

// Test the same thing, only with processes (forks)
#define P_OPEN_RACE_TRIES 200
bool open_race_processes() {
	int i;
	for (i=0; i<P_OPEN_RACE_TRIES; ++i) {
		UPDATE_PROG(i*100/P_OPEN_RACE_TRIES);
		SETUP_P(1,10);
		if (child_num) {	// All 10 children should try to open files
			int fd = open("/dev/snake0",O_RDWR);
			fd < 0 ? exit(1) : exit(0);	// Send the parent success status
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
		DESTROY_P();
	}
	return TRUE;
}

// Stress test: create GAMES_RACE_T_NUM_GAMES games and GAMES_RACE_T_NUM_THREADS threads,
// each randomly trying to join games until all games are full.
// IMPORTANT: GAMES_RACE_T_NUM_GAMES cannot be greater than 25, because the maximum number of threads in linux 2.4
// is 256, so if you set GAMES_RACE_T_NUM_GAMES=26 the function will try to create 26*10=260>256 threads.
#define GAMES_RACE_NUM_TRIES 50
#define GAMES_RACE_T_NUM_GAMES 25
#define GAMES_RACE_T_NUM_THREADS (10*GAMES_RACE_T_NUM_GAMES)
void* games_race_func(void* arg) {
	
	ThreadParam *p = (ThreadParam*)arg;
	
	// Randomize the order of the games
	int game_numbers[GAMES_RACE_T_NUM_GAMES];
	int i;
	for(i=0; i<GAMES_RACE_T_NUM_GAMES; ++i)
		game_numbers[i] = i;
	shuffle_arr(game_numbers,GAMES_RACE_T_NUM_GAMES);
	
	// Try to open a game!
	// First game successfully opened, update it's semaphore and return.
	// If no game opened, update the last semaphore and return.
	for(i=0; i<GAMES_RACE_T_NUM_GAMES; ++i) {
		int fd = open(get_node_name(game_numbers[i]),O_RDWR);
		if (fd >= 0) {
			PROG_BUMP();
			sem_post(p->sem_arr+game_numbers[i]);
			close(fd);
			return NULL;
		}
	}
	PROG_BUMP();
	sem_post(p->sem_arr+GAMES_RACE_T_NUM_GAMES);
	return NULL;
}
bool games_race_threads() {
	
	// Progress is updated every semaphore lock.
	// Each thread locks exactly one, so...
	PROG_SET(GAMES_RACE_T_NUM_THREADS*GAMES_RACE_NUM_TRIES);
	
	int j;
	for(j=0; j<GAMES_RACE_NUM_TRIES; ++j) {
		
		// GAMES_RACE_T_NUM_GAMES semaphores indicate how many threads opened a game.
		// The last semaphore indicates how many failed.
		int values[GAMES_RACE_T_NUM_GAMES+1] = { 0 };
		SETUP_T(GAMES_RACE_T_NUM_GAMES, GAMES_RACE_T_NUM_THREADS, games_race_func, GAMES_RACE_T_NUM_GAMES+1, values);
		T_WAIT();
		int val, i;
		for (i=0; i<GAMES_RACE_T_NUM_GAMES; ++i) {
			sem_getvalue(tp.sem_arr+i,&val);
			ASSERT(val == 2);
		}
		sem_getvalue(tp.sem_arr+GAMES_RACE_T_NUM_GAMES,&val);
		ASSERT(val == GAMES_RACE_T_NUM_THREADS-2*GAMES_RACE_T_NUM_GAMES);
		DESTROY_T();
	}
	
	return TRUE;
}

// Same thing, but with processes
#define GAMES_RACE_T_NUM_PROCS  (10*GAMES_RACE_T_NUM_GAMES)
bool games_race_processes() {
	
	
	int j;
	for(j=0; j<GAMES_RACE_NUM_TRIES; ++j) {
		
		UPDATE_PROG(j*100/GAMES_RACE_NUM_TRIES);
		
		SETUP_P(GAMES_RACE_T_NUM_GAMES,GAMES_RACE_T_NUM_PROCS);
		if (child_num) {
		
			// Randomize the order of the games
			int game_numbers[GAMES_RACE_T_NUM_GAMES];
			int i;
			for(i=0; i<GAMES_RACE_T_NUM_GAMES; ++i)
				game_numbers[i] = i;
			shuffle_arr(game_numbers,GAMES_RACE_T_NUM_GAMES);
			
			// Try to open a game!
			// First game successfully opened, exit() with it's index.
			// If no game opened, exit() with the last index.
			for(i=0; i<GAMES_RACE_T_NUM_GAMES; ++i) {
				int fd = open(get_node_name(game_numbers[i]),O_RDWR);
				if (fd >= 0) {
					close(fd);
					exit(game_numbers[i]);
				}
			}
			exit(GAMES_RACE_T_NUM_GAMES);
		}
		else {
			int status, results[GAMES_RACE_T_NUM_GAMES+1] = { 0 };
			while(wait(&status) != -1) {
				results[WEXITSTATUS(status)]++;
			}
			int i;
			for (i=0; i<GAMES_RACE_T_NUM_GAMES; ++i)
				ASSERT(results[i] == 2);
			ASSERT(results[GAMES_RACE_T_NUM_GAMES] == GAMES_RACE_T_NUM_PROCS-2*GAMES_RACE_T_NUM_GAMES);
		}
		DESTROY_P();
	}
	return TRUE;
}

/* ***************************
 READ TESTS
*****************************/
// Test multiple readers (asynchronous processes)
bool many_readers_p() {
	int i,num_tries = 50;
	char buf[GOOD_BUF_SIZE+1];
	buf[GOOD_BUF_SIZE] = '\0';	// So after read() we get a NULL-terminated string
	for (i=0; i<num_tries; ++i) {
		UPDATE_PROG(i*100/num_tries);
		SETUP_P(1,1);
		// TWO PROCESSES DOING THIS CODE:
		int j,num_reads = 10;		// Both processes: read lots of times
		int fd = open(get_node_name(0),O_RDWR);
		for(j=0; j<num_reads; ++j) {
			ASSERT(read(fd,buf,GOOD_BUF_SIZE) == GOOD_BUF_SIZE);	// Make sure read() succeeds completely
			ASSERT(is_good_init_grid(buf));
		}
		usleep(1000);	// Sleep 1ms, so the other process can read before we close()
		close(fd);
		// AFTER THIS, ONLY ONE PROCESS
		DESTROY_P();
	}
	return TRUE;
}

// Test multiple readers (asynchronous threads)
void* many_readers_func(void* arg) {
	int fd = open(get_node_name(0),O_RDWR);
	int i;
	int num_reads = 10;			// Both threads: read lots of times
	char buf[GOOD_BUF_SIZE+1];
	buf[GOOD_BUF_SIZE] = '\0';	// So after read() we get a NULL-terminated string
	for (i=0; i<num_reads; ++i) {
		ASSERT(read(fd,buf,GOOD_BUF_SIZE) == GOOD_BUF_SIZE);	// Make sure read() succeeds completely
		ASSERT(is_good_init_grid(buf));
	}
	usleep(1000);	// Sleep 1ms, so the other thread can read before we close()
	close(fd);
	return NULL;
}
bool many_readers_t() {
	int i,num_tries = 50;
	char buf[GOOD_BUF_SIZE+1];
	buf[GOOD_BUF_SIZE] = '\0';	// So after read() we get a NULL-terminated string
	for (i=0; i<num_tries; ++i) {
		UPDATE_PROG(i*100/num_tries);
		SETUP_T(1,2,many_readers_func,0,NULL);
		DESTROY_T();
	}
	return TRUE;
}

// Test multiple readers while there are multiple writers as well (processes)
bool many_readers_while_writers_p() {
	int i,num_tries = 50;
	char buf[GOOD_BUF_SIZE+1];
	buf[GOOD_BUF_SIZE] = '\0';	// So after read() we get a NULL-terminated string
	for (i=0; i<num_tries; ++i) {
		UPDATE_PROG(i*100/num_tries);
		SETUP_P(1,1);
		// Only do K reads, because we want to move as well and not starve.
		// Make the black player go UP->RIGHT->RIGHT->RIGHT and the white
		// player go DOWN->RIGHT->RIGHT->RIGHT
		int j;
		int fd = open(get_node_name(0),O_RDWR);
		for(j=0; j<4; ++j) {
			char move = RIGHT + '0';
			// No one wants to be Michael Jackson's baby.
			// First move means it matters if we're black or white.
			if (!j) {
				move = (ioctl(fd,SNAKE_GET_COLOR,NULL) == WHITE_COLOR ? DOWN : UP) + '0';
			}
			ASSERT(write(fd,&move,1) == 1);
			ASSERT(read(fd,buf,GOOD_BUF_SIZE) == GOOD_BUF_SIZE);	// Make sure read() succeeds completely
			// I would test if the read is OK here if I could.
			// Checking if a grid string is valid is an annoying thing to test...
		}
		// If I had the energy I'd use a barrier here.
		// Anyway, wait enough time for both processes to stop R&W operations.
		usleep(1000);
		close(fd);
		// AFTER THIS, ONLY ONE PROCESS
		DESTROY_P();
	}
	return TRUE;
}

// Test multiple readers while there are multiple writers as well (threads)
void* many_readers_while_writers_func(void* arg) {
	ThreadParam *p = (ThreadParam*)arg;
	int j;
	int fd = open(get_node_name(0),O_RDWR);
	char buf[GOOD_BUF_SIZE+1];
	buf[GOOD_BUF_SIZE] = '\0';	// So after read() we get a NULL-terminated string
	for(j=0; j<4; ++j) {
		char move = RIGHT + '0';
		// No one wants to be Michael Jackson's baby.
		// First move means it matters if we're black or white.
		if (!j)
			move = (ioctl(fd,SNAKE_GET_COLOR,NULL) == WHITE_COLOR ? DOWN : UP) + '0';
		ASSERT(write(fd,&move,1) == 1);
		ASSERT(read(fd,buf,GOOD_BUF_SIZE) == GOOD_BUF_SIZE);	// Make sure read() succeeds completely
		// I would test if the read is OK here if I could.
		// Checking if a grid string is valid is an annoying thing to test...
	}
	// 2-thread barrier
	if (!sem_trywait(p->sem_arr))	// First thread
		sem_wait(p->sem_arr);
	else							// Second thread
		sem_post(p->sem_arr);
	close(fd);
	return NULL;
}
bool many_readers_while_writers_t() {
	int i, num_tries=50;
	for (i=0; i<num_tries; ++i) {
		// One semaphore, to act as a thread barrier.
		int vals[] = {1};
		UPDATE_PROG(i*100/num_tries);
		SETUP_T(1,2,many_readers_while_writers_func,1,vals);
		DESTROY_T();
	}
	return TRUE;
}

// Reading 0 bytes should return 0
bool read_0_return_0() {
	SETUP_P(1,1);
	int fd = open(get_node_name(0),O_RDWR);
	ASSERT(read(fd,NULL,0) == 0);
	usleep(1000);	// Prevent reading after someone closes
	close(fd);
	DESTROY_P();
	return TRUE;
}

// Reading N<GRIDSIZE bytes should return N
bool read_N_lt_grid_returns_N() {
	// Test this by adding the last character '\n' and then checking the result
	char buf[GOOD_BUF_SIZE+1];
	buf[GOOD_BUF_SIZE]=buf[GOOD_BUF_SIZE-1]=buf[GOOD_BUF_SIZE-2]='x';	// Dummy character
	SETUP_P(1,1);
	int fd = open(get_node_name(0),O_RDWR);
	ASSERT(read(fd,buf,GOOD_BUF_SIZE-1) == GOOD_BUF_SIZE-1);
	ASSERT(buf[GOOD_BUF_SIZE] == 'x');	// Last two characters should be untouched,
	ASSERT(buf[GOOD_BUF_SIZE-1] == 'x');// but the third-to-last character should have
	ASSERT(buf[GOOD_BUF_SIZE-2] != 'x');// been overwritten.
	buf[GOOD_BUF_SIZE-1]='\n';			// Edit the buffer to be a good grid
	buf[GOOD_BUF_SIZE]='\0';
	ASSERT(is_good_init_grid(buf));		// Test it
	usleep(1000);
	close(fd);
	DESTROY_P();
	return TRUE;
}

// Reading N=GRIDSIZE bytes should return N and a complete grid (don't need NULL terminator)
bool read_N_eq_grid_returns_N() {
	char buf[GOOD_BUF_SIZE+1];
	buf[GOOD_BUF_SIZE]=buf[GOOD_BUF_SIZE-1]='x';	// Dummy character
	SETUP_P(1,1);
	int fd = open(get_node_name(0),O_RDWR);
	ASSERT(read(fd,buf,GOOD_BUF_SIZE) == GOOD_BUF_SIZE);
	ASSERT(buf[GOOD_BUF_SIZE] == 'x');	// Last character should be untouched, but the second-to-last
	ASSERT(buf[GOOD_BUF_SIZE-1] != 'x');// character should have been overwritten.
	buf[GOOD_BUF_SIZE]='\0';
	ASSERT(is_good_init_grid(buf));		// Test it
	usleep(1000);
	close(fd);
	DESTROY_P();
	return TRUE;
}

// Reading N>GRIDSIZE bytes should return GRIDSIZE and a complete grid, WITHOUT overwriting
// byte number GRIDSIZE+1
bool read_N_gt_grid_returns_grid() {
	char buf[GOOD_BUF_SIZE+10];
	buf[GOOD_BUF_SIZE]=buf[GOOD_BUF_SIZE-1]='x';	// Dummy character
	SETUP_P(1,1);
	int fd = open(get_node_name(0),O_RDWR);
	ASSERT(read(fd,buf,GOOD_BUF_SIZE+10) == GOOD_BUF_SIZE);
	ASSERT(buf[GOOD_BUF_SIZE] == 'x');	// Last character should be untouched, but the second-to-last
	ASSERT(buf[GOOD_BUF_SIZE-1] != 'x');// character should have been overwritten.
	buf[GOOD_BUF_SIZE]='\0';
	ASSERT(is_good_init_grid(buf));		// Test it
	usleep(1000);
	close(fd);
	DESTROY_P();
	return TRUE;
}

// Reading after release() should return -1 with errno=10
bool read_after_release() {
	char buf[GOOD_BUF_SIZE+1];
	buf[GOOD_BUF_SIZE]=buf[GOOD_BUF_SIZE-1]='x';	// Dummy character
	SETUP_P(1,1);
	int fd = open(get_node_name(0),O_RDWR);
	if (child_num) {
		close(fd);
		exit(0);
	}
	else wait(NULL);
	ASSERT(read(fd,buf,GOOD_BUF_SIZE) == -1);
	ASSERT(errno == 10);
	close(fd);
	DESTROY_P();
	return TRUE;
}

// Readers while some are calling release(). Every successful reader should get the grid in
// it's entirety (no partial read!), but it's OK to fail reading (return -1 and errno=10)
bool many_readers_while_releasing_p() {
	char buf[GOOD_BUF_SIZE+1] = {0};
	int i, pid, fd, ret, passes = 50, k;
	for (k=0; k<passes; ++k) {
		UPDATE_PROG(k*100/passes);
		setup_snake(1);
		errno = 0;		// Make sure we don't read old values
		pid = fork();
		fd = open(get_node_name(0),O_RDWR);
		if (pid) {
			int j, tries = 50;
			for (j=0; j<tries; ++j) {
				ret = read(fd,buf,GOOD_BUF_SIZE);
				if (ret<0) {	// If the game is closed, NOTHING should have been written
					for (i=0; i<GOOD_BUF_SIZE; ++i)
						ASSERT(buf[i] == 0);
					ASSERT(errno == 10);
					errno = 0;	// Make sure we don't read old values
				}
				else {			// Otherwise, a COMPLETE read should have been made
					ASSERT(is_good_init_grid(buf));
					--j;		// Keep at it, until the other process closes
				}
				for (i=0; i<GOOD_BUF_SIZE; ++i)
					buf[i] = 0;	// Reset the buffer for the next pass
			}
		}
		else {
			usleep(500);
			close(fd);
			exit(0);
		}
		wait(NULL);
		close(fd);
		destroy_snake();
	}
	return TRUE;
}

// Same thing, but with threads
void* many_readers_while_releasing_func(void* arg) {
	char buf[GOOD_BUF_SIZE+1] = {0};
	int i, j, ret;
	int fd = open(get_node_name(0),O_RDWR);
	if (ioctl(fd,SNAKE_GET_COLOR,NULL) == WHITE_COLOR) {
		int tries = 50;
		for (j=0; j<tries; ++j) {
			ret = read(fd,buf,GOOD_BUF_SIZE);
			if (ret<0) {	// If the game is closed, NOTHING should have been written
				for (i=0; i<GOOD_BUF_SIZE; ++i)
					ASSERT(buf[i] == 0);
				ASSERT(errno == 10);
				errno = 0;	// Make sure we don't read old values
			}
			else {			// Otherwise, a COMPLETE read should have been made.
				ASSERT(is_good_init_grid(buf));
				--j;		// Keep at it until the game closes...
			}
			for (i=0; i<GOOD_BUF_SIZE; ++i)
				buf[i] = 0;	// Reset the buffer for the next pass
		}
	}
	else {
		usleep(500);	// Wait a bit, then close
	}
	close(fd);
	return NULL;
}
bool many_readers_while_releasing_t() {
	int passes = 50, k;
	for (k=0; k<passes; ++k) {
		UPDATE_PROG(k*100/passes);
		setup_snake(1);
		errno = 0;		// Make sure we don't read old values
		CLONE(2,many_readers_while_releasing_func,NULL);
		T_WAIT();
		destroy_snake();
	}
	return TRUE;
	return TRUE;
}

/* ***************************
 GENERAL TESTS
*****************************/
// All functions should return -10 (-1 returned, errno=10) after one process releases
bool error_after_release() {
	
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
	
	// Seed the random number generator
	srand(time(NULL));
	
	// Test!
	START_TESTS();
	
	TEST_AREA("open & release");
	RUN_TEST(open_release_simple);
	RUN_TEST(two_releases_processes);
	RUN_TEST(two_releases_threads);
	RUN_TEST(open_release_open);
	RUN_TEST(first_open_is_white);
	RUN_TEST(open_race_threads);
	RUN_TEST(open_race_processes);
	RUN_TEST(games_race_threads);
	RUN_TEST(games_race_processes);
	
	TEST_AREA("read");
	RUN_TEST(many_readers_p);
	RUN_TEST(many_readers_t);
	RUN_TEST(many_readers_while_writers_p);
	RUN_TEST(many_readers_while_writers_t);
	RUN_TEST(read_0_return_0);
	RUN_TEST(read_N_lt_grid_returns_N);
	RUN_TEST(read_N_eq_grid_returns_N);
	RUN_TEST(read_N_gt_grid_returns_grid);
	RUN_TEST(read_after_release);
	RUN_TEST(many_readers_while_releasing_p);
	RUN_TEST(many_readers_while_releasing_t);
	
	// That's all folks
	END_TESTS();
	return 0;
	
}

