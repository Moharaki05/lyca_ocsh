
#include "pipeline.h"

void runPipelines(char *segments[], int nseg, int background)
{
    char *argv[MAX_CMDS][MAX_ARGS];
    int argc_[MAX_CMDS];
    char *inFile[MAX_CMDS], *outFile[MAX_CMDS];
    int append[MAX_CMDS];

    // Background support for simple commands only
    if (background && nseg != 1)
    {
        fprintf(stderr, "ocsh: background pipelines are not supported\n");
        return;
    }

    // Parse every segment first
    for (int i = 0; i < nseg; i++)
    {
        argc_[i] = splitArgs(segments[i], argv[i]);

        if (parseRedirections(argv[i], &argc_[i],
                &inFile[i], &outFile[i], &append[i]) < 0)
            return;

        if (argc_[i] == 0)
        {
            fprintf(stderr, "ocsh: Syntax error: empty command\n");
            return;
        }
    }

    // Create pipes and children
    pid_t pids[MAX_CMDS];
    int started = 0;
    int prevRead = -1;

    for (int i = 0; i < nseg; i++)
    {
        int fd[2] = {-1, -1};

        if (i < nseg - 1 && pipe(fd) < 0)
        {
            perror("pipe");
            break;
        }

        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
            if (fd[0] != -1)
            {
                close(fd[0]);
                close(fd[1]);
            }
            break;
        }

        if (pid == 0)
        {
            // Restore default signals in child
            signal(SIGINT, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            if (prevRead != -1)
            {
                dup2(prevRead, STDIN_FILENO);
                close(prevRead);
            }

            if (fd[1] != -1)
            {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }

            if (applyRedirections(inFile[i], outFile[i], append[i]) < 0)
                exit(EXIT_FAILURE);

            lastStatus = 0;

            if (runBuiltins(argv[i], argc_[i]))
                exit(lastStatus);

            resolveAlias(argv[i]);
            execvp(argv[i][0], argv[i]);

            fprintf(stderr, "%s: Command not found.\n", argv[i][0]);
            exit(127);
        }

        pids[started++] = pid;

        if (prevRead != -1)
            close(prevRead);

        if (fd[1] != -1)
            close(fd[1]);

        prevRead = fd[0];
    }

    if (prevRead != -1)
        close(prevRead);

    // Background execution
    if (background && started == 1)
    {
        addbgjob(pids[0], segments[0]);
        lastStatus = 0;
        return;
    }

    // Foreground execution: wait for all children
    for (int k = 0; k < started; k++)
    {
        int status = 0;

        if (waitpid(pids[k], &status, 0) > 0)
        {
            if (k == started - 1)
                lastStatus = statusToCode(status);
        }
    }

    if (started < nseg)
        lastStatus = 1;
}

