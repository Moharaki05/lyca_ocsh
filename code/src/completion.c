#include "completion.h"

static int cmpStr(const void *a, const void *b) {
	return strcmp(*(char *const *)a, *(char *const *)b);
}

//Add a copy of s to matches (no duplicates). Returns the new count
static int addMatch(char *matches[], int count, int max, const char *s) {
	if (count >= max) return count;
	for (int i = 0; i < count; i++)
		if (strcmp(matches[i], s) == 0) return count;
	char *copy = strdup(s);
	if (copy == NULL) return count;
	matches[count] = copy;
	return count + 1;
}

//Programs found in the PATH directories whose name starts with prefix
static int addPathPrograms(const char *prefix, char *matches[], int count, int max){
	const char *path = getenv("PATH");
	if (path == NULL) return count;
	size_t plength = strlen(prefix);

	while (*path != '\0' && count < max) {
		const char *sep = strchr(path, ':');
		size_t length = sep ? (size_t)(sep - path) : strlen(path);
		char dir[MAX_BUFFER];
		if (length > 0 && length < sizeof(dir)) {
			memcpy(dir, path, length);
			dir[length] = '\0';
			DIR *d = opendir(dir);
			if (d != NULL) {
				struct dirent *e;
				while ((e = readdir(d)) != NULL && count < max) {
					if (strncmp(e -> d_name, prefix, plength) != 0) continue;
					char full[2 * MAX_BUFFER];
					snprintf(full, sizeof(full), "%s/%s", dir, e -> d_name);
					if (access(full, X_OK) == 0)
						count = addMatch(matches, count, max, e->d_name);
				}
				closedir(d);
			}
		}
		path = sep ? sep + 1 : path + length;
	}
	return count;	
}

//Files and directories matching prefix ("src/ma -> dir "src", base "ma").
//Directories get a trailing '/'
static int addFiles(const char *prefix, char *matches[], int count, int max) {
	const char *slash = strrchr(prefix, '/');
	char dir[MAX_BUFFER] = "";
	const char *base = prefix;
	if (slash != NULL) {
		size_t dlength = (size_t)(slash - prefix) + 1;
		if(dlength >= sizeof(dir)) return count;
		memcpy(dir, prefix, dlength);
		dir[dlength] = '\0';
		base = slash + 1;
	}
	size_t blength = strlen(base);

	DIR *d = opendir(dir[0] != '\0' ? dir : ".");
	if (d == NULL) return count;
	struct dirent *e;
	while ((e = readdir(d)) != NULL && count < max) {
		if (strncmp(e->d_name, base, blength) != 0) continue;
		if (e->d_name[0] == '.' && base[0] != '.') continue;
		char candidate[2 * MAX_BUFFER];
		snprintf(candidate, sizeof(candidate), "%s%s", dir, e->d_name);
		struct stat st;
		if (stat(candidate, &st) == 0 && S_ISDIR(st.st_mode))
			strncat(candidate,"/", sizeof(candidate) - strlen(candidate) - 1);
		count = addMatch(matches, count, max, candidate);
	}
	closedir(d);
	return count;
}

//Candidates for the word being typed. Every match starts with prefix.
//isFirstWord = 1: builtins, aliases and PATH programs. Otherwise : files.
//The matches are malloc'ed : the caller frees them. Returns the number of matches.
int findCompletions(const char *prefix, int isFirstWord, char *matches[], int max) {
	int count = 0;
	size_t plength = strlen(prefix);

	if(isFirstWord && strchr(prefix, '/') == NULL) {
		for (int i = 0; i < infoCount; i++)
			if (strncmp(infoTable[i].name, prefix, plength) == 0)
				count = addMatch(matches, count, max, infoTable[i].name);
		for (int i = 0; i < aliasCount; i++)
			if(strncmp(aliases[i].name, prefix, plength) == 0)
				count = addMatch(matches, count, max, aliases[i].name);
		if (plength > 0)
			count = addPathPrograms(prefix, matches, count, max);
	}
	else {
		count = addFiles(prefix, matches, count, max);
	}
	qsort(matches, (size_t)count, sizeof(char *), cmpStr);
	return count;
}

size_t commonPrefixLength(char *matches[], int n) {
	if (n == 0) return 0;
	size_t length = strlen(matches[0]);
	for (int i = 1; i < n; i++) {
		size_t k = 0;
		while (k < length && matches[i][k] == matches[0][k]) k++;
		length = k;
	}
	return length;
}

static void writeOut(const char *s, size_t n) {
	if (write(STDOUT_FILENO, s, n) < 0) { /* nothing useful to do */ }
}

static void insertText(char *buf, size_t *length, size_t size, const char *s, size_t n) {
	for (size_t k = 0; k < n && *length < size - 1; k++) {
		buf[(*length)++] = s[k];
		writeOut(&s[k], 1);
	}
	buf[*length] = '\0';
}

//Handling events when Tab is pressed
static void handleTab(const char *prompt, char *buf, size_t *length, size_t size) {
	//Start of the word being typed
	size_t start = *length;
	while (start > 0 && buf[start - 1] != ' ' && buf[start - 1] != '\t')
		start--;

	//Is it the 1st word of the command (start of line, or right after "->")?
	size_t k = start;
	while (k > 0 && (buf[k - 1] == ' ' || buf[k - 1] == '\t'))
		k--;
	int isFirstWord = (k == 0) || (k >= 2 && buf[k - 2] == '-' && buf[k - 1] == '>');
	
	char prefix[MAX_BUFFER];
	size_t plength = *length - start;
	memcpy(prefix, buf + start, plength);
	prefix[plength] = '\0';

	char *matches[MAX_MATCHES];
	int n = findCompletions(prefix, isFirstWord, matches, MAX_MATCHES);

	if (n == 0) {
		writeOut("\a", 1);
	}
	else if (n == 1) {
		size_t m = strlen(matches[0]);
		insertText(buf, length, size, matches[0] + plength, m - plength);
		if (matches[0][m - 1] != '/')
			insertText(buf, length, size, " ", 1);
	}
	else {
		size_t common = commonPrefixLength(matches, n);
		if (common > plength) {
			insertText(buf, length, size, matches[0] + plength, common - plength);
		}
		else {
			writeOut("\n", 1);
			for (int i = 0; i < n; i++){
				writeOut(matches[i], strlen(matches[i]));
				writeOut("  ", 2);
			}
			writeOut("\n", 1);
			writeOut(prompt, strlen(prompt));
			writeOut(buf, *length);
		}
	}
	for (int i = 0; i < n; i++) free(matches[i]);
}

//Read one line with editing : Tab completion, Backspace, Ctrl+C, Ctrl+D.
//Returns 0 at the end of the input (Ctrl+D on an empty line), 1 otherwise
int readLine(const char *prompt, char *buf, size_t size){
	if (!isatty(STDIN_FILENO)) {
		if (fgets(buf, (int)size, stdin) == NULL) return 0;
		buf[strcspn(buf, "\n")] = '\0';
		return 1;
	}

	//Raw mode : keys arrive one by one, no echo, Ctrl+C is an ordinary key
	struct termios saved, raw;
	tcgetattr(STDIN_FILENO, &saved);
	raw = saved;
	raw.c_lflag &= ~(ICANON | ECHO | ISIG);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);

	writeOut(prompt, strlen(prompt));
	size_t length = 0;
	buf[0] = '\0';
	int result = 1;

	while(true) {
		//Wait for a key, but wake up every 200ms to report finished background jobs
		struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
		int ready = poll(&pfd, 1, 200);
		if (ready == 0 || (ready < 0 && errno == EINTR)) { //timeout or signal
			if(checkbgjobs() > 0){
				writeOut(prompt, strlen(prompt));
				writeOut(buf, length);
			}
			continue;
		}
		if (ready < 0) {
			ready = 0;
			break;
		}
	
		unsigned char c;
		ssize_t r = read(STDIN_FILENO, &c, 1);
		if (r < 0 && errno == EINTR) continue;
		//Input closed
		if (r <= 0) {
			result = 0;
			break;
		}

		if (c == '\n' || c == '\r'){
			writeOut("\n", 1);
			break;
		}
		else if (c == 4) { //Ctrl+D
			if (length == 0){
				writeOut("\n", 1);
				result = 0;
				break;
			}
		}
		else if (c == 3) { //Ctrl+C : drop the line
			writeOut("^C\n", 3);
			length = 0;
			buf[0] = '\0';
			break;
		}
		else if (c == 127 || c == 8) { //Backspace
			if(length > 0){
				//Remove a whole UTF-8 character
				while(length > 0 && ((unsigned char)buf[length - 1] & 0xC0) == 0x80)
					length--;
				if (length > 0) length--;
				buf[length] = '\0';
				writeOut("\b \b", 3);
			}
		}
		else if (c == '\t') {
			handleTab(prompt, buf, &length, size);
		}
		else if (c == 27){
			unsigned char s;
			if (read(STDIN_FILENO, &s, 1) == 1 && s == '[') {
				while (read(STDIN_FILENO, &s, 1) == 1 && !(s >= 0x40 && s <= 0x7E)) {/* skip */}
			}
			//TODO : Put History Navigation here (Up/Down keys)
		}
		else if (c >= 32) { //Printable character (or part of a UTF-8 one)
			if (length < size - 1){
				buf[length++] = (char)c;
				buf[length] = '\0';
				writeOut((const char *)&c, 1);
			}
		}
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &saved); //restore the terminal BEFORE running commands
	return result;
}

