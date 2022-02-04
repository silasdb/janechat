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

#include "cache.h"
#include "list.h"
#include "str.h"
#include "matrix.h"
#include "utils.h"

#define DEBUG_REQUEST 0
#define DEBUG_RESPONSE 0

/* Used for the /sync endpoint only. */
#define MATRIX_SYNC_TIMEOUT_MS 10000

/* Used for all opened sockets, hence for all enpoints. */
#define SOCKET_TIMEOUT_MS 60000

enum callback_info_type {
	CALLBACK_INFO_TYPE_SYNC,
	CALLBACK_INFO_TYPE_OTHER,
} type;

struct callback_info {
	void (*callback)(const char *, size_t, void *);
	Str *data;
	void *params;
	enum callback_info_type type;
};

enum HTTPMethod {
	HTTP_GET,
	HTTP_POST,
};

char *next_batch = NULL;
const char *token = NULL;
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

	/*
	 * TODO: disable SSL certificate verification so it works for our
	 * internal servers
	 */
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);

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
	enum callback_info_type type,
	const char *json,
	void (*callback)(const char *, size_t, void *),
	void *callback_params)
{
	struct callback_info *c = malloc(sizeof(struct callback_info));
	c->callback = callback;
	c->type = type;
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
	c->params = callback_params;
	
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
	curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, SOCKET_TIMEOUT_MS);
	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 60L);
	curl_easy_setopt(handle, CURLOPT_URL, str_buf(url));
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, send_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)aux);
	curl_easy_setopt(handle, CURLOPT_PRIVATE, (void *)c);

	/*
	 * TODO: disable SSL certificate verification so it works for our
	 * internal servers
	 */
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);

	/*
	 * TODO: libcurl by default uses HTTP 2 if possible. For the multi
	 * interface that we are using here, it tries to keep the same
	 * connection for different requests (for different easy handlers),
	 * which is a feature HTTP 2 support. But it fails if connection changes
	 * (e.g. if IP and/or routing table changes), affecting every easy
	 * handlers, even future ones, since libcurl tries to reuse connections,
	 * which are now dead. The solution found is to force libcurl to use
	 * HTTP 1.1 that creates a new connection for every request. It is less
	 * efficient, but works.
	 *
	 * For more info, see the following thread in the curl-library
	 * mailing list: https://curl.se/mail/lib-2022-01/0088.html
	 */
        curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

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
	matrix_send_async(HTTP_POST, str_buf(url), CALLBACK_INFO_TYPE_OTHER,
		s, NULL, NULL);
	free((void *)s);
	str_decref(url);
}

void matrix_receive_file(const char *output, size_t sz, void *p) {
	MatrixEvent event;
	event.type = EVENT_FILE;
	event.file.fileinfo = *((FileInfo *)p);
	event.file.payload = output;
	event.file.size = sz;
	event_handler_callback(event);
	free(p);
}

void matrix_request_file(FileInfo fileinfo) {
	Str *server = mxc_uri_extract_server_alloc(fileinfo.uri);
	Str *path = mxc_uri_extract_path_alloc(fileinfo.uri);
	Str *url = str_new_cstr("/_matrix/media/v3/download/");
	str_append(url, server);
	str_append_cstr(url, "/");
	str_append(url, path);
	str_decref(server);
	str_decref(path);
	FileInfo *fileinfoptr = malloc(sizeof(FileInfo));
	*fileinfoptr = fileinfo;
	matrix_send_async(HTTP_GET, str_buf(url), CALLBACK_INFO_TYPE_OTHER,
		NULL, matrix_receive_file, fileinfoptr);
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
	event.type = EVENT_ROOM_INFO;
	event.roominfo.id = str_new_cstr(json_string_value(roomid));
	event.roominfo.sender = str_new_cstr(sender);
	event.roominfo.name = NULL;
	event_handler_callback(event);
	str_decref(event.roominfo.id);
	str_decref(event.roominfo.name);
}

static void process_room_event(json_t *item, const char *roomid) {
	json_t *type = json_object_get(item, "type");
	assert(type != NULL);
	if (streq(json_string_value(type), "m.room.name")) {
		json_t *nam = json_path(item, "content", "name", NULL);
		assert(nam != NULL);
		const char *name = json_string_value(nam);
		MatrixEvent event;
		event.type = EVENT_ROOM_INFO;
		event.roominfo.id = str_new_cstr(roomid);
		event.roominfo.name = str_new_cstr(name);
		event.roominfo.sender = NULL;
		event_handler_callback(event);
		str_decref(event.roominfo.id);
		str_decref(event.roominfo.name);
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
		event.roomjoin.senderid =
			str_new_cstr(json_string_value(sender));
		json_t *name = json_path(item, "content", "displayname", NULL);
		/* TODO: See: https://spec.matrix.org/latest/client-server-api/#calculating-the-display-name-for-a-user */
		if (name) {
			/*
			 * TODO: case, on which name is not null but when we
			 * convert it to a string we get a NULL pointer. Let's
			 * try to debug it by adding an assert() and ananalyzing
			 * the core dump file generated when it happens again.
			 */
			assert(json_is_string(name));
			event.roomjoin.sendername =
				str_new_cstr(json_string_value(name));
		} else
			event.roomjoin.sendername = NULL;
		event_handler_callback(event);
		str_decref(event.roomjoin.roomid);
		str_decref(event.roomjoin.senderid);
		str_decref(event.roomjoin.sendername);
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

		/*
		 * There can be m.room.messages without a "msgtype", for
		 * instance, a m.room.redaction event. JSON is therefore in the following form:
		 *
		 * {
		 *	"content": {},
		 *	"type": "m.room.message",
		 *	"unsigned": {
		 *		"redacted_by": ...
		 *		"redacted_because": {
		 *			...
		 *			"type": "m.room.redaction",
		 *			...
		 *		}
		 *	}
		 * }
		 *
		 * If we find such events, we just ignore them.
		 */
		if (!msgtype)
			return;

		json_t *body = json_object_get(content, "body");
		assert(body != NULL);

		MatrixEvent event;
		event.type = EVENT_MSG;
		event.msg.roomid = str_new_cstr(roomid);
		event.msg.msg.sender = str_new_cstr(json_string_value(sender));

		if (streq(json_string_value(msgtype), "m.image")
		|| streq(json_string_value(msgtype), "m.audio")
		|| streq(json_string_value(msgtype), "m.video")
		|| streq(json_string_value(msgtype), "m.file")) {
			event.msg.msg.type = MSGTYPE_FILE;
			event.msg.msg.fileinfo.mimetype = str_new_cstr(
				json_string_value(json_path(
					content, "info", "mimetype", NULL)));
			/*
			 * TODO: I once got a error about NULL pointer when
			 * trying to get "url" string. Can it be NULL or a null
			 * string?
			 */
			event.msg.msg.fileinfo.uri = str_new_cstr(
				json_string_value(json_object_get(content, "url")));
			assert(event.msg.msg.fileinfo.uri);
			event_handler_callback(event);
			str_decref(event.msg.roomid);
			str_decref(event.msg.msg.sender);
			str_decref(event.msg.msg.fileinfo.uri);
			return;
		}

		event.msg.msg.text.content = str_new();
		event.msg.msg.type = MSGTYPE_TEXT;
		if (streq(json_string_value(msgtype), "m.text")) {
			event.msg.msg.text.content
				= str_new_cstr(json_string_value(body));
		} else {
			str_append_cstr(event.msg.msg.text.content, "==== ");
			str_append_cstr(event.msg.msg.text.content, json_string_value(msgtype));
			str_append_cstr(event.msg.msg.text.content, " ====");
		}
		event_handler_callback(event);
		str_decref(event.msg.roomid);
		str_decref(event.msg.msg.sender);
		str_decref(event.msg.msg.text.content);
	} else if (streq(json_string_value(type), "m.room.encrypted")) {
		MatrixEvent event;
		event.type = EVENT_MSG;
		event.msg.roomid = str_new_cstr(roomid);
		event.msg.msg.sender = str_new_cstr(json_string_value(sender));
		event.msg.msg.text.content = str_new_cstr("== encrypted message ==");
		event_handler_callback(event);
		str_decref(event.msg.roomid);
		str_decref(event.msg.msg.sender);
		str_decref(event.msg.msg.text.content);
	}
}

static void process_error(json_t *root) {
	MatrixEvent event;
	event.type = EVENT_MATRIX_ERROR;
	event.error.errorcode = str_new_cstr(json_string_value(json_object_get(root, "errcode")));
	event.error.error = str_new_cstr(json_string_value(json_object_get(root, "error")));
	event_handler_callback(event);
	str_decref(event.error.errorcode);
	str_decref(event.error.error);
}

static void process_push_rules(json_t *rule) {
	bool enabled = json_boolean_value(json_object_get(rule, "enabled"));
	/*
	 * TODO: check if the usage of "enabled" -> why would the server keep
	 * this rule if it is not enabled?
	 */
	if (!enabled)
		return;

	json_t *actions = json_object_get(rule, "actions");

	size_t i;
	json_t *item;
	json_array_foreach(actions, i, item) {
		if (!json_is_string(item))
			continue;
		if (streq(json_string_value(item), "dont_notify")) {
			MatrixEvent event;

			/*
			 * In the case of room rules, rule_id == roomid.
			 * See https://matrix.org/docs/spec/client_server/r0.6.1#predefined-rules
			 */
			const char *roomid =json_string_value(
				json_object_get(rule, "rule_id"));

			event.type = EVENT_ROOM_NOTIFY_STATUS;
			event.roomnotifystatus.roomid = str_new_cstr(roomid);
			event.roomnotifystatus.enabled = false;
			event_handler_callback(event);
			str_decref(event.roomnotifystatus.roomid);
			return;
		}
	}
}

static void process_rooms_join(json_t *root) {
	const char *roomid;
	json_t *item;

	/*
	 * m.room.create events come in room state events list, which is
	 * unsorted.  But when passing EVENT_ROOM_CREATE events to upper layers,
	 * we have to pass it before other events that alter the room state
	 * (because the room object need already to be created in order to
	 * receive these other events), so we look for events of this type first.
	 */
	json_object_foreach(root, roomid, item) {
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

	json_object_foreach(root, roomid, item)
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

}

static void process_sync_response(const char *output, size_t sz, void *params) {
	(void)sz;
	(void)params;
	assert(output);

	insync = false;

	json_t *root;
	root = str2json_alloc(output);

	/*
	 * TODO: This shouldn't happen but it does happen when there are network
	 * problems. Call abort to generate a core dump, so we can analyse it
	 * further.
	 */
	if (!root)
		abort();

	json_t *errorcode = json_object_get(root, "errcode");
	if (errorcode) {
		process_error(root);
		json_decref(root);
		return;
	}

	json_t *rooms_join = json_path(root, "rooms", "join", NULL);
	if (rooms_join)
		process_rooms_join(rooms_join);

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
			} else if (streq(t, "m.push_rules")) {
				size_t j;
				json_t *rule;

				json_t *room_rules = json_path(item,
					"content", "global", "room", NULL);
				json_array_foreach(room_rules, j, rule)
					process_push_rules(rule);

				json_t *override_rules = json_path(item,
					"content", "global", "override", NULL);
				json_array_foreach(override_rules, j, rule)
					process_push_rules(rule);
			}
		}
	}


	free(next_batch);
	json_t *n = json_object_get(root, "next_batch");
	assert(n != NULL);
	next_batch = strdup(json_string_value(n));
	/*
	 * TODO: it is better to store next_batch when exiting gracefully from
	 * janechat.
	 */
	cache_set("next_batch", next_batch);
	json_decref(root);
}

/*
 * This sync filter is used in both both initial and later access to the sync
 * endpoint. There is a very important difference between both, though: the
 * initial sync can specify a "limit":0 parameter to the timeline object,
 * limiting the response. We cannot have this limitation in later calls to the
 * sync endpoint because we can lose messages. In order to avoid duplicating
 * this string, we define it in a macro and let callers pass a random parameter
 * to the timeline object. Note that this parameter shall be an empty string or
 * include a trailing comma, to separate it from other parameter in the JSON
 * string.
 */
#define SYNC_REQUEST_FILTER(timeline_arg) \
	"filter={" \
		"\"room\":{" \
			"\"account_data\":{\"not_types\":[\"*\"]}," \
			"\"ephemeral\":{\"not_types\":[\"*\"]}," \
			"\"state\":{" \
				"\"lazy_load_members\":true," \
				"\"types\":[" \
					"\"m.room.create\"," \
					"\"m.room.member\"," \
					"\"m.room.name\"" \
				"]" \
			"}," \
			"\"timeline\":{" \
				timeline_arg \
				"\"types\":[" \
					"\"m.room.message\"," \
					"\"m.room.encrypted\"" \
				"]" \
			"}" \
		"}," \
		"\"account_data\":{" \
			"\"types\":[" \
				"\"m.direct\"," \
				"\"m.push_rules\"" \
			"]" \
		"}" \
	"}"

/*
 * Perform the initial sync, to retrieve initial state from the matrix server.
 * No message is retrieved in this phase.
 */
bool matrix_initial_sync(void) {
	Str *url = str_new();
	str_append_cstr(url, "/_matrix/client/r0/sync");
	str_append_cstr(url, "?");
	str_append_cstr(url, SYNC_REQUEST_FILTER("\"limit\":0,"));
	str_append_cstr(url, "&access_token=");
	str_append_cstr(url, token);
	Str *res = matrix_send_sync_alloc(HTTP_GET, str_buf(url), NULL);
	if (!res)
		return false;

	/*
	 * next_batch is stored with cache_set() in process_sync_response(), so
	 * we can retrieve it between different janechat executions.  Since
	 * process_sync_response() overwrites next_batch, we retrieve next_batch
	 * from the cache and stored it temporarely in n and, after
	 * process_sync_response() execution, we store it in next_batch.
	 *
	 * So, when starting janechat, the program either:
	 *
	 * 1) calls matrix_initial_sync() (to fetch state information from
	 * the server) that calls process_sync_response() and store next_batch
	 * from the retrieved json.
	 *
	 * 2) calls matrix_initial_sync() (to fetch state information from
	 * the server) that calls process_sync_response(), but retrieve
	 * next_batch from the cache, so the next sync will fetch information
	 * not received since last time janechat ran.
	 *
	 * The approach we have here fetch all information from the server,
	 * making it unnecessary to serialize and persist information (such as
	 * rooms, etc.) between different janechat executions. We achieve some
	 * level of preemption without too much complication!
	 *
	 * TODO: This approach surely has problems: what to do with users if
	 * lazy_load_members is enabled? Nowadays it is not a problem because we
	 * are not keeping a data structure to translate user ids to user names,
	 * but we'll do it soon. In case 2 above, we does not receive user
	 * information (because we received it somewhere in the past, before the
	 * stored next_batch), so some level of serialization might be
	 * necessary.
	 *
	 * TODO: next_batch handling is still very spaghetti code using a
	 * global variable scattered among different functions. How can we make
	 * it less spaghetti?
	 */

	char *n = cache_get_alloc("next_batch");

	process_sync_response(str_buf(res), str_len(res), NULL);

	if (n)
		next_batch = n;

	str_decref(res);
	return true;
}

void matrix_sync(void) {
	if (insync)
		return;
	insync = true;
	Str *url = str_new();
	str_append_cstr(url, "/_matrix/client/r0/sync");
	str_append_cstr(url, "?");
	str_append_cstr(url, SYNC_REQUEST_FILTER(""));
	str_append_cstr(url, "&since=");
	assert(next_batch);
	str_append_cstr(url, next_batch);
	str_append_cstr(url, "&timeout=");
#define INT2STR_(x) #x
#define INT2STR(x) INT2STR_(x)
	str_append_cstr(url, INT2STR(MATRIX_SYNC_TIMEOUT_MS));
#undef INT2STR
#undef INT2STR_
	str_append_cstr(url, "&access_token=");
	str_append_cstr(url, token);
	matrix_send_async(HTTP_GET, str_buf(url), CALLBACK_INFO_TYPE_SYNC,
		 NULL, process_sync_response, NULL);
	str_decref(url);
}

const char *matrix_login_alloc(
	const char *server,
	const char *user,
	const char *password)
{
	Str *username = str_new();
	str_append_cstr(username, "@");
	str_append_cstr(username, user);
	str_append_cstr(username, ":");
	str_append_cstr(username, server);
	matrix_server = strdup(server);
	json_t *root = json_object();
	json_object_set(root, "type", json_string("m.login.password"));
	json_object_set(root, "user", json_string(user));
	json_object_set(root, "password", json_string(password));
	json_object_set(root, "initial_device_display_name", json_string("janechat"));
	const char *s = json2str_alloc(root);
	json_decref(root);
	Str *res = matrix_send_sync_alloc(HTTP_POST,
		"/_matrix/client/v3/login", s);
	free((void *)s);
	json_t *jsonres = str2json_alloc(str_buf(res));
	str_decref(res);
	json_t *tok = json_object_get(jsonres, "access_token");
	if (!tok)
		return NULL;
	token = strdup(json_string_value(tok));
	json_decref(jsonres);
	return token;
}

void matrix_resume(void) {
	curl_multi_perform(mhandle, &still_running);
	
	/* TODO: check return value of curl_multi_perform() for errors */
	int msgs_in_queue;
	CURLMsg *msg;
	msg = curl_multi_info_read(mhandle, &msgs_in_queue);

	if (!msg)
		/*
		 * TODO: in what cases it can be NULL? Should we handle it as an
		 * error?
		 */
		return;

	CURL *handle;
	handle = msg->easy_handle;
	struct callback_info *c;
	curl_easy_getinfo(handle, CURLINFO_PRIVATE, &c); /* TODO: Check return code */

	if (msg->data.result != CURLE_OK) {
		/*
		 * TODO: handle possible values for curl error and send them as
		 * an enum to upper layers, so they can show something in the UI
		 * beautifuly.
		 */
		fprintf(stderr, "curl error: %d: %s\n",
			(int)msg->data.result,
			curl_easy_strerror(msg->data.result));
		MatrixEvent event;
		event.type = EVENT_CONN_ERROR;
		event_handler_callback(event);

		switch (c->type) {
		case CALLBACK_INFO_TYPE_SYNC:
			insync = false;
			break;
		case CALLBACK_INFO_TYPE_OTHER:
			/* TODO: requeue */
			break;
		}


		goto cleanup;
	}

	if (msg && msg->msg == CURLMSG_DONE) {
#if DEBUG_RESPONSE
		printf("DEBUG_RESPONSE: output: %s\n", str_buf(c->data));
#endif
		if (c->callback)
			c->callback(str_buf(c->data),
				str_len(c->data),
				c->params);
		str_decref(c->data);
		free(c);
	}

cleanup:
	curl_multi_remove_handle(mhandle, handle);
	curl_easy_cleanup(handle);
}
