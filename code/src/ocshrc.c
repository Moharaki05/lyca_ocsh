#include "ocshrc.h"

void start_ocshrc(void){
   
   FILE* ocshrc = fopen("~/.oshrc", "w+");
   if (ocshrc == NULL) exit(EXIT_FAILURE);

   fclose(ocshrc);

}
