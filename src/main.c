#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Hash.h"

#include "matrix.h"
#include "utils.h"

Hash *roomnames;
Hash *roomids;

void alarm_handler();
void process_input(char *s);
void usage();

bool logged_in = false;

int
main(int argc, char *argv[])
{
	if (argc != 2)
		usage();

	if (argv[1][0] != '@')
		usage();

	/* Consume '@' */
	argv[1]++;

	const char *user;
	const char *server;
	
	size_t offset;
	offset = strcspn(argv[1], ":");

	if (*(argv[1]+offset) == '\0')
		usage();

	argv[1][offset] = '\0';
	user = argv[1];
	server = argv[1]+offset+1;
	char *password;
	password = getpass("Password: ");

	matrix_login(server, user, password);
	memset(password, 0x0, strlen(password)); // TODO: is this optimized out?
	free(password);
	
	roomnames = hash_new();
	roomids = hash_new();

	signal(SIGALRM, alarm_handler);
	alarm_handler();

	char *line;
	for (;;) {
		line = read_line();
		process_input(line);
		free(line);
	}
	
	return 0;
}

const char *
roomid2alias(const char *id)
{
	const char *v = hash_get(roomnames, id);
	assert(v != NULL); // generate a core dump
	return v;
}

const char *
roomname2roomid(const char *name)
{
	return hash_get(roomids, name);
}

void
process_room_name(const char *id, const char *name)
{
	char *i = strdup(id);
	char *n = strdup(name);
	hash_insert(roomids, n, i);
	hash_insert(roomnames, i, n);
}

void
process_msg(const char *roomid, const char *sender, const char *text)
{
	const char *roomname = roomid2alias(roomid);
	printf("%c[38;5;4m%s%c[m: %c[38;5;2m%s%c[m: %s\n",
		0x1b, roomname, 0x1b,
		0x1b, sender, 0x1b,
		text);
}

void
process_input(char *s)
{
	if (strcmp(s, "/quit") == 0)
		exit(0);
	char *s2;
	s2 = strtok(s, "=");
	if (s2 == NULL) {
		printf(">Input not sent<\n");
		return;
	}
	const char *roomid = roomname2roomid(s2);
	if (!roomid) {
		printf(">Room not found<\n");
		return;
	}
	s = s2 + strlen(s2)+1;
	matrix_send_message(roomid, s);
}

void
usage()
{
	fprintf(stderr, "usage: janechat @username:server\n");
	exit(1);
}

void
alarm_handler()
{
	MatrixEvent *ev;
	while ((ev = matrix_next_event()) != NULL) {
		switch (ev->type) {
		case EVENT_ROOM:
			process_room_name(ev->room.id, ev->room.name);
			break;
		case EVENT_MSG:
			process_msg(ev->msg.roomid, ev->msg.sender, ev->msg.text);
			break;
		case EVENT_ERROR:
			printf("%s\n", ev->error.error);
			exit(1);
			break;
		case EVENT_LOGGED_IN:
			logged_in = true;
			break;
		}
		matrix_free_event(ev);
	}
	if (logged_in)
		matrix_sync();
	alarm(5); /* 5 seconds */
}
