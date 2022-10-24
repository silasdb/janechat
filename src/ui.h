#ifndef JANECHAT_UI_H
#define JANECHAT_UI_H

#include "common.h"
#include "str.h"

enum UiEventType {
	UIEVENTTYPE_SYNC,
	UIEVENTTYPE_SENDMSG,
	UIEVENTTYPE_OPENATTACHMENT,
	UIEVENTTYPE_NOTIFYSTATUS,
	UIEVENTTYPE_ROOM_RENAME,
};

struct UiEvent {
	enum UiEventType type;
	union {
		struct UiEventSendMessage {
			Str *roomid;
			Str *text;
		} msg;
		struct UiEventOpenAttachment {
			FileInfo fileinfo;
		} openattachment;
		struct UiEventNotifyStatus {
			Str *roomid;
			bool enabled;
		} roomnotifystatus;
		struct UiEventRoomRename {
			Str *roomid;
			Str *name;
		} roomrename;
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
