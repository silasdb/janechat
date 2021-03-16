#ifndef JANECHAT_ROOMS_H
#define JANECHAT_ROOMS_H

#include "list.h"

struct Room {
	const char *id;
	const char *name;
	List *msgs;
};

typedef struct Room Room;

void rooms_init();
Room *room_new(const char *, const char *);
Room *room_byid(const char *);
Room *room_byname(const char *);

#define ROOM_MESSAGES_FOREACH(r, iter) LIST_FOREACH(r->msgs, iter)

#endif /* !JANECHAT_ROOMS_H */
