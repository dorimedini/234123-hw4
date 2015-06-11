#include <unistd.h>		// For fork() and execv()
#include <sys/wait.h>	// For wait()
#include <stdlib.h>		// For exit()

int main() {
	
	int status;
	
	
	// Install module
	char *install = "/root/hw4/install.sh";
	char *argv1[] = { install, "1", '\0'};
	if (!fork()) execv(install, argv1);
	else while(wait(&status) != -1);
	
	// Open and close
	int pid = fork();
	int fd = open("/dev/snake0",O_RDWR);
	close(fd);
	
	// Remove module
	char *uninstall = "/root/hw4/uninstall.sh";
	char *argv2[] = {uninstall, '\0'};
	if (!fork()) execv(uninstall, argv2);
	else wait(&status);
	
	return 0;
	
}
