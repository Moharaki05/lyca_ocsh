#include "substitution.h"

//Run a command line in a child process and capture what it prints.
//Trailing newlines are removed, inner ones become spaces.
int captureOutput(const char *cmd, char *out, size_t size){
	int fd[2];
	if (pipe(fd) < 0){
		perror("pipe");
		return -1;
	}

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		close(fd[0]);
		close(fd[1]);
		return -1;
	}

	if(pid == 0){
		//Child process: its stdout goes into pipe
		close(fd[0]);
		dup2(fd[1], STDOUT_FILENO);
		close(fd[1]);

		char line[MAX_BUFFER];
		snprintf(line, sizeof(line), "%s", cmd);
		executeLine(line);
		exit(lastStatus);
	}

	//Parent process: read until EOF (every write end is closed)
	close(fd[1]);
	size_t length = 0;
	char tmp[256];
	while(1){
		ssize_t r = read(fd[0], tmp, sizeof(tmp));
		if (r < 0 && errno == EINTR) continue; //interrupted by signal : retry
		if (r <= 0) break;
		//Keep reading even when "out" is full, otherwise the child blocks on write
		size_t room = size - 1 - length;
		size_t take = ((size_t)r < room) ? (size_t)r : room;
		memcpy(out + length, tmp, take);
		length += take;
	}
	out[length] = '\0';
	close(fd[0]);

	int status = 0;
	waitpid(pid, &status, 0);
	lastStatus = statusToCode(status);

	while (length > 0 && out[length - 1] == '\n')	// strip trailing newlines
		out[--length] = '\0';
	for (size_t k = 0; k < length; k++)	// inner newlines -> spaces
		if (out[k] == '\n') out[k] = ' ';
	return 0;
}

//Find the ')' matching an already-opened '(' (s points just after it).
//Count nested parentheses, so $(say $(path)) works.
const char *findClosingParen(const char *s){
	int depth = 1;
	for (; *s != '\0'; s++){
		if (*s == '(') depth++;
		else if (*s == ')' && --depth == 0) return s;
	}
	return NULL;
}

//Run the command (length chars starting at cmdStart) and append its output to out
void substitute(const char *cmdStart, size_t length, char *out, size_t *o, size_t size){
	char cmd[MAX_BUFFER];
	char result[MAX_BUFFER];
	if (length >= sizeof(cmd)) length = sizeof(cmd) - 1;
	memcpy(cmd, cmdStart, length);
	cmd[length] = '\0';

	if (captureOutput(cmd, result, sizeof(result)) < 0) return;
	for (size_t k = 0; result[k] != '\0' && *o < size - 1; k++){
		out[(*o)++] = result[k];
	}
}

