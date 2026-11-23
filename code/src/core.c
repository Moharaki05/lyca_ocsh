#include "core.h"

// Command history
char *history[MAX_HISTORY];
int histCount = 0;


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

struct Info infoTable[] = {
	{"go", "go [dir]  change the current directory (HOME if no dir)"},
	{"quit", "quit	exit the shell"},
	{"path", "path	print the current directory"},
	{"say", "say [text]  print text"},
	{"info", "info [cmd] show help about a builtin"},
	{"past", "past	show the command history"},
	{"osinfo", "osinfo   show course information"},
	{"showenv", "showenv	show environment variables"},
	{"export", "export NAME[=value]	 make a variable visible to the programs you launch"}
};
int infoCount = sizeof(infoTable) / sizeof(infoTable[0]);

