-include ../config.mk

CC ?= gcc
CFLAGS += -std=c11 -Wall -Wpedantic

janechat: cache.o strbuf.o hash.o main.o matrix.o utils.o
	$(CC) ${LDFLAGS} -o $@ \
		cache.o \
		hash.o \
		main.o \
		matrix.o \
		strbuf.o \
		utils.o

hash.o: hash.c hash.h
	$(CC) ${CFLAGS} -c -o hash.o hash.c

strbuf.o: strbuf.c strbuf.h
	$(CC) ${CFLAGS} -c -o strbuf.o strbuf.c

cache.o: cache.h cache.c
	$(CC) ${CFLAGS} -c -o cache.o cache.c

main.o: cache.h hash.h strbuf.h
	$(CC) ${CFLAGS} -c -o main.o main.c

matrix.o: matrix.h strbuf.h utils.h
	$(CC) ${CFLAGS} -c -o matrix.o matrix.c

utils.o: utils.c utils.h
	$(CC) ${CFLAGS} -c -o utils.o utils.c

.PHONY: clean
clean:
	rm -f janechat janechat.core *.o
