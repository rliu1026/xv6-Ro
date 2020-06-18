# xv6_Ro
An advanced version of xv6 with MLFQ scheduler, file access protection, semaphore, file system checker, and more.

The original xv6 version I based on was used by UW Madison Computer Science Department in Comp Sci 537. Throughout my Operation System Course I have added several features and improvements to xv6. Here are my modifications:

1. Change the process scheduler from Round Robin to Multi-Level Feedback Queue (MLFQ). Details:
		Four priority levels.
    Number the queues from 3 for highest priority queue down to 0 for lowest priority queue.
    Whenever the xv6 timer tick (10 ms) occurs, the highest priority ready process is scheduled to run.
    The highest priority ready process is scheduled to run whenever the previously running process exits, sleeps, or otherwise yields the CPU.
    
    The time-slice associated with priority queues are as follows:
		Priority queue 3: 8 timer ticks (or ~80ms)
    Priority queue 2: 16 timer ticks
    Priority queue 1: 32 timer ticks
    Priority queue 0 it executes the process until completion.
    
    If there are more than one processes on the same priority level, then you scheduler should schedule all the processes at that particular level in a round-robin fashion.
    The round-robin slices differs for each queue and is as follows:
    Priority queue 3 slice : 1 timer ticks
    Priority queue 2 slice : 2 timer ticks
    Priority queue 1 slice : 4 timer ticks
    Priority queue 0 slice : 64 timer ticks
    
    When a new process arrives, it should start at priority 3 (highest priority).
		At priorities 3, 2, and 1, after a process consumes its time-slice it should be downgraded one priority. At priority 0, the process should be executed to completion.
    If a process voluntarily relinquishes the CPU before its time-slice expires at a particular priority level, its time-slice should not be reset; the next time that process is scheduled, it will continue to use the remainder of its existing time-slice at that priority level.
		To overcome the problem of starvation, we will implement a mechanism for priority boost. If a process has waited 10x the time slice in its current priority level, it is raised to the next higher priority level at this time (unless it is already at priority level 3). For the queue number 0 (lowest priority) consider the maximum wait time to be 6400ms which equals to 640 timer ticks. 
		To make the scheduling behavior more visible you will be implementing a system call that boosts a process priority by one level (unless it is already at priority level 3).

2. int getprocinfo(struct pstat *allstate)

	Storing info of all processes in MLFQ to the pstat struct passed in. NPROC is the number of processes. 
	
		struct pstat {
			int inuse[NPROC]; // whether this slot of the process table is in use (1 or 0)
			int pid[NPROC];   // PID of each process
			int priority[NPROC];  // current priority level of each process (0-3)
			enum procstate state[NPROC];  // current state (e.g., SLEEPING or RUNNABLE) of each process
			int ticks[NPROC][4];  // number of ticks each process has accumulated at each of 4 priorities
			int wait_ticks[NPROC][4]; // number of ticks each process has waited before being scheduled
		};

3. int boostproc(void)

	Increase the priority level of the current process by 1, unless it is already at queue 3, the highest priority.

4. int mprotect(void *addr, int len)
	
	Change the protection bits of the page range to be read only, starting at addr and of len pages. Writing to these pages will cause a trap. The page protections should be inherited on fork(). 
		
5. int munprotect(void *addr, int len)
		
	Reverse of mprotect: Set the region of pages back to both readable and writeable.

6. New pages are allocated randomly (Xorshift) throughout the free list of page frames.

7. int dump_allocated(int *frames, int numframes) 
		
	Store a list of frame numbers (amount = numframes) that are currently allocated into argument frames. For example, your freelist has frames with address as follows: (23, 19, 16, 15, 13, 8, 5, 3) . It then allocates four frames with address 8, 19, 5, 15 in order. The freelist should now become (23, 16, 13, 3). And a call to dump_allocted(frames, 3), should have frames pointing to array (15, 5, 19).
		
8. Semaphore API.

	8.1. sem_init(int* sem_id, int count)
	There are a limited number of semaphores that will be available in our OS (defined as NUM_SEMAPHORES in include/types.h ). When users want to create a semaphore they call sem_init and the kernel searches for an unused semaphore. If one is found, its value is initialized to count, its ID is filled by the kernel in sem_id and the system call returns 0. If no such semaphore can be found, the system call returns -1.
	
	8.2. sem_wait(int sem_id)
	This first decrements the count by 1 for the semaphore given by sem_id. If the count becomes negative, the kernel puts the calling thread to sleep. Read about the sleep() function as implemented in xv6 kernel/proc.c. If sem_id is not a valid semaphore this system call again returns -1. The calling thread sleeps until the count is positive and then decrements the count. 
	
	8.3. sem_post(int sem_id)
	This function increments the count by 1 for the semaphore given by sem_id and wakes up a thread (if any) waiting on the semaphore. Read about wakeup() function from kernel/proc.c to know more about how to wake up threads waiting in the kernel. If sem_id is not a valid semaphore this system call again returns -1.
	
	8.4. sem_destroy(int sem_id)
	This function "destroys" or wipes out the semaphore given by sem_id. Once a semaphore is destroyed it can be reused in a future call to sem_init.

9. int getfilenum(int pid)
	Returns the number of file descriptors in use by the process identified by argument pid.

10. There is a file system checker that examines the consistency of the file system, xfsck.c, stored in tools directory. To compile and run from xv6/tools directory, run command: 
		$ gcc -iquote ../include -Wall -Werror -ggdb -o xfsck xfsck.c
		$ xfsck file_system_image
e.g.: $ xfsck xfsckTest/Good. If nothing is printed in console, the file system img has no error.
Testing img are in directory xfsckTest. Copyrights of files in xfsckTest belongs to original authors. Visualization of these testing img is here: https://shawnzhong.github.io/xv6-file-system-visualizer/

Please do not directly copy the code for college assignment. This repo is solely for the purpose of sharing knowledge, referencing and self-studying. Any form of copying can be considered plagiarism and academic misconduct. With this warning I would not take responsibility to any results from any personnel's usage of this code. I am open for suggestions for improvements. Thank you!
