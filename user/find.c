#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int match(char *re, char *text);
void find(char *path, char *pattern, int is_name);

int main(int argc, char *argv[])
{
    int i = 2;
    if (argc < 3)
    {
        fprintf(2, "find: args error\n");
        exit(1);
    }

    int is_name = 0;
    if (strcmp(argv[2], "-name") == 0)
    {
        is_name = 1;
        i++;
        if (argc < 4)
        {
            fprintf(2, "find: args error\n");
            exit(1);
        }
    }
    for (; i < argc; i++)
    {
        find(argv[1], argv[i], is_name);
    }
    exit(0);
}

char *fmtname(char *path)
{
    char *p;
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    return p+1;
}

void find(char *path, char *pattern, int is_name)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_DEVICE:
    case T_FILE:
        if ((is_name == 1 && strcmp(pattern, fmtname(path)) == 0) || (is_name == 0 && match(pattern, fmtname(path))))
        {
            printf("%s\n", path);
        }
        break;
    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0)
            {
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            find(buf, pattern, is_name);
        }
        break;
    }
    close(fd);
}

// Regexp matcher from Kernighan & Pike,
// The Practice of Programming, Chapter 9, or
// https://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html

int matchhere(char *, char *);
int matchstar(int, char *, char *);

int match(char *re, char *text)
{
    if (re[0] == '^')
        return matchhere(re + 1, text);
    do
    { // must look at empty string
        if (matchhere(re, text))
            return 1;
    } while (*text++ != '\0');
    return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
    if (re[0] == '\0')
        return 1;
    if (re[1] == '*')
        return matchstar(re[0], re + 2, text);
    if (re[0] == '$' && re[1] == '\0')
        return *text == '\0';
    if (*text != '\0' && (re[0] == '.' || re[0] == *text))
        return matchhere(re + 1, text + 1);
    return 0;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
    do
    { // a * matches zero or more instances
        if (matchhere(re, text))
            return 1;
    } while (*text != '\0' && (*text++ == c || c == '.'));
    return 0;
}