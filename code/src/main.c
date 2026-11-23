
#include "main.h"
#include "script.h"

// Accept command-line arguments
int main(int argc, char *argv[]) {

    // Command Buffer
    char cmd[MAX_BUFFER];

    // Flush after every printf
    setbuf(stdout, NULL);

    // Check arguments
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [script.ocsh]\n", argv[0]);
        return 1;
    }

    setupsigs();

    // Script mode
    if (argc == 2) {
        return runScript(argv[1]);
    }

    // Move cursor to top-left, then clear the screen
    fprintf(stdout, "\033[H\033[2J");

    // Main REPL
    while (true) {

        if (g_child_chg) {
            g_child_chg = 0;
            checkbgjobs();
        }

        if (!readLine(prompt(), cmd, sizeof(cmd)))
            break;

        if (cmd[0] == '\0')
            continue;

	if (resolve_ocshrc() ==  -1) return 1;

    //Main REPL
    while (true) {

	if (g_child_chg)
	{
    	  g_child_chg = 0;
          checkbgjobs();
        } 

        addHistory(cmd);
        executeLine(cmd);
    }

    return 0;
}

