#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "ui-cli.h"

Room *current_room = NULL;

static void process_input(char *);
static void print_messages(Room *room);
static void print_msg(Str *roomname, Str *sender, Str *text);

void ui_cli_iter() {
	char *line;
	line = read_line_alloc();
	if (line)
		process_input(line);
	free(line);
}

void ui_cli_new_msg(Room *room, Str *sender, Str *text) {
	if (room != current_room)
		return;

	/*
	 * If we are in current room, just print the message and update the
	 * unread messages flag to zero, since we have just printed it to the
	 * user.
	 */
	print_msg(room->name, sender, text);
	room->unread_msgs = 0;
}

/*
 * For now, possible commands are:
 *
 * /quit -> exit this program
 * /stat -> list all rooms and number of unread messages
 * /names -> list all users in current room
 * /join room -> change the current room
 * text -> send "text" to the current room
 */
void process_input(char *s) {
	UiEvent ev;
	if (streq(s, "/quit"))
		exit(0);

	if (streq(s, "/stat")) {
		Room *room;
		size_t i;
		ROOMS_FOREACH(room, i) {
			printf("%s (unread messages: %zu)\n",
				str_buf(room->name), room->unread_msgs);
		}
		return;
	}

	if (streq(s, "/names")) {
		if (!current_room) {
			puts("No room selected.  Text not sent.\n");
			return;
		}
		char *iter;
		size_t i;
		ROOM_USERS_FOREACH(current_room, iter, i) {
			printf("%s\n", iter);
		}
		return;
	}

	if (strncmp(s, "/join ", strlen("/join ")) == 0) {
		s += strlen("/join ");
		Str *ss = str_new_cstr(s);
		Room *room = room_byname(ss);
		str_decref(ss);
		if (!room) {
			printf("Room \"%s\" not found.\n", s);
			return;
		}
		current_room = room;
		printf("Switched to room %s\n", s);
		print_messages(current_room);
		return;
	}

	if (streq(s, "/sync")) {
		ev.type = UIEVENTTYPE_SYNC;
		ui_event_handler_callback(ev);
		return;
	}

	if (*s == '\0') {
		ev.type = UIEVENTTYPE_SYNC;
		ui_event_handler_callback(ev);
		return;
	}

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

	ev.type = UIEVENTTYPE_SENDMSG;
	ev.msg.roomid = str_incref(current_room->id);
	ev.msg.text = str_new_cstr(s);
	ui_event_handler_callback(ev);
	str_decref(ev.msg.roomid);
	str_decref(ev.msg.text);
}

static void print_messages(Room *room) {
	Msg *msg;
	size_t i;
	ROOM_MESSAGES_FOREACH(room, msg, i) {
		print_msg(room->name, msg->sender, msg->text);
	}
	room->unread_msgs = 0;
}

static void print_msg(Str *roomname, Str *sender, Str *text) {
	printf("%c[38;5;4m%s%c[m: %c[38;5;2m%s%c[m: %s\n",
		0x1b, str_buf(roomname), 0x1b,
		0x1b, str_buf(sender), 0x1b,
		str_buf(text));
}
