#ifndef JANECHAT_UTILS_H
#define JANECHAT_UTILS_H

#include <stdbool.h>
#include <stdio.h>

void debug(const char *path, const char *format, ...);
char *read_line_alloc(void);
char *read_file_alloc(FILE *);
bool str2li(const char *, long int *);
size_t utf8_char_size(int);
int utf8_char_bytepos(const char *, size_t, size_t);
int utf8_char_width(const char *);

#endif /* !JANECHAT_UTILS_H */
