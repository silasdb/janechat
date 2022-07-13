#include <setjmp.h>
#include <unistd.h>
#include <stdarg.h>
#include <cmocka.h>

#include "../src/str.c"

static void str_append_test(void **state) {
	Str *s = str_new_cstr("abc");
	str_append_cstr(s, "def");
	assert_true(strcmp(str_buf(s), "abcdef") == 0);
	assert_true(str_len(s) == 6);
}

int main(int argc, char *argv[]) {
	int rc;
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(str_append_test),
	};

	cmocka_set_message_output(CM_OUTPUT_SUBUNIT);

	rc = cmocka_run_group_tests(tests, NULL, NULL);
	return rc;
}
