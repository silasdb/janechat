package require Expect

# References:
# https://espterm.github.io/docs/VT100%20escape%20codes.html
# https://unix.stackexchange.com/questions/288962/what-does-1049h-and-1h-ansi-escape-sequences-do
# https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
# https://shallowsky.com/blog/linux/noaltscreen.html
# https://shallowsky.com/linux/noaltscreen.html

# These are the first bytes of output to the terminal when opening janechat
#
# 00000000: 1b5b 3f31 3034 3968 1b5b 3f31 326c 1b5b  .[?1049h.[?12l.[
# 00000010: 3f32 3568 1b5b 481b 5b32 4a1b 5b3f 3168  ?25h.[H.[2J.[?1h
# 00000020: 1b3d 1b5b 3339 3b34 396d 1b5b 3339 3b34  .=.[39;49m.[39;4
# 00000030: 396d 1b5b 376d 5465 7374 2041 2028 3029  9m.[7mTest A (0)
# 00000040: 1b5b 480a 1b28 421b 5b6d 1b5b 3339 3b34  .[H..(B.[m.[39;4
# 00000050: 396d 5465 7374 2042 2028 3029 0d0a 5465  9mTest B (0)..Te
# 00000060: 7374 2043 2028 3029 1b5b 3339 3b34 396d  st C (0).[39;49m
# 00000070: 1b5b 3339 3b34 396d 1b5b 481b 5b32 4a1b  .[39;49m.[H.[2J.
# 00000080: 5b33 393b 3439 6d1b 5b37 6d54 6573 7420  [39;49m.[7mTest 
# 00000090: 4120 2830 291b 5b48 0a1b 2842 1b5b 6d1b  A (0).[H..(B.[m.
# 000000a0: 5b33 393b 3439 6d54 6573 7420 4220 2830  [39;49mTest B (0
# 000000b0: 290d 0a0a                                )...

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
	}
	-re {.*\n} {
		# Should delete control-characters before printing it to the screen
		puts newline
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
