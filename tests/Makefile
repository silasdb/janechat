TARGETS = str-test

.PHONY: test check
check: test
test: ${TARGETS}
	sh run.sh *.test

-include ../config.mk

str-test: str-test.c
	cc ${CFLAGS} ${LDFLAGS} -o $@ str-test.c

.PHONY: clean
clean:
	rm -f ${TARGETS}
