#ifndef JANECHAT_STRBUF_H
#define JANECHAT_STRBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

typedef struct StrBuf StrBuf;

/*
 * Fat pointer to our String type.  buf length is max+1, because we always make
 * room for the nullbyte, but don't count it in len and max.  StrBuf objects are
 * always guaranteed to have null-terminated strings.
 */
struct StrBuf {
	char *buf;	/* Buffer that store string (always with null byte) */
	size_t len;	/* Length of string in buf (don't count null byte )*/
	size_t max;	/* Length of buf - 1 (cause we don't count null byte */
	int rc;		/* Reference count */
};

inline const char *strbuf_buf(const StrBuf *ss) { return ss->buf; }

/*
 * Provide a common interface for string comparison that works for both C
 * strings (char *) and StrBuf *.  Return true or false.
 */
#define streq(x, y) \
	_Generic((x), \
		const StrBuf *: _Generic((y), \
			const StrBuf *: streq_strbuf_strbuf, \
			StrBuf *: streq_strbuf_strbuf, \
			default: streq_strbuf_cstr), \
		StrBuf *: _Generic((y), \
			const StrBuf *: streq_strbuf_strbuf, \
			StrBuf *: streq_strbuf_strbuf, \
			default: streq_strbuf_cstr), \
		default: _Generic((y), \
			const StrBuf *: streq_cstr_strbuf, \
			StrBuf *: streq_cstr_strbuf, \
			default: streq_cstr_cstr)) \
	(x, y)
inline bool streq_cstr_cstr(const char *x, const char *y) {
	return (strcmp(x, y) == 0);
}
inline bool streq_cstr_strbuf(const char *x, const StrBuf *y) {
	return (strcmp(x, strbuf_buf(y)) == 0);
}
inline bool streq_strbuf_cstr(const StrBuf *x, const char *y) {
	return (strcmp(strbuf_buf(x), y) == 0);
}
inline bool streq_strbuf_strbuf(const StrBuf *x, const StrBuf *y) {
	return (strcmp(strbuf_buf(x), strbuf_buf(y)) == 0);
}

#define strbuf_new(...) \
	strbuf_new_((struct strbuf_new_params){.len = 0, .cstr = NULL, __VA_ARGS__})
struct strbuf_new_params {
	size_t len;		/* Initial length of internal string array */
	const char *cstr;	/* An optional C string to be copied */
};
StrBuf *strbuf_new_(struct strbuf_new_params);

#define NARGS(...) NARGS_(__VA_ARGS__, 5, 4, 3, 2, 1, 0)
#define NARGS_(_5, _4, _3, _2, _1, N, ...) N
#define CONC(A, B) CONC_(A, B)
#define CONC_(A, B) A##B

#define strbuf_append(...) CONC(strbuf_append_, NARGS(__VA_ARGS__))(__VA_ARGS__)
struct strbuf_append_params {
	size_t len;
};
#define strbuf_append_2(ss, s) \
	strbuf_append_(ss, s, (struct strbuf_append_params){.len = 0})
#define strbuf_append_3(ss, s, A) \
	strbuf_append_(ss, s, (struct strbuf_append_params){A})
void strbuf_append_(StrBuf *ss, const char *s, struct strbuf_append_params);

size_t strbuf_len(const StrBuf *);
int strbuf_cmp(const StrBuf *, const StrBuf *);
void strbuf_reset(StrBuf *);
StrBuf *strbuf_incref(StrBuf *);
void strbuf_decref(StrBuf *);
size_t strbuf_len(const StrBuf *);


#endif /* !JANECHAT_STRBUF_H */
