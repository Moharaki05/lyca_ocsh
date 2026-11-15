#include "ocsh.h"

struct bgjob bgjobs[MAX_JOBS];
int bgjobc = 0;
static int nextJobId = 1;

void addbgjob(pid_t pid, const char *command)
{
    if (bgjobc >= MAX_JOBS)
        return;

    int i = bgjobc++;

    bgjobs[i].id = nextJobId++;
    bgjobs[i].pid = pid;

    strncpy(bgjobs[i].command, command, MAX_BUFFER - 1);
    bgjobs[i].command[MAX_BUFFER - 1] = '\0';

    printf("[%d] %d\n", bgjobs[i].id, pid);
}

void checkbgjobs(void)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        for (int i = 0; i < bgjobc; i++)
        {
            if (bgjobs[i].pid == pid)
            {
                printf("\n[%d] Done\t%s\n",
                    bgjobs[i].id,
                    bgjobs[i].command);

                for (int j = i; j < bgjobc - 1; j++)
                    bgjobs[j] = bgjobs[j + 1];

                bgjobc--;
                break;
            }
        }
    }
}
