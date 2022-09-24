#!/usr/bin/env tclsh
source virtterm.tcl

test -title "Chat room and message" -input {
	send "\r"
	send "This is a UTF-8 string. Café. 中文。苹果。Maçã. 睡觉。This is a UTF-8 string. Café."
	send $key_left
	send $key_left
	send X
}

test -title "Placing a character in between chinese characters." -input {
	for {set i 0} {$i < 30} {incr i} {
		send $key_left
	}
	send &
}
