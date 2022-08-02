#undef NDEBUG
#include <assert.h>

#include "../src/str.c"
#include "../src/utils.c" /* TODO: recursive include! */

static void test_str_append_cstr() {
	Str *s = str_new_cstr("abc");
	str_set_utf8(s, true);
	assert(str_bytelen(s) == 3);
	assert(str_len(s) == 3);
	assert(str_buf(s)[3] == '\0');

	str_append_cstr(s, "def");
	assert(strcmp(str_buf(s), "abcdef") == 0);
	assert(str_bytelen(s) == 6);
	assert(str_len(s) == 6);
	assert(str_buf(s)[6] == '\0');

	str_append_cstr(s, "老师café");
	assert(streq(str_buf(s), "abcdef老师café"));
	assert(str_len(s) == 12);
}

static void test_grow() {
	Str *s = str_new_bytelen(3);
	assert(s->max == 3);
	str_append_cstr(s, "1234567");
	assert(s->max == 12);
	str_append_cstr(s, "");
	str_append_cstr(s, "89012");
	assert(s->max == 12);
	str_append_cstr(s, "");
	assert(s->max == 12);
}

static void test_str_insert_cstr() {
	Str *s = str_new_bytelen(3);
	str_append_cstr(s, "abc");
	assert(s->max == 3);
	assert(str_sc_eq(s, "abc"));

	str_insert_cstr(s, "x", 1);
	assert(s->max == 6);
	assert(str_sc_eq(s, "axbc"));

	str_insert_cstr(s, "yz", 3);
	assert(s->max == 6);
	assert(str_sc_eq(s, "axbyzc"));

	str_insert_cstr(s, "-老师café-", 3);
	str_set_utf8(s, true);
	assert(s->max == 24);
	assert(str_sc_eq(s, "axb-老师café-yzc"));
	assert(str_len(s) == 14);
}

static void test_str_remove_char_at() {
	Str *s = str_new_cstr("I am 老师。 I love café.");
	str_set_utf8(s, true);

	str_remove_char_at(s, 0); /* Remove I */
	str_remove_char_at(s, 0); /* Remove space */ /* Remove space */
	assert(str_sc_eq(s, "am 老师。 I love café."));
	assert(str_len(s) == 19);

	str_remove_char_at(s, 5); /* Remove 。*/
	str_remove_char_at(s, 3); /* Remove 老 */
	assert(str_sc_eq(s, "am 师 I love café."));
	assert(str_len(s) == 17);
}

static void test_str_copy_utf8char_at() {
	Str *s = str_new_cstr("I am 老师。 I love café.");
	char uc[5];

	str_copy_utf8char_at(s, 0, uc);
	assert(streq(uc, "I"));
	str_copy_utf8char_at(s, 5, uc);
	assert(streq(uc, "老"));
	str_copy_utf8char_at(s, 19, uc);
	assert(streq(uc, "é"));
	str_copy_utf8char_at(s, 20, uc);
	assert(streq(uc, "."));
}

int main(int argc, char *argv[]) {
	test_str_append_cstr();
	test_grow();
	test_str_insert_cstr();
	test_str_remove_char_at();
	test_str_copy_utf8char_at();
	return 0;
}
