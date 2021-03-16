#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hash.h"
#include "cache.h"
#include "matrix.h"
#include "rooms.h"
#include "utils.h"

bool do_matrix_send_token();
void do_matrix_login();
void alarm_handler();
void process_input(char *s);

bool logged_in = false;

int main(int argc, char *argv[]) {
	/* TODO: what if the access_token expires or is invalid? */
	if (!do_matrix_send_token())
		do_matrix_login();
	else {
		logged_in = true;
		/*
		 * TODO: while we don't store the servername in cache,
		 * hardcode the server we are testing against.
		 */
		matrix_set_server("matrix.org");
	}

	rooms_init();

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

bool do_matrix_send_token() {
	char *token = cache_get("access_token");
	if (!token)
		return false;
	matrix_set_token(token);
	return true;
}

void do_matrix_login() {
	char *id;
	char *password;

	fputs("Matrix ID: ", stderr);
	id = read_line();

	if (id[0] != '@') {
		fputs("Invalid Matrix ID\n", stderr);
		exit(1);
	}

	/* Consume '@' */
	id++;

	const char *user;
	const char *server;
	
	size_t offset;
	offset = strcspn(id, ":");

	if (id[offset] == '\0') {
		fputs("Invalid Matrix ID\n", stderr);
		exit(1);
	}

	id[offset] = '\0';
	user = id;
	server = id+offset+1;
	
	password = getpass("Password: ");

	matrix_login(server, user, password);
	memset(password, 0x0, strlen(password)); // TODO: is this optimized out?
}

void process_room_name(const char *id, const char *name) {
	char *i = strdup(id);
	char *n = strdup(name);
	room_new(i, n);
}

void process_msg(const char *roomid, const char *sender, const char *text) {
	Room *room = room_byid(roomid);;
	printf("%c[38;5;4m%s%c[m: %c[38;5;2m%s%c[m: %s\n",
		0x1b, room->name, 0x1b,
		0x1b, sender, 0x1b,
		text);
}

void process_input(char *s) {
	if (strcmp(s, "/quit") == 0)
		exit(0);
	char *s2;
	s2 = strtok(s, "=");
	if (s2 == NULL) {
		printf(">Input not sent<\n");
		return;
	}
	Room *room = room_byname(s2);
	if (!room) {
		printf(">Room not found<\n");
		return;
	}
	s = s2 + strlen(s2)+1;
	matrix_send_message(room->id, s);
}

void alarm_handler() {
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
			cache_set("access_token", ev->login.token);
			break;
		}
		matrix_free_event(ev);
	}
	if (logged_in)
		matrix_sync();
	alarm(5); /* 5 seconds */
}
