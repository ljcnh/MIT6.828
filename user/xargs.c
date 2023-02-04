#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAX_ARG_LEN 1024

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "Usage: xargs command [args]\n");
        exit(1);
    }

    int idx = 0, pid;
    char tmp, pre_arg[MAX_ARG_LEN], *exec_args[MAXARG];

    for (int i = 1; i < argc; i++)
    {
        exec_args[i - 1] = argv[i];
    }

    while (read(0, &tmp, 1) > 0)
    {
        if (tmp == '\n')
        {
            // 末尾结束为0 设置结束位置
            pre_arg[idx] = 0;
            if ((pid = fork()) < 0)
            {
                fprintf(2, "fork error...\n");
                exit(1);
            }
            else if (pid == 0)
            {
                exec_args[argc - 1] = pre_arg;
                exec_args[argc] = 0;
                exec(exec_args[0], exec_args);
            }
            else
            {
                wait(0);
                // 可能有多个输入 重置下标
                idx = 0;
            }
        }
        else
        {
            pre_arg[idx++] = tmp;
        }
    }
    exit(0);
}

// 一个例子理解上述两个注解：(echo 111;echo 2) | xargs echo