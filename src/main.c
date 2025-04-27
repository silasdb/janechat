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
#include "common.h"
#include "matrix.h"
#include "rooms.h"
#include "ui.h"
#include "ui-cli.h"
#include "ui-curses.h"
#include "utils.h"

bool do_matrix_send_token(void);
void do_matrix_login(void);
void handle_matrix_event(MatrixEvent ev);
void handle_ui_event(UiEvent ev);

struct ui_hooks {
	void (*setup)();
	void (*init)();
	void (*iter)();
	void (*msg_new)(Room *room, Msg msg);
	void (*room_new)(Str *roomid);
} ui_hooks;

void usage(void) {
	fputs("usage: janechat [-f cli|curses]", stderr);
	exit(2);
}

int main(int argc, char *argv[]) {
	enum Ui {
		UI_CLI,
		UI_CURSES,
	} ui_frontend = UI_CURSES;

	/* Option processing */
	int c;
	extern char *optarg;
	extern int optind;
	while ((c = getopt(argc, argv, "f:p:")) != -1) {
		switch (c) {
		case 'f':
			if (streq(optarg, "cli"))
				ui_frontend = UI_CLI;
			else if (streq(optarg, "curses"))
				ui_frontend = UI_CURSES;
			else
				usage();
			break;
		case 'p':
			/*
			 * TODO: we need to canonize optarg to avoid path
			 * traversal and other problems.
			 */
			cache_set_profile(optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	if (argc != 0)
		usage();

	ui_set_event_handler(handle_ui_event);

	/* UI callback setup */
	switch (ui_frontend) {
	case UI_CLI:
		ui_hooks = (struct ui_hooks){
			.iter = ui_cli_iter,
			.msg_new = ui_cli_msg_new,
		};
		break;
	case UI_CURSES:
		ui_hooks = (struct ui_hooks){
			.setup = ui_curses_setup,
			.init = ui_curses_init,
			.iter = ui_curses_iter,
			.msg_new = ui_curses_msg_new,
			.room_new = ui_curses_room_new,
		};
		break;
	}

	matrix_set_event_handler(handle_matrix_event);

	rooms_init();

	if (ui_hooks.setup)
		ui_hooks.setup();

	/* TODO: what if the access_token expires or is invalid? */
	if (!do_matrix_send_token())
		do_matrix_login();
	else {
		char *server = cache_get_alloc("server");
		matrix_set_server(server);
	}

	puts("Performing initial sync...");
	if (!matrix_initial_sync()) {
		fprintf(stderr,
			"Error when performing initial sync. Exit.");
		exit(1);
	}
	puts("Done.");

	if (ui_hooks.init)
		ui_hooks.init();

	for (;;) {
		switch (select_matrix_stdin()) {
		case SELECTSTATUS_STDINREADY:
			ui_hooks.iter();
			break;
		case SELECTSTATUS_MATRIXRESUME:
			matrix_resume();
			break;
		}
		static time_t past = 0, now = 0;
		now = time(0);
		if (now > past + 1) {
			matrix_sync();
			past = now;
		}
	}
	
	return 0;
}

bool do_matrix_send_token(void) {
	char *token = cache_get_alloc("access_token");
	if (!token)
		return false;
	matrix_set_token(token);
	return true;
}

void do_matrix_login(void) {
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
	const char *access_token = matrix_login_alloc(server, user, password);
	matrix_set_token(strdup(access_token));
	if (!access_token) {
		fprintf(stderr, "Wrong username or password. Exit.");
		exit(1);
	}
	puts("Logged in.");
	cache_set("server", server);
	cache_set("access_token", access_token);
	free((void *)access_token);

	memset(password, 0x0, strlen(password)); // TODO: is this optimized out?

	free(ptr);
	// TODO: free(password)?
}

void process_room_create(Str *id, bool is_space) {
	Room *room = room_new(id, is_space);
	if (ui_hooks.room_new)
		ui_hooks.room_new(id);
	char *dn = cache_get_alloc(str_buf(id));
	if (dn) {
		Str *displayname = str_new_cstr(dn);
		room_set_displayname(room, displayname);
		str_decref(displayname);
	}
	free(dn);
}

void process_room_info(Str *roomid, Str *sender, Str *name) {
	Room *room = room_byid(roomid);
	room_set_info(room, sender, name);
}

void process_room_join(Str *roomid, Str *senderid, Str *sendername) {
	Room *room = room_byid(roomid);
	assert(room);
	room_append_user(room, senderid);
	user_add(senderid, sendername);
}

void process_msg(Str *roomid, Msg msg) {
	Room *room = room_byid(roomid);
	room_append_msg(room, msg);
	ui_hooks.msg_new(room, msg);
}

void open_file(FileInfo fileinfo) {
	Str *filepath = str_new_uri_extract_path(fileinfo.uri);
	Str *cmd = str_new();

	Str *f = str_shellquote_alloc(filepath);
	str_decref(filepath);
	filepath = f;
	Str *mimetype = str_shellquote_alloc(fileinfo.mimetype);

	/* janechat-attachment-handler.sh have to exist in PATH */
	str_append_cstr(cmd, "janechat-attachment-handler.sh open ");

	str_append_str(cmd, mimetype);
	str_append_cstr(cmd, " ");
	str_append_str(cmd, filepath);
	system(str_buf(cmd));
	str_decref(cmd);
	str_decref(filepath);
}

void handle_matrix_event(MatrixEvent ev) {
	switch (ev.type) {
	case EVENT_ROOM_CREATE:
		process_room_create(ev.roomcreate.id, ev.roomcreate.is_space);
		break;
	case EVENT_ROOM_INFO:
		process_room_info(ev.roominfo.id,
			ev.roominfo.sender, ev.roominfo.name);
		break;
	case EVENT_ROOM_NOTIFY_STATUS:
		{
		Room *room = room_byid(ev.roomnotifystatus.roomid);
		/* TODO: why there are room push_rules if the room is not created? */
		if (!room)
			return;
		room->notify = ev.roomnotifystatus.enabled;
		}
		break;
	case EVENT_ROOM_JOIN:
		process_room_join(ev.roomjoin.roomid,
			ev.roomjoin.senderid, ev.roomjoin.sendername);
		break;
	case EVENT_MSG:
		process_msg(ev.msg.roomid, ev.msg.msg);
		break;
	case EVENT_MATRIX_ERROR:
		printf("%s\n", str_buf(ev.error.error));
		exit(1);
		break;
	case EVENT_CONN_ERROR:
		//puts("Connection error.\n");
		break;
	case EVENT_FILE: {
		Str *filepath = str_new_uri_extract_path(ev.file.fileinfo.uri);
		Str *cmd = str_new();;


		Str *fp = str_shellquote_alloc(filepath);
		str_decref(filepath);
		filepath = fp;
		Str *mimetype = str_shellquote_alloc(ev.file.fileinfo.mimetype);

		str_append_cstr(cmd, "janechat-attachment-handler.sh save ");
		str_append_str(cmd, mimetype);
		str_append_cstr(cmd, " ");
		str_append_str(cmd, filepath);

		FILE *proc = popen(str_buf(cmd), "w");
		fwrite(ev.file.payload, 1, ev.file.size, proc);
		pclose(proc);

		str_decref(filepath);
		str_decref(cmd);

		open_file(ev.file.fileinfo);
		break;
	} }
}

void handle_ui_event(UiEvent ev) {
	switch (ev.type) {
	case UIEVENTTYPE_SYNC:
		matrix_sync();
		break;
	case UIEVENTTYPE_SENDMSG:
		matrix_send_message(ev.msg.roomid, ev.msg.text);
		break;
	case UIEVENTTYPE_OPENATTACHMENT: {
		Str *filepath = str_new_uri_extract_path(ev.openattachment.fileinfo.uri);


		Str *fp = str_shellquote_alloc(filepath);
		str_decref(filepath);
		filepath = fp;
		Str *mimetype = str_shellquote_alloc(ev.openattachment.fileinfo.mimetype);

		Str *cmd = str_new();;
		str_append_cstr(cmd, "janechat-attachment-handler.sh exists ");
		str_append_str(cmd, mimetype);
		str_append_cstr(cmd, " ");
		str_append_str(cmd, filepath);

		FILE *proc = popen(str_buf(cmd), "r");
		char buf[256] = { '\0' };
		fread(buf, 256, sizeof(char), proc);
		pclose(proc);

		/* TODO: what if janechat-attachment-handler.sh is not found? */
		assert(streq(buf, "yes\n") || streq(buf, "no\n"));

		/* Only request file if it doesn't exist in our local cache */
		if (streq(buf, "no\n"))
			matrix_request_file(ev.openattachment.fileinfo);
		else
			open_file(ev.openattachment.fileinfo);
		str_decref(filepath);
		str_decref(cmd);
		break; }
	case UIEVENTTYPE_NOTIFYSTATUS:
		matrix_set_room_notifystatus(
			ev.roomnotifystatus.roomid,
			ev.roomnotifystatus.enabled);
		break;
	case UIEVENTTYPE_ROOM_RENAME:
		cache_set(str_buf(ev.roomrename.roomid),
			str_buf(ev.roomrename.name));
		Room *room = room_byid(ev.roomrename.roomid);
		room_set_displayname(room, ev.roomrename.name);
		break;
	}
}
