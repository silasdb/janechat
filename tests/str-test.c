#include <setjmp.h>
#include <unistd.h>
#include <stdarg.h>
#include <cmocka.h>

#include "../src/str.c"

static void str_test_append(void **state) {
	assert_true(true);
}

int main(int argc, char *argv[]) {
	int rc;
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(str_test_append),
	};

	cmocka_set_message_output(CM_OUTPUT_SUBUNIT);

	rc = cmocka_run_group_tests(tests, NULL, NULL);
	return rc;
}
