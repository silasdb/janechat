.PHONY: test check
check: test
test: str-test
	sh run.sh *.test

-include ../config.mk

str-test: str-test.c
	cc ${CFLAGS} ${LDFLAGS} -o $@ str-test.c
