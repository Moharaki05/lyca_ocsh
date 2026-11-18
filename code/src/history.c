#include "history.h"

void addHistory(const char *line) {
    if (histCount < MAX_HISTORY)
        history[histCount++] = strdup(line);
}

int historyCount(void) { return histCount; }

const char *historyGet(int i) {
    return (i >= 0 && i < histCount) ? history[i] : NULL;
}
