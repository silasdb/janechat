#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "strbuf.h"

#define INITSIZE 256

struct StrBuf {
	char *buf;
	size_t len; // do not include null byte
	size_t max;
	bool attached;
};

char *strbuf_buf(const StrBuf *s) {
	return s->buf;
}

StrBuf *strbuf_new() {
	StrBuf *ss;
	ss = malloc(sizeof(struct StrBuf));
	ss->buf = malloc(sizeof(char) * INITSIZE);
	ss->buf[0] = '\0';
	ss->len = 0;
	ss->max = INITSIZE;
	ss->attached = true;
	return ss;
}

void strbuf_cat_c(StrBuf *ss, const char *s) {
	strbuf_ncat_c(ss, s, strlen(s));
}

void strbuf_ncat_c(StrBuf *ss, const char *s, size_t size) {
	char *sb;
	sb = &ss->buf[ss->len];
	while (size > 0) {
		assert(ss->len <= ss->max);
		if (ss->len == ss->max) {
			ss->max *= 2;
			ss->buf = realloc(ss->buf, sizeof(char) * ss->max);
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

void strbuf_free(StrBuf *ss) {
	if (ss->attached)
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

/*
 * Detach a buffer from the StrBuf object.  This is useful when we need to work
 * on the `char *` buffer directly and don't need the StrBuf object anymore.  The
 * caller will have to call strbuf_free() on the StrBuf object and free() on the
 * char * variable when after using the `char *` string.
 */
char *strbuf_detach(StrBuf *ss) {
	ss->attached = false;
	return ss->buf;
}
