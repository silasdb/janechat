#ifndef MATRIXEV
#define MATRIXEV

#include <fcntl.h>

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
			char *roomid;
			char *sender;
			char *text;
		} msg;
		struct MatrixEventRoomCreate {
			char *id;
		} roomcreate;
		struct MatrixEventRoomName {
			char *id;
			char *name;
		} roomname;
		struct MatrixEventRoomJoin {
			char *roomid;
			char *sender;
		} roomjoin;
		struct MatrixEventError {
			char *errorcode;
			char *error;
		} error;
		struct MatrixEventLogin {
			char *token;
		} login;
		// MatrixEventConnError - empty struct
	};
};

typedef struct MatrixEvent MatrixEvent;

enum SelectStatus {
	SELECTSTATUS_STDINREADY,
	SELECTSTATUS_MATRIXRESUME,
};

void matrix_sync();
void matrix_send_message(const char *roomid, const char *msg);
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
