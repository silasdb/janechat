#include <stdlib.h>

#include "hash.h"
#include "rooms.h"

Hash *rooms_hash;	/* Hash<const char *id, Room> */
List *rooms_list;	/* List<Room> */
Hash *roomnames2id;	/* Hash<const char *name, const char *id> */
size_t count;		/* Number of rooms */

void rooms_init() {
	rooms_hash = hash_new();
	roomnames2id = hash_new();
	rooms_list = list_new();
}

/*
 * TODO: is this the best name, since it changes the global variable?
 */
Room *room_new(const char *id, const char *name) {
	Room *room = malloc(sizeof(Room));
	room->id = id;
	room->name = name;
	room->msgs = list_new();
	room->unread_msgs = 0;
	hash_insert(rooms_hash, id, room);
	list_append(rooms_list, room);
	hash_insert(roomnames2id, name, id);
	return room;
}

Room *room_byid(const char *id) {
	return hash_get(rooms_hash, id);
}

Room *room_byname(const char *name) {
	const char *id;
	id = hash_get(roomnames2id, name);
	if (!id)
		return NULL;
	return room_byid(id);
}

void room_append_msg(Room *room, const char *sender, const char *text) {
	Msg *msg = malloc(sizeof(Msg));
	msg->sender = sender;
	msg->text = text;
	list_append(room->msgs, msg);
}
