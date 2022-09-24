#!/bin/sh
set -eu

passed=0
failed=0

for f in $@; do
	this_failed=0
	run_test "$f"

	if [ $this_failed -eq 0 ]; then
		passed=$((passed+1))
		echo ":-) $f passed"
	else
		failed=$((failed+1))
		echo ":-( $f failed"
	fi
done

echo "Failed: $failed	Passed: $passed"

