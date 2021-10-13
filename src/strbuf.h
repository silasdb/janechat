#ifndef JANECHAT_STRBUF_H
#define JANECHAT_STRBUF_H

#include <stddef.h>

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

size_t strbuf_len(const StrBuf *);
int strbuf_cmp(const StrBuf *, const StrBuf *);

/* Optional parameters for the strbuf_new() function. */
struct strbuf_new_params {
	size_t len;		/* Initial length of internal string array */
	const char *cstr;	/* An optional C string to be copied */
};
#define strbuf_new(...) \
	strbuf_new_((struct strbuf_new_params){.len = 0, .cstr = NULL, __VA_ARGS__})
StrBuf *strbuf_new_(struct strbuf_new_params);

void strbuf_cat_c(StrBuf *ss, const char *s);
void strbuf_ncat_c(StrBuf *, const char *, size_t);
void strbuf_reset(StrBuf *);
StrBuf *strbuf_incref(StrBuf *);
void strbuf_decref(StrBuf *);
size_t strbuf_len(const StrBuf *);
char *strbuf_detach(StrBuf *);


#endif /* !JANECHAT_STRBUF_H */
