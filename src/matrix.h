#ifndef MATRIXEV
#define MATRIXEV

#include <fcntl.h>

#include "str.h"

enum MatrixEventType {
	EVENT_MSG,
	EVENT_ROOM_CREATE,
	EVENT_ROOM_NAME,
	EVENT_ROOM_JOIN,
	EVENT_ERROR,
	EVENT_LOGGED_IN,
	EVENT_CONN_ERROR,
};

struct MatrixEvent {
	enum MatrixEventType type;
	union {
		// TODO: why fields above are not const?
		struct MatrixEventMsg {
			Str *roomid;
			Str *sender;
			Str *text;
		} msg;
		struct MatrixEventRoomCreate {
			Str *id;
		} roomcreate;
		struct MatrixEventRoomName {
			Str *id;
			Str *name;
		} roomname;
		struct MatrixEventRoomJoin {
			Str *roomid;
			Str *sender;
		} roomjoin;
		struct MatrixEventError {
			Str *errorcode;
			Str *error;
		} error;
		struct MatrixEventLogin {
			Str *token;
		} login;
		// MatrixEventConnError - empty struct
	};
};

typedef struct MatrixEvent MatrixEvent;

enum SelectStatus {
	SELECTSTATUS_STDINREADY,
	SELECTSTATUS_MATRIXRESUME,
};

void matrix_set_event_handler(void (*callback)(MatrixEvent));
void matrix_sync();
void matrix_send_message(const Str *roomid, const Str *msg);
MatrixEvent * matrix_next_event();
void matrix_set_server(char *token);
void matrix_set_token(char *token);
void matrix_login(const char *server, const char *user, const char *password);
void matrix_free_event(MatrixEvent *);
bool matrix_select();
void matrix_resume();
enum SelectStatus select_matrix_stdin();

extern fd_set fdread;

#endif
