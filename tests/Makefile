TARGETS = str.test ui-curses-fake

SANITIZER_FLAGS  = -fsanitize=address
SANITIZER_FLAGS += -fsanitize=undefined
SANITIZER_FLAGS += -fsanitize=signed-integer-overflow
SANITIZER_FLAGS += -fno-sanitize-recover=all

CFLAGS += -g  -std=c11 -Wall -Wextra -Wpedantic -fno-omit-frame-pointer 
#CFLAGS += ${SANITIZER_FLAGS}
#LDFLAGS += ${SANITIZER_FLAGS}

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
