#undef NDEBUG
#include <assert.h>

#include "../../src/str.c"
#include "../../src/utils.c" /* TODO: recursive include! */

static void test_str_new() {
	Str *s;
	s = str_new_cstr_fixed("foo");
	assert(str_bytelen(s) == strlen(str_buf(s)));
	assert(str_bytelen(s) == s->max);
}

static void test_str_append_cstr() {
	Str *s = str_new_cstr("abc");
	assert(str_bytelen(s) == 3);
	assert(str_utf8len(s) == 3);
	assert(str_buf(s)[3] == '\0');

	str_append_cstr(s, "def");
	assert(strcmp(str_buf(s), "abcdef") == 0);
	assert(str_bytelen(s) == 6);
	assert(str_utf8len(s) == 6);
	assert(str_buf(s)[6] == '\0');

	str_append_cstr(s, "老师café");
	assert(streq(str_buf(s), "abcdef老师café"));
	assert(str_utf8len(s) == 12);
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

static void test_str_insert_utf8char_at() {
	Str *s = str_new_bytelen(3);
	str_append_cstr(s, "abc");
	assert(s->max == 3);
	assert(str_sc_eq(s, "abc"));

	str_insert_utf8char_at(s, (Utf8Char){.c = "x",}, UTF8_INDEX(1));
	assert(s->max == 6);
	assert(str_sc_eq(s, "axbc"));

	str_insert_utf8char_at(s, (Utf8Char){.c = "y",}, UTF8_INDEX(3));
	str_insert_utf8char_at(s, (Utf8Char){.c = "z",}, UTF8_INDEX(4));
	assert(s->max == 6);
	assert(str_sc_eq(s, "axbyzc"));

	str_insert_utf8char_at(s, (Utf8Char){.c = "-",}, UTF8_INDEX(3));
	str_insert_utf8char_at(s, (Utf8Char){.c = "老",}, UTF8_INDEX(4));
	str_insert_utf8char_at(s, (Utf8Char){.c = "师",}, UTF8_INDEX(5));
	str_insert_utf8char_at(s, (Utf8Char){.c = "c",}, UTF8_INDEX(6));
	str_insert_utf8char_at(s, (Utf8Char){.c = "a",}, UTF8_INDEX(7));
	str_insert_utf8char_at(s, (Utf8Char){.c = "f",}, UTF8_INDEX(8));
	str_insert_utf8char_at(s, (Utf8Char){.c = "é",}, UTF8_INDEX(9));
	str_insert_utf8char_at(s, (Utf8Char){.c = "-",}, UTF8_INDEX(10));
	assert(s->max == 24);
	assert(str_sc_eq(s, "axb-老师café-yzc"));
	assert(str_utf8len(s) == 14);

	str_reset(s);
	str_append_cstr(s, "çaç");
	str_insert_utf8char_at(s, (Utf8Char){.c = "a"}, UTF8_INDEX(3));
	assert(str_sc_eq(s, "çaça"));
}

static void test_str_remove_utf8char_at() {
	Str *s = str_new_cstr("I am 老师。 I love café.");

	str_remove_utf8char_at(s, UTF8_INDEX(0)); /* Remove I */
	str_remove_utf8char_at(s, UTF8_INDEX(0)); /* Remove space */ /* Remove space */
	assert(str_sc_eq(s, "am 老师。 I love café."));
	assert(str_utf8len(s) == 19);

	str_remove_utf8char_at(s, UTF8_INDEX(5)); /* Remove 。*/
	str_remove_utf8char_at(s, UTF8_INDEX(3)); /* Remove 老 */
	assert(str_sc_eq(s, "am 师 I love café."));
	assert(str_utf8len(s) == 17);
}

static void test_str_utf8char_at() {
	Str *s = str_new_cstr("I am 老师。 I love café.");
	Utf8Char uc;

	uc = str_utf8char_at(s, UTF8_INDEX(0));
	assert(streq(uc.c, "I"));
	uc = str_utf8char_at(s, UTF8_INDEX(5));
	assert(streq(uc.c, "老"));
	uc = str_utf8char_at(s, UTF8_INDEX(19));
	assert(streq(uc.c, "é"));
	uc = str_utf8char_at(s, UTF8_INDEX(20));
	assert(streq(uc.c, "."));

	str_reset(s);
	uc = str_utf8char_at(s, UTF8_INDEX(5));
	assert(streq(uc.c, ""));

	str_decref(s);
}

static void test_str_starts_with_cstr() {
	Str *s = str_new_cstr("I am 老师。 I love café.");

	assert(str_starts_with_cstr(s, "I am"));
	assert(str_starts_with_cstr(s, "I am 老师。"));
	assert(!str_starts_with_cstr(s, "am 老师。"));
}

void test_str_reset(void) {
	Str *s = str_new_cstr("I am 老师。 I love café.");
	str_reset(s);
	assert(str_utf8len(s) == 0);
	assert(s->bytelen == 0);
}

void test_str_dup(void) {
	Str *s = str_new_cstr("abc");
	Str *t = str_dup(s);
	assert(streq(str_buf(s), str_buf(t)));
	assert(str_buf(s) != str_buf(t));
}

void test_str_utf8char_foreach() {
	Str *s = str_new_cstr("I am 老师。 I love café.");
	size_t i;
	Utf8Char uc;
	int count = 0;
	STR_UTF8CHAR_FOREACH(s, uc, i) {
		count++;
	}
	assert(count == 21);
	str_decref(s);
}

int main(int argc, char *argv[]) {
	test_str_new();
	test_str_append_cstr();
	test_grow();
	test_str_insert_utf8char_at();
	test_str_remove_utf8char_at();
	test_str_utf8char_at();
	test_str_starts_with_cstr();
	test_str_reset();
	test_str_dup();
	test_str_utf8char_foreach();
	return 0;
}
