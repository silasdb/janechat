#ifndef JANECHAT_UTILS_H
#define JANECHAT_UTILS_H

#include <stdbool.h>
#include <stdio.h>

void debug(const char *path, const char *format, ...);
char *read_line_alloc(void);
char *read_file_alloc(FILE *);
bool str2li(const char *, long int *);

#endif /* !JANECHAT_UTILS_H */
