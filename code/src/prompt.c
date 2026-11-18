#include "prompt.h"

char* prompt(void){
     
     static char pmp[4096]; 
     char hostname[MAX_HOSTNAME];
     char cwd[MAX_CWD];

 if ((gethostname(hostname, sizeof(hostname)) == 0) && ((getcwd(cwd, sizeof(cwd)) != NULL)) && (getlogin() != NULL)){
	 snprintf(pmp, sizeof(pmp),"%s@%s:%s > ", getlogin(), hostname, cwd);
 } else {
	 printf("ocsh > ");
 }
 return pmp;
}


