#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

char *read_line() {
	char *line = NULL;
	size_t linesize = 0, len;
	len = getline(&line, &linesize, stdin);
	assert(line != NULL);
	assert(line[len-1] == '\n');
	line[len-1] = '\0';
	return line;
}

/**
 * Read a file and return an allocated buffer that the caller must free.
 *
 * On error, return NULL.
 */
char *read_file(FILE *stream) {
	char *output = NULL;
	size_t size = 0;
	getdelim(&output, &size, EOF, stream);
	if (ferror(stream)) {
		perror("Fatal error when reading response");
		free(output);
		output = NULL;
	}
	return output;
}
