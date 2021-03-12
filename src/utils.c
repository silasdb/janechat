#include <assert.h>
#include <stdio.h>

#include "utils.h"

char *
read_line()
{
	char *line = NULL;
	size_t linesize = 0, len;
	len = getline(&line, &linesize, stdin);
	assert(line != NULL);
	assert(line[len-1] == '\n');
	line[len-1] = '\0';
	return line;
}

char *
read_file(FILE *stream)
{
	char *output = NULL;
	size_t size = 0;
	getdelim(&output, &size, EOF, stream);
	if (!output) {
		perror("Fatal error when reading response");
	}
	return output;
}
