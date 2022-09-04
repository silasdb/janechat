#!/usr/bin/env tclsh
source virtterm.tcl

test -title "Main window" -input {}

test -title "Simple input test" -input {
	send "\r"
	send "a b c"
}

test -title "Now adding string so it becomes wider than the terminal, testing key_left and backspace" -input {
	send " d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 0 A B C D E"
	send $key_left
	send "\b"
}

test -title "Sending it as a message" -input {send "\r"}

test -title "Go back to the index. Search for room \"Test C\" and open it" -input {
	send \x07; # CTRL('g')
	send "/test c"
	send "\r"
	send "\r"
}
