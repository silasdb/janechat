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
			/*
			 * TODO: mimetype can assume a relative small number of
			 * possible values. Would it be better to work with an
			 * enum or a static read-only string? (e.g. mimetype =
			 * "image/jpeg"), saving some bytes of memory?
			 */
			Str *mimetype;

			Str *url;
		} file;
	};
};

typedef struct Msg Msg;

#endif /* !JANECHAT_COMMON_H */
