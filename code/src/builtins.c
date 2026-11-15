#include "builtins.h"

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

