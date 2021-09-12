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
Room *room_new(const char *id) {
	Room *room;
	room = room_byid(id);
	if (room) {
		/*
		 * TODO: this is a temporary workaround because the same room
		 * can show up both as m.direct and m.room.name, but not always.
		 * Who creates the room?
		 */
		printf("Trying to create duplicated room (id: %s)\n", id);
		return room;
	}
	room = malloc(sizeof(Room));
	room->id = id;
	
	/*
	 * At the moment of creation, we don't know the room name yet, so we set
	 * it to be temporarily the same as id, but, because room name can be
	 * manipulated (like when changing a rooms name), while id is a fixed
	 * object, so we strdup() it.
	 *
	 * There could be other more optimized ways to do that, like setting
	 * name to NULL and checking if it is NULL whenever we need to print the
	 * name, but this would make things more complex outside of the rooms
	 * module, so we prefer this less effective but simplier way.
	 */
	room->name = strdup(id);

	room->users = list_new();
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

void room_set_name(Room *r, const char *name) {
	/*
	 * TODO: it seems client can receive m.room.name before m.room.create,
	 * so we'll need to handle that.
	 */
	if (!r)
		return;

	free((void *)r->name);
	r->name = name;
}

void room_append_msg(Room *room, const char *sender, const char *text) {
	Msg *msg = malloc(sizeof(Msg));
	msg->sender = sender;
	msg->text = text;
	list_append(room->msgs, msg);
	room->unread_msgs++;
}

void room_append_user(Room *room, const char *sender) {
	list_append(room->users, (void *)sender);
}
