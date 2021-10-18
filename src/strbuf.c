#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "strbuf.h"

#define INITSIZE 256

StrBuf *strbuf_new() {
	return strbuf_new_len(INITSIZE);
}

StrBuf *strbuf_new_cstr(const char *s) {
	StrBuf *ss = strbuf_new();
	strbuf_append_cstr(ss, s);
	return ss;
}

StrBuf *strbuf_new_len(size_t len) {
	StrBuf *ss;
	ss = malloc(sizeof(struct StrBuf));
	ss->buf = malloc(sizeof(char) * len + 1); /* +1 for null byte */
	ss->buf[0] = '\0';
	ss->len = 0;
	ss->max = len;
	ss->rc = 1;
	return ss;
}

void strbuf_append_cstr(StrBuf *ss, const char *s) {
	/*
	 * TODO: It is now O(2*strlen(s))) but we can make it O(strlen(s)) by
	 * not calculating strlen(s), instead passing s to the loop in
	 * strbuf_append_cstr_len() and appending characters to ss->buf until we
	 * find '\0' in s.
	 */
	strbuf_append_cstr_len(ss, s, strlen(s));
}

void strbuf_append_cstr_len(StrBuf *ss, const char *s, size_t len) {
	char *sb;
	sb = &ss->buf[ss->len];
	while (len > 0) {
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
		len--;
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
