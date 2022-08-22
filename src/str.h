#ifndef JANECHAT_STR_H
#define JANECHAT_STR_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * Fat pointer to our String type.  buf length is max+1, because we always make
 * room for the nullbyte, but don't count it in len and max.  Str objects are
 * always guaranteed to have null-terminated strings.
 */
struct Str {
	char *buf;	/* Buffer that store string (always with null byte) */
	size_t bytelen;	/* Length of string in buf (don't count null byte )*/
	size_t max;	/* Length of buf - 1 (cause we don't count null byte */
	int rc;		/* Reference count */
};
typedef struct Str Str;

struct Utf8Char {
	char c[5];
};
typedef struct Utf8Char Utf8Char;

struct str_utf8_index {
	size_t utf8_index;
};
#define UTF8_INDEX(x) ((struct str_utf8_index){ .utf8_index = x, })

#define STR_UTF8CHAR_FOREACH(s, uc, i) \
	for (i = 0; \
		 uc = str_utf8char_at(s, UTF8_INDEX(i)), uc.c[0] != '\0'; \
		 i++)

#define streq(x, y) (strcmp(x, y) == 0)
#define str_sc_eq(x, y) (strcmp(str_buf(x), y) == 0)

Str *str_new(void);
Str *str_new_cstr(const char *);
Str *str_new_cstr_fixed(const char *);
Str *str_new_bytelen(size_t len);
size_t str_utf8len(const Str *);
void str_append_str(Str *ss, const Str *s);
void str_append_cstr(Str *ss, const char *s);
void str_append_cstr_bytelen(Str *ss, const char *s, size_t);
void str_insert_utf8char_at(Str *, Utf8Char, struct str_utf8_index);
Str *str_dup(const Str *);
int str_cmp(const Str *, const Str *);
void str_reset(Str *);
Str *str_incref(Str *);
void str_decref(Str *);
Utf8Char str_utf8char_at(const Str *, struct str_utf8_index);
void str_remove_utf8char_at(Str *, struct str_utf8_index);
bool str_starts_with_cstr(Str *, const char *);

inline const char *str_buf(const Str *s) {
	return s->buf;
}

inline size_t str_bytelen(const Str *s) {
	return s->bytelen;
}

/* Other helper methods */

Str *str_new_uri_extract_server(Str *);
Str *str_new_uri_extract_path(Str *);

#endif /* !JANECHAT_STR_H */
