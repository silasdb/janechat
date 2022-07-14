#undef NDEBUG
#include <assert.h>

#include "../src/str.c"

static void str_append_test() {
	Str *s = str_new_cstr("abc");
	str_append_cstr(s, "def");
	assert(strcmp(str_buf(s), "abcdef") == 0);
	assert(str_len(s) == 6);
}

int main(int argc, char *argv[]) {
	str_append_test();
	return 0;
}
