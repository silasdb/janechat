package require Expect

# Instead of implementing sequences for modern terminals, we prefer to set a new
# terminfo database with only control sequences that matter for us. This is
# heavily inspired by expect's tkterm example:
# https://core.tcl-lang.org/expect/file?name=example/tkterm&ci=tip
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

proc term_clear {} {
	for {set row 0} {$row < $::rows} {incr row} {
		set ::termdata($row) ""
	}
}

proc term_text_append {text} {
	set ::termdata($::row) [string range $::termdata($::row) 0 \
		[expr {$::column - 1}]]
	incr ::column [string length $text]
	append ::termdata($::row) $text
}

proc term_set_cursor {row column} {
	set ::row $row
	set ::column $column
}

log_user 0
set row 0
set column 0
set timeout 1

# Initialize ::termdata array
term_clear

expect_background {
	-re "^\[^\x01-\x1f]+" {
		term_text_append $expect_out(0,string)
	} "^\n" {
		exit 1
	} "^\r" {
		set ::column 0
	} -re "^\x1bCURSOR_ADDRESS;\[0-9\]+;\[0-9\]+\x1b" {
		regexp "^\x1bCURSOR_ADDRESS;(\[0-9\]+);(\[0-9\]+)\x1b" \
			$expect_out(0,string) -> row column
		term_set_cursor $row $column
	} "^\x1bCLEAR\x1b" {
		term_clear
	} -re ".*" {
		# Unrecognized patters are printed char by char so we can debug
		# special cases
		set t $expect_out(0,string)
		for {set i 0} {$i < [string length $t]} {incr i} {
			set c [string range $t $i $i]
			scan $c %c v
			puts "$i: $c: $v"
		}
		exit 1
	}
}

proc iter {} {
	after 100 {set ::cycle 1}
	vwait ::cycle
	unset ::cycle
}

iter

send "\r"
send "abc"
send "\r"
send "a b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 A B C D E F G H I J K L M"
iter

foreach-row {i line} {
	puts $i:$line
}
