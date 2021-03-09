-include ../config.mk

CC ?= gcc
CFLAGS += -std=c11 -Wall -Wpedantic

janechat: Hash.o StrBuf.o cache.o main.o matrix.o utils.o
	$(CC) ${LDFLAGS} -o $@ \
		Hash.o \
		StrBuf.o \
		cache.o \
		main.o \
		matrix.o \
		utils.o

Hash.o: Hash.c Hash.h
	$(CC) ${CFLAGS} -c -o Hash.o Hash.c

StrBuf.o: StrBuf.c StrBuf.h
	$(CC) ${CFLAGS} -c -o StrBuf.o StrBuf.c

cache.o: cache.h cache.c
	$(CC) ${CFLAGS} -c -o cache.o cache.c

main.o: Hash.h StrBuf.h cache.h
	$(CC) ${CFLAGS} -c -o main.o main.c

matrix.o: matrix.h StrBuf.h utils.h
	$(CC) ${CFLAGS} -c -o matrix.o matrix.c

utils.o: utils.c utils.h
	$(CC) ${CFLAGS} -c -o utils.o utils.c

.PHONY: clean
clean:
	rm -f janechat janechat.core *.o
