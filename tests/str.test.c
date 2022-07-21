#undef NDEBUG
#include <assert.h>

#include "../src/str.c"

static void str_append_test() {
	Str *s = str_new_cstr("abc");

	assert(str_bytelen(s) == 3);
	assert(str_buf(s)[3] == '\0');

	str_append_cstr(s, "def");

	assert(strcmp(str_buf(s), "abcdef") == 0);
	assert(str_bytelen(s) == 6);
	assert(str_buf(s)[6] == '\0');
}

static void str_grow_test() {
	Str *s = str_new_bytelen(3);
	assert(s->max == 3);
	str_append_cstr(s, "1234567");
	assert(s->max == 12);
	str_append_cstr(s, "");
	str_append_cstr(s, "89012");
	assert(s->max == 12);
	str_append_cstr(s, "");
	assert(s->max == 12);
	str_append_cstr(s, "3");
	assert(s->max == 24);
}

int main(int argc, char *argv[]) {
	str_append_test();
	str_grow_test();
	return 0;
}
