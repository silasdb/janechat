package require Expect

# References:
# https://espterm.github.io/docs/VT100%20escape%20codes.html
# https://unix.stackexchange.com/questions/288962/what-does-1049h-and-1h-ansi-escape-sequences-do
# https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
# https://shallowsky.com/blog/linux/noaltscreen.html
# https://shallowsky.com/linux/noaltscreen.html

# Instead of implementing sequences for modern terminals, we prefer to set a new
# terminfo database with only control sequences that matter for us.
#
# TODO: ncurses can execute programs with minimal or even empty terminfo
# definitions, but NetBSD curses refuse to start the program. Does NetBSD curses
# require a minimal set of definitions?
set env(TERMINFO) /tmp
set env(TERM) virtterm
set terminfo_src "/tmp/virtterm.src"
set file [open $terminfo_src w]
puts $file {virtterm|virtual terminal emulator,
	clear=\ECLEAR\E,
	scroll_forward=\n,
	carriage_return=\r,
	cursor_address=\ECURSOR_ADDRESS;%d;%d\E,
}
close $file
exec tic $terminfo_src

spawn ../src/ui-curses-test
set rows 24
set cols 80
stty rows $rows cols $cols < $spawn_out(slave,name)

proc foreach-row {var body} {
	if {[llength $var] == 1} {
		upvar 1 $var r
	} else {
		upvar 1 [lindex $var 0] row
		upvar 1 [lindex $var 1] r
	}
	for {set row 0} {$row < $::rows} {incr row} {
		set r $::termdata($row)
		uplevel 1 $body
	}
}

for {set row 0} {$row < $rows} {incr row} {
	set termdata($row) ""
}

log_user 0
set row 0
set timeout 1

expect_before {
	timeout {
		set got_timeout 1
	} -re "^\[^\x01-\x1f]+" {
		puts text
	} -re {.*\n} {
		# Should delete control-characters before printing it to the screen
		puts newline
	} -re "^\x1bCURSOR_ADDRESS;.*?;.*?\x1b" {
		puts cursor_address
	} "^\x1bCLEAR\x1b" {
		puts clear
	}
}

set got_timeout 0

set row 0
while {!$got_timeout} {
	puts expect
	expect
	set termdata($row) $expect_out(0,string)
	incr row
}

send "\r"
send "abc"

foreach-row {i line} {
	puts $i:$line
}
