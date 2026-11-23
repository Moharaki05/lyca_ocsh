#include "ocshrc.h"

/* This program use 2 libc functions that invoke numerous syscalls (malloc(), fopen()) , and one syscall (creat()) with 2 parameters (path with type const char*, and modes with the type mode_t) 
 
 * we will use it to create .ocshrc (the configuration file for our shell) and this is by using 3 functions, one with the type int, and the others with type void, one for checking if the .ocshrc exists , and the other one to create it if the file pointer's value is NULL */


int resolve_ocshrc(void);

void create_ocshrc(char* filename){
   
   int fd = creat(filename, 0666); /* a syscall intended to create a file with 2 parameters, filename with the type char* and modes with the type mode_t  */

   if (fd == -1) return;

   close(fd);

}

void check_ocshrc(char* filename){
   
   FILE* ocshrc = fopen(filename, "r+"); /* a file pointer with type FILE* */
   
   if (!ocshrc) { /* alternatively we can check if it as equal as NULL */
	
	   create_ocshrc(filename);
	   ocshrc = fopen(filename, "r+");
   }

   if (!ocshrc) /* same for here, and this is a fallback for the other condition */
	   return;

   fclose(ocshrc); /* closes the file stream */

}

int resolve_ocshrc(void){
   
   char* home = getenv("HOME"); /* getting home path from $HOME */
   if (home == NULL) return -1; /* returning -1 if it NULL */

   char* filename = malloc(MAX_BUFFER * sizeof(char)); /* allocating 1KB worth of size to filename using malloc (dynamically) */
   if (filename == NULL) return -1; 
   
   snprintf(filename, MAX_BUFFER, "%s/.ocshrc", home); 

   check_ocshrc(filename);

   free(filename); /* freeing memory after usage */
   filename = NULL; /* as a safety consideration, we need to stop unattended behaviours from dangling pointers */

   return 0;

}
