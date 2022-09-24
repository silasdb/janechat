#!/bin/sh
set -eu
run_test () {
	radix="${f%.test.tcl}"
	tclsh "$f" > "$radix.result.tmp"
	diff -u "$radix.result" "$radix.result.tmp" || this_failed=1
	rm "$radix.result.tmp"
}
. ../common.sh
