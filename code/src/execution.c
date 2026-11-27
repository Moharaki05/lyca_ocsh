
#include "execution.h"

void executeLine(char *line)
{
    char expanded[MAX_BUFFER];
    char command[MAX_BUFFER];
    char *segments[MAX_CMDS];
    int background = 0;

    strncpy(command, line, MAX_BUFFER - 1);
    command[MAX_BUFFER - 1] = '\0';

    int len = strlen(command);

    while (len > 0 && isspace((unsigned char)command[len - 1]))
        command[--len] = '\0';

    if (len > 0 && command[len - 1] == '&')
    {
        background = 1;
        command[--len] = '\0';
    }

    expandVars(command, expanded, sizeof(expanded));

    int nseg = splitPipeline(expanded, segments);

    if (nseg <= 0)
        return;

    /* background execution would happen if its true */

    if (background)
    {
        runPipelines(segments, nseg, 1);
        return;
    }

    if (nseg == 1)
    {
        char *args[MAX_ARGS];
        char *inFile, *outFile;

        int n = splitArgs(segments[0], args);
        int append;

        if (parseRedirections(args, &n, &inFile, &outFile, &append) < 0)
            return;

        if (n == 0)
            return;

        if (getBuiltin(args[0]) != B_NONE)
        {
            int savedIn = dup(STDIN_FILENO);
            int savedOut = dup(STDOUT_FILENO);

            lastStatus = 0;

            if (applyRedirections(inFile, outFile, append) == 0)
                runBuiltins(args, n);
            else
                lastStatus = 1;

            dup2(savedIn, STDIN_FILENO);
            dup2(savedOut, STDOUT_FILENO);

            close(savedIn);
            close(savedOut);
            return;
        }

        resolveAlias(args);

        pid_t pid = fork();

        if (pid == 0)
        {
            if (applyRedirections(inFile, outFile, append) < 0)
                exit(1);

            execvp(args[0], args);

            fprintf(stderr, "\033[1;33m%s\033[0m: \033[1;31m[x] Command not found.\033[0m\n", args[0]);
            exit(127);
        }
        else if (pid > 0)
        {
            int status = 0;

            if (waitpid(pid, &status, 0) > 0)
                lastStatus = statusToCode(status);
        }
        else
        {
            perror("fork");
        }
    }
    else
    {
        runPipelines(segments, nseg, 0);
    }
}

