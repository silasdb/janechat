/* Common definitions for the whole program */

#ifndef JANECHAT_COMMON_H
#define JANECHAT_COMMON_H

#include "str.h"

struct Msg {
	enum {
		MSGTYPE_TEXT,
		MSGTYPE_FILE,
		MSGTYPE_UNSUPPORTED,
	} type;
	Str *sender;
	union {
		struct {
			Str *content;
		} text;
		struct {
			enum FileType {
				FILETYPE_IMAGE,
			} type;
			Str *url;
		} file;
	};
};

typedef struct Msg Msg;

#endif /* !JANECHAT_COMMON_H */
