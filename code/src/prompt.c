#include "prompt.h"

char* prompt(void){
	static char pmp[4096]; 
	char hostname[MAX_HOSTNAME];
	char cwd[MAX_CWD];

	const char *user = getenv("USER");
	if (user == NULL){
		struct passwd *pw = getpwuid(getuid());
		user = pw ? pw->pw_name : NULL;
	}

	if (user == NULL && 
	(gethostname(hostname, sizeof(hostname)) == 0) && 
	((getcwd(cwd, sizeof(cwd)) != NULL)) 
	&& (getlogin() != NULL)){
		snprintf(pmp, sizeof(pmp),"%s@%s:%s > ", getlogin(), hostname, cwd);
	} 
	else {
		snprintf(pmp, sizeof(pmp), "ocsh> ");
	}
	return pmp;
}


