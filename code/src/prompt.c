#include "prompt.h"

/*
 
This is a non-void function with the type (char*) with no arguments to print the correspending prompt for the user, with a significant error handling and a fallback prompt for the shell

*/

char* prompt(void){
     
     static char pmp[4096]; /* 4KB for the prompt */ 
     char hostname[MAX_HOSTNAME]; /* hostname for the machine */
     char cwd[MAX_CWD]; /* current working directory for the user */

 if ((gethostname(hostname, sizeof(hostname)) == 0) && ((getcwd(cwd, sizeof(cwd)) != NULL)) && (getlogin() != NULL)){
	 snprintf(pmp, sizeof(pmp),"\033[0;32m%s@%s\033[0m:\033[0;34m%s\033[0m > ", getlogin(), hostname, cwd);
 } else {
	 printf("\033[0;32mocsh\033[0m > "); /* a fallback to handle failure */
 }
 return pmp; 
}


