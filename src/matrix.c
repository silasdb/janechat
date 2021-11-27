/**
 * In this file there is the json decode of the Matrix protocol, according to
 * the protocol specified in https://matrix.org/docs/spec/client_server/latest
 *
 * External JSON parsers are used.  For now, only jansson
 * (https://github.com/akheron/jansson/) is supported
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>
#include <jansson.h>

#include "list.h"
#include "str.h"
#include "matrix.h"
#include "utils.h"

#define DEBUG_REQUEST 0
#define DEBUG_RESPONSE 0

struct callback_info {
	void (*callback)(const char *);
	Str *data;
};

enum HTTPMethod {
	HTTP_GET,
	HTTP_POST,
};

char *next_batch = NULL;
char *token = NULL;
const char *matrix_server = NULL;
static void (*event_handler_callback)(MatrixEvent) = NULL;

CURLM *mhandle = NULL;
fd_set fdread;
fd_set fdwrite;
fd_set fdexcep;
int maxfd = -1;
bool insync = false;

/*
 * -1 -> initial state
 *  0 -> no transfers
 * >0 -> there are transfer running
 */
int still_running = -1;

void matrix_set_event_handler(void (*callback)(MatrixEvent)) {
	event_handler_callback = callback;
}

/* Callback used for libcurl to retrieve web content. */
static size_t
send_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	Str *s = (Str *)userp;
	str_append_cstr_len(s, contents, size*nmemb);
	return size * nmemb;
}

Str * matrix_send_sync_alloc(
	enum HTTPMethod method,
	const char *path,
	const char *json)
{
	Str *url = str_new();
	str_append_cstr(url, "https://");
	assert(matrix_server != NULL);
	str_append_cstr(url, matrix_server);
	str_append_cstr(url, ":443");
	str_append_cstr(url, path);
	CURL *handle = curl_easy_init();
	assert(handle);
	CURLcode res;
	Str *aux = str_new();
	curl_easy_setopt(handle, CURLOPT_URL, str_buf(url));
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, send_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)aux);
	res = curl_easy_perform(handle);
	if (res != CURLE_OK) {
		fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
		return NULL;
	}
	curl_easy_cleanup(handle);
	str_decref(url);
	return aux;
}

static void matrix_send_async(
	enum HTTPMethod method,
	const char *path,
	const char *json,
	void (*callback)(const char *))
{
	struct callback_info *c = malloc(sizeof(struct callback_info));
	c->callback = callback;
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

	CURL *handle = NULL;

	if (!mhandle)
		mhandle = curl_multi_init();

	handle = curl_easy_init();
	Str *aux = str_new();

	c->data = aux;
	
	Str *url = str_new();
	
	str_append_cstr(url, "https://");
	assert(matrix_server != NULL);
	str_append_cstr(url, matrix_server);
	str_append_cstr(url, ":443");
	str_append_cstr(url, path);

#if DEBUG_REQUEST
	printf("DEBUG_REQUEST: url: %s\n", str_buf(url));
	if (json)
		printf("DEBUG_REQUEST: json: %s\n", json);
#endif
	
	/*
	 * TODO: libcurl timeouts with SIGALRM, so we need to caught this signal
	 * so the program doesn't abort.
	 */
	curl_easy_setopt(handle, CURLOPT_TIMEOUT, 60L);
	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 60L);
	curl_easy_setopt(handle, CURLOPT_URL, str_buf(url));
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, send_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)aux);
	curl_easy_setopt(handle, CURLOPT_PRIVATE, (void *)c);

	switch (method) {
	case HTTP_POST:
		/*
		 * TODO: According to
		 * https://curl.se/libcurl/c/curl_easy_setopt.html, we cannot
		 * handle strings longer than 8 MB.  Should we validate the
		 * string length?
		 */
		curl_easy_setopt(handle, CURLOPT_POST, 1L);
		curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, (long)strlen(json));
		curl_easy_setopt(handle, CURLOPT_COPYPOSTFIELDS, json);
		break;
	default:
		break;
	}

	curl_multi_add_handle(mhandle, handle);
	str_decref(url);
	curl_multi_perform(mhandle, &still_running);
}

/*
 * Given a json_t *root object, returns a deep nested object whose path matches
 * variable argument path. The argument list must end with NULL to indicate it
 * has finished. To return the first object (jansson preserves the order in
 * which they are inserted), use "^".
 *
 * Example: in order to retrieve object that has jq's path .foo.bar, one should
 * call this function as json_path(obj, "foo", "bar", NULL)
 */
static json_t *json_path(json_t *root, const char *path, ...) {
	va_list args;
	va_start(args, path);
	if (!path)
		return root;
	do {
		if (*path == '^') {
			void *iter = json_object_iter(root);
			root = json_object_iter_value(iter);
		} else {
			root = json_object_get(root, path);
		}
		if (!root)
			return root;
		path = va_arg(args, const char *);
	} while (path != NULL);
	return root;
}

static const char *json2str_alloc(json_t *j) {
	return json_dumps(j, 0);
}

static json_t *str2json_alloc(const char *s) {
	json_t *j;
	/*
	 * TODO: propagate JSON parsing errors up, so we be able to show it in
	 * the UI.
	 */
	json_error_t error;
	j = json_loads(s, 0, &error);
	if (!j) {
		printf("Error when parsing JSON line %d: %s\n", error.line, error.text);
		return NULL;
	}
	return j;
}

void matrix_send_message(const Str *roomid, const Str *msg) {
	Str *url = str_new();
	str_append_cstr(url, "/_matrix/client/r0/rooms/");
	str_append_cstr(url, str_buf(roomid));
	str_append_cstr(url, "/send/m.room.message?access_token=");
	str_append_cstr(url, token);
	json_t *root = json_object();
	json_object_set(root, "msgtype", json_string("m.text"));
	json_object_set(root, "body", json_string(str_buf(msg)));
	const char *s = json2str_alloc(root);
	matrix_send_async(HTTP_POST, str_buf(url), s, NULL);
	free((void *)s);
	str_decref(url);
}

void matrix_set_server(char *s) {
	matrix_server = s;
}

void matrix_set_token(char *tok) {
	token = tok;
}

static void process_direct_event(const char *sender, json_t *roomid) {
	MatrixEvent event;
	event.type = EVENT_ROOM_NAME;
	event.roomname.id = str_new_cstr(json_string_value(roomid));
	event.roomname.name = str_new_cstr(sender);
	event_handler_callback(event);
	str_decref(event.roomname.id);
	str_decref(event.roomname.name);
}

static void process_room_event(json_t *item, const char *roomid) {
	json_t *type = json_object_get(item, "type");
	assert(type != NULL);
	if (streq(json_string_value(type), "m.room.name")) {
		json_t *nam = json_path(item, "content", "name", NULL);
		assert(nam != NULL);
		const char *name = json_string_value(nam);
		MatrixEvent event;
		event.type = EVENT_ROOM_NAME;
		event.roomname.id = str_new_cstr(roomid);
		event.roomname.name = str_new_cstr(name);
		event_handler_callback(event);
		str_decref(event.roomname.id);
		str_decref(event.roomname.name);
	} else if (streq(json_string_value(type), "m.room.create")) {
		MatrixEvent event;
		event.type = EVENT_ROOM_CREATE;
		event.roomcreate.id = str_new_cstr(roomid);
		event_handler_callback(event);
		str_decref(event.roomcreate.id);
	} else if (streq(json_string_value(type), "m.room.member")) {
		json_t *membership = json_path(item, "content", "membership", NULL);
		assert(membership != NULL);
		if (!streq(json_string_value(membership), "join"))
			return;
		json_t *sender = json_object_get(item, "sender");
		assert(sender != NULL);
		MatrixEvent event;
		event.type = EVENT_ROOM_JOIN;
		event.roomjoin.roomid = str_new_cstr(roomid);
		event.roomjoin.sender = str_new_cstr(json_string_value(sender));
		event_handler_callback(event);
		str_decref(event.roomjoin.roomid);
		str_decref(event.roomjoin.sender);
	}
}

enum SelectStatus select_matrix_stdin(void) {
	struct timeval timeout;
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
	/* TODO: why (maxfd + 2) ?*/
	int res = select(maxfd + 2, &fdread, &fdwrite, &fdexcep, &timeout);
	switch (res) {
	case -1:
		if (errno == EINTR)
			return SELECTSTATUS_MATRIXRESUME;
		perror("select()");
		abort(); /* TODO */
		break;
	case 0:
		if (still_running)
			return SELECTSTATUS_MATRIXRESUME;
		break;
	default: /* res > 0 */
		if (FD_ISSET(0, &fdread))
			return SELECTSTATUS_STDINREADY;
		break;
	}
	return SELECTSTATUS_MATRIXRESUME;
}

static void process_timeline_event(json_t *item, const char *roomid) {
	json_t *type = json_object_get(item, "type");
	assert(type != NULL);
	if ((!streq(json_string_value(type), "m.room.message"))
	&& (!streq(json_string_value(type), "m.room.encrypted")))
		return;
	json_t *sender = json_object_get(item, "sender");
	assert(sender != NULL);
	json_t *content = json_object_get(item, "content");
	assert(content != NULL);
	if (streq(json_string_value(type), "m.room.message")) {
		json_t *msgtype = json_object_get(content, "msgtype");
		assert(msgtype != NULL);
		json_t *body = json_object_get(content, "body");
		assert(body != NULL);
		MatrixEvent event;
		event.type = EVENT_MSG;
		event.msg.sender = str_new_cstr(json_string_value(sender));
		event.msg.roomid = str_new_cstr(roomid);
		if (streq(json_string_value(msgtype), "m.text"))
			event.msg.text = str_new_cstr(json_string_value(body));
		else {
			event.msg.text = str_new();
			str_append_cstr(event.msg.text, "==== ");
			str_append_cstr(event.msg.text, "TODO: Type not supported: ");
			str_append_cstr(event.msg.text, json_string_value(msgtype));
			str_append_cstr(event.msg.text, " ====");
		}
		event_handler_callback(event);
		str_decref(event.msg.sender);
		str_decref(event.msg.roomid);
		str_decref(event.msg.text);
	} else if (streq(json_string_value(type), "m.room.encrypted")) {
		MatrixEvent event;
		event.type = EVENT_MSG;
		event.msg.sender = str_new_cstr(json_string_value(sender));
		event.msg.roomid = str_new_cstr(roomid);
		event.msg.text = str_new_cstr("== encrypted message ==");
		event_handler_callback(event);
		str_decref(event.msg.sender);
		str_decref(event.msg.roomid);
		str_decref(event.msg.text);
	}
}

static void process_error(json_t *root) {
	MatrixEvent event;
	event.type = EVENT_ERROR;
	event.error.errorcode = str_new_cstr(json_string_value(json_object_get(root, "errcode")));
	event.error.error = str_new_cstr(json_string_value(json_object_get(root, "error")));
	event_handler_callback(event);
	str_decref(event.error.errorcode);
	str_decref(event.error.error);
}

static void process_sync_response(const char *output) {
	insync = false;
	if (!output) {
		MatrixEvent event;
		event.type = EVENT_CONN_ERROR;
		event_handler_callback(event);
		return;
	}
	json_t *root;
	root = str2json_alloc(output);
	if (!root)
		return;
	json_t *errorcode = json_object_get(root, "errcode");
	if (errorcode) {
		process_error(root);
		json_decref(root);
		return;
	}
	json_t *tok = json_object_get(root, "access_token");
	if (tok) {
		assert(token == NULL);
		token = strdup(json_string_value(tok));
		MatrixEvent event;
		event.type = EVENT_LOGGED_IN;
		event.login.token = str_new_cstr(token);
		json_decref(root);
		event_handler_callback(event);
		str_decref(event.login.token);
		return;
	}

	json_t *rooms = json_object_get(root, "rooms");
	if (!rooms) {
		json_decref(root);
		return;
	}
	json_t *join = json_object_get(rooms, "join");
	if (!join) {
		json_decref(root);
		return;
	}

	const char *roomid;
	json_t *item;

	/*
	 * m.room.create events come in room state events list, which is
	 * unsorted.  But when passing EVENT_ROOM_CREATE events to upper layers,
	 * we have to pass it before other events that alter the room state
	 * (because the room object need already to be created in order to
	 * receive these other events), so we look for events of this type first.
	 */
	json_object_foreach(join, roomid, item) {
		json_t *events;
		events = json_path(item, "state", "events", NULL);
		size_t i;
		json_t *event;
		json_array_foreach(events, i, event) {
			json_t *type = json_object_get(event, "type");
			if (streq(json_string_value(type), "m.room.create"))
				process_room_event(event, roomid);
		}
	}

	json_t *events = json_path(root, "account_data", "events", NULL);
	if (events) {
		size_t i;
		json_t *item;
		json_array_foreach(events, i, item) {
			assert(item != NULL);
			json_t *type = json_object_get(item, "type");
			assert(type != NULL);
			const char *t;
			t = json_string_value(type);
			if (streq(t, "m.direct")) {
				json_t *content = json_object_get(item, "content"); 
				const char *sender;
				json_t *roomid;
				json_object_foreach(content, sender, roomid) {
					size_t i;
					json_t *it;
					json_array_foreach(roomid, i, it)
						process_direct_event(sender, it);
				}
			}
		}
	}

	json_object_foreach(join, roomid, item)
	{
		json_t *events;
		events = json_path(item, "state", "events", NULL);
		assert(events != NULL);
		size_t i;
		json_t *event;
		json_array_foreach(events, i, event) {
			json_t *type = json_object_get(event, "type");
			if (!streq(json_string_value(type), "m.room.create"))
				process_room_event(event, roomid);
		}
		events = json_path(item, "timeline", "events", NULL);
		assert(events != NULL);
		json_array_foreach(events, i, item) {
			assert(item != NULL);
			process_timeline_event(item, roomid);
		}
	}

	free(next_batch);
	json_t *n = json_object_get(root, "next_batch");
	assert(n != NULL);
	next_batch = strdup(json_string_value(n));
	json_decref(root);
}


bool matrix_initial_sync(void) {
	Str *url = str_new();
	str_append_cstr(url, "/_matrix/client/r0/sync");
	str_append_cstr(url, "?filter={\"room\":{\"timeline\":{\"limit\":1}}}");
	str_append_cstr(url, "&access_token=");
	str_append_cstr(url, token);
	Str *res = matrix_send_sync_alloc(HTTP_GET, str_buf(url), NULL);
	if (!res)
		return false;
	process_sync_response(str_buf(res));
	str_decref(res);
	return true;
}

void matrix_sync(void) {
	if (insync)
		return;
	insync = true;
	Str *url = str_new();
	str_append_cstr(url, "/_matrix/client/r0/sync");
	str_append_cstr(url, "?since=");
	str_append_cstr(url, next_batch);
	str_append_cstr(url, "&timeout=10000");
	str_append_cstr(url, "&access_token=");
	str_append_cstr(url, token);
	matrix_send_async(HTTP_GET, str_buf(url), NULL, process_sync_response);
	str_decref(url);
}

void matrix_login(const char *server, const char *user, const char *password) {
	matrix_server = strdup(server);
	json_t *root = json_object();
	json_object_set(root, "type", json_string("m.login.password"));
	json_object_set(root, "user", json_string(user));
	json_object_set(root, "password", json_string(password));
	json_object_set(root, "initial_device_display_name", json_string("janechat"));
	const char *s = json2str_alloc(root);
	matrix_send_async(HTTP_POST, "/_matrix/client/r0/login", s, process_sync_response);
	free((void *)s);
}

void matrix_resume(void) {
	curl_multi_perform(mhandle, &still_running);
	
	/* TODO: check return value of curl_multi_perform() for errors */
	/*
	TODO: also, check for errors in easy handler
	if (res != CURLE_OK) {
		printf("curl error: %s\n", curl_easy_strerror(res));
	}
	*/
	int msgs_in_queue;
	CURLMsg *msg;
	msg = curl_multi_info_read(mhandle, &msgs_in_queue);
	if (msg && msg->msg == CURLMSG_DONE) {
		CURL *handle;
		handle = msg->easy_handle;
		struct callback_info *c;
		curl_easy_getinfo(handle, CURLINFO_PRIVATE, &c); /* TODO: Check return code */
#if DEBUG_RESPONSE
		printf("DEBUG_RESPONSE: output: %s\n", str_buf(c->data));
#endif
		curl_multi_remove_handle(mhandle, handle);
		curl_easy_cleanup(handle);
		if (c->callback)
			c->callback(str_buf(c->data));
		str_decref(c->data);
		free(c);
	}
}
