#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"
#include "utils.h" /* TODO: recursive include! */

#define INITSIZE 256

/* Grow a Str internal buffer object at least `delta` bytes. */
static void grow(Str *s, size_t delta) {
	size_t min = s->bytelen + delta;
	while (s->max < min)
		s->max *= 2;
	/* +1 for null byte */
	s->buf = realloc(s->buf, (sizeof(char) * s->max) + 1);
}

Str *str_new(void) {
	return str_new_bytelen(INITSIZE);
}

Str *str_new_cstr(const char *s) {
	Str *ss = str_new();
	str_append_cstr(ss, s);
	return ss;
}

Str *str_new_bytelen(size_t len) {
	Str *ss;
	ss = malloc(sizeof(struct Str));
	ss->buf = malloc(sizeof(char) * len + 1); /* +1 for null byte */
	ss->buf[0] = '\0';
	ss->bytelen = 0;
	ss->max = len;
	ss->rc = 1;
	return ss;
}

void str_append(Str *ss, const Str *s) {
	str_append_cstr(ss, str_buf(s));
}

void str_append_cstr(Str *ss, const char *s) {
	/*
	 * TODO: It is now O(2*strlen(s))) but we can make it O(strlen(s)) by
	 * not calculating strlen(s), instead passing s to the loop in
	 * str_append_cstr_bytelen() and appending characters to ss->buf until we
	 * find '\0' in s.
	 */
	str_append_cstr_bytelen(ss, s, strlen(s));
}

void str_append_cstr_bytelen(Str *ss, const char *s, size_t len) {
	grow(ss, len);
	char *sb;
	sb = &ss->buf[ss->bytelen];
	while (len > 0) {
		*sb = *s;
		ss->bytelen++;
		sb++;
		s++;
		len--;
	}
	*sb = '\0';
}

Str *str_incref(Str *ss) {
	ss->rc++;
	return ss;
}

void str_decref(Str *ss) {
	if (!ss)
		return;
	ss->rc--;
	if (ss->rc > 0)
		return;
	free(ss->buf);
	free(ss);
}

size_t str_bytelen(const Str *ss) {
	return ss->bytelen;
}

void str_reset(Str *ss) {
	ss->bytelen = 0;
	ss->buf[0] = '\0';
}

void str_insert_cstr_bytelen(Str *s, const char *cstr, size_t offset, size_t sz) {
	grow(s, sz);
	size_t pos = utf8_char_bytepos(s->buf, offset);
	/* Make room for cstr */
	for (size_t i = s->bytelen+1; i > offset; i--)
		s->buf[i+sz-1] = s->buf[i-1];
	/* Insert cstr bytes */
	for (size_t i = 0; i < sz; i++)
		s->buf[pos+i] = cstr[i];
	s->bytelen += sz;
}

