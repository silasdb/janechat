#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"
#include "utils.h"

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

Str *str_new_cstr_fixed(const char *s) {
	Str *ss = malloc(sizeof(struct Str));
	ss->buf = strdup(s); 
	ss->bytelen = strlen(s);
	ss->max = ss->bytelen;
	ss->rc = 1;
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

size_t str_utf8len(const Str *s) {
	size_t sz;
	size_t len = 0;
	char *buf = s->buf;
	while (*buf != '\0') {
		sz = utf8_char_size(*buf);
		len += 1;
		buf += sz;
	}
	return len;
}

void str_append_str(Str *ss, const Str *s) {
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

void str_insert_utf8char_at(Str *s, Utf8Char utf8char, struct str_utf8_index uidx) {
	size_t sz = strlen(utf8char.c);
	assert(sz <= 4);
	grow(s, sz);
	size_t pos = utf8_char_bytepos(s->buf, uidx.utf8_index);
	/* Make room for utf8char.c */
	for (size_t i = s->bytelen+1; i > pos; i--)
		s->buf[i+sz-1] = s->buf[i-1];
	/* Insert utf8char.c bytes */
	for (size_t i = 0; i < sz; i++)
		s->buf[pos+i] = utf8char.c[i];
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

Utf8Char str_utf8char_at(const Str *s, struct str_utf8_index uidx) {
	Utf8Char uc;
	uc.c[0] = '\0';
	uc.c[1] = '\0';
	uc.c[2] = '\0';
	uc.c[3] = '\0';
	uc.c[4] = '\0';
	size_t p = utf8_char_bytepos(s->buf, uidx.utf8_index);
	size_t sz = utf8_char_size(s->buf[p]);
	for (size_t i = 0; i < sz; i++)
		uc.c[i] = s->buf[p+i];
	return uc;
}

void str_remove_utf8char_at(Str *s, struct str_utf8_index uidx) {
	size_t p = utf8_char_bytepos(s->buf, uidx.utf8_index);
	size_t sz = utf8_char_size(s->buf[p]);
	size_t i;
	for (i = p; i < s->bytelen-sz; i++)
		s->buf[i] = s->buf[i+sz];
	s->buf[i] = '\0';
	s->bytelen -= sz;
}

bool str_starts_with_cstr(Str *ss, const char *s) {
	return (strncmp(ss->buf, s, strlen(s)) == 0);
}

/* Other helper methods */

Str *str_new_uri_extract_server(Str *uri) {
	const char *a, *b;
	a = str_buf(uri) + strlen("mxc://");
	b = a + strcspn(a, "/");
	char *server = malloc(sizeof(char) * (b - a + 1));
	strncpy(server, a, b-a);
	server[b-a] = '\0';
	Str *ret = str_new_cstr(server);
	free(server);
	return ret;
}

Str *str_new_uri_extract_path(Str *uri) {
	const char *a;
	a = str_buf(uri) + strlen("mxc://");
	a += strcspn(a, "/") + 1;
	return str_new_cstr(a);
}
