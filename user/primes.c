#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void primes(int *fd)
{
    close(fd[1]);
    int fdr[2], pid, previous, next;
    if (read(fd[0], &previous, sizeof(int)) == 0)
    {
        close(fd[0]);
        exit(0);
    }

    fprintf(1, "prime %d\n", previous);

    pipe(fdr);
    if ((pid = fork()) < 0)
    {
        fprintf(2, "fork error...\n");
        close(fd[0]);
        close(fdr[0]);
        close(fdr[1]);
        exit(1);
    }
    else if (pid > 0)
    {
        close(fdr[0]);
        while (read(fd[0], &next, sizeof(int)))
        {
            if (next % previous != 0)
            {
                write(fdr[1], &next, sizeof(int));
            }
        }
        close(fd[0]);
        close(fdr[1]);
        wait(0);
        exit(0);
    }
    else
    {
        primes(fdr);
        exit(0);
    }
}

int main(int argc, char **argv)
{
    int maxnum = 35;
    if (argc > 1)
    {
        maxnum = atoi(argv[1]);
    }
    int fd[2], pid;
    pipe(fd);
    if ((pid = fork()) < 0)
    {
        fprintf(2, "fork error...\n");
        exit(1);
    }
    else if (pid > 0)
    {
        close(fd[0]);
        for (int i = 2; i <= maxnum; i++)
        {
            write(fd[1], &i, sizeof(int));
        }
        close(fd[1]);
        wait(0);
        exit(0);
    }
    else
    {
        primes(fd);
        exit(0);
    }
}
