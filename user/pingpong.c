#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define READ 0
#define WRITE 1
#define MAXSIZE 64
#define PING "ping"
#define PONG "pong"

int main(int argc, char *argv[])
{
    int p_p2c[2], p_c2p[2], pid;
    char buf[MAXSIZE];

    pipe(p_p2c);
    pipe(p_c2p);

    if ((pid = fork()) < 0)
    {
        fprintf(2, "fork error...\n");
        exit(1);
    }
    else if (pid == 0)
    {
        close(p_p2c[WRITE]);
        close(p_c2p[READ]);

        read(p_p2c[READ], buf, sizeof(buf));
        close(p_p2c[READ]);
        printf("%d: received %s\n", getpid(), buf);
        write(p_c2p[WRITE], PONG, strlen(PONG));
        close(p_c2p[WRITE]);
        exit(0);
    }
    else
    {
        close(p_p2c[READ]);
        close(p_c2p[WRITE]);

        write(p_p2c[WRITE], PING, strlen(PING));
        close(p_p2c[WRITE]);

        read(p_c2p[READ], buf, sizeof(buf));
        close(p_c2p[READ]);
        printf("%d: received %s\n", getpid(), buf);
        exit(0);
    }
}
