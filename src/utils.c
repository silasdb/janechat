#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "wchar.h"

void debug(const char *path, const char *format, ...) {
	FILE *f = fopen(path, "a");
	va_list args;
	va_start(args, format);
	vfprintf(f, format, args);
	fclose(f);
}

char *read_line_alloc(void) {
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

bool str2li(const char *str, long int *i) {
	if (*str == '\0')
		return false;
	char *ret;
	*i = strtol(str, &ret, 10);
	if (*ret != '\0')
		return false;
	return true;
}

Str *mxc_uri_extract_server_alloc(Str *uri) {
	const char *a, *b;
	a = str_buf(uri) + strlen("mxc://");
	b = a + strcspn(a, "/");
	char *server = malloc(sizeof(char) * (b - a + 1));
	strncpy(server, a, b-a);
	server[b-a] = '\0';
	Str *ret = str_new_cstr(server);
	free(server);
	return ret;
}

Str *mxc_uri_extract_path_alloc(Str *uri) {
	const char *a;
	a = str_buf(uri) + strlen("mxc://");
	a += strcspn(a, "/") + 1;
	return str_new_cstr(a);
}

size_t utf8_char_size(int c) {
	if ((c & 0x80) == 0)
		return 1;
	if ((c & 0xE0) == 0xC0)
		return 2;
	if ((c & 0xF0) == 0xE0)
		return 3;
	if ((c & 0xF8) == 0xF0)
		return 4;
	/* Invalid char. Return 1 byte. */
	return 1;
}

size_t utf8_char_bytepos(const char *s, size_t i) {
	size_t pos = 0;
	while (i--) {
		size_t sz;
		sz = utf8_char_size(*s);
		s += sz;
		pos += sz;
	}
	return pos;
}

int utf8_char_width(const char *s) {
	wchar_t wc;
	assert(mbtowc(&wc, s, utf8_char_size(*s)) > 0);
	return wcwidth(wc);
}
