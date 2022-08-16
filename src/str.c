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

static int utf8len(const char *s) {
	int len = 0;
	size_t sz;
	while (*s != '\0') {
		sz = utf8_char_size(*s);
		len += 1;
		s += sz;
	}
	return len;
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

void str_reset(Str *ss) {
	ss->bytelen = 0;
	ss->buf[0] = '\0';
}

void str_insert_cstr(Str *s, const char *cstr, size_t offset) {
	size_t sz = strlen(cstr);
	grow(s, sz);
	/* Make room for cstr */
	for (size_t i = s->bytelen+1; i > offset; i--)
		s->buf[i+sz-1] = s->buf[i-1];
	/* Insert cstr bytes */
	for (size_t i = 0; i < sz; i++)
		s->buf[offset+i] = cstr[i];
	s->bytelen += sz;
}

Str *str_dup(const Str *s) {
	Str *dup = str_new_bytelen(s->bytelen);
	strcpy(dup->buf, s->buf);
	dup->bytelen = s->bytelen;
	dup->buf[dup->bytelen] = '\0';
	dup->rc = 1;
	return dup;
}

bool str_starts_with_cstr(Str *ss, char *s) {
	return (strncmp(ss->buf, s, strlen(s)) == 0);
}

/*  Utf8Str related functions. */

Utf8Str *utf8str_new(void) {
	Utf8Str *us = malloc(sizeof(Utf8Str));
	us->str = str_new();
	return us;
}

Utf8Str *utf8str_new_cstr(const char *s) {
	Utf8Str *us = malloc(sizeof(Utf8Str));
	utf8str_append_cstr(us, s);
	return us;
}

Utf8Str *utf8str_incref(Utf8Str *us) {
	str_incref(us->str);
	return us;
}

void utf8str_append(Utf8Str *us1, Utf8Str *us2) {
	str_append_cstr(us1->str, us2->str);
	us1->utf8len += us2->utf8len;
}

void utf8str_append_cstr(Utf8Str *us, const char *s) {
	str_append_cstr(us->str, s);
	us->utf8len += utf8len(s);
}

void utf8str_decref(Utf8Str *us) {
	if (!us)
		return;
	int rc = us->str->rc;
	str_decref(us->str);
	if (rc == 1)
		free(us);
}

void utf8str_copy_utf8char_at(const Utf8Str *us, size_t pos, char uc[5]) {
	uc[0] = uc[1] = uc[2] = uc[3] = uc[4] = '\0';
	size_t p = utf8_char_bytepos(us->str->buf, pos);
	size_t sz = utf8_char_size(us->str->buf[p]);
	for (size_t i = 0; i < sz; i++)
		uc[i] = us->str->buf[p+i];
}

void utf8str_remove_char_at(Utf8Str *us, size_t pos) {
	size_t p = utf8_char_bytepos(us->str->buf, pos);
	size_t sz = utf8_char_size(us->str->buf[p]);
	size_t i;
	for (i = p; i < us->str->bytelen-sz; i++)
		us->str->buf[i] = us->str->buf[i+sz];
	us->str->buf[i] = '\0';
	us->str->bytelen -= sz;
	us->utf8len++;
}

void utf8str_insert_utf8char(Utf8Str *us, char uc[5], size_t i) {
	str_insert_cstr(us->str, uc, i);
	us->utf8len++;
}

bool utf8str_starts_with_cstr(Utf8Str *us, char *s) {
	return str_starts_with_cstr(us->str, s);
}

