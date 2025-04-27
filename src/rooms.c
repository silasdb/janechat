#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "rooms.h"

Hash *rooms_hash;	/* Hash<const char *id, Room> */
Vector *rooms_vector;	/* Vector<Room> */
size_t count;		/* Number of rooms */
Hash *users_hash;	/* Hash<const char *id, Str *> */

void rooms_init(void) {
	rooms_hash = hash_new();
	rooms_vector = vector_new();
	users_hash = hash_new();
}

/*
 * TODO: is this the best name, since it changes the global variable?
 */
Room *room_new(Str *id, bool is_space) {
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
	room->name = NULL;
	room->displayname = NULL;
	room->calculatedname = NULL;
	room->sender = NULL;
	room->is_space = is_space;
	
	room->users = vector_new();
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

Str *room_displayname(Room *r) {
	if (r->displayname)
		return r->displayname;
	if (r->name)
		return r->name;
	if (!r->calculatedname) {
		/*
		 * TODO: very ugly workaround to calculate a room name from its
		 * members if it doesn't already have a name. In the future we
		 * should be more serious about that and really implement
		 * https://spec.matrix.org/latest/client-server-api/#calculating-the-display-name-for-a-room
		 */
		Str *calculatedname = str_new();
		for (size_t i = 0; i < vector_len(r->users) && i < 5; i++) {
			str_append_str(calculatedname,
			 user_name(vector_at(r->users, i)));
			str_append_cstr(calculatedname, " / ");
		}
		r->calculatedname = calculatedname;
	}
	return r->calculatedname;
}

void room_set_displayname(Room *r, Str *displayname) {
	assert(r);
	r->displayname = str_incref(displayname);
}

void room_set_info(Room *r, Str *sender, Str *name) {
	/*
	 * TODO: it seems client can receive m.room.name before m.room.create,
	 * so we'll need to handle that.
	 */
	if (!r)
		return;

	if (sender)
		r->sender = str_incref(sender);
	if (name)
		r->name = str_incref(name);
}

void room_append_msg(Room *room, Msg m) {
	Msg *msg = malloc(sizeof(Msg));
	memcpy(msg, &m, sizeof(Msg));
	str_incref(msg->sender);
	if (msg->type == MSGTYPE_TEXT)
		str_incref(msg->text.content);
	else
		str_incref(msg->fileinfo.uri);
	vector_append(room->msgs, msg);
	room->unread_msgs++;
}

void room_append_user(Room *room, Str *sender) {
	vector_append(room->users, str_incref(sender));

	/*
	 * TODO: force the calculatedname to be recalculated when a user joins.
	 * Spaghetti code!
	 */
	free(room->calculatedname);
	room->calculatedname = NULL;
}

void user_add(Str *id, Str *name) {
	/*
	 * Maybe the user didn't specify its human readable name. Store NULL in
	 * users_hash anyway.
	 */
	if (name)
		str_incref(name);
	hash_insert(users_hash, str_buf(id), name);
}

Str *user_name(Str *id) {
	Str *name = hash_get(users_hash, str_buf(id));
	if (!name)
		return id;
	return name;
}
