#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "strbuf.h"

#define INITSIZE 256

StrBuf *strbuf_new_(struct strbuf_new_params params) {
	if (params.len == 0)
		params.len = INITSIZE;
	StrBuf *ss;
	ss = malloc(sizeof(struct StrBuf));
	ss->buf = malloc(sizeof(char) * params.len + 1); /* +1 for null byte */
	ss->buf[0] = '\0';
	ss->len = 0;
	ss->max = params.len;
	ss->rc = 1;
	if (params.cstr)
		strbuf_cat_c(ss, params.cstr);
	return ss;
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

StrBuf *strbuf_incref(StrBuf *ss) {
	ss->rc++;
	return ss;
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
