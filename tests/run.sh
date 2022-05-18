#!/bin/sh
passed=0
failed=0

for f in $@; do
	this_failed=0
	radix="${f%.tcl.test}"
	case "$f" in
	*.tcl.test)
		tclsh "$f" > "$radix.result.tmp"
		;;
	esac

	diff -u "$radix.result" "$radix.result.tmp" || this_failed=1
	rm "$radix.result.tmp"

	if [ $this_failed -eq 0 ]; then
		passed=$((passed+1))
		echo ":-) $f passed"
	else
		passed=$((passed+1))
		echo ":-( $f failed"
	fi
done

echo "Failed: $failed	Passed: $passed"

