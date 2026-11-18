#include "prompt.h"

/*
 
This is a non-void function with the type (char*) with no arguments to print the correspending prompt for the user, with a significant error handling and a fallback prompt for the shell

*/

char* prompt(void){
	static char pmp[4096]; 
	char hostname[MAX_HOSTNAME];
	char cwd[MAX_CWD];

	const char *user = getenv("USER");
	if (user == NULL){
		struct passwd *pw = getpwuid(getuid());
		user = pw ? pw->pw_name : NULL;
	}

	if (user == NULL && (gethostname(hostname, sizeof(hostname)) == 0) && 
	((getcwd(cwd, sizeof(cwd)) != NULL)) && (getlogin() != NULL)) {
		if(strncmp(cwd, getenv("HOME"), strlen(getenv("HOME"))) == 0) {
			snprintf(pmp, sizeof(pmp),"\033[0;32m%s@%s\033[0m:\033[0;34m~%s\033[0m> ", getlogin(), hostname, cwd + strlen(getenv("HOME")));
		}
		else {
			snprintf(pmp, sizeof(pmp),"\033[0;32m%s@%s\033[0m:\033[0;34m%s\033[0m> ", getlogin(), hostname, cwd);
		}
	}
	else snprintf(pmp, sizeof(pmp), "\033[0;32mocsh\033[0m> ");
	return pmp;
}


