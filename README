1. Memory allocator 
   
   比较简单 实验中已经说明了基本思路：

    ``` 
    The basic idea is to maintain a free list per CPU, each list 
    with its own lock. Allocations and frees on different CPUs can 
    run in parallel, because each CPU will operate on a different 
    list. The main challenge will be to deal with the case in 
    which one CPU's free list is empty, but another CPU's list has
     free memory; in that case, the one CPU must "steal" part of 
    the other CPU's free list. Stealing may introduce lock 
    contention, but that will hopefully be infrequent. 
    ```


    提示中也说了 会用到NCPU(cpu的个数)

```c
// 首先修改 kmem 定义
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

// 当然对应的初始化也要修改
void
kinit()
{
  char name[7];
  
  for (int i = 0; i < NCPU; i++) {
    // snprintf 提示中说了 不过name一样也没啥事
    snprintf(name, sizeof(name), "kmem_%d", i);
    initlock(&kmem[i].lock, name);
  }
  // Let freerange give all free memory to the CPU running freerange.
  // 意思就是 不管它
  freerange(end, (void*)PHYSTOP);
}


void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  
  // 获取cpuid 提示中说到要关闭中断才可
  push_off();
  int cid = cpuid();
  pop_off();

  // kmem 全都改为  kmem[cid]  就可以了
  acquire(&kmem[cid].lock);
  r->next = kmem[cid].freelist;
  kmem[cid].freelist = r;
  release(&kmem[cid].lock);
}

void *
kalloc(void)
{
  struct run *r;

  // 获取cpu id
  push_off();
  int cid = cpuid();
  pop_off();

  acquire(&kmem[cid].lock);
  r = kmem[cid].freelist;
  if(r) {
    kmem[cid].freelist = r->next;
  }
  release(&kmem[cid].lock);
  // 以上和之前差不多
  
  // 需要在其他cpu的freelist中申请 直接遍历
  if (!r) {
    for(int id = 0; id < NCPU && !r; id++) {
      if (cid == id) {
        continue;
      }
      acquire(&kmem[id].lock);
      r = kmem[id].freelist;
      if(r) {
        kmem[id].freelist = r->next;
      }
      release(&kmem[id].lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```
2. Buffer cache  

这个有点难......  参考了网上的思路

主要思路本上一个差不多， 

```c
struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  // prev 删掉就行了
  // 改成map了 ，使用链表法解决冲突  prev也没啥必要了
  // struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
  // 时间戳
  uint timestamp;
};
```

```c
#define NBUCKET 13
#define HASH(blockno) (blockno % NBUCKET)
// trap.c 
extern uint ticks;

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  // ...
  // 未被使用的buf的下标
  uint unused;
  struct buf buckets[NBUCKET];
  struct spinlock bucketlocks[NBUCKET];
} bcache;

// 初始化函数也要修改
// 在原初始化函数  (Create linked list of buffers)  的目的是初始化buf
// 保证在 bget 中能够使用已存在的buf
// 现在我们将使用 buckets 来维护buf的使用
// 但是 在一开始 buckets 没有任何的buf
// 有两个思路：
//   1. 将所有的buf保存在 buckets[0] 中
//   2. 在 bget 中分配 （我用的这种  所以需要一个未使用的 buf的下标）
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


void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int bucketid = HASH(b->blockno);
  // 申请对应的 buckets 的锁
  acquire(&bcache.bucketlocks[bucketid]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // 更新对应的时间戳
    b->timestamp = ticks;
  }
  release(&bcache.bucketlocks[bucketid]);
}

// bpin 和 bunpin 修改一下下标就行
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


// 这个很麻烦
// 思路：
// 1. 开始于之前相同 申请对应bucketlocks的锁
//    遍历 buckets 查看是不是已经有对应的buf了 
//    有则直接返回  没有就进入 2
// 2. 获取全局锁 bcache.lock
//    因为在获取 bcache.lock 之前有释放锁的操作 并在这段期间内不持有其他锁
//    所以有可能被其他的线程修改了 需要重新检测对应的 buckets
// 3. 若还是没有 那就需要检测是不是所有的buf都已经分配出去了
//    没有都分配出去那就分配
// 4. 这个时候就需要遍历其他的 buckets 看看有没有 cnt=0 的
//    有的话就将这个作为结果返回 当还要判断是不是同一 buckets，不是的话还需要移动
// 一定要注意锁的申请和释放！！！
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

  for(int i = 0; i < NBUCKET; i++) {
    mintimestamp = -1;
    acquire(&bcache.bucketlocks[i]);
    for(pre = &bcache.buckets[i],b = pre->next; b; pre = b, b = b->next){
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

      release(&bcache.bucketlocks[idx]);
      release(&bcache.lock);
      acquiresleep(&minb->lock);
      return minb;
    }

    release(&bcache.bucketlocks[i]);
  }
  release(&bcache.lock);

  panic("bget: no buffers");
}
```


就这样吧  懒得优化了 