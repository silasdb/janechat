#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"
#include "utils.h" /* TODO: recursive include! */

#define INITSIZE 256

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
	Str *ss;
	ss = malloc(sizeof(struct Str));
	ss->buf = buffer_new();
	ss->utf8len = 0;
	return ss;
}

void str_append(Str *ss, const Str *s) {
	buffer_append(ss, str_data(s));
	ss->utf8len += utf8len(str_data(s));
}

void str_append_cstr(Str *ss, const char *s) {
	buffer_append_cstr(ss, s);
	ss->utf8len += utf8len(s);
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
	buffer_free(ss->buf);
	free(ss);
}

void str_insert_utf8char_at(Str *s, char uc[5], size_t offset) {
	size_t pos = utf8_char_bytepos(str_data(s->buf), offset);
	buffer_insert_cstr(s->buf, uc, pos);
}

Str *str_dup(const Str *s) {
	Str *dup = str_new_bytelen(s->bytelen);
	strcpy(dup->buf, s->buf);
	dup->bytelen = s->bytelen;
	dup->buf[dup->bytelen] = '\0';
	dup->utf8 = s->utf8;
	dup->len = s->len;
	dup->utf8 = s->utf8;
	dup->rc = 1;
	return dup;
}

void str_copy_utf8char_at(const Str *s, size_t pos, char uc[5]) {
	uc[0] = uc[1] = uc[2] = uc[3] = uc[4] = '\0';
	size_t p = utf8_char_bytepos(s->buf, pos);
	size_t sz = utf8_char_size(s->buf[p]);
	for (size_t i = 0; i < sz; i++)
		uc[i] = s->buf[p+i];
}

void str_remove_char_at(Str *s, size_t pos) {
	size_t start = utf8_char_bytepos(str_data(s), pos);
	size_t sz = utf8_char_size(str_data(s)[p]);
	buffer_remove_range(s->buf, start, sz);
}

bool str_starts_with_cstr(Str *ss, char *s) {
	return (strncmp(ss->buf, s, strlen(s)) == 0);
}
