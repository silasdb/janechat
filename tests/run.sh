#!/bin/sh
set -eu

passed=0
failed=0

for f in $@; do
	this_failed=0
	case "$f" in
	*.test.tcl)
		radix="${f%.test.tcl}"
		tclsh "$f" > "$radix.result.tmp"
		diff -u "$radix.result" "$radix.result.tmp" || this_failed=1
		rm "$radix.result.tmp"
		;;
	*.test.c)
		./"${f%.c}" >/dev/null || this_failed=1
		;;
	esac

	if [ $this_failed -eq 0 ]; then
		passed=$((passed+1))
		echo ":-) $f passed"
	else
		passed=$((passed+1))
		echo ":-( $f failed"
	fi
done

echo "Failed: $failed	Passed: $passed"

