#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "hash.h"
#include "cache.h"
#include "matrix.h"
#include "rooms.h"
#include "ui.h"
#include "ui-cli.h"
#include "utils.h"

bool do_matrix_send_token();
void do_matrix_login();
void handle_matrix_event(MatrixEvent ev);
void handle_ui_event(UiEvent ev);

bool logged_in = false;

struct ui_hooks {
	void (*iter)();
	void (*new_msg)();
} ui_hooks;

int main(int argc, char *argv[]) {
	ui_set_event_handler(handle_ui_event);
	ui_hooks = (struct ui_hooks){
		.iter = ui_cli_iter,
		.new_msg = ui_cli_new_msg,
	};
	matrix_set_event_handler(handle_matrix_event);

	/* TODO: what if the access_token expires or is invalid? */
	if (!do_matrix_send_token())
		do_matrix_login();
		/*
		 * do_matrix_login() asks for user's id and password and, using
		 * matrix.c API, sends it to the matrix server, but it happens
		 * asynchronously, i.e., it is set only after we EVENT_LOGGED_IN
		 * is received.  So, we cannot call matrix_sync() right now and
		 * have to wait until logged_in variable is set to true.
		 */
	else {
		logged_in = true;
		/*
		 * TODO: while we don't store the servername in cache,
		 * hardcode the server we are testing against.
		 */
		matrix_set_server("matrix.org");
		matrix_sync();
	}

	rooms_init();

	for (;;) {
		switch (select_matrix_stdin()) {
		case SELECTSTATUS_STDINREADY:
			ui_hooks.iter();
			break;
		case SELECTSTATUS_MATRIXRESUME:
			matrix_resume();
			break;
		}
		if (logged_in) {
			static time_t past = 0, now = 0;
			now = time(0);
			if (now > past + 1) {
				matrix_sync();
				past = now;
			}
		}
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
	char *id, *ptr;
	char *password;

	fputs("Matrix ID: ", stderr);
	ptr = read_line_alloc();

	id = ptr;

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

	puts("Logging in...");
	matrix_login(server, user, password);
	puts("Logged in.");

	memset(password, 0x0, strlen(password)); // TODO: is this optimized out?

	free(ptr);
	// TODO: free(password)?
}

void process_room_create(StrBuf *id) {
	room_new(id);
}

void process_room_name(StrBuf *roomid, StrBuf *name) {
	Room *room = room_byid(roomid);
	room_set_name(room, name);
}

void process_room_join(StrBuf *roomid, StrBuf *sender) {
	Room *room = room_byid(roomid);
	assert(room);
	room_append_user(room, sender);
}

void process_msg(StrBuf *roomid, StrBuf *sender, StrBuf *text) {
	Room *room = room_byid(roomid);
	room_append_msg(room, sender, text);
	ui_hooks.new_msg(room, sender, text);
}

void handle_matrix_event(MatrixEvent ev) {
	switch (ev.type) {
	case EVENT_ROOM_CREATE:
		process_room_create(ev.roomcreate.id);
		break;
	case EVENT_ROOM_NAME:
		process_room_name(ev.roomname.id, ev.roomname.name);
		break;
	case EVENT_ROOM_JOIN:
		process_room_join(ev.roomjoin.roomid,
			ev.roomjoin.sender);
		break;
	case EVENT_MSG:
		process_msg(ev.msg.roomid, ev.msg.sender, ev.msg.text);
		break;
	case EVENT_ERROR:
		printf("%s\n", strbuf_buf(ev.error.error));
		exit(1);
		break;
	case EVENT_LOGGED_IN:
		logged_in = true;
		cache_set("access_token", strbuf_buf(ev.login.token));
		break;
	case EVENT_CONN_ERROR:
		//puts("Connection error.\n");
		break;
	}
}

void handle_ui_event(UiEvent ev) {
	switch (ev.type) {
	case UIEVENTTYPE_SYNC:
		if (logged_in)
			matrix_sync();
		break;
	case UIEVENTTYPE_SENDMSG:
		matrix_send_message(ev.msg.roomid, ev.msg.text);
		break;
	}
}
