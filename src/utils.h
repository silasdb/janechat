#ifndef JANECHAT_UTILS_H
#define JANECHAT_UTILS_H

#include <stdio.h>

void debug(const char *path, const char *format, ...);
char *read_line_alloc();
char *read_file_alloc(FILE *);

#endif /* !JANECHAT_UTILS_H */
