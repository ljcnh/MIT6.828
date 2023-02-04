#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int i;

    if (argc < 2)
    {
        fprintf(2, "Usage: sleep seconds...\n");
        exit(1);
    }

    for (i = 1; i < argc; i++)
    {
        int second = atoi(argv[i]);
        if (second == 0 && strcmp(argv[i], "0") != 0)
        {
            fprintf(2, "sleep: invalid time interval\n");
            exit(1);
        }
        sleep(second);
    }

    exit(0);
}
