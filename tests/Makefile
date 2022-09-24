TARGETS_UNIT = str.unit.test
TARGETS_UI = ui-curses-fake
TARGETS = ${TARGETS_UNIT} ${TARGETS_UI}

SANITIZER_FLAGS  = -fsanitize=address
SANITIZER_FLAGS += -fsanitize=undefined
SANITIZER_FLAGS += -fsanitize=signed-integer-overflow
SANITIZER_FLAGS += -fno-sanitize-recover=all

CFLAGS += -g  -std=c11 -Wall -Wextra -Wpedantic -fno-omit-frame-pointer 
#CFLAGS += ${SANITIZER_FLAGS}
#LDFLAGS += ${SANITIZER_FLAGS}

.PHONY: test check
check: unit
test: unit
all: unit ui

.PHONY: unit
unit: ${TARGETS_UNIT}
	@echo '=> Running unit tests...'
	sh run.sh *.unit.test.c

.PHONY: ui
ui: ${TARGETS_UI}
	@echo '=> Running UI tests...'
	sh run.sh *.ui.test.tcl

-include ../config.mk

str.test: str.unit.test.c
	cc ${CFLAGS} ${LDFLAGS} -o $@ str.unit.test.c

ui-curses-fake: ui-curses-fake.c
	cc -DTEST_UI_CURSES ${CFLAGS} ${LDFLAGS} -o $@ $<

.PHONY: clean
clean:
	rm -f ${TARGETS} *.core
