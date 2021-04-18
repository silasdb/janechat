#ifndef JANECHAT_ROOMS_H
#define JANECHAT_ROOMS_H

#include "list.h"

struct Msg {
	const char *sender;
	const char *text;
};
typedef struct Msg Msg;

struct Room {
	const char *id;
	const char *name;
	List *users;		/* List of joined users */
	List *msgs;
	size_t unread_msgs;	/* Should be reset by the caller */
};
typedef struct Room Room;

void rooms_init();
Room *room_new(const char *);
Room *room_byid(const char *);
Room *room_byname(const char *);
void room_set_name(Room *, const char *);
void room_append_msg(Room *, const char *, const char *);
void room_append_user(Room *, const char *);

extern List *rooms_list;

#define ROOM_MESSAGES_FOREACH(r, iter) LIST_FOREACH(r->msgs, iter)
#define ROOM_USERS_FOREACH(r, iter) LIST_FOREACH(r->users, iter)
#define ROOMS_FOREACH(iter) LIST_FOREACH(rooms_list, iter)

#endif /* !JANECHAT_ROOMS_H */
