
#include "sighandle.h"

volatile sig_atomic_t g_child_chg = 0;
volatile sig_atomic_t g_sigint_rcvd = 0;

void siginthandle(int sig)
{
    (void)sig;
    g_sigint_rcvd = 1;
    // write(STDOUT_FILENO, "\n", 1);
}

void sigchldhandle(int sig)
{
    (void)sig;
    g_child_chg = 1;
}

void setupsigs(void)
{
    struct sigaction sa_int;
    struct sigaction sa_chld;

    sa_int.sa_handler = siginthandle;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    sa_chld.sa_handler = sigchldhandle;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa_chld, NULL);
}

