#ifndef JANECHAT_UI_H
#define JANECHAT_UI_H

#include "strbuf.h"

enum UiEventType {
	UIEVENTTYPE_SYNC,
	UIEVENTTYPE_SENDMSG,
};

struct UiEvent {
	enum UiEventType type;
	union {
		struct UiEventSendMessage {
			StrBuf *roomid;
			StrBuf *text;
		} msg;
		/*
		 * UIEVENTTYPE_SYNC and UIEVENTTYPE_NONE have an associated
		 * empty struct.
		 */
	};
};

typedef struct UiEvent UiEvent;

#endif
