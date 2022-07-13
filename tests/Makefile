TARGETS = str.test ui-curses-fake

.PHONY: test check
check: test
test: ${TARGETS}
	sh run.sh *.test.*

-include ../config.mk

str.test: str.test.c
	cc ${CFLAGS} ${LDFLAGS} -o $@ str.test.c

UI_CURSES_FAKE_DEPS = ../src/hash.c \
	../src/utils.c \
	../src/list.c \
	../src/rooms.c \
	../src/str.c \
	../src/ui.c \
	../src/vector.c \
	../src/ui-curses.c

ui-curses-fake: ${UI_CURSES_FAKE_DEPS}
	cc -DTEST_UI_CURSES ${CFLAGS} ${LDFLAGS} -o $@ ${UI_CURSES_FAKE_DEPS}

.PHONY: clean
clean:
	rm -f ${TARGETS}
