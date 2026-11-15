#include "pipeline.h"

void runPipelines(char *segments[], int nseg){
	char *argv[MAX_CMDS][MAX_ARGS];
	int argc_[MAX_CMDS];
	char *inFile[MAX_CMDS], *outFile[MAX_CMDS];
	int append[MAX_CMDS];

	//Parses every segment first; abort the whole pipeline on error
	for (int i = 0; i < nseg; i++) {
		argc_[i] = splitArgs(segments[i], argv[i]);
		if (parseRedirections(argv[i], &argc_[i], &inFile[i], &outFile[i], &append[i]) < 0) return;
		if (argc_[i] == 0){
			fprintf(stderr, "osch : Syntax error: empty command near '->'\n");
			return;
		}
	}

	//Creates the pipes and one child per command
	pid_t pids[MAX_CMDS];
	int started = 0;
	int prevRead = -1; //red end of previous pipe (-1 for the first command)

	for (int i = 0; i < nseg; i++) {
		int fd[2] = {-1, -1}; // stays -1 for the last command (no pipe after it)
		if (i < nseg - 1 && pipe(fd) < 0) {
			perror("pipe");
			break;
		}

		pid_t pid = fork();
		if (pid < 0){
			if (fd[0] != -1) {
				close(fd[0]);
				close(fd[1]);
			}
			break;
		}

		if (pid == 0) {
			//Child process : plug stdin on previous pipe, stdout on the next one.
			if (prevRead != -1){
				dup2(prevRead, STDIN_FILENO);
				close(prevRead);
			}
			if (fd[1] != -1){
				dup2(fd[1], STDOUT_FILENO);
				close(fd[0]);
				close(fd[1]);
			}

			//Explicit redirections override the pipe
			if (applyRedirections(inFile[i], outFile[i], append[i]) < 0)
				exit(EXIT_FAILURE);

			lastStatus = 0;
			
			//A builtin inside a pipe runs in the child
			if (runBuiltins(argv[i], argc_[i]))
				exit(lastStatus);

			resolveAlias(argv[i]);
			execvp(argv[i][0], argv[i]);
			fprintf(stderr, "%s: Command not found.\n", argv[i][0]);
			exit(127);
		}

		//Parent process : remember the child, close the ends it doesn't need anymore
		pids[started++] = pid;
		if (prevRead != -1) close(prevRead);
		if (fd[1] != -1) close(fd[1]);
		prevRead = fd[0]; //becomes the stdin source of the next child
	}
	
	if (prevRead != -1) close(prevRead); // only reached after a break;

	//Wait for all children, only after all of them are started
	for(int k = 0; k < started; k++){
		int status = 0;
		waitpid(pids[k], &status, 0);
		if (k == nseg - 1) lastStatus = statusToCode(status);
	}
	if (started < nseg) lastStatus = 1;
}

