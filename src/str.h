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
	size_t max;	/* Length of buf - 1 (cause we don't count null byte */
	int rc;		/* Reference count */
};

struct str_new_params {
	/*
	 * TODO: Necessary parameter so struct initialization don't
	 * expand to {}, which is forbidden by ISO C. We also don't want to turn
	 * off gcc's -Woverride-init, although having a "dummy" parameter to
	 * satisfy compiler is also not very beautiful! We should study how
	 * https://facil.io/ solves this.
	 */
	int dummy;

	const char *cstr;
};

inline const char *str_buf(const Str *ss) { return ss->buf; }

#define streq(x, y) (strcmp(x, y) == 0)

#define str_new(...) \
	str_new_((struct str_new_params){.dummy = 0, __VA_ARGS__})
Str *str_new_(struct str_new_params);

void str_append(Str *ss, const Str *s);
void str_append_cstr(Str *ss, const char *s);
void str_append_cstr_bytelen(Str *ss, const char *s, size_t);
size_t str_bytelen(const Str *);
int str_cmp(const Str *, const Str *);
void str_reset(Str *);
Str *str_incref(Str *);
void str_decref(Str *);
size_t str_bytelen(const Str *);

#endif /* !JANECHAT_STR_H */
