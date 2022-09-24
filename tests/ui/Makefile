TARGETS = ui-curses-fake

-include ../../config.mk

all: ui-curses-fake
	sh run.sh *.test.tcl

ui-curses-fake: ui-curses-fake.c
	cc -DTEST_UI_CURSES ${CFLAGS} ${LDFLAGS} -o $@ $<

.PHONY: clean
clean:
	rm -f ${TARGETS}
