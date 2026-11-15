#include "execution.h"

//Run one full command line (expansion, pipes, redirs, builtins, programs)
void executeLine(char *line){	
	char expanded[MAX_BUFFER];
    expandVars(line, expanded, sizeof(expanded));
		
    char* segments[MAX_CMDS];
    int nseg = splitPipeline(expanded, segments);

	if (nseg == 1) {
		char* args[MAX_ARGS];
		char *inFile, *outFile;
		int n = splitArgs(segments[0], args);
		int append;
		if(parseRedirections(args, &n, &inFile, &outFile, &append) < 0) return;
		if (n == 0) return;
		
		if (getBuiltin(args[0]) != B_NONE){
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
			if(pid == 0){
				//Child process
				if(applyRedirections(inFile, outFile, append) < 0)
					exit(1);
				execvp(args[0], args);
								
				//only if execvp fails
				fprintf(stderr, "%s: Command not found.\n", args[0]); 
				exit(127); //127 is the default error code for command not found		
			}
			else if (pid > 0){
				//Parent process
				int status = 0;
				if(wait(&status) > 0)
					lastStatus = statusToCode(status);
			}
			else {
				perror("fork");
			}
	}
	else {
		runPipelines(segments, nseg);
	}        
}

