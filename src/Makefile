LDFLAGS += -lasan -g

-include ../config.mk

# TODO: split debug / release build rules
CC ?= gcc
CFLAGS += -fsanitize=address -g -std=c11 -Wall -Wpedantic
LDFLAGS += -lasan

janechat: cache.o strbuf.o hash.o list.o main.o matrix.o rooms.o utils.o
	$(CC) ${LDFLAGS} -o $@ \
		cache.o \
		hash.o \
		list.o \
		main.o \
		matrix.o \
		rooms.o \
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

main.o: main.c  cache.h hash.h strbuf.h
	$(CC) ${CFLAGS} -c -o main.o main.c

matrix.o: matrix.c  list.h matrix.h strbuf.h utils.h
	$(CC) ${CFLAGS} -c -o matrix.o matrix.c

rooms.o: hash.h list.h rooms.c rooms.h
	$(CC) ${CFLAGS} -c -o rooms.o rooms.c
	
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
