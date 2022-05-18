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
	key_left=\EKEY_LEFT\E,
	key_right=\EKEY_RIGHT\E,
	key_backspace=^H,
}
close $file
exec tic $terminfo_src

log_user 0
spawn ../src/ui-curses-test
set rows 24
set cols 80
stty rows $rows cols $cols < $spawn_out(slave,name)

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
		puts $::termdata($row)
	}
}

set key_left "\x1bKEY_LEFT\x1b"
set key_right "\x1bKEY_RIGHT\x1b"

iter
