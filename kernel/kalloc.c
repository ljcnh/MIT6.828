// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

#define POS(p)  (((uint64)p - KERNBASE) >> 12)  //   /PGSIZE 等价于 >> 12
#define CLEN    (PHYSTOP - KERNBASE + PGSIZE - 1)/PGSIZE

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  // 所有界面的引用次数
  struct spinlock reflock;
  uint8 refercount[CLEN];
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kmem.reflock, "kmem refcnt");
  memset(&kmem.refercount, 1, sizeof(kmem.refercount));
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if(kreferCount((void*)pa, -1) != 0){
    return;
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    kreferCount((void*)r, 1);
  }
  return (void*)r;
}

// c > 0 : +1
// c < 0 : -1
uint8
kreferCount(void *pa, int c)
{
  int pos = POS(pa);
  uint refcnt = 0;
  acquire_reflock();
  if(c < 0){
    if(kmem.refercount[pos] == 0){
      panic("refer count less than 0");
    }
    kmem.refercount[pos] -= 1;
  }else{
    if(kmem.refercount[pos] == 255){
      panic("refer count more than 255");
    }
    kmem.refercount[pos] += 1;
  }
  refcnt = kmem.refercount[pos];
  release_reflock();
  return refcnt;
}

uint8
getrefcnt(uint64 pa)
{
  uint8 refcnt = 0;
  int pos = POS(pa);
  acquire_reflock();
  refcnt = kmem.refercount[pos];
  release_reflock();
  return refcnt;
}

void
acquire_reflock()
{
  acquire(&kmem.reflock);
}

void
release_reflock()
{
  release(&kmem.reflock);
}
