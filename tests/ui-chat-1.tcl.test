#!/usr/bin/env tclsh
source virtterm.tcl

printterm "Main window"

send "\r"
send "a b c"
printterm "Simple input test"

send " d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 0 A B C D E"
send $key_left
send "\b"
printterm "Now adding string so it becomes wider than the terminal, testing key_left and backspace"

send "\r"
printterm "Sending it as a message"
