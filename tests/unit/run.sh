#!/bin/sh
set -eu
run_test () {
	./"${1%.c}" >/dev/null || this_failed=1
}
. ../common.sh 


