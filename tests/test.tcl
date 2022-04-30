package require Expect

spawn ../src/ui-curses-test
set rows 24
set cols 80
stty rows $rows cols $cols < $spawn_out(slave,name)

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

foreach k [array names termdata] {
	puts "$k: $termdata($k)"
}
