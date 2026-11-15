/* The main source code of OCSH mini-shell */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <termios.h>
#include <dirent.h>
#include <sys/stat.h>

extern char **environ;

#define MAX_BUFFER 1024
#define MAX_ARGS 64
#define MAX_CMDS 16
#define MAX_HISTORY 100
#define MAX_VARS 64
#define MAX_NAME 64
#define MAX_VALUE 256
#define MAX_MATCHES 256

void expandVars(const char *in, char *out, size_t size);
const char *findClosingParen(const char *s);
void substitute(const char *cmdStart, size_t length, char *out, size_t *o, size_t size);

// Command history
char *history[MAX_HISTORY];
int histCount = 0;

// Shell vars ($VAR, ${VAR})
struct Var {
	char name[MAX_NAME];
	char value[MAX_VALUE];
};

struct Var vars[MAX_VARS];
int varCount = 0;

int lastStatus = 0;	// $? : exit status of last foreground command
pid_t lastBgPid = 0;	// $! : PID of the last bg job (set by the "&" code)

// Checks if the name is valid : letters, digits, '_' and not starting with a digit
int isValidName(const char *s, size_t length) {
	if (length == 0 || !(isalpha((unsigned char)s[0]) || s[0] == '_'))
		return 0;
	for (size_t i = 1; i < length; i++){
		if (!(isalnum((unsigned char)s[i]) || s[i] == '_'))
			return 0;
	}
	return 1;
}

// Checks if the word is of the form NAME=value
int isAssignment(const char *word) {
	const char *eq = strchr(word, '=');
	return eq != NULL && isValidName(word, (size_t)(eq - word));	
}

// Shell table first, then the env (HOME, PATH, USER...).
// Returns NULL if unset.
const char *getVar(const char *name){
	for (int i = 0; i < varCount; i++)
		if(strcmp(vars[i].name, name) == 0) return vars[i].value;
	return getenv(name);
}

int setVar (const char *name, const char *value){
	int index = -1;
	for (int i = 0; i < varCount; i++)
		if(strcmp(vars[i].name, name) == 0){
			index = i;
			break;
		}
	if (index < 0) {
		if (varCount >= MAX_VARS) {
			fprintf(stderr, "ocsh: too many variables\n");
			return -1;
		}
		index = varCount++;
		snprintf(vars[index].name, MAX_NAME, "%s", name);
	}
	snprintf(vars[index].value, MAX_VALUE, "%s", value);
	if(getenv(name) != NULL) //Checks if env var already got exported
		setenv(name, value, 1);
	return 0;
}

// Exit status from waitpid : normal exit code, or 128 + signal number.
int statusToCode(int status){
	// WIFEXITED checks if a child process has been terminated correctly
	// WIFSIGNALED checks if a child process has been terminated because of a signal
	// WTERMSIG returns the interrupting signal's number
	if (WIFEXITED(status)) return WEXITSTATUS(status);
	if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);

	return 1;
}

// Alias table: ocsh names -> real Linux commands
struct Alias {
	const char *name;
	const char *realCmd;
};
struct Alias aliases[] = {
	{"list", "ls"},
	{"show", "cat"},
	{"copy", "cp"},
	{"move", "mv"},
	{"del", "rm"},
	{"newdir", "mkdir"},
	{"newfile", "touch"},
};
int aliasCount = sizeof(aliases) / sizeof(aliases[0]);

//If args[0] is an alias, replace it by the real command
void resolveAlias(char *args[]){
	for(int i = 0; i < aliasCount; i++){
		if(strcmp(args[0], aliases[i].name) == 0){
			args[0] = (char *)aliases[i].realCmd;
			return;
		}
	}
}
//Built-ins
enum Builtin {
	B_NONE,
	B_GO,
	B_QUIT,
	B_PATH,
	B_SAY,
	B_INFO,
	B_PAST,
	B_OSINFO,
	B_SHOWENV,
	B_ASSIGN,
	B_EXPORT
};

enum Builtin getBuiltin(const char *name){
	if(strcmp(name, "go") == 0) return B_GO;
	if(strcmp(name, "quit") == 0) return B_QUIT;
	if(strcmp(name, "path") == 0) return B_PATH;
	if(strcmp(name, "say") == 0) return B_SAY;
	if(strcmp(name, "info") == 0) return B_INFO;
	if(strcmp(name, "past") == 0) return B_PAST;
	if(strcmp(name, "osinfo") == 0) return B_OSINFO;
	if(strcmp(name, "showenv") == 0) return B_SHOWENV;
	if(isAssignment(name)) return B_ASSIGN;
	if(strcmp(name, "export") == 0) return B_EXPORT;
	return B_NONE;
}

struct Info {
	const char *name;
	const char *text;
};
struct Info infoTable[] = {
	{"go", "go [dir]	change the current directory (HOME if no dir)"},
	{"quit", "quit	exit the shell"},
	{"path", "path	print the current directory"},
	{"say", "say [text]	print text"},
	{"info", "info [cmd]	show help about a builtin"},
	{"past", "past	show the command history"},
	{"osinfo", "osinfo	show course information"},
	{"showenv",	"showenv	show environment variables"},
	{"export",	"export NAME[=value]	make a variable visible to the programs you launch"}
};
int infoCount = sizeof(infoTable) / sizeof(infoTable[0]);

static int cmpStr(const void *a, const void *b) {
	return strcmp(*(char *const *)a, *(char *const *)b);
}

//Add a copy of s to matches (no duplicates). Returns the new count
static int addMatch(char *matches[], int count, int max, const char *s) {
	if (count >= max) return count;
	for (int i = 0; i < count; i++)
		if (strcmp(matches[i], s) == 0) return count;
	char *copy = strdup(s);
	if (copy == NULL) return count;
	matches[count] = copy;
	return count + 1;
}

//Programs found in the PATH directories whose name starts with prefix
static int addPathPrograms(const char *prefix, char *matches[], int count, int max){
	const char *path = getenv("PATH");
	if (path == NULL) return count;
	size_t plength = strlen(prefix);

	while (*path != '\0' && count < max) {
		const char *sep = strchr(path, ':');
		size_t length = sep ? (size_t)(sep - path) : strlen(path);
		char dir[MAX_BUFFER];
		if (length > 0 && length < sizeof(dir)) {
			memcpy(dir, path, length);
			dir[length] = '\0';
			DIR *d = opendir(dir);
			if (d != NULL) {
				struct dirent *e;
				while ((e = readdir(d)) != NULL && count < max) {
					if (strncmp(e -> d_name, prefix, plength) != 0) continue;
					char full[2 * MAX_BUFFER];
					snprintf(full, sizeof(full), "%s/%s", dir, e -> d_name);
					if (access(full, X_OK) == 0)
						count = addMatch(matches, count, max, e->d_name);
				}
				closedir(d);
			}
		}
		path = sep ? sep + 1 : path + length;
	}
	return count;	
}

//Files and directories matching prefix ("src/ma -> dir "src", base "ma").
//Directories get a trailing '/'
static int addFiles(const char *prefix, char *matches[], int count, int max) {
	const char *slash = strrchr(prefix, '/');
	char dir[MAX_BUFFER] = "";
	const char *base = prefix;
	if (slash != NULL) {
		size_t dlength = (size_t)(slash - prefix) + 1;
		if(dlength >= sizeof(dir)) return count;
		memcpy(dir, prefix, dlength);
		dir[dlength] = '\0';
		base = slash + 1;
	}
	size_t blength = strlen(base);

	DIR *d = opendir(dir[0] != '\0' ? dir : ".");
	if (d == NULL) return count;
	struct dirent *e;
	while ((e = readdir(d)) != NULL && count < max) {
		if (strncmp(e->d_name, base, blength) != 0) continue;
		if (e->d_name[0] == '.' && base[0] != '.') continue;
		char candidate[2 * MAX_BUFFER];
		snprintf(candidate, sizeof(candidate), "%s%s", dir, e->d_name);
		struct stat st;
		if (stat(candidate, &st) == 0 && S_ISDIR(st.st_mode))
			strncat(candidate,"/", sizeof(candidate) - strlen(candidate) - 1);
		count = addMatch(matches, count, max, candidate);
	}
	closedir(d);
	return count;
}

//Candidates for the word being typed. Every match starts with prefix.
//isFirstWord = 1: builtins, aliases and PATH programs. Otherwise : files.
//The matches are malloc'ed : the caller frees them. Returns the number of matches.
int findCompletions(const char *prefix, int isFirstWord, char *matches[], int max) {
	int count = 0;
	size_t plength = strlen(prefix);

	if(isFirstWord && strchr(prefix, '/') == NULL) {
		for (int i = 0; i < infoCount; i++)
			if (strncmp(infoTable[i].name, prefix, plength) == 0)
				count = addMatch(matches, count, max, infoTable[i].name);
		for (int i = 0; i < aliasCount; i++)
			if(strncmp(aliases[i].name, prefix, plength) == 0)
				count = addMatch(matches, count, max, aliases[i].name);
		if (plength > 0)
			count = addPathPrograms(prefix, matches, count, max);
	}
	else {
		count = addFiles(prefix, matches, count, max);
	}
	qsort(matches, (size_t)count, sizeof(char *), cmpStr);
	return count;
}

size_t commonPrefixLength(char *matches[], int n) {
	if (n == 0) return 0;
	size_t length = strlen(matches[0]);
	for (int i = 1; i < n; i++) {
		size_t k = 0;
		while (k < length && matches[i][k] == matches[0][k]) k++;
		length = k;
	}
	return length;
}

static void writeOut(const char *s, size_t n) {
	if (write(STDOUT_FILENO, s, n) < 0) { /* nothing useful to do */ }
}

static void insertText(char *buf, size_t *length, size_t size, const char *s, size_t n) {
	for (size_t k = 0; k < n && *length < size - 1; k++) {
		buf[(*length)++] = s[k];
		writeOut(&s[k], 1);
	}
	buf[*length] = '\0';
}

//Handling events when Tab is pressed
static void handleTab(const char *prompt, char *buf, size_t *length, size_t size) {
	//Start of the word being typed
	size_t start = *length;
	while (start > 0 && buf[start - 1] != ' ' && buf[start - 1] != '\t')
		start--;

	//Is it the 1st word of the command (start of line, or right after "->")?
	size_t k = start;
	while (k > 0 && (buf[k - 1] == ' ' || buf[k - 1] == '\t'))
		k--;
	int isFirstWord = (k == 0) || (k >= 2 && buf[k - 2] == '-' && buf[k - 1] == '>');
	
	char prefix[MAX_BUFFER];
	size_t plength = *length - start;
	memcpy(prefix, buf + start, plength);
	prefix[plength] = '\0';

	char *matches[MAX_MATCHES];
	int n = findCompletions(prefix, isFirstWord, matches, MAX_MATCHES);

	if (n == 0) {
		writeOut("\a", 1);
	}
	else if (n == 1) {
		size_t m = strlen(matches[0]);
		insertText(buf, length, size, matches[0] + plength, m - plength);
		if (matches[0][m - 1] != '/')
			insertText(buf, length, size, " ", 1);
	}
	else {
		size_t common = commonPrefixLength(matches, n);
		if (common > plength) {
			insertText(buf, length, size, matches[0] + plength, common - plength);
		}
		else {
			writeOut("\n", 1);
			for (int i = 0; i < n; i++){
				writeOut(matches[i], strlen(matches[i]));
				writeOut("  ", 2);
			}
			writeOut("\n", 1);
			writeOut(prompt, strlen(prompt));
			writeOut(buf, *length);
		}
	}
	for (int i = 0; i < n; i++) free(matches[i]);
}

//Read one line with editing : Tab completion, Backspace, Ctrl+C, Ctrl+D.
//Returns 0 at the end of the input (Ctrl+D on an empty line), 1 otherwise
int readLine(const char *prompt, char *buf, size_t size){
	if (!isatty(STDIN_FILENO)) {
		if (fgets(buf, (int)size, stdin) == NULL) return 0;
		buf[strcspn(buf, "\n")] = '\0';
		return 1;
	}

	//Raw mode : keys arrive one by one, no echo, Ctrl+C is an ordinary key
	struct termios saved, raw;
	tcgetattr(STDIN_FILENO, &saved);
	raw = saved;
	raw.c_lflag &= ~(ICANON | ECHO | ISIG);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);

	writeOut(prompt, strlen(prompt));
	size_t length = 0;
	buf[0] = '\0';
	int result = 1;

	while(true) {
		unsigned char c;
		ssize_t r = read(STDIN_FILENO, &c, 1);
		if (r < 0 && errno == EINTR) continue;
		//Input closed
		if (r <= 0) {
			result = 0;
			break;
		}

		if (c == '\n' || c == '\r'){
			writeOut("\n", 1);
			break;
		}
		else if (c == 4) { //Ctrl+D
			if (length == 0){
				writeOut("\n", 1);
				result = 0;
				break;
			}
		}
		else if (c == 3) { //Ctrl+C : drop the line
			writeOut("^C\n", 3);
			length = 0;
			buf[0] = '\0';
			break;
		}
		else if (c == 127 || c == 8) { //Backspace
			if(length > 0){
				//Remove a whole UTF-8 character
				while(length > 0 && ((unsigned char)buf[length - 1] & 0xC0) == 0x80)
					length--;
				if (length > 0) length--;
				buf[length] = '\0';
				writeOut("\b \b", 3);
			}
		}
		else if (c == '\t') {
			handleTab(prompt, buf, &length, size);
		}
		else if (c == 27){
			unsigned char s;
			if (read(STDIN_FILENO, &s, 1) == 1 && s == '[') {
				while (read(STDIN_FILENO, &s, 1) == 1 && !(s >= 0x40 && s <= 0x7E)) {/* skip */}
			}
			//TODO : Put History Navigation here (Up/Down keys)
		}
		else if (c >= 32) { //Printable character (or part of a UTF-8 one)
			if (length < size - 1){
				buf[length++] = (char)c;
				buf[length] = '\0';
				writeOut((const char *)&c, 1);
			}
		}
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &saved); //restore the terminal BEFORE running commands
	return result;
}

int runBuiltins(char *args[], int n){
	switch(getBuiltin(args[0])){
		case B_GO: {
			const char *dir = (n > 1) ? args[1] : getenv("HOME"); //goes to the wanted directory if given, goes to home otherwise
			if (dir == NULL || chdir(dir) != 0){
				perror("go");
				lastStatus = 1;
			}
			return 1;
		}
		case B_QUIT: 
			exit(0);
		
		case B_PATH: {
			char buf[1024];
			if(getcwd(buf, sizeof(buf)) != NULL) //check if the current working dir exists
				fprintf(stdout, "%s\n", buf);
			else{
				perror("path");
				lastStatus = 1;
			}
			return 1;
		}
		case B_SAY: 
			for(int i = 1; i < n; i++)
				fprintf(stdout, "%s%s", args[i], (i<n-1) ? " " : "");
			fprintf(stdout, "\n");
			return 1;
		
		case B_INFO:
			if(n == 1) {
				//No args : list every builtin
				for (int i = 0; i < infoCount; i++){
					fprintf(stdout,"%s\n", infoTable[i].text);
				}
			}
			else {
				//Argument : help about one builtin
				for (int i = 0; i < infoCount; i++){
					if(strcmp(args[1], infoTable[i].name) == 0){
						fprintf(stdout, "%s\n", infoTable[i].text);
						return 1;
					}
				}
				fprintf(stderr, "info: no help for '%s'\n", args[1]);
				lastStatus = 1;
			}
			return 1;
		
		case B_PAST:
			for (int i = 0; i < histCount; i++)
				fprintf(stdout, "%4d	%s\n", i + 1, history[i]);
			return 1;
			
		case B_OSINFO:
			fprintf(stdout,"Systèmes d'Exploitation -- UIR/ESIN\n");
			return 1;

		case B_SHOWENV: {
			fprintf(stdout, "* ENV:\n");
			for(char **e = environ; *e != NULL; e++){
				fprintf(stdout, "%s\n", *e);
			}
			return 1;
		}

		case B_ASSIGN: {
			char *eq = strchr(args[0], '=');
			*eq = '\0'; // split "NAME=value" into "NAME" and "value"
			if (setVar(args[0], eq + 1) < 0) lastStatus = 1;
			return 1;
		}

		case B_EXPORT: {
			if (n == 1){
				for (int i = 0; i < varCount; i++)
					fprintf(stdout, "%s=%s\n", vars[i].name, vars[i].value);
				return 1;
			}
			for (int i = 1; i < n; i++){
				char *eq = strchr(args[i], '=');
				size_t length = eq ? (size_t)(eq - args[i]) : strlen(args[i]);
				if (!isValidName(args[i], length)){
					fprintf(stderr, "export: '%s': not a valid identifier\n", args[i]);
					lastStatus = 1;
					continue;
				}
				if (eq) {
					*eq = '\0';
					setVar(args[i], eq + 1);
				}
				const char *val = getVar(args[i]);
				setenv(args[i], val ? val : "", 1);
			}
			return 1;
		}
			
		default:
			return 0; //not a builtin
	}
}

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

void expandVars(const char *in, char *out, size_t size){
	size_t o = 0, i = 0;
	while (in[i] != '\0' && o < size - 1){
		if (in[i] == '`') {
			const char *end = strchr(in + i + 1, '`');
			if (end != NULL) {
				substitute(in + i + 1, (size_t)(end - (in + i + 1)), out, &o, size);
				i = (size_t)(end - in) + 1;
				continue;
			}
		}
		
		if (in[i] != '$'){
			out[o++] = in[i++];
			continue;
		}
		i++;	// skip the '$'
		char name[MAX_NAME];
		char num[32];
		const char *val = NULL;

		if (in[i] == '?'){
			snprintf(num, sizeof(num), "%d", lastStatus);
			val = num;
			i++;
		}
		else if (in[i] == '!'){
			if (lastBgPid > 0)
				snprintf(num, sizeof(num), "%d", (int)lastBgPid);
			else num[0] = '\0'; // no BG job yet : empty
			val = num;
			i++;
		}
		else if (in[i] == '('){
			const char *end = findClosingParen(in + i + 1);
			if (end == NULL){
				out[o++] = '$';
				continue;
			}
			substitute(in + i + 1, (size_t)(end - (in + i + 1)), out , &o, size);
			i = (size_t)(end - in) + 1;
		}
		else if (in[i] == '{'){
			const char *end = strchr(in + i, '}');
			size_t length = end ? (size_t)(end - (in + i + 1)) : 0;
			if (end == NULL || length >= MAX_NAME || !isValidName(in + i + 1, length)) {
				out[o++] = '$'; // not a valid ${NAME}: keep it as a text
				continue;
			}
			//memcpy copies memory blocks from a mem address to a specified one.
			memcpy(name, in + i + 1, length);
			name[length] = '\0';
			val = getVar(name);
			i = (size_t)(end - in) + 1;
		}
		else if (isalpha((unsigned char)in[i]) || in[i] == '_') {
			size_t length = 0;
			while ((isalnum((unsigned char)in[i]) || in[i] == '_') && length < MAX_NAME - 1)
				name[length++] = in[i++];
			name[length] = '\0';
			val = getVar(name);
		}		
		else {
			out[o++] = '$'; // lone '$': keep it as text
			continue;
		}

		for (size_t k = 0; val && val[k] != '\0' && o < size - 1; k++)
			out[o++] = val[k];
	}
	out[o] = '\0';
}

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

int main(void){
    //Command Buffer
    char cmd[MAX_BUFFER];
    //Flush after every printf
    setbuf(stdout, NULL);

	//Move cursor to top-left, then clear the screen
    fprintf(stdout,"\033[H\033[2J");

    //Main REPL
    while (true) {
        if(!readLine("ocsh> ", cmd, sizeof(cmd))) break;
        if(cmd[0] == '\0') continue;

        if(histCount < MAX_HISTORY){
        	history[histCount++] = strdup(cmd);
		}
		executeLine(cmd);
  	}
}
