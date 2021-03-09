#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <json.h>

#include "StrBuf.h"
#include "matrix.h"
#include "utils.h"

#define DEBUG_REQUEST 0
#define DEBUG_RESPONSE 0

char *next_batch = NULL;
char *token = NULL;
const char *matrix_server = NULL;

static void enqueue_event(MatrixEvent *);
static void send(const char *method, const char *path, const char *json);

struct node {
	MatrixEvent *event;
	struct node *next;
};

// linked list for the matrixevent queue
// TODO: is it possible to unify it with the linked list implementation of hash.c?
struct {
	struct node *head;
	struct node *tail;
} event_queue = {NULL, NULL};

void
matrix_send_message(const char *roomid, const char *msg)
{
	StrBuf *url = strbuf_new();
	strbuf_cat_c(url, "/_matrix/client/r0/rooms/");
	strbuf_cat_c(url, roomid);
	strbuf_cat_c(url, "/send/m.room.message?access_token=");
	strbuf_cat_c(url, token);
	json_object *root = json_object_new_object();
	json_object_object_add(root, "msgtype",
		json_object_new_string("m.text"));
	json_object_object_add(root, "body", json_object_new_string(msg));
	send("-XPOST", strbuf_buf(url), json_object_to_json_string(root));
	strbuf_free(url);
}

void
matrix_sync()
{
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

void
matrix_login(const char *server, const char *user, const char *password)
{
	matrix_server = server;
	json_object *root = json_object_new_object();
	json_object_object_add(root, "type", json_object_new_string("m.login.password"));
	json_object_object_add(root, "user", json_object_new_string(user));
	json_object_object_add(root, "password", json_object_new_string(password));
	send("-XPOST", "/_matrix/client/r0/login", json_object_to_json_string(root));
}

static void
process_direct_event(char *sender, json_object *roomid)
{
	char *id = strdup(json_object_get_string(roomid));
	MatrixEvent *event = malloc(sizeof(MatrixEvent));
	event->type = EVENT_ROOM;
	event->room.id = id;
	event->room.name = sender;
	enqueue_event(event);
}

static void
process_room_event(json_object *item, const char *roomid)
{
	json_object *type = json_object_object_get(item, "type");
	assert(type != NULL);
	if (strcmp(json_object_get_string(type), "m.room.name") == 0) {
		json_object *content = json_object_object_get(item, "content");
		assert(content != NULL);
		json_object *nam = json_object_object_get(content, "name");
		assert(nam != NULL);
		const char *name = json_object_get_string(nam);
		char *id = strdup(roomid);
		char *nn = strdup(name);
		MatrixEvent *event = malloc(sizeof(MatrixEvent));
		event->type = EVENT_ROOM;
		event->room.id = id;
		event->room.name = nn;
		enqueue_event(event);
	}
}

static void
process_timeline_event(json_object *item, const char *roomid)
{
	json_object *type = json_object_object_get(item, "type");
	assert(type != NULL);
	if ((strcmp(json_object_get_string(type), "m.room.message") != 0)
	&& (strcmp(json_object_get_string(type), "m.room.encrypted") != 0))
		return;
	json_object *sender = json_object_object_get(item, "sender");
	assert(sender != NULL);
	json_object *content = json_object_object_get(item, "content");
	assert(content != NULL);
	if (strcmp(json_object_get_string(type), "m.room.message") == 0) {
		json_object *msgtype = json_object_object_get(content, "msgtype");
		assert(msgtype != NULL);
		if (strcmp(json_object_get_string(msgtype), "m.text") != 0) {
			printf("==== TODO: Type not supported: %s====\n", json_object_get_string(msgtype));
			return;
		}
		json_object *body = json_object_object_get(content, "body");
		assert(body != NULL);
		MatrixEvent *event = malloc(sizeof(MatrixEvent));
		event->type = EVENT_MSG;
		event->msg.sender = strdup(json_object_get_string(sender));
		event->msg.roomid = strdup(roomid);
		event->msg.text = strdup(json_object_get_string(body));
		enqueue_event(event);
	} else if (strcmp(json_object_get_string(type), "m.room.encrypted") == 0) {
		MatrixEvent *event = malloc(sizeof(MatrixEvent));
		event->type = EVENT_MSG;
		event->msg.sender = strdup(json_object_get_string(sender));
		event->msg.roomid = strdup(roomid);
		event->msg.text = strdup("== encrypted message ==");
		enqueue_event(event);
	}
}

static void
process_error(json_object *root)
{
	MatrixEvent *event = malloc(sizeof(MatrixEvent));
	event->type = EVENT_ERROR;
	event->error.errorcode = strdup(
		json_object_get_string(json_object_object_get(root, "errcode")));
	event->error.error = strdup(
		json_object_get_string(json_object_object_get(root, "error")));
	enqueue_event(event);
}

static void
process_matrix_response(const char *output)
{
	json_object *root = json_tokener_parse(output);
	json_object *errorcode = json_object_object_get(root, "errcode");
	if (errorcode) {
		process_error(root);
		return;
	}
	json_object *tok = json_object_object_get(root, "access_token");
	if (tok) {
		assert(token == NULL);
		token = strdup(json_object_get_string(tok));
		MatrixEvent *event = malloc(sizeof(MatrixEvent));
		event->type = EVENT_LOGGED_IN;
		enqueue_event(event);
		return;
	}
	json_object *account_data = json_object_object_get(root, "account_data");
	if (account_data) {
		json_object *events = json_object_object_get(account_data, "events");
		assert(events != NULL);
		for (size_t i = 0; i < json_object_array_length(events); i++) {
			json_object *item = json_object_array_get_idx(events, i);
			assert(item != NULL);
			json_object *type = json_object_object_get(item, "type");
			assert(type != NULL);
			const char *t;
			t = json_object_get_string(type);
			if (strcmp(t, "m.direct") == 0) {
				json_object *content = json_object_object_get(item, "content"); 
				json_object_object_foreach(content, sender, roomid) {
					for (size_t i = 0; i < json_object_array_length(roomid); i++) {
						json_object *item = json_object_array_get_idx(roomid, i);
						process_direct_event(sender, item);
					}
				}
			}
		}
	}
	json_object *rooms = json_object_object_get(root, "rooms");
	if (!rooms)
		return;
	json_object *join = json_object_object_get(rooms, "join");
	if (!join)
		return;
	json_object_object_foreach(join, roomid, item) {
		json_object *state = json_object_object_get(item, "state");
		assert(state != NULL);
		json_object *events = json_object_object_get(state, "events");
		assert(events != NULL);
		for (size_t i = 0; i < json_object_array_length(events); i++) {
			json_object *item = json_object_array_get_idx(events, i);
			assert(item != NULL);
			process_room_event(item, roomid);
		}
		
		json_object *timeline = json_object_object_get(item, "timeline");
		assert(timeline != NULL);
		events = json_object_object_get(timeline, "events");
		assert(events != NULL);
		for (size_t i = 0; i < json_object_array_length(events); i++) {
			json_object *item = json_object_array_get_idx(events, i);
			assert(item != NULL);
			process_timeline_event(item, roomid);
		}
	}

	free(next_batch);
	json_object *n = json_object_object_get(root, "next_batch");
	assert(n != NULL);
	next_batch = strdup(json_object_get_string(n));
	json_object_put(root);
}

static void
enqueue_event(MatrixEvent *event)
{
	if (!event_queue.head) {
		event_queue.tail = malloc(sizeof(struct node));
		event_queue.head = event_queue.tail;
		event_queue.tail->event = event;
		event_queue.tail->next = NULL;
		return;
	} else {
		struct node *q = malloc(sizeof(struct node));
		q->event = event;
		q->next = NULL;
		event_queue.tail->next = q;
		event_queue.tail = q;
	}
}

// a.k.a. dequeue_event
MatrixEvent *
matrix_next_event()
{
	if (!event_queue.head)
		return NULL;

	MatrixEvent *event;
	event = event_queue.head->event;
	struct node *q;
	q = event_queue.head;
	event_queue.head = event_queue.head->next;
	free(q);
	return event;
}

void
matrix_free_event(MatrixEvent *event)
{
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
		break;
	}
	free(event);
}

static void
send(const char *method, const char *path, const char *json)
{
	StrBuf *url = strbuf_new();
	strbuf_cat_c(url, "https://");
	assert(matrix_server != NULL);
	strbuf_cat_c(url, matrix_server);
	strbuf_cat_c(url, ":443");
	strbuf_cat_c(url, path);

	const char *argv[7];
	if (json != NULL) {
		argv[0] = method;
		argv[1] = "-d";
		argv[2] = json;
		argv[3] = "-s";
		argv[4] = "--globoff";
		argv[5] = strbuf_buf(url);
		argv[6] = NULL;
	} else {
		argv[0] = method;
		argv[1] = "-s";
		argv[2] = "--globoff";
		argv[3] = strbuf_buf(url);
		argv[4] = NULL;
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
	FILE *f;
	f = popenve("/usr/pkg/bin/curl", (char *const *)argv, NULL, "rw");
	if (!f)
		exit(1);
	char *output = read_file(f);
#if DEBUG_RESPONSE
	printf("%s\n", output);
#endif
	strbuf_free(url);
	pclose(f);
	process_matrix_response(output);
	free(output);
}
