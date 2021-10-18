#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"

#define INITSIZE 256

Str *str_new() {
	return str_new_len(INITSIZE);
}

Str *str_new_cstr(const char *s) {
	Str *ss = str_new();
	str_append_cstr(ss, s);
	return ss;
}

Str *str_new_len(size_t len) {
	Str *ss;
	ss = malloc(sizeof(struct Str));
	ss->buf = malloc(sizeof(char) * len + 1); /* +1 for null byte */
	ss->buf[0] = '\0';
	ss->len = 0;
	ss->max = len;
	ss->rc = 1;
	return ss;
}

void str_append_cstr(Str *ss, const char *s) {
	/*
	 * TODO: It is now O(2*strlen(s))) but we can make it O(strlen(s)) by
	 * not calculating strlen(s), instead passing s to the loop in
	 * str_append_cstr_len() and appending characters to ss->buf until we
	 * find '\0' in s.
	 */
	str_append_cstr_len(ss, s, strlen(s));
}

void str_append_cstr_len(Str *ss, const char *s, size_t len) {
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

Str *str_incref(Str *ss) {
	ss->rc++;
	return ss;
}

void str_decref(Str *ss) {
	ss->rc--;
	if (ss->rc > 0)
		return;
	free(ss->buf);
	free(ss);
}

size_t str_len(const Str *ss) {
	return ss->len;
}

void str_reset(Str *ss) {
	ss->len = 0;
	ss->buf[0] = '\0';
}
