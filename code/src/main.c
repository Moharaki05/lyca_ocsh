#include "main.h"

int main(void){
    //Command Buffer
    char cmd[MAX_BUFFER];
    //Flush after every printf
    setbuf(stdout, NULL);

	//Move cursor to top-left, then clear the screen
    fprintf(stdout,"\033[H\033[2J");

    setupsigs();

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
