#ifndef OCSH_H
#define OCSH_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <termios.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>

extern char **environ;
extern volatile sig_atomic_t g_child_chg;
extern volatile sig_atomic_t g_sigint_rcvd;

#define MAX_BUFFER 1024
#define MAX_ARGS 64
#define MAX_CMDS 16
#define MAX_HISTORY 100
#define MAX_VARS 64
#define MAX_NAME 64
#define MAX_VALUE 256
#define MAX_MATCHES 256

struct Var {
	char name[MAX_NAME];
	char value[MAX_VALUE];
};

struct Alias {
	const char *name;
	const char *realCmd;
};

enum Builtin {
	B_NONE,
	B_GO,
	B_QUIT,
	B_PATH,
	B_SAY,
	B_INFO,
	B_PAST,
	B_OSINFO,
	B_SHOWENV,
	B_ASSIGN,
	B_EXPORT
};

struct Info {
	const char *name;
	const char *text;
};

extern char *history[MAX_HISTORY];
extern int histCount;
extern struct Var vars[MAX_VARS];
extern int varCount;
extern int lastStatus;
extern pid_t lastBgPid;
extern struct Alias aliases[];
extern int aliasCount;
extern struct Info infoTable[];
extern int infoCount;

int isValidName(const char *s, size_t length);
int isAssignment(const char *word);
const char *getVar(const char *name);
int setVar(const char *name, const char *value);
int statusToCode(int status);
void resolveAlias(char *args[]);
enum Builtin getBuiltin(const char *name);

int findCompletions(const char *prefix, int isFirstWord, char *matches[], int max);
size_t commonPrefixLength(char *matches[], int n);
int readLine(const char *prompt, char *buf, size_t size);

int runBuiltins(char *args[], int n);
int splitPipeline(char *line, char *segments[]);
int splitArgs(char *segment, char *args[]);
int parseRedirections(char *args[], int *n, char **inFile, char **outFile, int *append);
int applyRedirections(const char *inFile, const char *outFile, int append);
void runPipelines(char *segments[], int nseg);

void expandVars(const char *in, char *out, size_t size);
const char *findClosingParen(const char *s);
void substitute(const char *cmdStart, size_t length, char *out, size_t *o, size_t size);
void executeLine(char *line);
int captureOutput(const char *cmd, char *out, size_t size);

void setupsigs(void);
void siginthandle(int sig);
void sigchldhandle(int sig);


#endif
