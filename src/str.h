#ifndef JANECHAT_STR_H
#define JANECHAT_STR_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "buffer.h"

typedef struct Str Str;

/*
 * Fat pointer to our String type.  buf length is max+1, because we always make
 * room for the nullbyte, but don't count it in len and max.  Str objects are
 * always guaranteed to have null-terminated strings.
 */
struct Str {
	Buffer *buf;	/* Buffer that store string (always with null byte) */
	int utf8len;	/* UTF-8 length (don't count null byte) */
	int rc;		/* Reference count */
};

inline const char *str_data(const Str *ss) { return buffer_data(ss->buf); }
inline size_t str_utf8len(const Str *ss) { return ss->utf8len; }

#define str_sc_eq(x, y) (strcmp(str_buf(x), y) == 0)
Str *str_new(void);
int str_len(const Str *);
void str_append(Str *ss, const Str *s);
void str_append_cstr(Str *ss, const char *s);
void str_append_cstr_bytelen(Str *ss, const char *s, size_t);
void str_insert_cstr(Str *, const char *s, size_t);
Str *str_dup(const Str *);
int str_cmp(const Str *, const Str *);
void str_reset(Str *);
Str *str_incref(Str *);
void str_decref(Str *);
void str_copy_utf8char_at(const Str *, size_t, char [5]);
void str_remove_char_at(Str *, size_t);
bool str_starts_with_cstr(Str *, char *);

#endif /* !JANECHAT_STR_H */
