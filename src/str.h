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
	size_t bytelen;	/* Length of string in buf (don't count null byte )*/
	bool utf8;
	int len;	/* UTF-8 length (don't count null byte) */
	size_t max;	/* Length of buf - 1 (cause we don't count null byte */
	int rc;		/* Reference count */
};

inline const char *str_buf(const Str *ss) { return ss->buf; }
inline size_t str_bytelen(const Str *ss) { return ss->bytelen; }

#define streq(x, y) (strcmp(x, y) == 0)
#define str_sc_eq(x, y) (strcmp(str_buf(x), y) == 0)
Str *str_new(void);
Str *str_new_cstr(const char *);
Str *str_new_bytelen(size_t);
void str_set_utf8(Str *, bool);
int str_len(const Str *);
void str_append(Str *ss, const Str *s);
void str_append_cstr(Str *ss, const char *s);
void str_append_cstr_bytelen(Str *ss, const char *s, size_t);
void str_insert_cstr(Str *, const char *s, size_t);
int str_cmp(const Str *, const Str *);
void str_reset(Str *);
Str *str_incref(Str *);
void str_decref(Str *);
void str_copy_utf8char_at(const Str *, size_t, char [5]);
void str_remove_char_at(Str *, size_t);

#endif /* !JANECHAT_STR_H */
