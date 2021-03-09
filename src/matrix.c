#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <json.h>

#include "StrBuf.h"
#include "matrix.h"
#include "utils.h"

#define DEBUG_REQUEST 0
#define DEBUG_RESPONSE 0

/* Shorten some json-c function names */
#define J_GETSTR json_object_get_string
#define J_NEWSTR json_object_new_string
#define J_OBJADD json_object_object_add
#define J_OBJGET json_object_object_get

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
	J_OBJADD(root, "msgtype", J_NEWSTR("m.text"));
	J_OBJADD(root, "body", J_NEWSTR(msg));
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
	J_OBJADD(root, "type", J_NEWSTR("m.login.password"));
	J_OBJADD(root, "user", J_NEWSTR(user));
	J_OBJADD(root, "password", J_NEWSTR(password));
	J_OBJADD(root, "initial_device_display_name", J_NEWSTR("janechat"));
	send("-XPOST", "/_matrix/client/r0/login", json_object_to_json_string(root));
}

static void
process_direct_event(char *sender, json_object *roomid)
{
	char *id = strdup(J_GETSTR(roomid));
	MatrixEvent *event = malloc(sizeof(MatrixEvent));
	event->type = EVENT_ROOM;
	event->room.id = id;
	event->room.name = sender;
	enqueue_event(event);
}

static void
process_room_event(json_object *item, const char *roomid)
{
	json_object *type = J_OBJGET(item, "type");
	assert(type != NULL);
	if (strcmp(J_GETSTR(type), "m.room.name") == 0) {
		json_object *content = J_OBJGET(item, "content");
		assert(content != NULL);
		json_object *nam = J_OBJGET(content, "name");
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

static void
process_timeline_event(json_object *item, const char *roomid)
{
	json_object *type = J_OBJGET(item, "type");
	assert(type != NULL);
	if ((strcmp(J_GETSTR(type), "m.room.message") != 0)
	&& (strcmp(J_GETSTR(type), "m.room.encrypted") != 0))
		return;
	json_object *sender = J_OBJGET(item, "sender");
	assert(sender != NULL);
	json_object *content = J_OBJGET(item, "content");
	assert(content != NULL);
	if (strcmp(J_GETSTR(type), "m.room.message") == 0) {
		json_object *msgtype = J_OBJGET(content, "msgtype");
		assert(msgtype != NULL);
		if (strcmp(J_GETSTR(msgtype), "m.text") != 0) {
			printf("==== TODO: Type not supported: %s====\n",
				J_GETSTR(msgtype));
			return;
		}
		json_object *body = J_OBJGET(content, "body");
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

static void
process_error(json_object *root)
{
	MatrixEvent *event = malloc(sizeof(MatrixEvent));
	event->type = EVENT_ERROR;
	event->error.errorcode = strdup(J_GETSTR(J_OBJGET(root, "errcode")));
	event->error.error = strdup(J_GETSTR(J_OBJGET(root, "error")));
	enqueue_event(event);
}

static void
process_matrix_response(const char *output)
{
	json_object *root = json_tokener_parse(output);
	json_object *errorcode = J_OBJGET(root, "errcode");
	if (errorcode) {
		process_error(root);
		return;
	}
	json_object *tok = J_OBJGET(root, "access_token");
	if (tok) {
		assert(token == NULL);
		token = strdup(J_GETSTR(tok));
		MatrixEvent *event = malloc(sizeof(MatrixEvent));
		event->type = EVENT_LOGGED_IN;
		enqueue_event(event);
		return;
	}
	json_object *account_data = J_OBJGET(root, "account_data");
	if (account_data) {
		json_object *events = J_OBJGET(account_data, "events");
		assert(events != NULL);
		for (size_t i = 0; i < json_object_array_length(events); i++) {
			json_object *item = json_object_array_get_idx(events, i);
			assert(item != NULL);
			json_object *type = J_OBJGET(item, "type");
			assert(type != NULL);
			const char *t;
			t = J_GETSTR(type);
			if (strcmp(t, "m.direct") == 0) {
				json_object *content = J_OBJGET(item, "content"); 
				json_object_object_foreach(content, sender, roomid) {
					for (size_t i = 0; i < json_object_array_length(roomid); i++) {
						json_object *item = json_object_array_get_idx(roomid, i);
						process_direct_event(sender, item);
					}
				}
			}
		}
	}
	json_object *rooms = J_OBJGET(root, "rooms");
	if (!rooms)
		return;
	json_object *join = J_OBJGET(rooms, "join");
	if (!join)
		return;
	json_object_object_foreach(join, roomid, item) {
		json_object *state = J_OBJGET(item, "state");
		assert(state != NULL);
		json_object *events = J_OBJGET(state, "events");
		assert(events != NULL);
		for (size_t i = 0; i < json_object_array_length(events); i++) {
			json_object *item = json_object_array_get_idx(events, i);
			assert(item != NULL);
			process_room_event(item, roomid);
		}
		
		json_object *timeline = J_OBJGET(item, "timeline");
		assert(timeline != NULL);
		events = J_OBJGET(timeline, "events");
		assert(events != NULL);
		for (size_t i = 0; i < json_object_array_length(events); i++) {
			json_object *item = json_object_array_get_idx(events, i);
			assert(item != NULL);
			process_timeline_event(item, roomid);
		}
	}

	free(next_batch);
	json_object *n = J_OBJGET(root, "next_batch");
	assert(n != NULL);
	next_batch = strdup(J_GETSTR(n));
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

static char *
find_curl()
{
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
#if DEBUG_RESPONSE
	printf("%s\n", output);
#endif
	strbuf_free(url);
	pclose(f);
	process_matrix_response(output);
	free(output);
}
