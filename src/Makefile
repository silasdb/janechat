OBJS = cache.o \
	hash.o \
	list.o \
	main.o \
	matrix.o \
	rooms.o \
	str.o \
	ui.o \
	ui-cli.o \
	ui-curses.o \
	utils.o \
	vector.o

-include ../config.mk

# TODO: split debug / release build rules
CC ?= gcc
CFLAGS += -std=gnu11 -Wall -Wextra -Wpedantic -fno-omit-frame-pointer
CFLAGS += ${SANITIZER_FLAGS}
LDFLAGS += ${SANITIZER_FLAGS}

janechat: ${OBJS}
	# TODO: why the following line works NetBSD but not on Alpine
	# Linux?
	# $(CC) ${LDFLAGS} -o $@ ${OBJS}
	$(CC) -o $@ ${OBJS} ${LDFLAGS} -lc

hash.o: hash.c hash.h list.h
	$(CC) ${CFLAGS} -c -o hash.o hash.c

list.o: list.c list.h
	$(CC) ${CFLAGS} -c -o list.o list.c

str.o: str.c str.h
	$(CC) ${CFLAGS} -c -o str.o str.c

cache.o: cache.h cache.c
	$(CC) ${CFLAGS} -c -o cache.o cache.c

main.o: main.c cache.h hash.h str.h ui.h 
	$(CC) ${CFLAGS} -c -o main.o main.c

matrix.o: matrix.c  list.h matrix.h str.h utils.h
	$(CC) ${CFLAGS} -c -o matrix.o matrix.c

rooms.o: hash.h list.h rooms.c rooms.h
	$(CC) ${CFLAGS} -c -o rooms.o rooms.c

ui.o: ui.h
	$(CC) ${CFLAGS} -c -o ui.o ui.c

ui-cli.o: rooms.h ui-cli.c ui-cli.h utils.h ui.h
	$(CC) ${CFLAGS} -c -o ui-cli.o ui-cli.c

ui-curses.o: rooms.h str.h ui-curses.c ui-curses.h ui.h vector.h
	$(CC) ${CFLAGS} -c -o ui-curses.o ui-curses.c
	
utils.o: utils.c utils.h
	$(CC) ${CFLAGS} -c -o utils.o utils.c

vector.o: vector.c vector.h
	$(CC) ${CFLAGS} -c -o $@ vector.c

tags:
	# Execute exctags, if not found, execute old ctags.
	if ! exctags *.h *.c; then	\
		ctags -dt *.h *.c;	\
	fi

.PHONY: clean
clean:
	rm -f janechat janechat.core *.o tags
