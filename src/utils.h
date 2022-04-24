#ifndef JANECHAT_UTILS_H
#define JANECHAT_UTILS_H

#include <stdbool.h>
#include <stdio.h>

#include "str.h"

void debug(const char *path, const char *format, ...);
char *read_line_alloc(void);
char *read_file_alloc(FILE *);
bool str2li(const char *, long int *);
Str *mxc_uri_extract_server_alloc(Str *);
Str *mxc_uri_extract_path_alloc(Str *);
size_t utf8_char_size(int);
size_t utf8_char_bytepos(const char *, size_t);

#endif /* !JANECHAT_UTILS_H */
