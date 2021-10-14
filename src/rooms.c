#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "rooms.h"

Hash *rooms_hash;	/* Hash<const char *id, Room> */
List *rooms_list;	/* List<Room> */
size_t count;		/* Number of rooms */

void rooms_init() {
	rooms_hash = hash_new();
	rooms_list = list_new();
}

/*
 * TODO: is this the best name, since it changes the global variable?
 */
Room *room_new(StrBuf *id) {
	Room *room;
	room = room_byid(id);
	if (room) {
		/*
		 * TODO: this is a temporary workaround because the same room
		 * can show up both as m.direct and m.room.name, but not always.
		 * Who creates the room?
		 */
		printf("Trying to create duplicated room (id: %s)\n",
			strbuf_buf(id));
		return room;
	}
	room = malloc(sizeof(Room));
	room->id = strbuf_incref(id);
	
	/*
	 * At the moment of creation, we don't know the room name yet, so we set
	 * it to be temporarily the same as id and increment its reference
	 * counter.
	 */
	room->name = strbuf_incref(id);

	room->users = list_new();
	room->msgs = list_new();
	room->unread_msgs = 0;
	hash_insert(rooms_hash, strbuf_buf(id), room);
	list_append(rooms_list, room);
	return room;
}

Room *room_byid(const StrBuf *id) {
	return hash_get(rooms_hash, strbuf_buf(id));
}

Room *room_byname(const StrBuf *name) {
	ROOMS_FOREACH(iter) {
		Room *r = iter;
		if (streq(r->name, name))
			return r;
	}
	return NULL;
}

void room_set_name(Room *r, StrBuf *name) {
	/*
	 * TODO: it seems client can receive m.room.name before m.room.create,
	 * so we'll need to handle that.
	 */
	if (!r)
		return;

	/*
	 * We strbuf_decref() old value because it was set to the room id at
	 * strbuf_new() while we don't receive the room name.
	 */
	strbuf_decref(r->name);

	r->name = strbuf_incref(name);
}

void room_append_msg(Room *room, StrBuf *sender, StrBuf *text) {
	Msg *msg = malloc(sizeof(Msg));
	msg->sender = strbuf_incref(sender);
	msg->text = strbuf_incref(text);
	list_append(room->msgs, msg);
	room->unread_msgs++;
}

void room_append_user(Room *room, StrBuf *sender) {
	list_append(room->users, (void *)strbuf_buf(strbuf_incref(sender)));
}
