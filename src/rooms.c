#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "rooms.h"

Hash *rooms_hash;	/* Hash<const char *id, Room> */
Vector *rooms_vector;	/* Vector<Room> */
size_t count;		/* Number of rooms */

void rooms_init(void) {
	rooms_hash = hash_new();
	rooms_vector = vector_new();
}

/*
 * TODO: is this the best name, since it changes the global variable?
 */
Room *room_new(Str *id) {
	Room *room;
	room = room_byid(id);
	if (room) {
		/*
		 * TODO: this is a temporary workaround because the same room
		 * can show up both as m.direct and m.room.name, but not always.
		 * Who creates the room?
		 */
		printf("Trying to create duplicated room (id: %s)\n",
			str_buf(id));
		return room;
	}
	room = malloc(sizeof(Room));
	room->id = str_incref(id);
	
	/*
	 * At the moment of creation, we don't know the room name yet, so we set
	 * it to be temporarily the same as id and increment its reference
	 * counter.
	 */
	room->name = str_incref(id);

	room->msgs = vector_new();
	room->unread_msgs = 0;
	room->notify = true;
	hash_insert(rooms_hash, str_buf(id), room);
	vector_append(rooms_vector, room);
	return room;
}

Room *room_byid(const Str *id) {
	return hash_get(rooms_hash, str_buf(id));
}

Room *room_byname(const Str *name) {
	Room *iter;
	size_t i;
	ROOMS_FOREACH(iter, i) {
		if (streq(iter->name->buf, name->buf))
			return iter;
	}
	return NULL;
}

void room_set_name(Room *r, Str *name) {
	/*
	 * TODO: it seems client can receive m.room.name before m.room.create,
	 * so we'll need to handle that.
	 */
	if (!r)
		return;

	/*
	 * We str_decref() old value because it was set to the room id at
	 * str_new() while we don't receive the room name.
	 */
	str_decref(r->name);

	r->name = str_incref(name);
}

void room_append_msg(Room *room, Str *sender, Str *text) {
	Msg *msg = malloc(sizeof(Msg));
	msg->sender = str_incref(sender);
	msg->text = str_incref(text);
	vector_append(room->msgs, msg);
	room->unread_msgs++;
}
