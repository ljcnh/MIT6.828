有点难 对我来说有点难

参考了以下别人的思路 但是绝大部分都是21年的lab 唉...

```sh
== Test running cowtest ==
$ make qemu-gdb
(11.6s)
== Test   simple ==
  simple: OK
== Test   three ==
  three: OK
== Test   file ==
  file: OK
== Test usertests ==
$ make qemu-gdb
(52.4s)
== Test   usertests: copyin ==
  usertests: copyin: OK
== Test   usertests: copyout ==
  usertests: copyout: OK
== Test   usertests: all tests ==
  usertests: all tests: OK
== Test time ==
time: OK
Score: 110/110
```

defs.h 中添加

```c
// kalloc.c
uint8           kreferCount(void *,int);
uint8           getrefcnt(uint64);
void            acquire_reflock();
void            release_reflock();

// vm.c
int             handleCOW(pagetable_t, uint64);
int             isCOW(pagetable_t, uint64);
```
上面几个函数下面会说明



kalloc.c 文件

```c
// POS 根据物理地址计算对应的下标
#define POS(p)  (((uint64)p - KERNBASE) >> 12)  //   /PGSIZE 等价于 >> 12
// CLEN 数据的长度
#define CLEN    (PHYSTOP - KERNBASE + PGSIZE - 1)/PGSIZE

struct {
  struct spinlock lock;
  struct run *freelist;
  // reflock 用于refercount的锁
  struct spinlock reflock;
  // 所有界面的引用次数
  // refercount 用来存放对应界面的引用次数
  // 单位为uint8 范围就是 [0,255]
  // 这些都是在内核中的
  uint8 refercount[CLEN];
} kmem;

// 对新添加的变量进行初始化
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kmem.reflock, "kmem refcnt");
  // 在 freerange 函数中，会进行 free 操作
  // 引用次数应该是大于等于0的，这里将所有界面的引用次数设置为 1
  memset(&kmem.refercount, 1, sizeof(kmem.refercount));
  freerange(end, (void*)PHYSTOP);
}


void
kfree(void *pa)
{
  struct run *r;
  
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
 
  // 添加下面三行
  // COW机制 使得我们对每个界面增加一个引用次数的记录
  // 当引用次数为 0 的时候，才会释放对应界面
  // kreferCount 函数就是进行 -1 操作，然后返回对应引用次数
  if(kreferCount((void*)pa, -1) != 0){
    return;
  }

  // ...
}

void *
kalloc(void)
{
  // ...
  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    // 只添加了一行
    // kalloc 的时候 引用次数需要加 1
    // 这样就可以了
    kreferCount((void*)r, 1);
  }
  return (void*)r;
}

// 进行加一 减一操作
// 这里判断了一下数值范围
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

// 获得 pa 对应的引用次数
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

```

risv.h

PTE_PREW 如果页面之前不能被修改 那么之后也不能被修改
如果没有 `test textwrite: FAILED` 可能会报错
(
  对 是可能  因为还可以直接判断 va是否小于PGSIZE 像下面这样
```c
  // ...
  if(uncopied_cow(p->pagetable,r_stval()) > 0){
    if(r_stval() < PGSIZE)  //对0起始地址等低地址直接写，那么直接退出
      p->killed = 1;
    // ...
  }
```
) 

```c
// 设置标志位  该标志位表示当前界面是否除外COW机制中
// 就是在进行fork时，该界面是不是被映射到多个进程中
#define PTE_C (1L << 8) // cow
#define PTE_PREW (1L << 9) // 之前 PTE_W 的状态
```

vm.c

```c
// fork 中使用 uvmcopy函数
// 对 uvmcopy 进行修改就可以了
// 
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    
    // 不会直接申请新的内存 
    // 而是重新设置标志位 不可写标识  和  COW表示
    // PTE_PREW 之前 PTE_W 的状态
    // 根据 PTE_W 设置 PTE_PREW
    if((*pte) & PTE_W)
      *pte = (((*pte) & ~PTE_W) | PTE_PREW | PTE_C);
    else
      *pte = ((*pte) | PTE_C);
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    // 当然在map的时候 也是直接将新的 pagetable与之前的pa进行映射
    if(mappages(new, i, PGSIZE, pa, flags) != 0){
      goto err;
    }
    kreferCount((void*)pa, 1);
  }
  return 0;

 err:
  panic("uvmcopy error");
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// 提示中说了 copyout也要修改
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    
    // 主要添加了 下面 start-end这部分
    // 其实就是判断当前界面是不是 cow
    //   大于0 - 是  执行 handleCOW 
    //   小于0 - 报错 
    //   等于0 - 按照原有逻辑执行
    // start
    int res = isCOW(pagetable,va0);
    
    if(res > 0){
      if(handleCOW(pagetable, va0) != 0)
        goto err;
    }
    else if(res < 0){
        goto err;
    }
    // end

    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
err:
  return -1;
}

// 除了cow 缺页时候的错误
// 这个函数和原始的 uvmcopy函数 功能类似
//   就是 申请新的页 将原有页面的内容拷贝到当前页面上 
//   然后进行map即可
//   uvmcopy函数 是将所有涉及到的界面拷贝
//   这里只是拷贝 缺失的那一部分
// 在这里 还判断了一下 getrefcnt() 当前页面的引用次数
// 若 等于 1  那么直接修改 标志位 返回就行了
int
handleCOW(pagetable_t pagetable, uint64 va)
{
  pte_t *pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("handleCOW: pte should exist");
  if((*pte & PTE_V) == 0)
    panic("handleCOW: page not present");

  uint flags = PTE_FLAGS(*pte);

  if (flags & (PTE_PREW))
    flags = ((flags & (~PTE_C) & (~PTE_PREW)) | PTE_W);
  else
    flags = (flags & (~PTE_C));

  uint64 pa = PTE2PA(*pte);
  uint64 va_sta = PGROUNDDOWN(va); 

  uint8 refcnt = getrefcnt(pa);
  if (refcnt > 1){
    char *mem;
    if((mem = kalloc()) == 0)
      return -1;
    
    memmove(mem, (char*)pa, PGSIZE);
    uvmunmap(pagetable, va_sta, 1, 1);

    if(mappages(pagetable, va_sta, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      return -1;
    }
  } 
  else if(refcnt == 1) {
    if (*pte & (PTE_PREW))
        *pte = (((*pte) & (~PTE_C) & (~PTE_PREW)) | PTE_W);
    else
        *pte = ((*pte) & (~PTE_C));
  } else {
    panic("refer count error");
  }
  return 0;
}

// 判断是不是一个cow页面
int
isCOW(pagetable_t pagetable, uint64 va)
{
  if(va >= MAXVA){
    return -1;
  }
  pte_t* pte = walk(pagetable, va, 0);
  if(pte == 0)
    return -2;
  if((*pte & PTE_V) == 0)
    return -3;
  if((*pte & PTE_U) == 0)
    return -4;
  return ((*pte) & PTE_C);
}
```



trap.c

```c
void
usertrap(void)
{
  // ... 省略
  if(r_scause() == 8){
    // ... 省略
  } else if(r_scause() == 15 && isCOW(p->pagetable, r_stval()) > 0){
    // 只修改这一部分
    // 就是获取  r_scause 原因  然后判断是不是由于COW机制
    
    // 这个判断 上一个分支也写了 我就也加上了
    if(killed(p))
      exit(-1);

    if (handleCOW(p->pagetable, r_stval()) < 0){
        p->killed = 1;
    }
  } else if((which_dev = devintr()) != 0) {
    // ... 省略
  }

}
```
