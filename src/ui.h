#ifndef JANECHAT_UI_H
#define JANECHAT_UI_H

#include "str.h"

enum UiEventType {
	UIEVENTTYPE_SYNC,
	UIEVENTTYPE_SENDMSG,
};

struct UiEvent {
	enum UiEventType type;
	union {
		struct UiEventSendMessage {
			Str *roomid;
			Str *text;
		} msg;
		/*
		 * UIEVENTTYPE_SYNC has an associated
		 * empty struct.
		 */
	};
};

typedef struct UiEvent UiEvent;

extern void (*ui_event_handler_callback)(UiEvent);

void ui_set_event_handler(void (*callback)(UiEvent));

#endif
