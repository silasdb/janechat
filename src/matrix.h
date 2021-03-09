#ifndef MATRIXEV
#define MATRIXEV

enum MatrixEventType {
	EVENT_MSG,
	EVENT_ROOM,
	EVENT_ERROR,
	EVENT_LOGGED_IN,
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
		struct MatrixEventRoomName {
			char *id;
			char *name;
		} room;
		struct MatrixEventError {
			char *errorcode;
			char *error;
		} error;
		struct MatrixEventLogin {
			char *token;
		} login;
	};
};

typedef struct MatrixEvent MatrixEvent;

void matrix_sync();
void matrix_send_message(const char *roomid, const char *msg);
MatrixEvent * matrix_next_event();
void matrix_set_server(char *token);
void matrix_set_token(char *token);
void matrix_login(const char *server, const char *user, const char *password);
void matrix_free_event(MatrixEvent *);

#endif
