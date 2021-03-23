/**
 * In this file there is the json decode of the Matrix protocol, according to
 * the protocol specified in https://matrix.org/docs/spec/client_server/latest
 *
 * External JSON parsers are used.  For now, one of the following parsers are
 * needed:
 *
 * - jansson (https://github.com/akheron/jansson/) (preferred)
 * - json-c (https://github.com/json-c/json-c)
 *
 * More files will be supported in the future, or maybe we'll embed one?
 *
 * We set macros to function names of parsers and try to use them whenever
 * possible, but there is some places where we cannot just use macros and have
 * to separate code related to each parser using #if .. #else .. #endif
 * conditionals.
 *
 * This code turned to be rather confusing.  In the future, we might create a
 * generic json wrapper for every parser (json-wrapper-jansson.c,
 * json-wrapper-json-c.c, etc.) that share a common interface (json-wrapper.h)
 * so we don't have to deal with different parsers in the same code file.
 */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if HAVE_JANSSON
# include <jansson.h>

# define J_T		json_t
# define J_NEWOBJ	json_object
# define J_GETSTR	json_string_value
# define J_NEWSTR	json_string
# define J_OBJADD	json_object_set
# define J_OBJGET	json_object_get
# define J_FREE		json_decref

#elif HAVE_JSONC
# include <json.h>

# define J_T		json_object
# define J_NEWOBJ	json_object_new_object
# define J_GETSTR	json_object_get_string
# define J_NEWSTR	json_object_new_string
# define J_OBJADD	json_object_object_add
# define J_OBJGET	json_object_object_get
# define J_FREE		json_object_put

#else
# error No jansson nor json-c found.
#endif

#include "list.h"
#include "strbuf.h"
#include "matrix.h"
#include "utils.h"

#define DEBUG_REQUEST 0
#define DEBUG_RESPONSE 0

char *next_batch = NULL;
char *token = NULL;
const char *matrix_server = NULL;

static void enqueue_event(MatrixEvent *);
static void send(const char *method, const char *path, const char *json);

List *event_queue = NULL;

void matrix_send_message(const char *roomid, const char *msg) {
	StrBuf *url = strbuf_new();
	strbuf_cat_c(url, "/_matrix/client/r0/rooms/");
	strbuf_cat_c(url, roomid);
	strbuf_cat_c(url, "/send/m.room.message?access_token=");
	strbuf_cat_c(url, token);
	J_T *root = J_NEWOBJ();
	J_OBJADD(root, "msgtype", J_NEWSTR("m.text"));
	J_OBJADD(root, "body", J_NEWSTR(msg));
#if HAVE_JANSSON
	char *s = json_dumps(root, 0);
	send("-XPOST", strbuf_buf(url), s);
	free(s);
#elif HAVE_JSONC
	send("-XPOST", strbuf_buf(url), json_object_to_json_string(root));
#endif
	strbuf_free(url);
}

void matrix_set_server(char *s) {
	matrix_server = s;
}

void matrix_set_token(char *tok) {
	token = tok;
}


void matrix_sync() {
	StrBuf *url = strbuf_new();
	strbuf_cat_c(url, "/_matrix/client/r0/sync");
	if (!next_batch)
		strbuf_cat_c(url, "?filter={\"room\":{\"timeline\":{\"limit\":1}}}");
	else {
		/*
		 * TODO: because of the blocking nature of janechat (curl
		 * "blocks" until it receives a response) we set timeout=5000 (5
		 * seconds) to prevent long polling, otherwise we would not be
		 * able to return to the main loop that reads stdin.  This will
		 * be fixed in the future.
		 */
		strbuf_cat_c(url, "?since=");
		strbuf_cat_c(url, next_batch);
		strbuf_cat_c(url, "&timeout=5000");
	}
	strbuf_cat_c(url, "&access_token=");
	strbuf_cat_c(url, token);
	send("-XGET", strbuf_buf(url), NULL);
	strbuf_free(url);
}

void matrix_login(const char *server, const char *user, const char *password) {
	matrix_server = server;
	J_T *root = J_NEWOBJ();
	J_OBJADD(root, "type", J_NEWSTR("m.login.password"));
	J_OBJADD(root, "user", J_NEWSTR(user));
	J_OBJADD(root, "password", J_NEWSTR(password));
	J_OBJADD(root, "initial_device_display_name", J_NEWSTR("janechat"));
#if HAVE_JANSSON
	char *s = json_dumps(root, JSON_COMPACT);
	assert(s != NULL);
	send("-XPOST", "/_matrix/client/r0/login", s);
	free(s);
#elif HAVE_JSONC
	send("-XPOST", "/_matrix/client/r0/login", json_object_to_json_string(root));
#endif
}

static void process_direct_event(const char *sender, J_T *roomid) {
	char *id = strdup(J_GETSTR(roomid));
	MatrixEvent *event = malloc(sizeof(MatrixEvent));
	event->type = EVENT_ROOM;
	event->room.id = strdup(id);
	event->room.name = strdup(sender);
	enqueue_event(event);
}

static void process_room_event(J_T *item, const char *roomid) {
	J_T *type = J_OBJGET(item, "type");
	assert(type != NULL);
	if (strcmp(J_GETSTR(type), "m.room.name") == 0) {
		J_T *content = J_OBJGET(item, "content");
		assert(content != NULL);
		J_T *nam = J_OBJGET(content, "name");
		assert(nam != NULL);
		const char *name = J_GETSTR(nam);
		char *id = strdup(roomid);
		char *nn = strdup(name);
		MatrixEvent *event = malloc(sizeof(MatrixEvent));
		event->type = EVENT_ROOM;
		event->room.id = id;
		event->room.name = nn;
		enqueue_event(event);
	}
}

static void process_timeline_event(J_T *item, const char *roomid) {
	J_T *type = J_OBJGET(item, "type");
	assert(type != NULL);
	if ((strcmp(J_GETSTR(type), "m.room.message") != 0)
	&& (strcmp(J_GETSTR(type), "m.room.encrypted") != 0))
		return;
	J_T *sender = J_OBJGET(item, "sender");
	assert(sender != NULL);
	J_T *content = J_OBJGET(item, "content");
	assert(content != NULL);
	if (strcmp(J_GETSTR(type), "m.room.message") == 0) {
		J_T *msgtype = J_OBJGET(content, "msgtype");
		assert(msgtype != NULL);
		if (strcmp(J_GETSTR(msgtype), "m.text") != 0) {
			printf("==== TODO: Type not supported: %s====\n",
				J_GETSTR(msgtype));
			return;
		}
		J_T *body = J_OBJGET(content, "body");
		assert(body != NULL);
		MatrixEvent *event = malloc(sizeof(MatrixEvent));
		event->type = EVENT_MSG;
		event->msg.sender = strdup(J_GETSTR(sender));
		event->msg.roomid = strdup(roomid);
		event->msg.text = strdup(J_GETSTR(body));
		enqueue_event(event);
	} else if (strcmp(J_GETSTR(type), "m.room.encrypted") == 0) {
		MatrixEvent *event = malloc(sizeof(MatrixEvent));
		event->type = EVENT_MSG;
		event->msg.sender = strdup(J_GETSTR(sender));
		event->msg.roomid = strdup(roomid);
		event->msg.text = strdup("== encrypted message ==");
		enqueue_event(event);
	}
}

static void process_error(J_T *root) {
	MatrixEvent *event = malloc(sizeof(MatrixEvent));
	event->type = EVENT_ERROR;
	event->error.errorcode = strdup(J_GETSTR(J_OBJGET(root, "errcode")));
	event->error.error = strdup(J_GETSTR(J_OBJGET(root, "error")));
	enqueue_event(event);
}

static void process_matrix_response(const char *output) {
	J_T *root;
#if HAVE_JANSSON
	json_error_t error;
	root = json_loads(output, 0, &error);
	if (!root) {
		printf("Error when parsing JSON line %d: %s\n", error.line, error.text);
		return;
	}
#elif HAVE_JSONC
	root = json_tokener_parse(output);
#endif
	J_T *errorcode = J_OBJGET(root, "errcode");
	if (errorcode) {
		process_error(root);
		J_FREE(root);
		return;
	}
	J_T *tok = J_OBJGET(root, "access_token");
	if (tok) {
		assert(token == NULL);
		token = strdup(J_GETSTR(tok));
		MatrixEvent *event = malloc(sizeof(MatrixEvent));
		event->type = EVENT_LOGGED_IN;
		event->login.token = strdup(token);
		enqueue_event(event);
		J_FREE(root);
		return;
	}
	J_T *account_data = J_OBJGET(root, "account_data");
	if (account_data) {
		J_T *events = J_OBJGET(account_data, "events");
		assert(events != NULL);
#if HAVE_JANSSON
		size_t i;
		J_T *item;
		json_array_foreach(events, i, item) {
#elif HAVE_JSONC
		for (size_t i = 0; i < json_object_array_length(events); i++) {
			J_T *item = json_object_array_get_idx(events, i);
#endif
			assert(item != NULL);
			J_T *type = J_OBJGET(item, "type");
			assert(type != NULL);
			const char *t;
			t = J_GETSTR(type);
			if (strcmp(t, "m.direct") == 0) {
#if HAVE_JANSSON
				J_T *content = J_OBJGET(item, "content"); 
				const char *sender;
				J_T *roomid;
				json_object_foreach(content, sender, roomid) {
					size_t i;
					J_T *it;
					json_array_foreach(roomid, i, it)
						process_direct_event(sender, it);
				}
#elif HAVE_JSONC
				json_object *content = J_OBJGET(item, "content"); 
				json_object_object_foreach(content, sender, roomid) {
					for (size_t i = 0; i < json_object_array_length(roomid); i++) {
						json_object *item = json_object_array_get_idx(roomid, i);
						process_direct_event(sender, item);
					}
				}
#endif
			}
		}
	}
	J_T *rooms = J_OBJGET(root, "rooms");
	if (!rooms) {
		J_FREE(root);
		return;
	}
	J_T *join = J_OBJGET(rooms, "join");
	if (!join) {
		J_FREE(root);
		return;
	}
#if HAVE_JANSSON
	const char *roomid;
	J_T *item;
	json_object_foreach(join, roomid, item)
#elif HAVE_JSONC
	json_object_object_foreach(join, roomid, item)
#endif
	{
		J_T *state = J_OBJGET(item, "state");
		assert(state != NULL);
		J_T *events = J_OBJGET(state, "events");
		assert(events != NULL);
#if HAVE_JANSSON
		size_t i;
		J_T *event;
		json_array_foreach(events, i, event) {
			assert(item != NULL);
			process_room_event(event, roomid);
		}
#elif HAVE_JSONC
		for (size_t i = 0; i < json_object_array_length(events); i++) {
			json_object *item = json_object_array_get_idx(events, i);
			assert(item != NULL);
			process_room_event(item, roomid);
		}
#endif
		J_T *timeline = J_OBJGET(item, "timeline");
		assert(timeline != NULL);
		events = J_OBJGET(timeline, "events");
		assert(events != NULL);
#if HAVE_JANSSON
		json_array_foreach(events, i, item) {
			assert(item != NULL);
			process_timeline_event(item, roomid);
		}
#elif HAVE_JSONC
		for (size_t i = 0; i < json_object_array_length(events); i++) {
			json_object *item = json_object_array_get_idx(events, i);
 			assert(item != NULL);
 			process_timeline_event(item, roomid);
 		}
#endif
	}

	free(next_batch);
	J_T *n = J_OBJGET(root, "next_batch");
	assert(n != NULL);
	next_batch = strdup(J_GETSTR(n));
	J_FREE(root);
}

static void enqueue_event(MatrixEvent *event) {
	if (!event_queue)
		event_queue = list_new();
	list_append(event_queue, event);
}

// a.k.a. dequeue_event
MatrixEvent *matrix_next_event() {
	if (!event_queue)
		return NULL;
	return list_pop_head(event_queue);
}

void matrix_free_event(MatrixEvent *event) {
	switch (event->type) {
	case EVENT_MSG:
		free(event->msg.roomid);
		free(event->msg.sender);
		free(event->msg.text);
		break;
	case EVENT_ROOM:
		free(event->room.id);
		free(event->room.name);
		break;
	case EVENT_ERROR:
		free(event->error.errorcode);
		free(event->error.error);
		break;
	case EVENT_LOGGED_IN:
		free(event->login.token);
		break;
	}
	free(event);
}

static char *find_curl() {
	char *pathenv = strdup(getenv("PATH"));
	char *p;
	char *ret = NULL;
	char path[PATH_MAX];

	p = strtok(pathenv, ":");
	do {
		snprintf(path, sizeof(path), "%s/curl", p);
		if (access(path, X_OK) == 0) {
			ret = strdup(path);
			break;
		}
	} while ((p = strtok(NULL, ":")) != NULL);

	free(pathenv);
	
	return ret;
}

static void send(const char *method, const char *path, const char *json) {
	StrBuf *url = strbuf_new();
	strbuf_cat_c(url, "https://");
	assert(matrix_server != NULL);
	strbuf_cat_c(url, matrix_server);
	strbuf_cat_c(url, ":443");
	strbuf_cat_c(url, path);

	const char *argv[11];
	if (json != NULL) {
		argv[0] = method;
		argv[1] = "-d";
		argv[2] = json;
		argv[3] = "-s";
		argv[4] = "--globoff";
		argv[5] = "--connect-timeout";
		argv[6] = "60";
		argv[7] = "--max-time";
		argv[8] = "60";
		argv[9] = strbuf_buf(url);
		argv[10] = NULL;
	} else {
		argv[0] = method;
		argv[1] = "-s";
		argv[2] = "--globoff";
		argv[3] = "--connect-timeout";
		argv[4] = "60";
		argv[5] = "--max-time";
		argv[6] = "60";
		argv[7] = strbuf_buf(url);
		argv[8] = NULL;
	}
#if DEBUG_REQUEST
	/* Don't debug username/password information. */
	if (strcmp(path, "/_matrix/client/r0/login") != 0) {
		const char **aux = argv;
		while (*aux != NULL)
			printf("%s ", *(aux++));
		puts("\n");
	}
#endif
	/* TODO: popenve exists only in NetBSD? */
	char *curl = find_curl();
	if (!curl) {
		fprintf(stderr, "Error: curl not found on this system\n");
		exit(1);
	}
	FILE *f;
	f = popenve(curl, (char *const *)argv, NULL, "rw");
	free(curl);
	if (!f)
		exit(1);
	char *output = read_file(f);
	if (!output) {
		strbuf_free(url);
		pclose(f);
		return;
	}
#if DEBUG_RESPONSE
	printf("%s\n", output);
#endif
	strbuf_free(url);
	pclose(f);
	process_matrix_response(output);
	free(output);
}
