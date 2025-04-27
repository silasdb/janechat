#ifndef JANECHAT_ROOMS_H
#define JANECHAT_ROOMS_H

#include "common.h"
#include "vector.h"
#include "str.h"

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

	/* User-set room displayname. */
	Str *displayname;

	/*
	 * If the user didn't set displayname and the room doesn't have a name
	 * set, the display name is calculated using members' names and store it
	 * in this variable.
	 *
	 * See:
	 * https://spec.matrix.org/latest/client-server-api/#calculating-the-display-name-for-a-user
	 */
        Str *calculatedname;

	Vector *users;		/* Vector of joined users: Vector<Str*> */
	Vector *msgs;
	size_t unread_msgs;	/* Should be reset by the caller */
	bool notify;
	bool is_space;
};
typedef struct Room Room;

void rooms_init(void);
Room *room_new(Str *, bool);
Room *room_byid(const Str *);
Str *room_displayname(Room *);
void room_set_displayname(Room *, Str *);
void room_set_info(Room *, Str *, Str *);
void room_append_msg(Room *, Msg msg);
void room_append_user(Room *, Str *);

void user_add(Str *, Str *);
Str *user_name(Str *);

extern Vector *rooms_vector;

#define ROOM_MESSAGES_FOREACH(r, iter, i) VECTOR_FOREACH(r->msgs, iter, i)
#define ROOM_USERS_FOREACH(r, iter, i) VECTOR_FOREACH(r->users, iter, i)
#define ROOMS_FOREACH(iter, i) VECTOR_FOREACH(rooms_vector, iter, i)

#endif /* !JANECHAT_ROOMS_H */
