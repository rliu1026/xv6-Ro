#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#include "pstat.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

/* Key variables added by Roxin Liu: */

// Variables for MLFQ: 
static struct proc *mlfq[NLAYER][512];
static int mlfq_size[NLAYER] = {0,0,0,0};

int limit_total[NLAYER] = {64, 32, 16, 8};
int limit_rr[NLAYER] = {64, 4, 2, 1};

// Variables for Semaphores:
struct semaphore {
  int id;
  int value;
  int used;
  //void* channel;
  struct spinlock splock;
};
struct semaphore sem_list[NUM_SEMAPHORES];
int num_sem = 0;
struct spinlock lock_semlist;


void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack if possible.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  // Place p in mlfq highest level:
  p -> level = 3;
  for (int i=0; i<NLAYER; i++)
  {
    p -> ticks_curr = 0;
    p -> wait_ticks_curr = 0;
    p -> ticks[i] = 0;
    p -> wait_ticks[i] = 0;
  }
  
  mlfq[3][mlfq_size[3]] = p;
  mlfq_size[3] += 1;

  return p;
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  acquire(&ptable.lock);
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
 
  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
clone(void(*fcn)(void*, void*), void *arg1, void *arg2, void *stack)
{
  int i, pid;
  struct proc *np;
  char *stack_physical;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  np->pgdir = proc->pgdir;
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  // Set initial esp, initial eip, initial ebp, and ustack for new process
  np->tf->esp = (uint)stack + PGSIZE - 12;
  np->tf->eip = (uint)fcn;
  np->tf->ebp = 0x0;
  np->ustack = (char *)stack;

  // Push arguments and return address onto stack
  stack_physical = uva2ka(proc->pgdir, (char *)stack);
  *(void **)(stack_physical + PGSIZE - 4) = arg2;
  *(void **)(stack_physical + PGSIZE - 8) = arg1;
  *(uint *)(stack_physical + PGSIZE - 12) = 0xffffffff;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
join(void **stack)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc || p->pgdir != proc->pgdir)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        if (p == proc) {
          panic("proc");
        }
        *stack = (void *)p->ustack;
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        p->ustack = 0;
        p->pgdir = 0;
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      *stack = NULL;
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}


// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  iput(proc->cwd);
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Added by Roxin Liu: 
// helper for MLFQ

void
rm_priority(int level, int index)
{
  //struct proc *p = mlfq[level][index];
  if (mlfq_size[level] > 1)
  {
    int i;
    for (i=index; i<mlfq_size[level]-1; i++)
    {
      mlfq[level][i] = mlfq[level][i+1];
      mlfq[level][i+1] = NULL;
    }    
  }
  else
    mlfq[level][index] = NULL;
  
  mlfq_size[level] -= 1;
  return;
}

// Edited by Roxin Liu: 

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
// MLFQ scheduler:
void
scheduler(void)
{
  struct proc *p;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    // Lock ptable:
    acquire(&ptable.lock);

    int lvl;
    for (lvl = NLAYER-1; lvl >= 0; lvl--)
    {
      int i; 
      for (i = 0; i < mlfq_size[lvl]; i++)
      {
        // Check each proc in priority queue:
        // If not runnable, continue to next iteration;
        // If there is no runnable proc in this level, 
        // move to next level when the for loop exits.
        p = mlfq[lvl][i];
        if (p -> state != RUNNABLE)
          continue;
          
        // Runnable process found:
        proc = p;
     
        // Incrementing wait_ticks on each proc's own level:
        struct proc *p_wait;
        for(p_wait = ptable.proc; p_wait < &ptable.proc[NPROC]; p_wait++) {
          if (p_wait -> state == RUNNABLE && p_wait->pid > 0) {
            p_wait -> wait_ticks[p_wait->level] += 1;
            p_wait -> wait_ticks_curr += 1;
          }
        }

        // Run p:
        switchuvm(p); 
        p -> state = RUNNING;
        p -> ticks_curr += 1;
        p -> wait_ticks_curr = 0;
        p -> ticks[lvl] += 1;
        //p -> wait_ticks[lvl] = 0;
        swtch(&cpu->scheduler, proc->context); // context switch and run
        switchkvm();
        proc = 0;
        
        // Downgrading:
        if (lvl > 0 && p -> ticks_curr >= limit_total[lvl])
        { 
          // Remove p from current level: 
          rm_priority(lvl, i);

          // Add p to new level:
          mlfq[lvl-1][mlfq_size[lvl-1]] = p;
          mlfq_size[lvl-1] += 1;

          // Set p's struct members:
          p -> ticks_curr = 0;
          p -> wait_ticks_curr = 0;
          p -> wait_ticks[lvl] = 0; // Bad idea
          //p -> wait_ticks[lvl] = 0;
          p -> level = lvl - 1;
        }
        
        // Round robin:
        else if ((p->ticks_curr % limit_rr[lvl]) == 0)
        {
          //cprintf("RR: level %d index %d\n", lvl, i);
          
          // Remove p from current level: 
          rm_priority(lvl, i);
          
          // Add p to the end of the queue:
          mlfq[lvl][mlfq_size[lvl]] = p;
          mlfq_size[lvl] += 1;
        }
        
        
        // Promotion:
        struct proc *p_starve;
        for (lvl = NLAYER-1; lvl >= 0; lvl--)
        {
          int i; 
          for (i = 0; i < mlfq_size[lvl]; i++)
          {
            p_starve = mlfq[lvl][i];
            
            // Check if p_starve has waited 10 times the time slice in its current level:
            if (p_starve -> wait_ticks_curr >= 10 * limit_total[p_starve->level])
            {
              rm_priority(lvl, i);
              mlfq[lvl+1][mlfq_size[lvl+1]] = p_starve;
              mlfq_size[lvl+1] += 1;
              p_starve -> ticks_curr = 0;
              p_starve -> wait_ticks_curr = 0;
              p_starve -> wait_ticks[lvl] = 0; // Bad idea
              p_starve -> level += 1;
            }
          }
        }

        i = -1; 
          // Reset to run from beginning of the queue if a proc has been run
      }
    }

    /* 
     * ROUND-ROBIN DEFAULT SCHEDULER:
     *
    
    // Loop over process table looking for process to run.
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p); 
      	// switch to user virtual memory, 
	    // which assigns PTBR the address of user mode page table.
	    // PTBR stands for page table base register 
      p->state = RUNNING;

      //cprintf("Running process PID %d and name %s\n", p->pid, p->name);
      //cprintf("time before switch = %LF\n", (long double)ticks);

      // Jumps or switches the execution from here 
      // to whatever is in proc->context:
      swtch(&cpu->scheduler, proc->context); // src: kernel/swtch.S
	
      //cprintf("time after switch = %LF\n", ticks); 	
      
      switchkvm(); // switch to kernel virtual memory, setting PTBR.

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    */
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    // If wake up happens after this line, 
    // then we will not be interrupted
    release(lk);
    // we can now safely release the lock having ptable lock grabbed
  }

  // Go to sleep.
  proc->chan = chan; // Remember the channel passed to it
  proc->state = SLEEPING;
  sched();  // switch to scheduler context 
            // to pick some other thread to run
  // the next line here will be executed only when this thread is waken up 

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

/* Added by Roxin Liu: */


// This system call stores info of all proc in mlfq to pstat:
int 
getprocinfo(struct pstat *allstat)
{
  if (allstat == NULL)
    return -1; // failure

  struct proc *p;
  int i, j;
  
  acquire(&ptable.lock);

  // Iterate through all procs in mlfq:

  //for(i=0; i<NPROC; i++){
    //p = &ptable.proc[i]; 
  for (int lvl = NLAYER-1; lvl >= 0; lvl--)
  {
    //cprintf("lvl = %d\n", lvl);
    for (i = 0; i < mlfq_size[lvl]; i++)
    {
      p = mlfq[lvl][i]; 
      
      // Collect info:
      allstat -> inuse[i] = 1;
      allstat -> pid[i] = p -> pid;
      allstat -> priority[i] = p -> level;
      allstat -> state[i] = p -> state;

      for (j=0; j<NLAYER; j++)
      {
        allstat -> ticks[i][j] = p -> ticks[j];
        allstat -> wait_ticks[i][j] = p -> wait_ticks[j];
      }
    
    }
  }
  release(&ptable.lock);
  return 0;	
}

// This system call boost the current process to one higher priority level.
int 
boostproc(void)
{
  struct proc *p = proc; // p points to current process
  int level = p -> level; // proc's current level
  if (level == 3) 
    return 0; // Do not boost at highest level

  int i, index = -1;
  for (i = 0; i < mlfq_size[level]; i++)
  {
      if (p -> pid == mlfq[level][i] -> pid)
      {
        index = i; // location of p found in its level
        break;
      }
  }
  if (index == -1)
    return 0; // location of p not found
  
  // Removing p from current level:
  rm_priority(level, index);

  // Add p to higher level:
  mlfq[level+1][mlfq_size[level+1]] = p;
    mlfq_size[level+1] += 1;
  
  // Change p's members:
  p -> ticks_curr = 0;
  p -> wait_ticks_curr = 0;
  p -> wait_ticks[level] = 0; // Bad idea
  p -> level += 1;

	return 0;
}


int 
mprotect(void *addr, int len)
{
  // Invalid addr:  
  if (addr != PGROUNDDOWN(addr))
    return -1; // Not aligned with page starting address
  if (len <= 0)
    return -1;

  char *last = addr + (len - 1) * PGSIZE;
  for (;;)
  {
    // Find the target page of the page table:
    pde_t *pde = &(proc->pgdir[PDX(addr)]);
    if(*pde & PTE_P)
    {
      pte_t *pgtab = (pte_t*)PTE_ADDR(*pde);
      pte_t *pte = &pgtab[PTX(addr)];
      
      // If this PTE exists:
      if(*pte & PTE_P) 
      {
        *pte = *pte & 0xFFFFFFFD; 
        // Change protection bit (2nd last bit) to 0
        // 0xFFD = 1111 1111 1101
      }
      else
        return -1;
    }
    if (addr == last)
      break;
    addr += PGSIZE;

    //if (first == last)
      //break;
    //first += PGSIZE;
    
  }
  
  return 0;
}

int 
munprotect(void *addr, int len)
{
  // Invalid addr:  
  if (addr != PGROUNDDOWN(addr))
    return -1; // Not aligned with page starting address
  if (len <= 0)
    return -1;

  char *last = addr + (len - 1) * PGSIZE;
  for (;;)
  {
    // Find the target page of the page table:
    pde_t *pde = &(proc -> pgdir[PDX(addr)]);
    if(*pde & PTE_P)
    {
      pte_t *pgtab = (pte_t*)PTE_ADDR(*pde);
      pte_t *pte = &pgtab[PTX(addr)];
      
      // If this PTE exists:
      if(*pte & PTE_P) 
      {
        *pte = *pte | 0x0000002; 
        // Change protection bit (2nd last bit) to 1
        // 0x002 = 0000 0000 0010
      }
      else
        return -1;
    }
    if (addr == last)
      break;
    addr += PGSIZE;
  }
  
  return 0;
}

// Semaphore-related systemcalls below:

// Wake up all processes sleeping on chan.
void
wakeup2(void *chan)
{
  acquire(&ptable.lock);
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    
    // Check if the proc is sleeping and is sleeping on this channel:
    int* pchan_buff = (int*)(p->chan);
    int* chan_buff = (int*)chan;
    
    if(p->state == SLEEPING && *pchan_buff == *chan_buff) {
      p->state = RUNNABLE; // let scheduler consider p to run next time
      //break;
    } 
  }
  release(&ptable.lock);
}

// Initialize one semaphore
int
sem_init(int* sem_id, int count)
{
  if (num_sem >= NUM_SEMAPHORES) 
    return -1;
  
  int foundSem = 0;
  acquire(&lock_semlist);

  // loop through sem_list to find one unused:
  for (int i = 0; i < NUM_SEMAPHORES; i++)
  {
    struct semaphore* sem = &sem_list[i];
    if (sem -> used == 0)
    {
      foundSem = 1;

      *sem_id = i; 
        // sem_id is assigned as the index of the sem
      sem -> id = *sem_id;
      sem -> value = count;
      sem -> used = 1;
      initlock(&sem->splock, "SemaSpinLock");

      num_sem ++;
      break;
    }

  }
  release(&lock_semlist);

  if (foundSem == 0)
    return -1;
  
  return 0;
}

// Decrement sem->value, 
// put current thread to sleep if non-positive. 
int 
sem_wait(int sem_id)
{ 
  int foundSem = 0;
  struct semaphore* sem = &sem_list[sem_id];
  if (sem -> id == sem_id && sem -> used == 1)
  {
    foundSem = 1;

    acquire(&(sem -> splock));
    
    while (sem -> value <= 0)
    {
      sleep(&sem, &(sem -> splock));
        // use &sem (unique for each sem) as channel so thread
        // remembers which sem it is waiting for while sleeping 
    }
    sem -> value -= 1;

    release(&(sem -> splock));
  }
  
  if (foundSem == 0)
    return -1;

  return 0;
}

// Increment sem->value
// and wake up all threads sleeping on this sem.
int
sem_post(int sem_id)
{
  int foundSem = 0;
  struct semaphore* sem = &sem_list[sem_id];
  if (sem -> id == sem_id && sem -> used == 1)
  {
    foundSem = 1;

    acquire(&(sem -> splock));

    sem -> value += 1;
    wakeup2(&sem);
    // wakes all threads waiting for this semaphore
    
    release(&(sem -> splock));
  }
  
  if (foundSem == 0)
    return -1;
  
  return 0;
}

// Destroy the semaphore with sem_id.
int
sem_destroy(int sem_id)
{
  int foundSem = 0;
  struct semaphore* sem = &sem_list[sem_id];
  if (sem -> id == sem_id)
  {
    foundSem = 1;
    acquire(&lock_semlist);
    sem -> used = 0;
    release(&lock_semlist);
  }
  if (foundSem == 0)
    return -1;
  return 0;
}

int 
getfilenum(int pid)
{
	struct proc *p;
	if (pid == 0) {
		return -1;
	}
	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) 
	{
		if (p -> pid == pid) 
		{

			// Find number of files opened 
			// from file descriptor: 
			int fd;
			int filenum = 0;
			//prinf(1, "NOFILE = %d\n", NOFILE);
			for (fd = 0; fd < NOFILE; fd++) 
			{
				if (p -> ofile[fd] != 0) 
				{
					filenum++;
				}
			}
			return filenum;
		}
	}
	return -1;
}