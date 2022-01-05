#ifndef JANECHAT_STR_H
#define JANECHAT_STR_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

typedef struct Str Str;

/*
 * Fat pointer to our String type.  buf length is max+1, because we always make
 * room for the nullbyte, but don't count it in len and max.  Str objects are
 * always guaranteed to have null-terminated strings.
 */
struct Str {
	char *buf;	/* Buffer that store string (always with null byte) */
	size_t len;	/* Length of string in buf (don't count null byte )*/
	size_t max;	/* Length of buf - 1 (cause we don't count null byte */
	int rc;		/* Reference count */
};

inline const char *str_buf(const Str *ss) { return ss->buf; }

#define streq(x, y) (strcmp(x, y) == 0)
Str *str_new(void);
Str *str_new_cstr(const char *);
Str *str_new_len(size_t);
void str_append(Str *ss, const Str *s);
void str_append_cstr(Str *ss, const char *s);
void str_append_cstr_len(Str *ss, const char *s, size_t);
size_t str_len(const Str *);
int str_cmp(const Str *, const Str *);
void str_reset(Str *);
Str *str_incref(Str *);
void str_decref(Str *);
size_t str_len(const Str *);

#endif /* !JANECHAT_STR_H */
