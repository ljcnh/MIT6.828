### Backtrace
[lecture notes](https://pdos.csail.mit.edu/6.1810/2022/lec/l-riscv.txt)
```
    Stack
                    
        +->          
        |   +-----------------+   |
        |   | return address  |   |
        |   |   previous fp ------+
        |   | saved registers |
        |   | local variables |
        |   |       ...       | <-+
        |   +-----------------+   |
        |   | return address  |   |
        +------ previous fp   |   |
            | saved registers |   |
            | local variables |   |
        +-> |       ...       |   |
        |   +-----------------+   |
        |   | return address  |   |
        |   |   previous fp ------+
        |   | saved registers |
        |   | local variables |
        |   |       ...       | <-+
        |   +-----------------+   |
        |   | return address  |   |
        +------ previous fp   |   |
            | saved registers |   |
            | local variables |   |
    $fp --> |       ...       |   |
            +-----------------+   |
            | return address  |   |
            |   previous fp ------+
            | saved registers |
    $sp --> | local variables |
            +-----------------+
```

Stack来自上面链接

Backtrace不算难，但是感觉没有完全理解，太赶时间了...

1. 根据提示在defs.h中定义
    ```c
    void            backtrace(void);
    ```
2. 根据提示在riscv.h中复制
    ```c
    static inline uint64
    r_fp()
    {
        uint64 x;
        asm volatile("mv %0, s0" : "=r" (x) );
        return x;
    }
    ```
    按照描述，这个函数是用来读取当前帧指针的  
    需要在backtrace函数使用

3. 根据提示在sys_sleep()函数中添加backtrack()的调用
   
4. 实现 backtrack() 函数
   
    提示3  `Note that the return address lives at a fixed offset (-8) from the frame pointer of a stackframe, and that the saved frame pointer lives at fixed offset (-16) from the frame pointer.`

    结合上面的那个stack的图
    还要明确
    ```c
    SP：stack pointer 堆栈指针，总是指向栈顶

    FP：frame pointer 栈帧指针，每个进程的栈空间为一帧，FP指向 当前进程栈空间的 栈底。
    ```

    r_fp() 返回当前的 `stackframe` 指针
    那么返回的就是图中的 `$fp`
    也就是说
    
        return address 的地址就是 fp-8
        previous fp 的地址就是 fp-16
    
    注意 这里存放的是 **地址**  (我开始就搞糊涂了)，上面的图也比较明显了

    如果我想要获得上一个fp的地址就是
    ```c
        // 把括号全标上了 好辨认
        pre_fp = *((*uint64) (fp - 16));

        // 对于输出 则是 address 存放的内容
        printf("%p\n", *((uint64*)(fp - 8)));
    ```

    需要 判断循环结束的条件，根据提示4 
    `A useful fact is that the memory allocated for each kernel stack consists of a single page-aligned page, so that all the stack frames for a given stack are on the same page. You can use PGROUNDDOWN(fp) (see kernel/riscv.h) to identify the page that a frame pointer refers to.`
    写出循环条件

    ```c
    // 最终 得到
    // 和别人写的不一样  但能过test...
    void
    backtrace(void)
    {
        uint64 fp = r_fp();
        uint64 bottom = PGROUNDDOWN(fp);
        while(PGROUNDDOWN(fp) == bottom){
            printf("%p\n", *((uint64*)(fp - 8)));
            fp = *((uint64*)(fp - 16));
        }
    }
    ```

### Alarm 

Alarm 分为两部分

1. test0: invoke handler

    这个按照提示做可以了
    
    在Makefile添加
    
    ```
    UPROGS=\
	$U/_alarmtest\
    ```

    proc.h文件 allocproc()函数 添加
    ```c
    p->alarm_interval = 0;
    p->handler_va = 0;
    p->passed_ticks = 0;
    ```

    proc.h  文件
    ```c
    struct proc {
        // ...
        int alarm_interval;
        uint64 handler_va;
        int passed_ticks;
    }
    ```
    
    syscall.c
    ```c
    extern uint64 sys_sigalarm(void);
    extern uint64 sys_sigreturn(void);

    static uint64 (*syscalls[])(void) = {
        // 添加
        [SYS_sigalarm]   sys_sigalarm,
        [SYS_sigreturn]   sys_sigreturn,
    }
    ```

    syscall.h
    ```c
    #define SYS_sigalarm  22
    #define SYS_sigreturn  23
    ```

    sysproc.c
    ```c
    uint64 sys_sigreturn(void) {
        return 0;
    }
    ```

    trap.c
    ```c
    void usertrap(void) {
        ...

        // give up the CPU if this is a timer interrupt.
        if(which_dev == 2) {
            p->passed_ticks ++;
            if(p->alarm_interval <= p->passed_ticks){
                p->trapframe->epc = p->handler_va;
            }

            yield();
        }
    }
    ```

    user.h
    ```c
    int sigalarm(int ticks, void (*handler)());
    int sigreturn(void);
    ```

    user.pl

    ```c
    entry("sigreturn");
    entry("sigalarm");
    ```

    test0 可以通过了

    
    
2. test1/test2()/test3(): resume interrupted code
   
   这部分一开始做的时候有点懵 参考了[这里](https://www.chens.life/posts/mit-xv6-lab4/#alarm)和[这里](https://juejin.cn/post/7010653349024366606#heading-3)

        首先需要明确的是 我们需要保证对应的 trapframe（为了使得 sigreturn 能够回到原有的状态）

        提示中还提高不能重复调用，需要等到结束之后才能再次调用
   
        所以 添加
    ```c
        struct proc {
            int returned;
            struct trapframe *saved_trapframe;
        }
    ```

    proc.c
    ```c
    static struct proc* allocproc(void){
        // ...
        // 和之前lab一样 进行初始化操作
        p->alarm_interval = 0;
        p->handler_va = 0;
        p->passed_ticks = 0;
        p->returned = 1;
        // Allocate a trapframe page.
        if((p->saved_trapframe = (struct trapframe *)kalloc()) == 0){
            freeproc(p);
            release(&p->lock);
            return 0;
        }

    }

    static struct proc* freeproc(void){
        // ...
        // 和之前lab一样 进行free操作
        p->alarm_interval = 0;
        p->handler_va = 0;
        p->passed_ticks = 0;
        if(p->saved_trapframe)
            kfree((void*)p->saved_trapframe);
        p->saved_trapframe = 0;
    }
    ```

    实现 `sigalarm` 和 `sigreturn` 函数

    ```c
    uint64 sys_sigalarm(void) {
        // 获取参数 
        int ticks;
        uint64 handler_va;
        argint(0, &ticks);
        argaddr(1, &handler_va);
        
        // 获取当前的prco 进行赋值   （就这些 没了
        struct proc* proc = myproc();
        proc->alarm_interval = ticks;
        proc->handler_va = handler_va;
        
        return 0;
    }

    uint64 sys_sigreturn(void) {
        struct proc* proc = myproc();
        // 覆盖trapframe 回到之前的状态
        *(proc->trapframe) = *(proc->saved_trapframe);
        // sigalarm结束 可以重新调用
        proc->returned = 1;
        // 这个地方我之前之间return 0  test3报错了
        // 然后看了看其他人的
        // a0 默认保存当前函数的返回值 也就是说 
        // 如果在该函数 return 0 那么a0 = 0
        // 为了保证回到 初始状态  
        // 直接 return trapframe 之中的 a0
        return proc->trapframe->a0;
    }
    ```
   
    
    trap.c 
    ```c
    void usertrap(void){
        // ...

        // 修改
        // give up the CPU if this is a timer interrupt.
        if(which_dev == 2) {
            if(p->alarm_interval !=0 && p->returned){
                p->passed_ticks ++;
                if (p->passed_ticks == p->alarm_interval) {
                    // 拷贝 保存当前的栈
                    *(p->saved_trapframe) = *p->trapframe;
                    p->trapframe->epc = p->handler_va;
                    p->passed_ticks = 0;
                    // 此时可以再次调用
                    p->returned = 0;
                }
            }
            yield();
        }
    }
    ```



