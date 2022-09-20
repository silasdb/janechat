#!/usr/bin/env tclsh
source virtterm.tcl

test -title "Add a long message" -input {
	send "\r"
	send "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
	send "1234567890_1234567890_1234567890_1234567890"
	send "\r"
}

force_resize 24 50

test -title "After resizing" -input {}

force_resize 6 50

test -title "Scroll up" -input {
	send [ctrl g]
	send "\r"
	send "ABC\r"
	send "123\r"
	send "abc\r"
	send "xyz\r"
	send "asd\r"
	send "qwe\r"
	send [ctrl b]
}

test -title "Scroll up, down, down" -input {
	send [ctrl f]
	send [ctrl f]
	send "foo\n"
}

test -title "Scroll up, down, input" -input {
	send [ctrl b]
	send [ctrl f]
	send "bar\n"
}
