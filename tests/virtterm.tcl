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
	cursor_right=\ECURSOR_RIGHT\E,
	key_left=\EKEY_LEFT\E,
	key_right=\EKEY_RIGHT\E,
	key_backspace=^H,
}
close $file
exec tic $terminfo_src

log_user 0
spawn ../src/test-ui-curses
set rows 24
set cols 80
stty rows $rows cols $cols < $spawn_out(slave,name)

proc debug {args} {
	set o {}
	for {set i 0} {$i < [llength $args]} {incr i} {
		set a [lindex $args $i]
		upvar $a v
		append o "[lindex $args $i]: $v"
		if {$i < [llength $args]-1} {
			append o ", "
		}
	}
	puts $o
}

proc assert {cond} {
	if {![uplevel 1 [list expr $cond]]} {
		return -code error -errorinfo {assertion $cond failed}
	}
}

proc term_clear {} {
	for {set row 0} {$row < $::rows} {incr row} {
		set ::termdata($row) ""
	}
}

proc wcwidth {ch} {
	if {$ch in [list 中 文 平 果 睡 觉 。]} {
		return 2
	}
	return 1
}


proc foreach_char {var str body} {
	for {set i 0} {$i < [string length $str]} {incr i} {
		upvar $var v
		set v [string range $str $i $i]
		uplevel 1 $body
	}
}

proc string_column_range {str start end} {
	if {$end < $start} {
		return {}
	}
	set i 0
	set si 0
	foreach_char ch $str {
		if {$si == $start} {
			break
		}
		if {$si > $end} {
			incr i -1
			break
		}
		incr i
		incr si [wcwidth $ch]
	}
	set j 0
	set sj 0
	foreach_char ch $str {
		if {$sj == $end}  {
			break
		}
		if {$sj > $end} {
			incr j -1
			break
		}
		incr j
		incr sj [wcwidth $ch]
	}
	return [string range $str $i $j]
}

set test "a中b文睡觉abc"
assert {[string_column_range $test 0 0] eq "a"}
assert {[string_column_range $test 0 1] eq "a中"}
assert {[string_column_range $test 0 2] eq "a中"}
assert {[string_column_range $test 0 3] eq "a中b"}
assert {[string_column_range $test 1 1] eq "中"}
assert {[string_column_range $test 2 2] eq "中"}
assert {[string_column_range $test 3 4] eq "b文"}
#assert {[string_column_range $test 0 0] eq "a"}
#assert {[string_column_range $test 0 1] eq "a"}
#assert {[string_column_range $test 0 2] eq "a中"}
#assert {[string_column_range $test 0 3] eq "a中b"}
#assert {[string_column_range $test 1 1] eq ""}
#assert {[string_column_range $test 2 2] eq ""}
#assert {[string_column_range $test 3 4] eq "b"}

proc term_text_append {text} {
	set ::termdata($::row) [string_column_range $::termdata($::row) 0 \
		[expr {$::column - 1}]]
	incr ::column [string length $text]
	append ::termdata($::row) $text
}

proc term_set_cursor {row column} {
	set ::row $row
	set ::column $column
}

set row 0
set column 0

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
	} "^\x1bCURSOR_RIGHT\x1b" {
		term_text_append " "
	} "^\x1bCLEAR\x1b" {
		term_clear
	}
}

proc iter {} {
	set ::cycle 1
	after 100 {set ::cycle 1}
	vwait ::cycle
}

proc printterm {title} {
	iter
	puts "TITLE: $title"
	for {set row 0} {$row < 24} {incr row} {
		# ncurses likes to insert spaces to clear older data. In order
		# not to have trailing spaces everywhere, we trim them off.
		puts [string trimright $::termdata($row)]
	}
}

set key_left "\x1bKEY_LEFT\x1b"
set key_right "\x1bKEY_RIGHT\x1b"

iter
