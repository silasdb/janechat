#include <stdlib.h>

#include "hash.h"
#include "rooms.h"

Hash *rooms;		/* Hash<const char *id> -> Room */
Hash *roomnames2id;	/* Hash<const char *name> -> const char *id */
size_t count;		/* Number of rooms */

void rooms_init() {
	rooms = hash_new();
	roomnames2id = hash_new();
}

/*
 * TODO: is this the best name, since it changes the global variable?
 */
Room *room_new(const char *id, const char *name) {
	Room *room = malloc(sizeof(Room));
	room->id = id;
	room->name = name;
	room->msgs = list_new();
	hash_insert(rooms, id, room);
	hash_insert(roomnames2id, name, id);
	return room;
}

Room *room_byid(const char *id) {
	return hash_get(rooms, id);
}

Room *room_byname(const char *name) {
	const char *id;
	id = hash_get(roomnames2id, name);
	if (!id)
		return NULL;
	return room_byid(id);
}
