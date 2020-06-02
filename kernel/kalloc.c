// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "spinlock.h"
#include "rand.h"

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

extern char end[]; // first address after kernel loaded from ELF file

/* Variables created by Roxin Liu: */
char *allocated[512]; // History: The addresses of all allocated page in order
int num_alloc = 0;  // the number of allocated pages
int size_freelist = 0;  // number of free pages in freelist

// Initialize free list of physical pages.
void
kinit(void)
{
  char *p;

  initlock(&kmem.lock, "kmem");
  p = (char*)PGROUNDUP((uint)end);
  for(; p + PGSIZE <= (char*)PHYSTOP; p += PGSIZE) 
    kfree(p);
  
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || (uint)v >= PHYSTOP) 
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  size_freelist += 1;
  release(&kmem.lock);
}

/* Edited by Roxin Liu: */

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r; // points the page to be allocated

  acquire(&kmem.lock);
  
  r = kmem.freelist; // head of the freelist
  int i, rd = xv6_rand() % size_freelist;
    // rd is the index of the page to be allocated in freelist
  
  // Case when first page is allocated:
  if (rd == 0)
    kmem.freelist = r->next; // r->next becomes the head

  // Case when the page is else where:
  else 
  {
    // Traverse through the freelist to find the page:
    struct run *r_prev;
    for (i=0; i<rd; i++)
    {
      r_prev = r;
      r = r -> next;
    }
    // Remove r from the freelist
    r_prev -> next = r -> next;
  }

  release(&kmem.lock);

  allocated[num_alloc] = (char*)r;
  num_alloc += 1;
  size_freelist -= 1;
  
  return (char*)r;
}

// Store and return the addresses of the last n pages allocated,
// where n = numframes.
int 
dump_allocated(int *frames, int numframes) 
{
  // Invalid numframes:
  if (numframes > num_alloc)
    return -1;

  for (int i=0; i<numframes; i++)
    frames[i] = (int)allocated[num_alloc-1-i];

  return 0;
}
