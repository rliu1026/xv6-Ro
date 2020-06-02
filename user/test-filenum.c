/* Written by Roxin Liu */

#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"
#include "fcntl.h"

int main(void) 
{
	printf(1, "PID of current proc = %d\n", getpid());
	
	int fd1 = open("grave.txt", O_CREATE | O_RDWR);
	int fd2 = open("grave.txt", O_CREATE | O_RDWR);

	printf(1, "file number after opening two files = %d\n", getfilenum(getpid()));
	printf(1, "fd1 at descriptor %d, fd2 at descriptor %d\n", fd1, fd2);
	
	close(fd1);
	printf(1, "file number after closing one = %d\n", getfilenum(getpid()));	
	
	close(fd2);
	printf(1, "file number after closing both = %d\n", getfilenum(getpid()));
	
	printf(1, "file number for current proc = %d\n", getfilenum(getpid()));
	printf(1, "\nAll results below should be -1: \n");
	printf(1, "file number = %d\n", getfilenum(NULL));
	printf(1, "file number = %d\n", getfilenum(-1));
	printf(1, "file number = %d\n", getfilenum(99999));

	exit();
}
