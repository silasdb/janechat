#ifndef JANECHAT_ROOMS_H
#define JANECHAT_ROOMS_H

#include "list.h"
#include "strbuf.h"

struct Msg {
	StrBuf *sender;
	StrBuf *text;
};
typedef struct Msg Msg;

struct Room {
	StrBuf *id;
	StrBuf *name;
	List *users;		/* List of joined users */
	List *msgs;
	size_t unread_msgs;	/* Should be reset by the caller */
};
typedef struct Room Room;

void rooms_init();
Room *room_new(StrBuf *);
Room *room_byid(const StrBuf *);
Room *room_byname(const StrBuf *);
void room_set_name(Room *, StrBuf *);
void room_append_msg(Room *, StrBuf *, StrBuf *);
void room_append_user(Room *, StrBuf *);

extern List *rooms_list;

#define ROOM_MESSAGES_FOREACH(r, iter) LIST_FOREACH(r->msgs, iter)
#define ROOM_USERS_FOREACH(r, iter) LIST_FOREACH(r->users, iter)
#define ROOMS_FOREACH(iter) LIST_FOREACH(rooms_list, iter)

#endif /* !JANECHAT_ROOMS_H */
