#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "strbuf.h"

#define INITSIZE 256

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

char *strbuf_buf(const StrBuf *s) {
	return s->buf;
}

StrBuf *strbuf_new() {
	StrBuf *ss;
	ss = malloc(sizeof(struct StrBuf));
	ss->buf = malloc(sizeof(char) * INITSIZE + 1); /* +1 for null byte */
	ss->buf[0] = '\0';
	ss->len = 0;
	ss->max = INITSIZE;
	ss->rc = 1;
	return ss;
}

StrBuf *strbuf_new_c(const char *s) {
	StrBuf *sb;
	sb = strbuf_new();
	strbuf_cat_c(sb, s);
	return sb;
}

void strbuf_cat_c(StrBuf *ss, const char *s) {
	/*
	 * TODO: It is now O(2*strlen(s))) but we can make it O(strlen(s)) by
	 * not calculating strlen(s) and passing to strbuf_ncat_c() but
	 * appending characters to ss until we find '\0'.
	 */
	strbuf_ncat_c(ss, s, strlen(s));
}

void strbuf_ncat_c(StrBuf *ss, const char *s, size_t size) {
	char *sb;
	sb = &ss->buf[ss->len];
	while (size > 0) {
		assert(ss->len <= ss->max);
		if (ss->len == ss->max) {
			ss->max *= 2;
			ss->buf = realloc(ss->buf,
			 (sizeof(char) * ss->max) + 1); /* +1 for null byte */
			sb = &ss->buf[ss->len]; // because the pointer may have changed
		}
		*sb = *s;
		ss->len++;
		sb++;
		s++;
		size--;
	}
	*sb = '\0';
}

void strbuf_increc(StrBuf *ss) {
	ss->rc++;
}

void strbuf_decref(StrBuf *ss) {
	ss->rc--;
	if (ss->rc > 0)
		return;
	free(ss->buf);
	free(ss);
}

size_t strbuf_len(const StrBuf *ss) {
	return ss->len;
}

void strbuf_reset(StrBuf *ss) {
	ss->len = 0;
	ss->buf[0] = '\0';
}
