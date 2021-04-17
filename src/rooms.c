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
Room *room_new(const char *id, const char *name) {
	Room *room;
	room = room_byid(id);
	if (room) {
		/*
		 * TODO: this is a temporary workaround because the same room
		 * can show up both as m.direct and m.room.name, but not always.
		 * Who creates the room?
		 */
		printf("Duplicated room found (id: %s) (name: %s/%s)\n", id,
		 name, room->name);
		return room;
	}
	room = malloc(sizeof(Room));
	room->id = id;
	room->name = name;
	room->msgs = list_new();
	room->unread_msgs = 0;
	hash_insert(rooms_hash, id, room);
	list_append(rooms_list, room);
	return room;
}

Room *room_byid(const char *id) {
	return hash_get(rooms_hash, id);
}

Room *room_byname(const char *name) {
	ROOMS_FOREACH(iter) {
		Room *r = iter;
		if (strcmp(r->name, name) == 0)
			return r;
	}
	return NULL;
}

void room_append_msg(Room *room, const char *sender, const char *text) {
	Msg *msg = malloc(sizeof(Msg));
	msg->sender = sender;
	msg->text = text;
	list_append(room->msgs, msg);
}
