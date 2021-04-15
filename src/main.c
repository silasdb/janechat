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

Room *current_room = NULL;

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
		line = read_line_alloc();
		if (line)
			process_input(line);
		free(line);
	}
	
	return 0;
}

bool do_matrix_send_token() {
	char *token = cache_get_alloc("access_token");
	if (!token)
		return false;
	matrix_set_token(token);
	return true;
}

void do_matrix_login() {
	char *id;
	char *password;

	fputs("Matrix ID: ", stderr);
	id = read_line_alloc();

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

	free(id);
	// TODO: free(password)?
}

void process_room_name(const char *id, const char *name) {
	char *i = strdup(id);
	char *n = strdup(name);
	room_new(i, n);
}

void print_msg(const char *roomname, const char *sender, const char *text) {
	printf("%c[38;5;4m%s%c[m: %c[38;5;2m%s%c[m: %s\n",
		0x1b, roomname, 0x1b, 0x1b, sender, 0x1b, text);
}

void process_msg(const char *roomid, const char *sender, const char *text) {
	Room *room = room_byid(roomid);
	room_append_msg(room, strdup(sender), strdup(text));
	if (room == current_room)
		print_msg(room->name, sender, text);
	else
		room->unread_msgs++;
}

void print_messages(Room *room) {
	ROOM_MESSAGES_FOREACH(room, iter) {
		Msg *msg = (Msg *)iter;
		print_msg(room->name, msg->sender, msg->text);
	}
	room->unread_msgs = 0;
}

/*
 * For now, possible commands are:
 *
 * /quit -> exit this program
 * /stat -> list all rooms and number of unread messages
 * /join room -> change the current room
 * text -> send "text" to the current room
 */
void process_input(char *s) {
	if (strcmp(s, "/quit") == 0)
		exit(0);
	if (strcmp(s, "/stat") == 0) {
		ROOMS_FOREACH(iter) {
			Room *room = iter;
			printf("%s (unread messages: %zu)\n",
				room->name, room->unread_msgs);
		}
		return;
	}
	if (strncmp(s, "/join ", strlen("/join ")) == 0) {
		s += strlen("/join ");
		Room *room = room_byname(s);
		if (!room) {
			printf("Room \"%s\" not found.\n", s);
			return;
		}
		current_room = room;
		printf("Switched to room %s\n", s);
		print_messages(current_room);
		return;
	}
	if (*s == '\0')
		return;
	/*
	 * Prevent misspelled commands to be sent to current_room.  This has the
	 * drawback of not allowing user to send text that start with "/" (being
	 * necessary to prepend it with a trailing space
	 */
	if (*s == '/') {
		printf("Invalid command: %s\n", s);
		return;
	}
	if (!current_room) {
		puts("No room selected.  Text not sent.\n");
		return;
	}
	matrix_send_message(current_room->id, s);
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
		case EVENT_CONN_ERROR:
			//puts("Connection error.\n");
			break;
		}
		matrix_free_event(ev);
	}
	if (logged_in)
		matrix_sync();
	alarm(5); /* 5 seconds */
}
