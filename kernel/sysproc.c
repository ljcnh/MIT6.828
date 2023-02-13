#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 va;
  int pagenum;
  uint64 bitsaddr;
  argaddr(0, &va);
  argint(1, &pagenum);
  argaddr(2, &bitsaddr);
  
//   if(pagenum > 63){
//     printf("the arg num [%d] is too large: pagenum<=64 \n" , pagenum);
//     return -1;
//   }

  uint64 bitsbuf = 0;
  struct proc* p = myproc();
  for(int i = 0 ; i < pagenum; i++){
    pte_t* pte = walk(p->pagetable, va+i*PGSIZE, 0);
    if(pte == 0){
      continue;
    }
    if((*pte)&PTE_A){
        bitsbuf = bitsbuf|(1L<<i);
    }
    // x = ((x&(1 << n)) ^ x) ^ (a << n)
    *pte = (((*pte)&PTE_A) ^ (*pte)) ^ 0;
  }
  if(copyout(p->pagetable,bitsaddr,(char *)&bitsbuf,sizeof(bitsbuf))<0){
    panic("sys_pgaccess copyout faild");
  }
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
