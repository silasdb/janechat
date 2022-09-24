TARGETS = str.test

-include ../../config.mk

all: ${TARGETS}
	sh run.sh *.test.c

str.test: str.test.c
	cc ${CFLAGS} ${LDFLAGS} -o $@ str.test.c

.PHONY: clean
clean:
	rm -f ${TARGETS}

