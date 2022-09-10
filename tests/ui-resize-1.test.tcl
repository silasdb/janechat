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
