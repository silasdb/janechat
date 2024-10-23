#undef NDEBUG
#include "../../src/hash.c"
#include "../../src/utils.c"
#include "../../src/list.c"
#include "../../src/rooms.c"
#include "../../src/str.c"
#include "../../src/ui.c"
#include "../../src/vector.c"
#include "../../src/ui-curses.c"

void fake_event_handler(UiEvent ev) {
	switch (ev.type) {
	case UIEVENTTYPE_SENDMSG: {
		Msg *msg = malloc(sizeof(struct Msg));
		msg->sender = str_new_cstr("test");
		msg->type = MSGTYPE_TEXT;
		msg->text.content = str_dup(cur_buffer->buf);
		vector_append(cur_buffer->room->msgs, msg);
		chat_msgs_fill();
		}
		break;
	default:
		abort();
		break;
	}
}

int main(int argc, char *argv[]) {
	rooms_init();
	ui_set_event_handler(fake_event_handler);
	ui_curses_setup();

	Room *room;
	Str *name_s;
	Str *id_s;
#define new_room(id, name) \
	id_s = str_new_cstr(id); \
	name_s = str_new_cstr(name); \
	room = room_new(id_s, false); \
	room_set_info(room, str_new_cstr("@example:matrix.org"), name_s); \
	ui_curses_room_new(id_s);

	new_room("#test1:matrix.org", "Test A");
	new_room("#test2:matrix.org", "Test C");
	new_room("#test3:matrix.org", "Test B");

	Msg msg;
	Str *roomid = str_new();;
	Str *sender = str_new_cstr_fixed("test");
#define new_msg(roomid_cstr, msg_cstr) \
	str_reset(roomid); \
	str_append_cstr(roomid, roomid_cstr); \
	room = room_byid(roomid); \
	msg.sender = sender; \
	msg.text.content = str_new_cstr(msg_cstr); \
	room_append_msg(room, msg);

	for (int i = 0; i < 100; i++) {
		new_msg("#test1:matrix.org", "test");
	}

	ui_curses_init();
	bottom = vector_len(buffers);

	resize();

	for (;;)
		ui_curses_iter();

	return 0;
}
