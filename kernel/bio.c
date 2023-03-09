// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define HASH(blockno) (blockno % NBUCKET)
// trap.c 
extern uint ticks;

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  uint unused;
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  struct buf buckets[NBUCKET];
  struct spinlock bucketlocks[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  
  bcache.unused = 0;
  initlock(&bcache.lock, "bcache");

  char name[9];
  for(int i = 0; i < NBUCKET; i++) {
    snprintf(name, sizeof(name), "bcache_%d", i);
    initlock(&bcache.lock, name);
  }

  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucketid = HASH(blockno);

  struct buf *pre, *minb = 0, *minpre;
  uint mintimestamp;
  
  acquire(&bcache.bucketlocks[bucketid]);
  // Is the block already cached?
  for(b = &bcache.buckets[bucketid]; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucketlocks[bucketid]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucketlocks[bucketid]);


  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&bcache.lock);
  
  // 上方有释放锁的过程 在这个过程中可能会有其他的线程 做出改变
  acquire(&bcache.bucketlocks[bucketid]);
  for(pre = &bcache.buckets[bucketid],b = pre->next; b; pre = b, b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucketlocks[bucketid]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  if(bcache.unused < NBUF) {
    b = &bcache.buf[bcache.unused++];
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->next = bcache.buckets[bucketid].next;
    bcache.buckets[bucketid].next = b;

    release(&bcache.bucketlocks[bucketid]);
    release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.bucketlocks[bucketid]);

  for(int i = 0, idx = bucketid; i < NBUCKET; i++) {
    mintimestamp = -1;
    acquire(&bcache.bucketlocks[idx]);
    for(pre = &bcache.buckets[idx],b = pre->next; b; pre = b, b = b->next){
      if(b->refcnt == 0 && b->timestamp < mintimestamp){
        minb = b;
        minpre = pre;
        mintimestamp = b->timestamp;
      }
    }
    
    if(minb){
      minb->dev = dev;
      minb->blockno = blockno;
      minb->valid = 0;
      minb->refcnt = 1;

      if(idx != bucketid) {
        minpre->next = minb->next;
        release(&bcache.bucketlocks[idx]);

        acquire(&bcache.bucketlocks[bucketid]);
        minb->next = bcache.buckets[bucketid].next;
        bcache.buckets[bucketid].next = minb;
        release(&bcache.bucketlocks[bucketid]);
      } else {
        release(&bcache.bucketlocks[idx]);
      }

      release(&bcache.lock);
      acquiresleep(&minb->lock);
      return minb;
    }

    release(&bcache.bucketlocks[idx]);

    if(++idx == NBUCKET) {
      idx = 0;
    }
  }
  release(&bcache.lock);

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int bucketid = HASH(b->blockno);
  acquire(&bcache.bucketlocks[bucketid]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    b->timestamp = ticks;
  }
  
  release(&bcache.bucketlocks[bucketid]);
}

void
bpin(struct buf *b) {
  int bucketid = HASH(b->blockno);
  acquire(&bcache.bucketlocks[bucketid]);
  b->refcnt++;
  release(&bcache.bucketlocks[bucketid]);
}

void
bunpin(struct buf *b) {
  int bucketid = HASH(b->blockno);
  acquire(&bcache.bucketlocks[bucketid]);
  b->refcnt--;
  release(&bcache.bucketlocks[bucketid]);
}


