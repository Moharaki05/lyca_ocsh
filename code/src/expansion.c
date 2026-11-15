#include "expansion.h"

void expandVars(const char *in, char *out, size_t size){
	size_t o = 0, i = 0;
	while (in[i] != '\0' && o < size - 1){
		if (in[i] == '`') {
			const char *end = strchr(in + i + 1, '`');
			if (end != NULL) {
				substitute(in + i + 1, (size_t)(end - (in + i + 1)), out, &o, size);
				i = (size_t)(end - in) + 1;
				continue;
			}
		}
		
		if (in[i] != '$'){
			out[o++] = in[i++];
			continue;
		}
		i++;	// skip the '$'
		char name[MAX_NAME];
		char num[32];
		const char *val = NULL;

		if (in[i] == '?'){
			snprintf(num, sizeof(num), "%d", lastStatus);
			val = num;
			i++;
		}
		else if (in[i] == '!'){
			if (lastBgPid > 0)
				snprintf(num, sizeof(num), "%d", (int)lastBgPid);
			else num[0] = '\0'; // no BG job yet : empty
			val = num;
			i++;
		}
		else if (in[i] == '('){
			const char *end = findClosingParen(in + i + 1);
			if (end == NULL){
				out[o++] = '$';
				continue;
			}
			substitute(in + i + 1, (size_t)(end - (in + i + 1)), out , &o, size);
			i = (size_t)(end - in) + 1;
		}
		else if (in[i] == '{'){
			const char *end = strchr(in + i, '}');
			size_t length = end ? (size_t)(end - (in + i + 1)) : 0;
			if (end == NULL || length >= MAX_NAME || !isValidName(in + i + 1, length)) {
				out[o++] = '$'; // not a valid ${NAME}: keep it as a text
				continue;
			}
			//memcpy copies memory blocks from a mem address to a specified one.
			memcpy(name, in + i + 1, length);
			name[length] = '\0';
			val = getVar(name);
			i = (size_t)(end - in) + 1;
		}
		else if (isalpha((unsigned char)in[i]) || in[i] == '_') {
			size_t length = 0;
			while ((isalnum((unsigned char)in[i]) || in[i] == '_') && length < MAX_NAME - 1)
				name[length++] = in[i++];
			name[length] = '\0';
			val = getVar(name);
		}		
		else {
			out[o++] = '$'; // lone '$': keep it as text
			continue;
		}

		for (size_t k = 0; val && val[k] != '\0' && o < size - 1; k++)
			out[o++] = val[k];
	}
	out[o] = '\0';
}

