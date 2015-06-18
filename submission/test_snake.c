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
	CREATE_BUF();
	
	// First, simple pass
	Matrix m;
	SETUP_P(1,1);
	if (child_num)
		usleep(500);
	int fd = open(get_node_name(0),O_RDWR);
	read(fd,buf,GOOD_BUF_SIZE);
	ASSERT(parse_board(buf,&m));
	ASSERT(is_good_grid(&m));
	usleep(5000);	// Enough for both to read the board
	close(fd);
	DESTROY_P();

	// Complex pass
	int i,num_tries = 50;
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
	CREATE_BUF();
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
	CREATE_BUF();
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
	CREATE_BUF();
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
			Matrix m;
			ASSERT(parse_board(buf,&m));
			ASSERT(is_good_grid(&m));
		}
		// If I had the energy I'd use a barrier here.
		// Anyway, wait enough time for both processes to stop R&W operations.
		usleep(10000);
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
	CREATE_BUF();
	for(j=0; j<4; ++j) {
		char move = RIGHT + '0';
		// No one wants to be Michael Jackson's baby.
		// First move means it matters if we're black or white.
		if (!j)
			move = (ioctl(fd,SNAKE_GET_COLOR,NULL) == WHITE_COLOR ? DOWN : UP) + '0';
		ASSERT(write(fd,&move,1) == 1);
		ASSERT(read(fd,buf,GOOD_BUF_SIZE) == GOOD_BUF_SIZE);	// Make sure read() succeeds completely
		Matrix m;
		ASSERT(parse_board(buf,&m));
		ASSERT(is_good_grid(&m));
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

// Reading with NULL buffer should return EFAULT.
// Reading NULL with count=0 should return 0
bool read_null_efault() {
	SETUP_P(1,1);
	int fd = open(get_node_name(0),O_RDWR);
	errno = 0;
	ASSERT(read(fd,NULL,1) == -1);
	ASSERT(errno == EFAULT);
	errno = 0;
	ASSERT(read(fd,NULL,0) == 0);
	ASSERT(errno == 0);
	usleep(1000);
	close(fd);
	DESTROY_P();
	return TRUE;
}

// Reading 0 bytes should return 0
bool read_0_return_0() {
	SETUP_P(1,1);
	int fd = open(get_node_name(0),O_RDWR);
	char buf;
	ASSERT(read(fd,&buf,0) == 0);
	usleep(1000);	// Prevent reading after someone closes
	close(fd);
	DESTROY_P();
	return TRUE;
}

// Reading N<GRIDSIZE bytes should return N
bool read_N_lt_grid_returns_N() {
	// Test this by adding the last character '\n' and then checking the result
	CREATE_BUF();
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
	CREATE_BUF();
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

// Reading N>GRIDSIZE bytes should return GRIDSIZE and a complete grid, the rest of the buffer
// should contain zeros
bool read_N_gt_grid_returns_N() {
	char buf[GOOD_BUF_SIZE+10];
	int i;
	for(i=GOOD_BUF_SIZE; i<GOOD_BUF_SIZE+10; ++i)
		buf[i] = 'x';					// Dummy character
	SETUP_P(1,1);
	int fd = open(get_node_name(0),O_RDWR);
	ASSERT(read(fd,buf,GOOD_BUF_SIZE+10) == GOOD_BUF_SIZE+10);
	for(i=GOOD_BUF_SIZE; i<GOOD_BUF_SIZE+10; ++i)
		ASSERT(buf[i] == 0);			// Make sure trailing zeros were inserted
	ASSERT(is_good_init_grid(buf));		// Test it
	usleep(1000);
	close(fd);
	DESTROY_P();
	return TRUE;
}

// Reading after release() should return -1 with errno=10
bool read_after_release() {
	CREATE_BUF();
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
 WRITE TESTS
*****************************/
// Make sure the result of a move can be seen in the board
bool write_shows_up() {
	int i, tries=20;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		SETUP_P(1,1);
		int fd = open(get_node_name(0), O_RDWR);
		char move = '2';	// DOWN, white's first move
		ASSERT(write(fd,&move,1) == 1);
		CREATE_BUF();
		ASSERT(read(fd,buf,GOOD_BUF_SIZE) == GOOD_BUF_SIZE);
		Matrix m;
		ASSERT(parse_board(buf,&m));
		// Make sure the correct move happened.
		// The move may have cause the sanek to eat, so only check what we know...
		ASSERT(is_good_grid(&m));
		ASSERT(m[1][0] == 1);	// 1 2 3 .      2 3 . .
		ASSERT(m[0][0] == 2);	// . . . . ===> 1 . . .
		ASSERT(m[0][1] == 3);
		usleep(1000);
		close(fd);
		DESTROY_P();
	}
	return TRUE;
}

// Writing to a finished game should return the number of characters written.
// Check for both single writes and bulk writes
bool write_valid_to_finished() {
	int i, tries=20;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		// Single write:
		SETUP_P(1,1);
		int fd = open(get_node_name(0), O_RDWR);
		char move = '8';	// UP, crash into the top so we lose (white player)
		errno = 0;
		if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
			errno = 0;
			ASSERT(write(fd,&move,1) == 1);		// Lose
			ASSERT(write(fd,&move,1) == 0);		// Valid character, but game is over
			ASSERT(errno == 0);
		}
		else {
			// No need to move, white player lost on his first move.
			usleep(1000);
		}
		close(fd);
		DESTROY_P();
		// Multi write:
		SETUP_P(1,1);
		fd = open(get_node_name(0), O_RDWR);
		char moves[] = {'8','2'};
		errno = 0;
		if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
			errno = 0;
			ASSERT(write(fd,moves,2) == 1);		// Second write fails, return written bytes
			ASSERT(errno == 0);
		}
		else {
			// No need to move, white player lost on his first move.
			usleep(1000);
		}
		close(fd);
		DESTROY_P();
	}
	return TRUE;
}

// Writing an invalid character to an active game should return in error
bool write_invalid_to_good_game() {
	int i, tries=20;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		// Single write:
		SETUP_P(1,1);
		int fd = open(get_node_name(0), O_RDWR);
		char move = 'x';	// Invalid character
		errno = 0;
		if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
			errno = 0;
			ASSERT(write(fd,&move,1) == -1);	// Lose, return invalid
			ASSERT(errno == ERRNO_INVALID_MOVE_ACTIVE_GAME);
		}
		else {
			usleep(1000);
		}
		close(fd);
		DESTROY_P();
		// Multi write:
		SETUP_P(1,1);
		fd = open(get_node_name(0), O_RDWR);
		char moves[] = {'2','x'};
		errno = 0;
		if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
			errno = 0;
			ASSERT(write(fd,moves,2) == -1);		// Second write fails due to invalid character, but the game is active!
			ASSERT(errno == ERRNO_INVALID_MOVE_ACTIVE_GAME);
		}
		else {
			move = '8';
			ASSERT(write(fd,&move,1) == 1);			// One move, so the white player 'moves' twice
			usleep(1000);
		}
		close(fd);
		DESTROY_P();
	}
	return TRUE;
}

// Writing an invalid character to a non-active game should return an error
bool write_invalid_to_finished() {
	int i, tries=20;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		// Single write:
		SETUP_P(1,1);
		int fd = open(get_node_name(0), O_RDWR);
		char move;
		errno = 0;
		if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
			errno = 0;
			move = '8';							// Crash into the top wall
			ASSERT(write(fd,&move,1) == 1);		// Lose
			ASSERT(errno == 0);
			move = 'x';							// Invalid character
			ASSERT(write(fd,&move,1) == -1);
			ASSERT(errno == ERRNO_INVALID_MOVE_INACTIVE_GAME);
		}
		else {
			usleep(1000);
		}
		close(fd);
		DESTROY_P();
		// Multi write:
		SETUP_P(1,1);
		fd = open(get_node_name(0), O_RDWR);
		char moves[] = {'8','x'};					// Crash, and then invalid
		errno = 0;
		if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
			errno = 0;
			ASSERT(write(fd,moves,2) == -1);		// Second write fails due to invalid character, but the game is inactive
			ASSERT(errno == ERRNO_INVALID_MOVE_INACTIVE_GAME);
		}
		else {
			// No need to wake the white player up with another move, he lost already after move 1
			usleep(1000);
		}
		close(fd);
		DESTROY_P();
	}
	return TRUE;
}

// Make sure read() and ioctl() still work after loss
bool read_ioctl_after_loss() {
	int i, tries=20;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		// Single write:
		SETUP_P(1,1);
		int fd = open(get_node_name(0), O_RDWR);
		char move = '8';					// UP, crash into the top so we lose (white player)
		if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
			ASSERT(write(fd,&move,1) == 1);		// Lose
		}
		else {
			usleep(1000);						// Wait for the move
		}
		// Because both players tried to move, the following code will only be executed
		// AFTER white player lost.
		ASSERT(ioctl(fd,SNAKE_GET_COLOR)>0);
		ASSERT(ioctl(fd,SNAKE_GET_WINNER)>0);
		CREATE_BUF();
		ASSERT(read(fd,buf,GOOD_BUF_SIZE) == GOOD_BUF_SIZE);
		Matrix m;
		parse_board(buf, &m);
		ASSERT(is_good_grid(&m));
		usleep(1000);
		close(fd);
		DESTROY_P();
	}
	return TRUE;
}

// Same thing, but even after writing after loss (should return error, but the board
// should still be readable)
bool read_ioctl_after_loss_and_then_write() {
	int i, tries=20;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		// Single write:
		SETUP_P(1,1);
		int fd = open(get_node_name(0), O_RDWR);
		char move = '8';					// UP, crash into the top so we lose (white player)
		if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
			ASSERT(write(fd,&move,1) == 1);		// Lose
			ASSERT(write(fd,&move,1) == 0);		// Can't write again!
		}
		else {
			usleep(1000);						// Wait for the move
		}
		// Because both players tried to move, the following code will only be executed
		// AFTER white player lost.
		ASSERT(ioctl(fd,SNAKE_GET_COLOR)>0);
		ASSERT(ioctl(fd,SNAKE_GET_WINNER)>0);
		CREATE_BUF();
		ASSERT(read(fd,buf,GOOD_BUF_SIZE) == GOOD_BUF_SIZE);
		Matrix m;
		parse_board(buf, &m);
		ASSERT(is_good_grid(&m));
		usleep(1000);
		close(fd);
		DESTROY_P();
	}
	return TRUE;
}


// Make sure we lose after K steps with no food.
// THIS ASSUMES K=5!
// This is complicated... we have to keep calculating the next move based on where the food is.
// Doing a "dumb" (constant) moveset and "giving up on starving" if it doesn't work out isn't good,
// statistically it is VERY LIKELY that the snake WILL eat within 5 moves...
// We have to:
// * Check where to move (where there is no food). It's important that we let black finish moving first before checking.
// * Move
// * Wait for black to move (if we don't, black's move may place food in front of us...)
// * Repeat (5 times)
// * Make sure black player wins
// If we have NO CHOICE but to eat, we'll try again (decrement the counter, so we starve a total of 'tries' times).
// Use the do_starve_move() function to move to a place with no food (returns FALSE if it's impossible)
// The helper function assumes the white player stays at the top half of the board, and the black player at the bottom:
// W W W W
// W W W W
// B B B B
// B B B B
// And also assumes we only need 6 moves (if move_num>6, returns FALSE).
// Do this MACRO style, so ASSERT()s work correctly.
// Assumes the following are declared:
// Matrix m;
// char move;
// int couldnt_starve;
#define DO_STARVE_MOVE(move_num, fd, is_black, force) \
	ASSERT(read_and_parse(fd,&m) || !force); \
	couldnt_starve = 0; \
	if (!is_black) switch(move_num) { \
			case 1: \
				/* White board looks like:
				   1 2 3 .
				   ? . . .
				   If ?=FOOD, we can't starve */ \
				if (m[1][0] == FOOD) { \
					couldnt_starve = 1; \
					break; \
				} \
				move = '2'; \
				PRINT("Move #%d for %s player is '%c'\n",move_num,is_black?"black":"white",move); \
				ASSERT(write(fd,&move,1) == 1 || !force); \
				break; \
			case 2: \
				/* White board looks like:
				   2 3 . .
				   1 ? . .
				   If ?=FOOD, we can't starve */ \
				if (m[1][1] == FOOD) { \
					couldnt_starve = 1; \
					break; \
				} \
				move = '6'; \
				PRINT("Move #%d for %s player is '%c'\n",move_num,is_black?"black":"white",move); \
				ASSERT(write(fd,&move,1) == 1 || !force);	 \
				break; \
			case 3: \
				/* White board looks like:
				   3 ? . .
				   2 1 ? .
				   If one of the ? signs is FOOD, choose the other */ \
				move = (m[1][2] == FOOD) ? '8' : '6'; \
				PRINT("Move #%d for %s player is '%c'\n",move_num,is_black?"black":"white",move); \
				ASSERT(write(fd,&move,1) == 1 || !force); \
				break; \
			case 4: \
				/* White board can be:
				   . 1 ? .
				   3 2 . .
				   or:
				   . . ? .
				   3 2 1 ?
				   Some choices to make... */ \
				if (m[0][1] == 1) { /* Option 1 */ \
					if (m[0][2] == FOOD) { \
						couldnt_starve = 1; \
						break; \
					} \
					move = '6'; \
				} \
				else { /* Option 2 */ \
					move = (m[1][3] == FOOD) ? '8' : '6'; \
				} \
				PRINT("Move #%d for %s player is '%c'\n",move_num,is_black?"black":"white",move); \
				ASSERT(write(fd,&move,1) == 1 || !force); \
				break; \
			case 5: \
				/* White board can be:
				   . . 1 ?
				   . 3 2 .
				   or:
				   . . . ?
				   . 3 2 1
				   or:
				   . 2 1 ?
				   . 3 ? .
				   Some choices to make...
				   Options 1 and 2 means we want to go up & right anyway (to keep it simple...) */ \
				if (m[1][2] == 2) /* Option 1 or 2 */ { \
					if (m[0][3] == FOOD) { \
						couldnt_starve = 1; \
						break; \
					} \
					move = (m[0][2] == 1) ? '6' : '8';	/* If we went up, go right & vice versa */ \
				} \
				else { /* Option 3 */ \
					move = (m[0][3] == FOOD) ? '2' : '6'; \
				} \
				PRINT("Move #%d for %s player is '%c'\n",move_num,is_black?"black":"white",move); \
				ASSERT(write(fd,&move,1) == 1 || !force); \
				break; \
			case 6: \
				/* White board can be:
					. . 2 1
					. . 3 ?
					or:
					. . ? 1
					. . 3 2
					or:
					. 3 2 1
					. . . ?
					or:
					. 3 2 .
					. ? 1 ?
					Some choices to make...
					Options 1 and 3 means we want to go down anyway (to keep it simple...) */ \
				if (m[0][2] == 2 && m[0][3]) { /* Options 1 & 3 */ \
					if (m[1][3] == FOOD) { \
						couldnt_starve = 1; \
						break; \
					} \
					move = '2'; \
				} \
				else if (m[0][3] == 1) { /* Option 2 */ \
					if (m[0][2] == FOOD) { \
						couldnt_starve = 1; \
						break; \
					} \
					move = '4'; \
				} \
				else { /* Option 4 */ \
					move = (m[1][3] == FOOD) ? '4' : '6'; \
				} \
				PRINT("Move #%d for %s player is '%c'\n",move_num,is_black?"black":"white",move); \
				ASSERT(write(fd,&move,1) == 1 || !force); \
				break; \
			default: /* If we couldn't move, try again */ \
				couldnt_starve = 1; \
				break; \
		} \
	else switch(move_num) { \
			case 1: \
				/* Black board looks like:
				    ?  .  .  .
				   -1 -2 -3  .
				   If ?=FOOD, we can't starve */ \
				if (m[2][0] == FOOD) { \
					couldnt_starve = 1; \
					break; \
				} \
				move = '8'; \
				PRINT("Move #%d for %s player is '%c'\n",move_num,is_black?"black":"white",move); \
				ASSERT(write(fd,&move,1) == 1 || !force); \
				break; \
			case 2: \
				/* Black board looks like:
				   -1  ?  .  .
				   -2 -3  .  .
				   If ?=FOOD, we can't starve */ \
				if (m[2][1] == FOOD) { \
					couldnt_starve = 1; \
					break; \
				} \
				move = '6'; \
				PRINT("Move #%d for %s player is '%c'\n",move_num,is_black?"black":"white",move); \
				ASSERT(write(fd,&move,1) == 1 || !force);	 \
				break; \
			case 3: \
				/* Black board looks like:
				   -2 -1  ?  .
				   -3  ?  .  .
				   If one of the ? signs is FOOD, choose the other */ \
				move = (m[2][2] == FOOD) ? '2' : '6'; \
				PRINT("Move #%d for %s player is '%c'\n",move_num,is_black?"black":"white",move); \
				ASSERT(write(fd,&move,1) == 1 || !force); \
				break; \
			case 4: \
				/* Black board can be:
				   -3 -2  .  .
				    . -1  ?  .
				   or:
				   -3 -2 -1  ?
				    .  .  ?  .
				   Some choices to make... */ \
				if (m[3][1] == -1) { /* Option 1 */ \
					if (m[3][2] == FOOD) { \
						couldnt_starve = 1; \
						break; \
					} \
					move = '6'; \
				} \
				else { /* Option 2 */ \
					move = (m[2][3] == FOOD) ? '2' : '6'; \
				} \
				PRINT("Move #%d for %s player is '%c'\n",move_num,is_black?"black":"white",move); \
				ASSERT(write(fd,&move,1) == 1 || !force); \
				break; \
			case 5: \
				/* Black board can be:
				   . -3 -2  .
				   .  . -1  ?
				   or:
				   . -3 -2 -1
				   .  .  .  ?
				   or:
				   . -3  ?  .
				   . -2 -1  ?
				   Some choices to make...
				   Options 1 and 2 means we want to go down & right anyway (to keep it simple...) */ \
				if (m[2][2] == -2) /* Option 1 or 2 */ { \
					if (m[3][3] == FOOD) { \
						couldnt_starve = 1; \
						break; \
					} \
					move = (m[3][2] == -1) ? '6' : '2';	/* If we went down, go right & vice versa */ \
				} \
				else { /* Option 3 */ \
					move = (m[3][3] == FOOD) ? '8' : '6'; \
				} \
				PRINT("Move #%d for %s player is '%c'\n",move_num,is_black?"black":"white",move); \
				ASSERT(write(fd,&move,1) == 1 || !force); \
				break; \
			default: /* If we couldn't move, try again */ \
				couldnt_starve = 1; \
				break; \
		}
// The test itself:
bool starve_to_death() {
	int i, tries=50;
	Matrix m;			// NEED THESE THREE for DO_STARVE_MOVE().
	char move;
	int couldnt_starve;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		// To do this, just keep trying until we CAN move without eating.
		// We only need K moves anyway.
		SETUP_P(1,1);
		// Make sure the parent is the WHITE player (so 'i' is valid).
		// Enforce this by making the child sleep before opening.
		if (child_num) usleep(1000);
		int fd = open(get_node_name(0), O_RDWR);
		int move_num;
		if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
			for (move_num=1; move_num<=5; ++move_num) {
				ASSERT(ioctl(fd,SNAKE_GET_WINNER) == -1);	// Not dead yet
				DO_STARVE_MOVE(move_num,fd,FALSE,TRUE);		// Move. Force success (no reason not to)
				if (couldnt_starve) break;
				usleep(1000);								// Wait for black to move (so FOOD is recalculated)
			}
			if (couldnt_starve) {
				--i;	// Try starving again...
			}
			else {
				ASSERT(ioctl(fd,SNAKE_GET_WINNER) == BLACK_COLOR);
			}
		}
		else {
			// The black player doesn't have to starve for this test, just
			// move him UP->RIGHT->RIGHT->RIGHT->DOWN.
			// Also, don't make sure it worked, just try while the white player is moving.
			char moves[] = {'8','6','6','6','2'};
			write(fd,moves,5);
			usleep(1000);	// Wait for white player, he has all kinds of ASSERT()s that we don't want failing...
		}
		close(fd);
		DESTROY_P();
	}
	return TRUE;
}

// Make sure white loses if both snakes don't eat for K turns.
// Uses the macros defined above for starve_to_death().
void* white_starves_first_func(void* arg) {
	ThreadParam* p = (ThreadParam*)arg;
	Matrix m;			// NEED THESE THREE for DO_STARVE_MOVE().
	char move;
	int couldnt_starve=0;
	int fd = open(get_node_name(0), O_RDWR);
	int move_num;
	bool is_black = (ioctl(fd,SNAKE_GET_COLOR,NULL) == BLACK_COLOR);
	// BOTH players try to starve.
	for (move_num=1; move_num<=5; ++move_num) {
		if (!sem_trywait(p->sem_arr)) {
			sem_post(p->sem_arr);
			close(fd);
			return NULL;
		}
		DO_STARVE_MOVE(move_num,fd,is_black,FALSE);
		if (couldnt_starve) {
			PRINT("%s couldn't starve (move %d)\n",is_black? "Black":"White",move_num);
			sem_post(p->sem_arr);
			usleep(1000);
			close(fd);
			return NULL;
		}
	}
	if (!sem_trywait(p->sem_arr)) {				// If the other thread starved, ioctl won't work
		close(fd);
		sem_post(p->sem_arr);					// Tell daddy
		return NULL;
	}
	int winner = ioctl(fd,SNAKE_GET_WINNER,NULL);
	usleep(1000);
	close(fd);
	if (winner != BLACK_COLOR)
		sem_post(p->sem_arr+1);
	return NULL;
}
bool white_starves_first() {
	int i, tries=30;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		// Use threads. If a thread can lock the semaphore, that means one of them couldn't starve.
		// The second semaphore is signalled if the test failed
		int vals[] = {0,0};
		SETUP_T(1,2,white_starves_first_func,2,vals);
		T_WAIT();
		// If the 1st semaphore was signalled, we need to try again...
		int val;
		sem_getvalue(tp.sem_arr, &val);
		PRINT("val=%d\n",val);
		if (val != 0)
			--i;
		else {
			sem_getvalue(tp.sem_arr+1, &val);
			ASSERT(val == 0);	// Wrong winner
		}
		DESTROY_T();
	}
	return TRUE;
}

// Make sure we do survive K more turns after eating.
// For this to work, make sure the first instance of FOOD is at m[1][0],
// so the white player can eat and then starve in 6 moves
void* eat_and_starve_later_func(void* arg) {
	ThreadParam* p = (ThreadParam*)arg;
	Matrix m;			// NEED THESE THREE for DO_STARVE_MOVE().
	char move;
	int couldnt_starve=0;
	int fd = open(get_node_name(0), O_RDWR);
	int move_num;
	bool is_black = (ioctl(fd,SNAKE_GET_COLOR,NULL) == BLACK_COLOR);
	// Check to see if m[1][0]==FOOD. If not, signal father and try again...
	read_and_parse(fd,&m);
	if (m[1][0] != FOOD) {
		sem_post(p->sem_arr);
		usleep(1000);
		close(fd);
		return NULL;
	}
	// Otherwise, the white player should first move down
	if (!is_black) {
		move = '2';
		write(fd,&move,1);
	}
	else {
		usleep(1000);	// Wait for white's first move, to recalculate food
	}
	// BOTH players try to starve.
	for (move_num=1; move_num<=5; ++move_num) {
		if (!sem_trywait(p->sem_arr)) {				// If the other thread starved...
			close(fd);
			sem_post(p->sem_arr);					// Tell daddy
			return NULL;
		}
		// Move (DON'T FORCE! We may fail moving because the other color already failed to starve)
		// If this is the white player moving, it's his move_num+1 move (he already ate)
		DO_STARVE_MOVE(is_black ? move_num : move_num+1,fd,is_black,FALSE);
		if (couldnt_starve) {
			sem_post(p->sem_arr);
			usleep(1000);			// So the other thread has time to react before we close
			close(fd);
			return NULL;
		}
		usleep(1000);								// Wait for other player to move (so FOOD is recalculated)
		if(is_black) PRINT_BOARD(fd);
	}
	if (!sem_trywait(p->sem_arr)) {				// If the other thread starved, ioctl won't work
		close(fd);
		sem_post(p->sem_arr);					// Tell daddy
		return NULL;
	}
	usleep(1000);
	int winner = ioctl(fd,SNAKE_GET_WINNER,NULL);	// This time, white wins!
	usleep(1000);	// So the ioctl works before we close()
	close(fd);
	if (winner != WHITE_COLOR)
		sem_post(p->sem_arr+1);
	return NULL;
}
bool eat_and_starve_later() {
	int i, tries=30;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		// Use threads. If a thread can lock the first semaphore, that means one of them couldn't starve,
		// or m[1][0] != FOOD (more likely).
		// The second semaphore is signalled if the test failed.
		int vals[] = {0,0};
		SETUP_T(1,2,eat_and_starve_later_func,2,vals);
		T_WAIT();
		// If the semaphore was signalled (for any reason), we need to try again...
		int val;
		sem_getvalue(tp.sem_arr, &val);
		if (val != 0)		
			--i;
		else {
			sem_getvalue(tp.sem_arr+1, &val);
			ASSERT(val == 0);
		}
		DESTROY_T();
	}
	return TRUE;
}

// Make sure you can move to where the last segment of your tail is (it's legal)
bool move_to_tail() {
	Matrix m;
	
	int i, tries=100;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
			
		// To do this, keep trying until there is food at m[1][0] or m[1][1].
		// If so, do (white player) '268'. Make sure we don't lose.
		// (If there's food at m[1][0], make sure that after eating it there's no food at m[1][1])
		SETUP_P(1,1);
		// Make sure the parent is the white player
		if (!P_IS_FATHER()) usleep(500);
		int fd = open(get_node_name(0),O_RDWR);
		ASSERT(!P_IS_FATHER() || ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR);
		
		// Start moving
		if (P_IS_FATHER()) {
			int move_num;
			for (move_num = 1; move_num <= 3; move_num++) {
				// Check the food stuff
				read_and_parse(fd,&m);
				if(move_num == 1 && m[1][0] != FOOD && m[1][1] != FOOD) {
					--i;
					break;
				}
				// If we've already eaten and another food appeared, skip this shit... try again
				if (move_num == 2 && m[0][2] == 4 && m[1][1] == FOOD) {
					--i;
					break;
				}
				// Otherwise, keep moving
				char move;
				switch (move_num) {
					case 1: move = '2'; break;
					case 2: move = '6'; break;
					case 3: move = '8'; break;
				}
				ASSERT(write(fd,&move,1) == 1);				// Move OK
				// If, by any chance, after we there is NO FOOD at location m[1][1],
				// the black player moved+ate and then FOOD appeared at location m[1][1],
				// this could cause us to fail the test...
				// In this case, m[0][2]==5. Check for that.
				if (move_num == 2) {
					read_and_parse(fd,&m);
					if (m[0][2] == 5) {
						--i;
						break;
					}
				}				
				ASSERT(ioctl(fd,SNAKE_GET_WINNER) == -1);	// Nobody died
			}
			close(fd);
		}
		else {
			char moves[] = {'8','6','6'};
			write(fd,moves,3);
			usleep(1000);
			close(fd);
			exit(0);
		}
		DESTROY_P();
	}
	
	return TRUE;
}

// Multiple games, single write operations - make sure they're turn-based.
bool single_write_turns() {
	int i, tries=30;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		SETUP_P(10,19);
		// All children with odd remainders should wait, so the even processes
		// are the white players
		if (child_num%2) usleep(500);
		int fd = open(get_node_name(child_num/2),O_RDWR);
		bool is_black = (ioctl(fd,SNAKE_GET_COLOR,NULL) == BLACK_COLOR);
		
		// Move 4 moves.
		// Each time, we should know exactly how the board should look,
		// so check it.
		char w_moves[] = {'2','6','6','6'};
		char b_moves[] = {'8','6','6','6'};
		int move;
		for (move=0; move<4; move++) {
			// Move
			PRINT("%s player is going to write '%c' as his %dth move\n",is_black?"Black":"White",*((is_black? b_moves:w_moves)+move),move);
			errno = 0;
			int ret = write(fd,(is_black? b_moves:w_moves)+move,1);
			PRINT("%s player move resulted in %d, errno=%d\n",is_black?"Black":"White",ret,errno);
			PRINT_BOARD(fd);
			ASSERT(ret == 1);
			//continue;
			// Get the board and check it.
			// It should be in exactly one of two states, depending on whether or not
			// the other player got to move before we printed.
			Matrix m;
			read_and_parse(fd,&m);
			ASSERT(is_good_grid(&m));
			if (!is_black) switch(move) {
				case 0:
					ASSERT(m[0][0] == 2 && m[0][1] == 3 && m[1][0] == 1);	// White snake's state
					ASSERT(m[3][0] == -1 || m[2][0] == -1);					// Black snake could be in two places
					break;
				case 1:
					ASSERT(m[0][0] == 3 && m[1][0] == 2 && m[1][1] == 1);
					ASSERT(m[2][1] == -1 || m[2][0] == -1);					// Black snake could be in two places
					break;
				case 2:
					ASSERT(m[1][0] == 3 && m[1][1] == 2 && m[1][2] == 1);
					ASSERT(m[2][2] == -1 || m[2][1] == -1);					// Black snake could be in two places
					break;
				case 3:
					ASSERT(m[1][1] == 3 && m[1][2] == 2 && m[1][3] == 1);
					ASSERT(m[2][3] == -1 || m[2][2] == -1);					// Black snake could be in two places
					break;
			}
			else switch(move) {
				case 0:
					ASSERT(m[2][0] == -1 && m[3][0] == -2 && m[3][1] == -3);// Black snake's state
					ASSERT(m[1][0] == 1 || m[1][1] == 1);					// White snake could be ahead
					break;
				case 1:
					ASSERT(m[2][1] == -1 && m[2][0] == -2 && m[3][0] == -3);
					ASSERT(m[1][1] == 1 || m[1][2] == 1);					// White snake could be ahead
					break;
				case 2:
					ASSERT(m[2][2] == -1 && m[2][1] == -2 && m[2][0] == -3);
					ASSERT(m[1][2] == 1 || m[1][3] == 1);					// White snake could be ahead
					break;
				case 3:
					ASSERT(m[2][3] == -1 && m[2][2] == -2 && m[2][1] == -3);
					ASSERT(m[1][3] == 1);									// White snake can ONLY be here
					break;
			}
		}
		usleep(100000);
		//usleep(1000);
		close(fd);
		PRINT("After close, destroying...\n");
		DESTROY_P();
		
	}
	return TRUE;
}

// Multiple games, bulk write operations - make sure they're turn-based.
bool bulk_write_turns() {
	int i, tries=30;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		SETUP_P(10,19);
		// All children with odd remainders should wait, so the even processes
		// are the white players
		if (child_num%2) usleep(500);
		int fd = open(get_node_name(child_num/2),O_RDWR);
		bool is_black = (ioctl(fd,SNAKE_GET_COLOR,NULL) == BLACK_COLOR);
		
		// Move 4 moves.
		// Each time, we should know exactly how the board should look,
		// so check it.
		char w_moves[] = {'2','6','6','6'};
		char b_moves[] = {'8','6','6','6'};
		
		// Black writes bulk, white doesn't (we can't test it otherwise)
		if (is_black) {
			ASSERT(write(fd,b_moves,4) == 4);
		}
		else {
			int move;
			for (move=0; move<4; move++) {
				// Move
				PRINT("%s player is going to write '%c' as his %dth move\n",is_black?"Black":"White",*((is_black? b_moves:w_moves)+move),move);
				errno = 0;
				int ret = write(fd,w_moves+move,1);
				PRINT("%s player move resulted in %d, errno=%d\n",is_black?"Black":"White",ret,errno);
				PRINT_BOARD(fd);
				ASSERT(ret == 1);
				//continue;
				// Get the board and check it.
				// It should be in exactly one of two states, depending on whether or not
				// the other player got to move before we printed.
				Matrix m;
				read_and_parse(fd,&m);
				ASSERT(is_good_grid(&m));
				switch(move) {
					case 0:
						ASSERT(m[0][0] == 2 && m[0][1] == 3 && m[1][0] == 1);	// White snake's state
						ASSERT(m[3][0] == -1 || m[2][0] == -1);					// Black snake could be in two places
						break;
					case 1:
						ASSERT(m[0][0] == 3 && m[1][0] == 2 && m[1][1] == 1);
						ASSERT(m[2][1] == -1 || m[2][0] == -1);					// Black snake could be in two places
						break;
					case 2:
						ASSERT(m[1][0] == 3 && m[1][1] == 2 && m[1][2] == 1);
						ASSERT(m[2][2] == -1 || m[2][1] == -1);					// Black snake could be in two places
						break;
					case 3:
						ASSERT(m[1][1] == 3 && m[1][2] == 2 && m[1][3] == 1);
						ASSERT(m[2][3] == -1 || m[2][2] == -1);					// Black snake could be in two places
						break;
				}
			}
		}
		usleep(1000);
		close(fd);
		PRINT("After close, destroying...\n");
		DESTROY_P();
		
	}
	return TRUE;
}

// Invalid (not 2,4,6,8) move should cause loss
bool invalid_move_loses() {
	// White loses
	SETUP_P(1,1);
	int fd = open(get_node_name(0), O_RDWR);
	if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
		char move = 'x';
		write(fd,&move,1);
		ASSERT(ioctl(fd,SNAKE_GET_WINNER) == BLACK_COLOR);
	}
	else {
		usleep(1000);
	}
	close(fd);
	DESTROY_P();
	// Black loses
	SETUP_P(1,1);
	fd = open(get_node_name(0), O_RDWR);
	if (ioctl(fd,SNAKE_GET_COLOR) == BLACK_COLOR) {
		char move = 'x';
		write(fd,&move,1);
		ASSERT(ioctl(fd,SNAKE_GET_WINNER) == WHITE_COLOR);
	}
	else {
		// We need to move, so the black player gets a chance to lose
		char move = '2';
		write(fd,&move,1);
		usleep(1000);
	}
	close(fd);
	DESTROY_P();
	return TRUE;
}

// Invalid (not 2,4,6,8) move at the nth move should cause loss
bool invalid_nth_move_loses() {
	int i,n=5;
	char b_moves[] = {'8','6','6','6','2'};
	char w_moves[] = {'2','6','6','6','8'};
	for (i=0; i<n; ++i) {
		UPDATE_PROG(i*100/n);
		// White loses:
		SETUP_P(1,1);
		int fd = open(get_node_name(0), O_RDWR);
		if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
			char tmp = w_moves[i];	// Remember the good move
			w_moves[i] = 'x';		// Set the i-th move to something invalid
			write(fd,w_moves,i+1);
			ASSERT(ioctl(fd,SNAKE_GET_WINNER) == BLACK_COLOR);
			w_moves[i] = tmp;		// Restore the good move
		}
		else {
			// Keep checking that we haven't won yet (and allow white to keep writing)
			int j;
			for (j=0; j<i; ++j) {
				ASSERT(write(fd,b_moves+j,1) == 1);
			}
			// Now, the white player is free to lose.
			usleep(1000);
		}
		close(fd);
		DESTROY_P();
		// Black loses:
		SETUP_P(1,1);
		fd = open(get_node_name(0), O_RDWR);
		if (ioctl(fd,SNAKE_GET_COLOR) == BLACK_COLOR) {
			char tmp = b_moves[i];	// Remember the good move
			b_moves[i] = 'x';		// Set the i-th move to something invalid
			write(fd,b_moves,i+1);
			ASSERT(ioctl(fd,SNAKE_GET_WINNER) == WHITE_COLOR);
			b_moves[i] = tmp;		// Restore the good move
		}
		else {
			// Keep checking that we haven't won yet (and allow black to keep writing).
			// Unlike the black player, we need to move up to and including i moves,
			// because we're first.
			int j;
			for (j=0; j<=i; ++j) {
				ASSERT(write(fd,w_moves+j,1) == 1);
			}
			// Now, the white player is free to lose.
			usleep(1000);
		}
		close(fd);
		DESTROY_P();
	}
	return TRUE;
}

// Multiple processes, same game - make sure they can all write at the same time
// but can only move one at a time
bool multiple_white_writers() {
	int i, tries=30;
	for (i=0; i<tries; ++i) {
		UPDATE_PROG(i*100/tries);
		char move;
		
		setup_snake(1);
		
		// Fork, make father the white player
		int pid = fork();
		if (!pid) usleep(1000);
		PRINT("pid=%d opening\n",pid);
		int fd = open(get_node_name(0),O_RDWR);
		
		// Father (white player) forks and moves a lot (3xRIGHT).
		// The game should be OK at all times
		// First, father should move down once
		if (pid) {
			PRINT("Father moving down\n");
			move = '2';
			ASSERT(write(fd,&move,1) == 1);
			move = '6';
			PRINT("Father forking...\n");
			FORK(2);
			PRINT("child_num=%d, moving right\n",child_num);
			ASSERT(write(fd,&move,1) == 1);	// Move right 3 times
			PRINT("child_num=%d, moved right. Entering cleanup\n",child_num);
			P_CLEANUP();					// This also waits for the black player
			PRINT("Father cleaned up, closing\n");
			close(fd);
		}
		// The black player needs to be the one to test the game's integrity
		else {
			move = '8';
			PRINT("Black moving up\n");
			ASSERT(write(fd,&move,1) == 1);
			int i;
			move = '6';
			for (i=0; i<3; ++i) {
				PRINT("Black doing right #%d/3\n",i+1);
				ASSERT(write(fd,&move,1) == 1);
				PRINT("Black did right #%d/3, checking...\n",i+1);
				// Get the board and check it.
				// It should be in exactly one of two states, depending on whether or not
				// the other player got to move before we printed.
				Matrix m;
				read_and_parse(fd,&m);
				ASSERT(is_good_grid(&m));
				switch(i) {
				case 0:
					ASSERT(m[2][1] == -1 && m[2][0] == -2 && m[3][0] == -3);
					ASSERT(m[1][1] == 1 || m[1][2] == 1);					// White snake could be ahead
					break;
				case 1:
					ASSERT(m[2][2] == -1 && m[2][1] == -2 && m[2][0] == -3);
					ASSERT(m[1][2] == 1 || m[1][3] == 1);					// White snake could be ahead
					break;
				case 2:
					ASSERT(m[2][3] == -1 && m[2][2] == -2 && m[2][1] == -3);
					ASSERT(m[1][3] == 1);									// White snake can ONLY be here
					break;
				}
			}
			PRINT("Black closing & exiting\n");
			close(fd);
			exit(0);
		}
		
		PRINT("Father destroying\n");
		destroy_snake();
		
	}
	return TRUE;
}

// Writing with a NULL pointer should return EFAULT,
// unless count==0 in which case 0
bool write_null_chars() {
	SETUP_P(1,1);
	int fd = open(get_node_name(0),O_RDWR);
	errno = 0;
	ASSERT(write(fd,NULL,1) == -1);
	ASSERT(errno == EFAULT);
	errno = 0;
	ASSERT(write(fd,NULL,0) == 0);	// This should be OK
	ASSERT(errno == 0);
	usleep(1000);
	close(fd);
	DESTROY_P();
	return TRUE;
}

// Writing 0 chars should return 0
bool write_0_chars() {
	SETUP_P(1,1);
	int fd = open(get_node_name(0),O_RDWR);
	char move;
	errno = 0;
	ASSERT(write(fd,&move,0) == 0);
	ASSERT(errno == 0);
	usleep(1000);
	close(fd);
	DESTROY_P();
	return TRUE;
}

// Writing N chars (successfully) should return N
bool write_N_chars() {
	char w_moves[] = {'2','6','6','6'};
	char b_moves[] = {'8','6','6','6'};
	int i;
	for (i=0; i<4; ++i) {
		SETUP_P(1,1);
		int fd = open(get_node_name(0),O_RDWR);
		if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
			ASSERT(write(fd,w_moves,i) == i);
		}
		else {
			ASSERT(write(fd,b_moves,i) == i);
		}
		usleep(1000);
		close(fd);
		DESTROY_P();
	}
	return TRUE;
}

// Writing N+M chars where character N+1 causes loss returns N
bool write_N_then_loss() {
	char b_moves[] = {'8','6','6','6'};
	char w_moves[] = {'2','6','6','6'};
	char loss_char = '4';	// For either color, at any stage of the game
	int i;
	for (i=0; i<4; ++i) {
		UPDATE_PROG(i*100/4);
		// White loser:
		SETUP_P(1,1);
		int fd = open(get_node_name(0),O_RDWR);
		if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
			char tmp = w_moves[i];
			w_moves[i] = loss_char;
			int ret = write(fd,w_moves,4);
			PRINT("Write() returned %d\n",ret);
			ASSERT(ret == i+1);
			ASSERT(ioctl(fd,SNAKE_GET_WINNER) == BLACK_COLOR);
			w_moves[i] = tmp;
		}
		else {
			write(fd,b_moves,4);	// Let the white player lose
		}
		usleep(1000);
		close(fd);
		DESTROY_P();
		// Black loser:
		SETUP_P(1,1);
		fd = open(get_node_name(0),O_RDWR);
		if (ioctl(fd,SNAKE_GET_COLOR) == BLACK_COLOR) {
			char tmp = b_moves[i];
			b_moves[i] = loss_char;
			ASSERT(write(fd,b_moves,4) == i+1);
			ASSERT(ioctl(fd,SNAKE_GET_WINNER) == WHITE_COLOR);
			b_moves[i] = tmp;
		}
		else {
			write(fd,w_moves,4);	// Let the black player lose
		}
		usleep(1000);
		close(fd);
		DESTROY_P();
	}
	return TRUE;
}

// Try writing bulk (2+ moves), and while we're waiting for the other player, the other
// player calls close(). Make sure we won't have deadlock.
bool single_listen_for_rival_close() {
	char b_moves[] = {'8','6','6','6'};
	char w_moves[] = {'2','6','6','6'};
	int i;
	for (i=0; i<4; ++i) {
		UPDATE_PROG(i*100/4);
		// White loser:
		SETUP_P(1,1);
		int fd = open(get_node_name(0),O_RDWR);
		if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
			write(fd,w_moves,i);	// Write a bit, and then close
			close(fd);
		}
		else {
			write(fd,b_moves,4);	// Make sure we get the message...
			usleep(1000);
			close(fd);
		}
		DESTROY_P();
		// Black loser:
		SETUP_P(1,1);
		fd = open(get_node_name(0),O_RDWR);
		if (ioctl(fd,SNAKE_GET_COLOR) == BLACK_COLOR) {
			write(fd,b_moves,i);	// Write a bit, and then close
			close(fd);
		}
		else {
			write(fd,w_moves,4);	// Make sure we get the message...
			usleep(1000);
			close(fd);
		}
		DESTROY_P();
	}
	return TRUE;
}

// Same thing, but many processes (all the same color player) try to write (and wait),
// then the other player calls close(). Make sure they ALL get the signal
bool many_listen_for_rival_close() {
	char b_moves[] = {'8','6','6','6'};
	char w_moves[] = {'2','6','6','6'};
	int i;
	for (i=0; i<4; ++i) {
		
		// Once for the white player:
		setup_snake(1);
		
		// Fork, father should be the white player
		int is_white = fork();
		if (!is_white) usleep(500);
		int fd = open(get_node_name(0),O_RDWR);
		
		// White player moves down, then splits into 3,
		// and each process moves right.
		if (is_white) {
			write(fd,w_moves,1);
			int kids[2];
			kids[0] = fork();
			if (kids[0]) {
				kids[1] = fork();
			}
			// Three children from here. Each one should move right.
			write(fd,w_moves+1,1);
			// Make the kids exit. They may close the game, so usleep() here
			usleep(1000);
			if (!kids[0] || !kids[1]) exit(0);
		}
		// Black player just calls close after moving i times
		else {
			write(fd,b_moves,i);	// Move a little, close fast (DONT WAIT), this
			close(fd);				// is what we're testing
			exit(0);
		}
		close(fd);		// Only the father gets here
		while(wait(NULL) != -1);
		destroy_snake();
		
		// Once for the black player:
		setup_snake(1);
		
		// Fork, father should be the black player
		int is_black = fork();
		if (is_black) usleep(500);
		fd = open(get_node_name(0),O_RDWR);
		
		// White player moves down, then splits into 3,
		// and each process moves right.
		if (is_black) {
			write(fd,b_moves,1);
			int kids[2];
			kids[0] = fork();
			if (kids[0]) {
				kids[1] = fork();
			}
			// Three children from here. Each one should move right.
			write(fd,b_moves+1,1);
			// Make the kids exit. They may close the game, so usleep() here
			usleep(1000);
			if (!kids[0] || !kids[1]) exit(0);
		}
		// Black player just calls close after moving i times
		else {
			write(fd,w_moves,i);	// Move a little, close fast (DONT WAIT), this
			close(fd);				// is what we're testing
			exit(0);
		}
		close(fd);		// Only the father gets here
		while(wait(NULL) != -1);
		destroy_snake();
	}
	return TRUE;
}

// Same thing, but the closing process is the same color as the writers
bool many_listen_for_friendly_close() {
	char b_moves[] = {'8','6','6','6'};
	char w_moves[] = {'2','6','6','6'};
	// White loser:
	setup_snake(1);
	int pid = fork();
	if (!pid) usleep(500);	// Make sure the father is the white player
	int fd = open(get_node_name(0),O_RDWR);
	if (pid) {		// Father (white)
		write(fd,w_moves,1);	// Do first move (down)
		FORK(2);				// Make three white processes
		if (child_num)
			write(fd,w_moves+1,1);	// Go right (twice)
		else {
			usleep(500);
			close(fd);
		}
		usleep(1000);
		if (child_num)
			exit(0);
		close(fd);				// Make sure we get here
	}
	else {			// Child (black)
		write(fd,b_moves,4);	// Write a little, and then close! Make sure the other player gets it...
		usleep(1000);
		close(fd);
		exit(0);
	}
	P_CLEANUP();
	usleep(1000);
	destroy_snake();
	return TRUE;
}

// Lose by hitting a wall. Make sure each wall hit (all 4) causes loss.
// Thanks to Naama for the heads up that this may be a problem...
bool hit_walls() {
	// Two white wall hits, two black ones
	char move;
	// LEFT WALL:
	SETUP_P(1,1);
	int fd = open(get_node_name(0),O_RDWR);
	if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
		move = '4';
		write(fd,&move,1);
		ASSERT(ioctl(fd,SNAKE_GET_WINNER) == BLACK_COLOR);
		close(fd);
	}
	else {
		usleep(500);
		close(fd);
	}
	DESTROY_P();
	// TOP WALL:
	SETUP_P(1,1);
	fd = open(get_node_name(0),O_RDWR);
	if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
		move = '8';
		write(fd,&move,1);
		ASSERT(ioctl(fd,SNAKE_GET_WINNER) == BLACK_COLOR);
		close(fd);
	}
	else {
		usleep(500);
		close(fd);
	}
	DESTROY_P();
	// BOTTOM WALL:
	SETUP_P(1,1);
	fd = open(get_node_name(0),O_RDWR);
	if (ioctl(fd,SNAKE_GET_COLOR) == WHITE_COLOR) {
		move = '2';	// Don't lose
		write(fd,&move,1);	// Let black player move
		usleep(500);
		close(fd);
	}
	else {
		move = '2';
		write(fd,&move,1);
		ASSERT(ioctl(fd,SNAKE_GET_WINNER) == WHITE_COLOR);
		close(fd);
	}
	DESTROY_P();
	// RIGHT WALL:
	// TOO LAZY FOR THIS.
	// We need to make sure both snakes eat sometime, otherwise we'll die of starvation and
	// not from crashing...
	return TRUE;
}

/* ***************************
 IOCTL TESTS
*****************************/

// GET_WINNER should return -1 if no one has won yet.
bool no_winner_yet() {
	SETUP_OPEN_SIMPLE(FALSE);
	ASSERT(ioctl(fd,SNAKE_GET_WINNER) == -1);
	DESTROY_CLOSE_SIMPLE();
	return TRUE;
}

// GET_WINNER should return -1 (with errno 10) if the game is closed.
bool winner_after_closed() {
	SETUP_OPEN_SIMPLE(FALSE);
	if (P_IS_FATHER())
		close(fd);
	else {
		errno = 0;
		ASSERT(ioctl(fd,SNAKE_GET_WINNER) == -1);
		ASSERT(errno == 10);
		close(fd);
	}
	DESTROY_P();
	return TRUE;
}

// Test GET_WINNER after win (also test several concurrent calls)
bool winner_wins() {
	SETUP_OPEN_SIMPLE(TRUE);
	if (P_IS_FATHER()) {
		char move = '8';	// Lose
		write(fd,&move,1);
	}
	usleep(500);
	int i, tries = 10;
	for (i=0; i<tries; ++i)
		ASSERT(ioctl(fd,SNAKE_GET_WINNER) == BLACK_COLOR);
	DESTROY_CLOSE_SIMPLE();
	return TRUE;
}

// GET_COLOR should work before and after win
bool color_before_after_win() {
	SETUP_OPEN_SIMPLE(TRUE);
	int i, tries=10;
	for(i=0; i<tries; ++i)
		ASSERT(ioctl(fd,SNAKE_GET_COLOR) == (P_IS_FATHER()? WHITE_COLOR:BLACK_COLOR));
	if (P_IS_FATHER()) {
		char move = '8';	// Lose
		write(fd,&move,1);
	}
	for(i=0; i<tries; ++i)
		ASSERT(ioctl(fd,SNAKE_GET_COLOR) == (P_IS_FATHER()? WHITE_COLOR:BLACK_COLOR));
	DESTROY_CLOSE_SIMPLE();
	return TRUE;
}

// GET_COLOR shouldn't work after exit
bool color_fail_after_close() {
	SETUP_OPEN_SIMPLE(FALSE);
	if (P_IS_FATHER())
		close(fd);
	else {
		errno = 0;
		ASSERT(ioctl(fd,SNAKE_GET_COLOR) == -1);
		ASSERT(errno == 10);
		close(fd);
	}
	DESTROY_P();
	return TRUE;
}

// Make sure values other than SNAKE_GET_COLOR and SNAKE_GET_WINNER return ENOTTY
bool ioctl_no_op() {
	SETUP_OPEN_SIMPLE(FALSE);
	// I don't know what the numbers are, so just take some from far away
	errno = 0;
	ASSERT(ioctl(fd,1000) == -1);
	ASSERT(errno == ENOTTY);
	errno = 0;
	ASSERT(ioctl(fd,-1000) == -1);
	ASSERT(errno == ENOTTY);
	DESTROY_CLOSE_SIMPLE();
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
	RUN_TEST(read_null_efault);
	RUN_TEST(read_0_return_0);
	RUN_TEST(read_N_lt_grid_returns_N);
	RUN_TEST(read_N_eq_grid_returns_N);
	RUN_TEST(read_N_gt_grid_returns_N);
	RUN_TEST(read_after_release);
	RUN_TEST(many_readers_while_releasing_p);
	RUN_TEST(many_readers_while_releasing_t);

	TEST_AREA("write");
	RUN_TEST(write_shows_up);
	RUN_TEST(write_valid_to_finished);
	RUN_TEST(write_invalid_to_good_game);
	RUN_TEST(write_invalid_to_finished);
	RUN_TEST(read_ioctl_after_loss);
	RUN_TEST(read_ioctl_after_loss_and_then_write);
	RUN_TEST(starve_to_death);
	RUN_TEST(white_starves_first);
	RUN_TEST(eat_and_starve_later);
	RUN_TEST(move_to_tail);
	RUN_TEST(single_write_turns);
	RUN_TEST(bulk_write_turns);
	RUN_TEST(multiple_white_writers);
	RUN_TEST(invalid_move_loses);
	RUN_TEST(invalid_nth_move_loses);
	RUN_TEST(write_null_chars);
	RUN_TEST(write_0_chars);
	RUN_TEST(write_N_chars);
	RUN_TEST(write_N_then_loss);
	RUN_TEST(single_listen_for_rival_close); 
	RUN_TEST(many_listen_for_rival_close);	// WHY DOESN'T THIS FUCKING WORK
	RUN_TEST(many_listen_for_friendly_close);
	RUN_TEST(hit_walls);

	TEST_AREA("ioctl");
	RUN_TEST(no_winner_yet);
	RUN_TEST(winner_after_closed);
	RUN_TEST(winner_wins);
	RUN_TEST(color_before_after_win);
	RUN_TEST(color_fail_after_close);
	RUN_TEST(ioctl_no_op);
	
	// That's all folks
	END_TESTS();
	return 0;
	
}

