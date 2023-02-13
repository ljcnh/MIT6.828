## 有点难了  记录一下

### Speed up system calls
(感觉就是共享内存？ )

根据提示应该是需要修改三个地方 
    proc_pagetable 
    allocproc
    freeproc

文中还提到了 `ugetpid`  
```c
int
ugetpid(void)
{
  struct usyscall *u = (struct usyscall *)USYSCALL;
  return u->pid;
}
```
USYSCALL 处存放的数据应该是一个usyscall类型的结构体 然后返回结构体中的pid

USYSCALL是什么东西？
```c
// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
//   USYSCALL (shared with kernel)
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
```
在用户空间中 依次存放 USYSCALL TRAPFRAME TRAMPOLINE

那么在 `proc_pagetable` 中
```c
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;
  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }
  return pagetable;
}
```
可以看到  依次使用mappages对  trampoline 和 p->trapframe 进行映射

照葫芦画瓢

在`proc`中添加 usyscall 成员，并在`proc_pagetable`中添加映射

```c
  if(mappages(pagetable, USYSCALL, PGSIZE,
              (uint64)(p->usyscall), PTE_R | PTE_U) < 0){
    // 中间的几个uvmunmap  都是仿照上面来的
    // 不过也合理  你都初始化失败那只能退出了  
    // 那退出了 已经进行映射的page肯定要取消啊
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmunmap(pagetable, USYSCALL, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }
```

`allocproc`函数负责 初始化在内核中运行所需的状态
```c
static struct proc*
allocproc(void)
{
  // 这一部分就是遍历当前的进程 若找到那么跳转到found位置
  // 没有找点那就只能返回了
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // 按照上面的来
  // 对usyscall做同样的事情
  if((p->usyscall = (struct usyscall *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  // 对usyscall下的pid进行初始化
  // proc p 自身有一个pid 
  // 但是ugetpid 是通过USYSCALL位置的pid获得的 进行赋值
  p->usyscall->pid = p->pid;

  // ......
}
```

再看`freeproc`
```c
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  // free usyscall  同trapframe
  p->trapframe = 0;
  if(p->usyscall)
    kfree((void*)p->usyscall);
  p->usyscall = 0;

  // 注意这里有个proc_freepagetable
  // 一开始没注意
  // proc_freepagetable：释放一个进程的页表，并释放它引用的物理内存
  // 前面使用mappages进行地址的映射 
  // kfree 只是释放 pa 指向的物理内存页面
  // 所以我们的pagetable并没有取消映射
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);

  // ......
}
```

```c
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  // 添加USYSCALL的uvmunmap就可以了
  // 在proc_pagetable中，当进程初始化失败的时候
  // 我们也进行了同样的操作
  uvmunmap(pagetable, USYSCALL, 1, 0);
  uvmfree(pagetable, sz);
}
```

### Print a page table 
这个简单 就一个递归回溯就完了

def.h 添加声明

```c
void 
backtrace(pagetable_t pagetable,int level)
{
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      uint64 child = PTE2PA(pte);
      // 一共只有三层的页表
      // 第三层在另一个分支打印 else if(pte & PTE_V){
      // 若level==1 在打印一个 " .." 就可以了
      printf(".. ");
      if( level == 1 ){
        printf("..");
      }
      printf("%d: pte %p pa %p\n", i, pte, child);
      backtrace((pagetable_t)child, level+1);
    } else if(pte & PTE_V){
      // 没有下一层page table
      uint64 pa = PTE2PA(pte);
      printf(".. .. ..%d: pte %p pa %p\n", i, pte, pa);
    }
  }
}

// pagetable 是一个三层的页表
void 
vmprint(pagetable_t pagetable)
{
  printf("page table %p\n",pagetable);
  backtrace(pagetable, 0);
}
```
### Detect which pages have been accessed 

这个有点难

31 20  19 10  9 8 7 6 5 4 3 2 1 0
PPN[1] PPN[0] RSW D A G U X W R V
 12     10     2  1 1 1 1 1 1 1 1

这本书里面的 https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMFDQC-and-Priv-v1.11/riscv-privileged-20190608.pdf
没看这本书 看得别人的博客。。。

所以 得到

```c
#define PTE_A (1L << 6)
```

首先根据提示获取三个参数
```c
  // 至于
  // 虚拟地址
  uint64 va;
  // 页数
  int pagenum;
  // 位掩码
  uint64 bits;
  argaddr(0, &va);
  argint(1, &pagenum);
  argaddr(2, &bits);
   
```

设置第i个位置的值 参考 
https://www.chens.life/posts/how-to-set-a-specified-bit-of-a-binary-value-to-a-specified-value/

```c
  // 根据提示设置一个buf
  uint64 bitsbuf = 0;
  struct proc* p = myproc();
  // 遍历从va开始的pagenum个页面
  for(int i = 0 ; i < pagenum; i++){
    // 使用walk获取对应的pte
    pte_t* pte = walk(p->pagetable, va+i*PGSIZE, 0);
    if(pte == 0){
      continue;
    }
    // 判断PTE_A是否为1
    // 若为1则该界面被访问过
    if((*pte)&PTE_A){
        bitsbuf = bitsbuf|(1L<<i);
    }
    // x = ((x&(1 << n)) ^ x) ^ (a << n)
    // 根据提示此处应将第6位设置为0（否则该界面每次查询都是访问状态）
    *pte = (((*pte)&PTE_A) ^ (*pte)) ^ 0;
  }
  // 根据提示使用copyout 将数据拷贝到user space中
  if(copyout(p->pagetable,bitsaddr,(char *)&bitsbuf,sizeof(bitsbuf))<0){
    panic("sys_pgaccess copyout faild");
  }
  return 0;
```

