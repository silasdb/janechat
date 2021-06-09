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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

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

enum HTTPMethod {
	HTTP_GET,
	HTTP_POST,
};

char *next_batch = NULL;
char *token = NULL;
const char *matrix_server = NULL;

static void event_queue_append(MatrixEvent *);
static void event_queue_prepend(MatrixEvent *);
static void matrix_send(enum HTTPMethod, const char *, const char *,
	void (*callback)(const char *));
static const char * json2str_alloc(J_T *);
static J_T *str2json_alloc(const char *);
static void process_sync_response(const char *);

List *event_queue = NULL;

CURLM *mhandle = NULL;
CURL *handle = NULL;
StrBuf *aux = NULL;
fd_set fdread;
fd_set fdwrite;
fd_set fdexcep;
int maxfd = -1;
void (*callback)(const char *);

/*
 * -1 -> initial state
 *  0 -> no transfers
 * >0 -> there are transfer running
 */
int still_running = -1;

void matrix_send_message(const char *roomid, const char *msg) {
	StrBuf *url = strbuf_new();
	strbuf_cat_c(url, "/_matrix/client/r0/rooms/");
	strbuf_cat_c(url, roomid);
	strbuf_cat_c(url, "/send/m.room.message?access_token=");
	strbuf_cat_c(url, token);
	J_T *root = J_NEWOBJ();
	J_OBJADD(root, "msgtype", J_NEWSTR("m.text"));
	J_OBJADD(root, "body", J_NEWSTR(msg));
	const char *s = json2str_alloc(root);
	matrix_send(HTTP_POST, strbuf_buf(url), s, NULL);
	free((void *)s);
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
	matrix_send(HTTP_GET, strbuf_buf(url), NULL, process_sync_response);
	strbuf_free(url);
}

void matrix_login(const char *server, const char *user, const char *password) {
	matrix_server = strdup(server);
	J_T *root = J_NEWOBJ();
	J_OBJADD(root, "type", J_NEWSTR("m.login.password"));
	J_OBJADD(root, "user", J_NEWSTR(user));
	J_OBJADD(root, "password", J_NEWSTR(password));
	J_OBJADD(root, "initial_device_display_name", J_NEWSTR("janechat"));
	const char *s = json2str_alloc(root);
	matrix_send(HTTP_POST, "/_matrix/client/r0/login", s, process_sync_response);
	free((void *)s);
}

static void process_direct_event(const char *sender, J_T *roomid) {
	MatrixEvent *event;
	
	char *id = strdup(J_GETSTR(roomid));
	
	event = malloc(sizeof(MatrixEvent));
	event->type = EVENT_ROOM_NAME;
	event->roomname.id = strdup(id);
	event->roomname.name = strdup(sender);
	event_queue_append(event);
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
		event->type = EVENT_ROOM_NAME;
		event->roomname.id = id;
		event->roomname.name = nn;
		event_queue_append(event);
	} else if (strcmp(J_GETSTR(type), "m.room.create") == 0) {
		MatrixEvent *event = malloc(sizeof(MatrixEvent));
		event->type = EVENT_ROOM_CREATE;
		event->roomcreate.id = strdup(roomid);
		/*
		 * m.room.create events come in room state events list, which
		 * are unsorted.  But when passing EVENT_ROOM_CREATE events to
		 * upper layers, we have to pass it before other events that
		 * alter the room state (because the room object need already to
		 * be created in order to receive these other events), so we
		 * prepend it to the event queue.
		 */
		event_queue_prepend(event);
	} else if (strcmp(J_GETSTR(type), "m.room.member") == 0) {
		J_T *content = J_OBJGET(item, "content");
		assert(content != NULL);
		J_T *membership = J_OBJGET(content, "membership");
		assert(membership != NULL);
		if (strcmp(J_GETSTR(membership), "join") != 0)
			return;
		J_T *sender = J_OBJGET(item, "sender");
		assert(sender != NULL);
		MatrixEvent *event = malloc(sizeof(MatrixEvent));
		event->type = EVENT_ROOM_JOIN;
		event->roomjoin.roomid = strdup(roomid);
		event->roomjoin.sender = strdup(J_GETSTR(sender));
		event_queue_append(event);
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
		event_queue_append(event);
	} else if (strcmp(J_GETSTR(type), "m.room.encrypted") == 0) {
		MatrixEvent *event = malloc(sizeof(MatrixEvent));
		event->type = EVENT_MSG;
		event->msg.sender = strdup(J_GETSTR(sender));
		event->msg.roomid = strdup(roomid);
		event->msg.text = strdup("== encrypted message ==");
		event_queue_append(event);
	}
}

static void process_error(J_T *root) {
	MatrixEvent *event = malloc(sizeof(MatrixEvent));
	event->type = EVENT_ERROR;
	event->error.errorcode = strdup(J_GETSTR(J_OBJGET(root, "errcode")));
	event->error.error = strdup(J_GETSTR(J_OBJGET(root, "error")));
	event_queue_append(event);
}

static void process_sync_response(const char *output) {
	if (!output) {
		MatrixEvent *event = malloc(sizeof(MatrixEvent));
		event->type = EVENT_CONN_ERROR;
		event_queue_append(event);
		return;
	}
	J_T *root;
	root = str2json_alloc(output);
	if (!root)
		return;
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
		event_queue_append(event);
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

static void event_queue_append(MatrixEvent *event) {
	if (!event_queue)
		event_queue = list_new();
	list_append(event_queue, event);
}

static void event_queue_prepend(MatrixEvent *event) {
	if (!event_queue)
		event_queue = list_new();
	list_prepend(event_queue, event);
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
	case EVENT_ROOM_CREATE:
		free(event->roomcreate.id);
		break;
	case EVENT_ROOM_NAME:
		free(event->roomname.id);
		free(event->roomname.name);
		break;
	case EVENT_ROOM_JOIN:
		free(event->roomjoin.roomid);
		free(event->roomjoin.sender);
		break;
	case EVENT_ERROR:
		free(event->error.errorcode);
		free(event->error.error);
		break;
	case EVENT_LOGGED_IN:
		free(event->login.token);
		break;
	case EVENT_CONN_ERROR:
		// empty struct - do nothing
		break;
	}
	free(event);
}

/* Callback used for libcurl to retrieve web content. */
static size_t
send_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	StrBuf *s = (StrBuf *)userp;
	strbuf_ncat_c(s, contents, size * nmemb);
	return size * nmemb;
}

enum SelectStatus select_matrix_stdin() {
	if (still_running == -1)
		return SELECTSTATUS_MATRIXREADY;
	struct timeval timeout;
	long curl_timeo = -1;
	timeout.tv_sec = 0;
	timeout.tv_usec = 100;
	
	/* TODO: use curl_multi_timeout()? */

	FD_ZERO(&fdread);
	FD_ZERO(&fdwrite);
	FD_ZERO(&fdexcep);

	/* TODO: place it in a better place */
	FD_SET(0 /* TODO: stdin */, &fdread);
		
	curl_multi_fdset(
		mhandle,
		&fdread,
		&fdwrite,
		&fdexcep,
		&maxfd);
	int res = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
	if (res == -1) {
		perror("select()");
		abort(); /* TODO */
	}
	if (res == 0) {
		if (still_running)
			return SELECTSTATUS_MATRIXRESUME;
		return SELECTSTATUS_MATRIXREADY;
	}
	if (FD_ISSET(0, &fdread))
		return SELECTSTATUS_STDINREADY;
	if (still_running)
		return SELECTSTATUS_MATRIXRESUME;
	return SELECTSTATUS_MATRIXREADY;
}

void matrix_resume() {
	curl_multi_perform(mhandle, &still_running);
	
	/* TODO: check return value of curl_multi_perform() for errors */
	/*
	TODO: also, check for errors in easy handler
	if (res != CURLE_OK) {
		printf("curl error: %s\n", curl_easy_strerror(res));
	}
	*/
	if (still_running == 0) {
		char *output = strdup(strbuf_buf(aux));
		strbuf_free(aux);
		curl_multi_remove_handle(mhandle, handle);
		curl_easy_cleanup(handle);
		curl_multi_cleanup(mhandle);
		aux = NULL;
		handle = NULL;
		mhandle = NULL;
#if DEBUG_RESPONSE
		printf("DEBUG_RESPONSE: output: %s\n", output);
#endif
		if (callback)
			callback(output);
		free(output);
	}
}

static void matrix_send(
	enum HTTPMethod method,
	const char *path,
	const char *json,
	void (*c)(const char *))
{
	callback = c;
	static bool curl_initialized = false;
	if (!curl_initialized) {
		 /* TODO: we should enable only what we need */
		curl_global_init(CURL_GLOBAL_ALL);
		curl_initialized = true;
		/*
		 * TODO: should we call curl_global_cleanup() at the end of the
		 * program?
		 */
	}

	/* Quick-n-dirty message queue system! */
	struct msg {
		enum HTTPMethod method;
		const char *path;
		const char *json;
	};
	struct msg *m = malloc(sizeof(struct msg));
	m->method = method;
	m->path = strdup(path);
	m->json = NULL;
	if (json)
		m->json = strdup(json);
	static List *msgs = NULL;
	if (!msgs)
		msgs = list_new();
	if (mhandle) {
		list_append(msgs, m);
		return;
	}
	struct msg *m2;
	m2 = list_pop_head(msgs);
	if (m2) {
		list_append(msgs, m);
		m = m2;
	}

	mhandle = curl_multi_init();

	handle = curl_easy_init();
	aux = strbuf_new();
	
	StrBuf *url = strbuf_new();
	
	strbuf_cat_c(url, "https://");
	assert(matrix_server != NULL);
	strbuf_cat_c(url, matrix_server);
	strbuf_cat_c(url, ":443");
	strbuf_cat_c(url, m->path);

#if DEBUG_REQUEST
	printf("DEBUG_REQUEST: url: %s\n", strbuf_buf(url));
	if (m->json)
		printf("DEBUG_REQUEST: json: %s\n", m->json);
#endif
	
	/*
	 * TODO: libcurl timeouts with SIGALRM, so we need to caught this signal
	 * so the program doesn't abort.
	 */
	curl_easy_setopt(handle, CURLOPT_TIMEOUT, 60L);
	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 60L);
	curl_easy_setopt(handle, CURLOPT_URL, strbuf_buf(url));
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, send_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)aux);

	switch (m->method) {
	case HTTP_POST:
		curl_easy_setopt(handle, CURLOPT_POST, 1L);
		curl_easy_setopt(handle, CURLOPT_POSTFIELDS, m->json);
		curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, (long)strlen(m->json));
		break;
	default:
		break;
	}

	curl_multi_add_handle(mhandle, handle);
	CURLMcode res;
	strbuf_free(url);

	curl_multi_perform(mhandle, &still_running);

	/* TODO: I cannot free now because curl uses it asynchronously */
	//free(m->json);
	//free(m->path);
	free(m);
}

static const char *json2str_alloc(J_T *j) {
#if HAVE_JANSSON
	return json_dumps(j, 0);
#elif HAVE_JSONC
	return json_object_to_json_string(j);
#endif

}

static J_T *str2json_alloc(const char *s) {
	J_T *j;
	/*
	 * TODO: propagate JSON parsing errors up, so we be able to show it in
	 * the UI.
	 */
#if HAVE_JANSSON
	json_error_t error;
	j = json_loads(s, 0, &error);
	if (!j) {
		printf("Error when parsing JSON line %d: %s\n", error.line, error.text);
		return NULL;
	}
#elif HAVE_JSONC
	j = json_tokener_parse(s);
#endif
	if (j == NULL) {
		/* TODO: how to handle this error? */
		printf("Error when parsing string: %s\n", s);
		return NULL;
	}
	return j;
}
