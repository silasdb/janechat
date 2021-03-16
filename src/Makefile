-include ../config.mk

CC ?= gcc
CFLAGS += -std=c11 -Wall -Wpedantic

janechat: cache.o strbuf.o hash.o list.o main.o matrix.o utils.o
	$(CC) ${LDFLAGS} -o $@ \
		cache.o \
		hash.o \
		list.o \
		main.o \
		matrix.o \
		strbuf.o \
		utils.o

hash.o: hash.c hash.h list.h
	$(CC) ${CFLAGS} -c -o hash.o hash.c

list.o: list.c list.h
	$(CC) ${CFLAGS} -c -o list.o list.c

strbuf.o: strbuf.c strbuf.h
	$(CC) ${CFLAGS} -c -o strbuf.o strbuf.c

cache.o: cache.h cache.c
	$(CC) ${CFLAGS} -c -o cache.o cache.c

main.o: cache.h hash.h strbuf.h
	$(CC) ${CFLAGS} -c -o main.o main.c

matrix.o: list.h matrix.h strbuf.h utils.h
	$(CC) ${CFLAGS} -c -o matrix.o matrix.c

utils.o: utils.c utils.h
	$(CC) ${CFLAGS} -c -o utils.o utils.c

tags:
	# Execute exctags, if not found, execute old ctags.
	if ! exctags *.h *.c; then	\
		ctags -dt *.h *.c;	\
	fi

.PHONY: clean
clean:
	rm -f janechat janechat.core *.o tags
