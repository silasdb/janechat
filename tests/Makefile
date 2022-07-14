TARGETS = str.test ui-curses-fake
CFLAGS += -g

.PHONY: test check
check: test
test: ${TARGETS}
	sh run.sh *.test.c *.test.tcl

-include ../config.mk

str.test: str.test.c
	cc ${CFLAGS} ${LDFLAGS} -o $@ str.test.c

ui-curses-fake: ui-curses-fake.c
	cc -DTEST_UI_CURSES ${CFLAGS} ${LDFLAGS} -o $@ $<

.PHONY: clean
clean:
	rm -f ${TARGETS}
