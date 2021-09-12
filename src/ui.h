#ifndef JANECHAT_UI_H
#define JANECHAT_UI_H

enum UiEventType {
	UIEVENTTYPE_SYNC,
	UIEVENTTYPE_NONE,
	UIEVENTTYPE_SENDMSG,
};

struct UiEvent {
	enum UiEventType type;
	union {
		struct UiEventSendMessage {
			const char *roomid;
			const char *text;
		} msg;
		/*
		 * UIEVENTTYPE_SYNC and UIEVENTTYPE_NONE have an associated
		 * empty struct.
		 */
	};
};

typedef struct UiEvent UiEvent;

#endif
