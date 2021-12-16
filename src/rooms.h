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
	/* The Matrix ID of the room. It is always set */
	Str *id;

	/*
	 * The name of the room. It is always set for normal rooms, but it can
	 * optionally be set for direct message rooms as well. 
	 */
	Str *name;

	/*
	 * It is always set for direct message rooms but not for normal rooms.
	 */
	Str *sender;

	Vector *users;		/* Vector of joined users */
	Vector *msgs;
	size_t unread_msgs;	/* Should be reset by the caller */
	bool notify;
};
typedef struct Room Room;

void rooms_init(void);
Room *room_new(Str *);
Room *room_byid(const Str *);
Str *room_displayname(Room *);
void room_set_info(Room *, Str *, Str *);
void room_append_msg(Room *, Str *, Str *);
void room_append_user(Room *, Str *);

void user_add(Str *, Str *);
Str *user_name(Str *);

extern Vector *rooms_vector;

#define ROOM_MESSAGES_FOREACH(r, iter, i) VECTOR_FOREACH(r->msgs, iter, i)
#define ROOM_USERS_FOREACH(r, iter, i) VECTOR_FOREACH(r->users, iter, i)
#define ROOMS_FOREACH(iter, i) VECTOR_FOREACH(rooms_vector, iter, i)

#endif /* !JANECHAT_ROOMS_H */
