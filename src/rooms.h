#ifndef JANECHAT_ROOMS_H
#define JANECHAT_ROOMS_H

#include "vector.h"
#include "str.h"

struct Msg {
	Str *sender;
	Str *text;
};
typedef struct Msg Msg;

struct Room {
	Str *id;
	Str *name;
	Vector *msgs;
	size_t unread_msgs;	/* Should be reset by the caller */
	bool notify;
};
typedef struct Room Room;

void rooms_init(void);
Room *room_new(Str *);
Room *room_byid(const Str *);
Room *room_byname(const Str *);
void room_set_name(Room *, Str *);
void room_append_msg(Room *, Str *, Str *);

extern Vector *rooms_vector;

#define ROOM_MESSAGES_FOREACH(r, iter, i) VECTOR_FOREACH(r->msgs, iter, i)
#define ROOMS_FOREACH(iter, i) VECTOR_FOREACH(rooms_vector, iter, i)

#endif /* !JANECHAT_ROOMS_H */
