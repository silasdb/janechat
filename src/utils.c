#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

char *read_line_alloc() {
	char *line = NULL;
	size_t linesize = 0, len;
	len = getline(&line, &linesize, stdin);
	
	/*
	 * TODO: before using libcurl, we used to have:
	 *
	 * assert(line != NULL);
	 *
	 * because getline() blocked indepently of the process received SIGALRM.
	 *
	 * With libcurl, it just returns a NULL line whenever SIGALRM is sent
	 * (see main.c). We decide to allow return of a NULL line to the caller
	 * (this also needed an adaptation on the caller).
	 *
	 * It is not very clear why it happens, maybe curl configures signals so
	 * they behave differently.  It also seems it uses SIGALRM in some cases.
	 * I should study it more and write an appropriate technote about it.
	 * For now, let's live with this change.
	 */
	if (line) {
		assert(line[len-1] == '\n');
		line[len-1] = '\0';
	}
	return line;
}

/**
 * Read a file and return an allocated buffer that the caller must free.
 *
 * On error, return NULL.
 */
char *read_file_alloc(FILE *stream) {
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
