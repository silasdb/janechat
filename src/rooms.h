#ifndef JANECHAT_ROOMS_H
#define JANECHAT_ROOMS_H

#include "list.h"
#include "str.h"

struct Msg {
	Str *sender;
	Str *text;
};
typedef struct Msg Msg;

struct Room {
	Str *id;
	Str *name;
	List *users;		/* List of joined users */
	List *msgs;
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

extern List *rooms_list;

#define ROOM_MESSAGES_FOREACH(r, iter) LIST_FOREACH(r->msgs, iter)
#define ROOM_USERS_FOREACH(r, iter) LIST_FOREACH(r->users, iter)
#define ROOMS_FOREACH(iter) LIST_FOREACH(rooms_list, iter)

#endif /* !JANECHAT_ROOMS_H */
