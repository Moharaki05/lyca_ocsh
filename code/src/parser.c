#include "parser.h"

int splitPipeline(char *line, char *segments[]){
 	int count = 0;
   	char *start = line;
   	char *pos;

   	//strstr returns a pointer to the next "->", or NULL if none is left in the command
   	while(count < MAX_CMDS - 1 && (pos = strstr(start, "->")) != NULL){
   		*pos = '\0'; //end the current segment here
   		segments[count++] = start;
   		start = pos + 2; //skips the two characters of "->"
   	}
   	segments[count++] = start; //last segment
   	return count;
}

int splitArgs(char *segment, char *args[]){
   	int n = 0;
   	//strtok splits command into words, using spaces and tabs as separators
   	//The first call (strtok(cmd,...)) returns the first word
   	//Next calls return the next words, then NULL when none are left.
   	for (char *tok = strtok(segment, " \t"); tok && n < MAX_ARGS - 1; tok = strtok(NULL, " \t"))
   		args[n++] = tok;
   	args[n] = NULL;
   	return n;
}

//Remove "<", ">" and ">>" (and the file name after each) from args.
//Fills inFile/outFile and sets append to 1 for ">>".
//Returns -1 on syntax error
int parseRedirections(char *args[], int *n, char **inFile, char **outFile, int *append){
	int j = 0; //writing index
	*inFile = NULL;
	*outFile = NULL;
	*append = 0;

	for(int i = 0; i < *n; i++){
		if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0){
			if (i + 1 >= *n){ //checks if the word at i+1 is the last word
				fprintf(stderr, "ocsh: Syntax error: Missing file name after '%s'\n", args[i]);
				return -1;
			}
			if (args[i][0] == '<'){ //if the second "word" is "<", we keep args[i+1] in inFile
				*inFile = args[i+1];
			}
			else { //else, we keep it in outFile
				*outFile = args[i+1];
				*append = (args[i][1] == '>'); //true : ">>" -> append, false : ">"
			}
			i++;
		}
		else {
			args[j++] = args[i]; //copy the arg at index j then increment j
		}
	}
	args[j] = NULL;
	*n = j;
	return 0;
}

//Redirect stdin/stdout to the given files. Returns -1 if a file can't be opened
int applyRedirections(const char *inFile, const char *outFile, int append){
	if(inFile){
		int fd = open(inFile, O_RDONLY);
		if (fd < 0) {
			perror(inFile);
			return -1;
		}
		dup2(fd, STDIN_FILENO); //fd 0 now points to the file
		close(fd); //the extra descriptor is no longer needed
	}
	if(outFile){
		int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
		int fd = open(outFile, flags, 0644);
		if (fd < 0) {
			perror(outFile);
			return -1;
		}
		dup2(fd, STDOUT_FILENO); //fd 1 now points to the file
		close(fd);
	}
	return 0;
}

