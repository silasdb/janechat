#ifndef JANECHAT_MATRIX_H
#define JANECHAT_MATRIX_H

#include <fcntl.h>

#include "common.h"
#include "str.h"

enum MatrixEventType {
	EVENT_MSG,
	EVENT_ROOM_CREATE,
	EVENT_ROOM_INFO,
	EVENT_ROOM_JOIN,
	EVENT_ROOM_NOTIFY_STATUS,
	EVENT_MATRIX_ERROR,
	EVENT_CONN_ERROR,
};

struct MatrixEvent {
	enum MatrixEventType type;
	union {
		// TODO: why fields above are not const?
		struct MatrixEventMsg {
			Str *roomid;
			struct Msg msg;
		} msg;
		struct MatrixEventRoomCreate {
			Str *id;
		} roomcreate;
		struct MatrixEventRoomInfo {
			Str *id;
			Str *sender;
			Str *name;
		} roominfo;
		struct MatrixEventRoomNotifyStatus {
			Str *roomid;
			bool enabled;
		} roomnotifystatus;
		struct MatrixEventRoomJoin {
			Str *roomid;
			Str *senderid;
			Str *sendername;
		} roomjoin;
		struct MatrixEventMatrixError {
			Str *errorcode;
			Str *error;
		} error;
		// MatrixEventConnError - empty struct
	};
};

typedef struct MatrixEvent MatrixEvent;

enum SelectStatus {
	SELECTSTATUS_STDINREADY,
	SELECTSTATUS_MATRIXRESUME,
};

void matrix_set_event_handler(void (*callback)(MatrixEvent));
bool matrix_initial_sync(void);
void matrix_sync(void);
void matrix_send_message(const Str *roomid, const Str *msg);
void matrix_request_file(FileInfo);
MatrixEvent * matrix_next_event();
void matrix_set_server(char *token);
void matrix_set_token(char *token);
const char *matrix_login_alloc(const char *server, const char *user, const char *password);
void matrix_free_event(MatrixEvent *);
bool matrix_select(void);
void matrix_resume(void);
enum SelectStatus select_matrix_stdin();

extern fd_set fdread;

#endif /* !JANECHAT_MATRIX_H */
