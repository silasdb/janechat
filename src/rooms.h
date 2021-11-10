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
	Vector *users;		/* Vector of joined users */
	Vector *msgs;
	size_t unread_msgs;	/* Should be reset by the caller */
};
typedef struct Room Room;

void rooms_init();
Room *room_new(Str *);
Room *room_byid(const Str *);
Room *room_byname(const Str *);
void room_set_name(Room *, Str *);
void room_append_msg(Room *, Str *, Str *);
void room_append_user(Room *, Str *);

extern Vector *rooms_vector;

#define ROOM_MESSAGES_FOREACH(r, iter, i) VECTOR_FOREACH(r->msgs, iter, i)
#define ROOM_USERS_FOREACH(r, iter, i) VECTOR_FOREACH(r->users, iter, i)
#define ROOMS_FOREACH(iter, i) VECTOR_FOREACH(rooms_vector, iter, i)

#endif /* !JANECHAT_ROOMS_H */
