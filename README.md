# xv6_Ro
An advanced version of xv6. Multiple features added and changed.

The original xv6 version I based on was used by UW Madison Computer Science Department for teaching. Throughout my Operation System course I have added several features and improvements to xv6. Here are my modifications:

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
		
		Store a list of frame numbers (amount = numframes) that are currently allocated into parameter frames. For example, your freelist has frames with address as follows: (23, 19, 16, 15, 13, 8, 5, 3) . It then allocates four frames with address 8, 19, 5, 15 in order. The freelist should now become (23, 16, 13, 3). And a call to dump_allocted(frames, 3), should have frames pointing to array (15, 5, 19).
		
8. Semaphore API.

		8.1. sem_init(int* sem_id, int count)
		
		8.2. sem_wait(int sem_id)
		
		8.3. sem_post(int sem_id)
		
		8.4. sem_destroy(int sem_id)
		
