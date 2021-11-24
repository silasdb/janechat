/* curses front-end for janechat */

#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>

#include "ui.h"
#include "rooms.h"
#include "str.h"
#include "vector.h"
#include "utils.h"

/* We can detect Ctrl+key sequences by masking the return of getch() */
#define CTRL(x) (x & 037)

/*
 * We call the data structure that holds the room information for the UI as
 * "buffer".  We have very few UI elements, the only thing that changes is the
 * buffer, that is unique for each room.
 */
struct buffer {
	Room *room;
	char buf[256]; /* Input buffer. TODO: use Str? */
	size_t pos; /* Cursor position */
	size_t len; /* String length - does not include the null byte */
	size_t left; /* left-most character index showed in the input window */
	size_t right; /* right-most character index showed in the input window*/

	/*
	 * As messages text are rendered backwards (on the wchat_msgs window),
	 * we need to store the index of the last line. If last_line == -1, then
	 * the last line is the most recent message received.
	 */
	int last_line;
};

WINDOW *windex; /* The index window - that shows rooms */
WINDOW *wchat; /* The chat window, that show room messages and the input field */
WINDOW *wchat_msgs; /* A subwindow for the chat window: show messages received */
WINDOW *wchat_input; /* A subwindow for the chat window: input */

enum Focus {
	/* Focus is on the index window */
	FOCUS_INDEX,
	/* wchat window is visible and focus is on its input subwindow */
	FOCUS_CHAT_INPUT,
} focus = FOCUS_INDEX;

/* A vector of struct buffer. Used in the index window */
Vector *buffers = NULL; /* Vector<struct buffer> */

/* Current buffer selected. NULL if focus is in index window */
struct buffer *cur_buffer = NULL;

/*
 * Index for the current selected buffer. This is used not only to index buffers
 * vector, but to correct draw cursor on the screen.
 */
size_t index_idx = 0;

/* The first and last rooms showed in the index window. */
size_t top = 0;
size_t bottom = 0;

void chat_input_redraw(void);
void set_focus(enum Focus);
void index_draw(void);
void index_update_top_bottom(void);
void chat_input_clear(void);
void chat_drawline(void);
void chat_msgs_fill(void);

/*
 * A SIGINT handler. We use Ctrl-C to cleanup buffer input, so we need to
 * intercept SIGINT
 */
void handle_sigint(int sig) {
	(void)sig;
	chat_input_clear();
}

/*
 * Private helper functions
 */

int buffer_comparison(const void *a, const void *b) {
	const struct buffer **x = (const struct buffer **)a;
	const struct buffer **y = (const struct buffer **)b;
	int res;
	res = strcmp(str_buf((*x)->room->name), str_buf((*y)->room->name));
	if (res)
		return res;
	/*
	 * If both rooms have the same name, compare their ids - this prevent
	 * random sorting on the UI
	 */
	return strcmp(str_buf((*x)->room->id), str_buf((*y)->room->id));
}

void set_cur_buffer(struct buffer *buffers) {
	cur_buffer = buffers;
	int maxy, maxx;
	getmaxyx(wchat_input, maxy, maxx);
	(void)maxy;
	cur_buffer->left = 0;
	cur_buffer->right = maxx-1;
	cur_buffer->room->unread_msgs = 0;
}

void set_focus(enum Focus f) {
	focus = f;
	switch (focus) {
	case FOCUS_INDEX:
		cur_buffer = NULL;
		focus = FOCUS_INDEX;
		index_draw();
		wrefresh(windex);
		break;
	case FOCUS_CHAT_INPUT:
		chat_msgs_fill();
		chat_drawline();
		chat_input_redraw();
		wrefresh(wchat);
		break;
	}
}

void resize(void) {
	int maxy, maxx;
	clear();
	getmaxyx(stdscr, maxy, maxx);
	wresize(windex, maxy, maxx);
	wresize(wchat, maxy, maxx);
	wresize(wchat_msgs, maxy-2, maxx);
	wresize(wchat_input, 1, maxx);
	mvwin(wchat_input, maxy-1, 0);
	switch (focus) {
	case FOCUS_CHAT_INPUT:
		chat_msgs_fill();
		break;
	case FOCUS_INDEX:
		index_update_top_bottom();
		index_draw();
		break;
	}
	chat_drawline();
	if (cur_buffer)
		/* We force a chat_input_redraw of the current buffer input window */
		set_cur_buffer(cur_buffer);
}

void send_msg(void) {
	/*
	 * If buf is an empty string or only if it is only consisted of spaces,
	 * don't send anything.
	 */
	char *c = &cur_buffer->buf[0];
	while (isspace(*c))
		c++;
	if (*c == '\0')
		return;

#if UI_CURSES_TEST
	Msg *msg = malloc(sizeof(struct Msg));
	msg->sender = str_new_cstr("test");
	msg->text = str_new_cstr(cur_buffer->buf);
	vector_append(cur_buffer->room->msgs, msg);
	chat_msgs_fill();
#else
	struct UiEvent ev;
	ev.type = UIEVENTTYPE_SENDMSG,
	ev.msg.roomid = str_incref(cur_buffer->room->id);
	ev.msg.text = str_new_cstr(cur_buffer->buf);
	ui_event_handler_callback(ev);
	str_decref(ev.msg.roomid);
	str_decref(ev.msg.text);
#endif
}

/*
 * Private function for windex window.
 */

/* Increment cursor position. Cursor wraps around. */
void index_cursor_inc(int offset) {
	/* TODO: test case vector_len(buffers) == 0 */
	if (index_idx == 0 && offset < 0)
		index_idx = vector_len(buffers);
	if (index_idx >= vector_len(buffers)-1 && offset > 0)
		index_idx = -1;
	index_idx += offset;
}

/* Jump to the next room with unread message. Cursor wraps around. */
void index_next_unread(int direction) {
	int idx = index_idx;
	do  {
		index_cursor_inc(direction);
		struct buffer *b;
		b = vector_at(buffers, index_idx);
		if (b->room->unread_msgs > 0)
			break;
	} while (idx != index_idx);
}

/* Update top and bottom variables. Called when resizing. */
void index_update_top_bottom(void) {
	int maxy, maxx;
	getmaxyx(windex, maxy, maxx);
	(void)maxx;
	top = 0;
	bottom = maxy-1;
	if (bottom >= vector_len(buffers)-1)
		bottom = vector_len(buffers)-1;
}

/* Draw the windex window */
void index_draw(void) {
	if (vector_len(buffers) == 0)
		return;

	/* If cursor is off-screen, adjust top and bottom to show cursor. */
	if (index_idx > bottom) {
		top += index_idx - bottom;
		bottom = index_idx;
	} else if (index_idx < top) {
		bottom -= top - index_idx;
		top = index_idx;
	}

	assert(top >= 0 && top < vector_len(buffers));
	assert(bottom >= 0 && bottom < vector_len(buffers));
	assert(index_idx >= top && index_idx <= bottom);

	/* Draw the window */
	werase(windex);
	for (size_t i = top; i <= bottom; i++) {
		struct buffer *tb;
		tb = vector_at(buffers, i);
		if (index_idx == i)
			wattron(windex, A_REVERSE); 
		if (tb->room->unread_msgs > 0)
			wattron(windex, A_BOLD);
		mvwprintw(windex, i-top, 0, "%s (%zu)",
			tb->room->name->buf, tb->room->unread_msgs);
		if (index_idx == i)
			wattroff(windex, A_REVERSE); 
		if (tb->room->unread_msgs > 0)
			wattroff(windex, A_BOLD);
	}
	wrefresh(windex);
}

void index_key(void) {
	int c = wgetch(windex);
	switch (c) {
	case 'Q':
		endwin();
		exit(0);
		break;
	case 'k':
	case KEY_UP:
		index_cursor_inc(-1);
		index_draw();
		break;
	case 'j':
	case KEY_DOWN:
		index_cursor_inc(+1);
		index_draw();
		break;
	case 'K':
		index_next_unread(-1);
		index_draw();
		break;
	case 'J':
		index_next_unread(+1);
		index_draw();
		break;
	case KEY_RESIZE:
		resize();
		break;
	case 10:
	case 13:
		/* TODO: what if buffers is empty? */
		set_cur_buffer(vector_at(buffers, index_idx));
		set_focus(FOCUS_CHAT_INPUT);
		break;
	}
}

/*
 * Private functions for wchat window behaviour.
 */

void chat_drawline(void) {
	int maxy, maxx;
	getmaxyx(wchat, maxy, maxx);
	mvwhline(wchat, maxy-2, 0, '-', maxx);
}

void chat_msgs_fill(void) {
	werase(wchat_msgs);
	int last = -1;
	if (cur_buffer->last_line == -1)
		last = vector_len(cur_buffer->room->msgs)-1;
	if (last < 0)
		return;
	int maxy, maxx;
	getmaxyx(wchat_msgs, maxy, maxx);
	/*
	 * We make a naive calculation of line height and print them backwards.
	 * TODO: consider that a message can have a line break.
	 */
	int y = maxy;
	for (ssize_t i = last; i >= 0; i--) {
		Msg *msg = (Msg *)vector_at(cur_buffer->room->msgs, i);
		size_t len;
		len = str_len(msg->sender);
		len += 2; /* collon between sender and text */
		len += str_len(msg->text);
		int height = len / maxx;
		height++;
		y -= height;
		if (y < 0)
			break;
		mvwprintw(wchat_msgs, y, 0, "%s: %s\n",
			str_buf(msg->sender), str_buf(msg->text));
	}
	wrefresh(wchat_msgs);
}

void chat_input_clear() {
	if (!cur_buffer)
		return;
	cur_buffer->buf[0] = '\0';
	cur_buffer->pos = 0;
	cur_buffer->len = 0;
	chat_input_redraw();
}

void chat_input_redraw(void) {
	werase(wchat_input);
	mvwprintw(wchat_input, 0, 0, "%.*s",
		(int)(cur_buffer->right - cur_buffer->left + 1),
		&cur_buffer->buf[cur_buffer->left]);
	wmove(wchat_input, 0, cur_buffer->pos - cur_buffer->left);
	wrefresh(wchat_input);
}

void chat_input_cursor_inc(int offset) {
	if (cur_buffer->pos + offset < 0)
		return;
	if (cur_buffer->pos + offset > cur_buffer->len)
		return;
	cur_buffer->pos += offset;
}

void chat_input_cursor_show(void) {
	size_t *pos = &cur_buffer->pos;
	size_t *left = &cur_buffer->left;
	size_t *right = &cur_buffer->right;
	if (*pos > *right) {
		*left += *pos - *right;
		*right = *pos;
	} else if (*pos < *left) {
		*right -= *left - *pos;
		*left = *pos;
	}
}

void chat_input_key(void) {
	int c = wgetch(wchat_input);
	switch (c) {
	case 127: /* TODO: why do I need this in urxvt but not in xterm? - https://bbs.archlinux.org/viewtopic.php?id=56427*/
	case KEY_BACKSPACE:
		if (cur_buffer->pos == 0)
			break;
		for (size_t i = cur_buffer->pos-1; i < cur_buffer->len; i++)
			cur_buffer->buf[i] = cur_buffer->buf[i+1];
		cur_buffer->pos--;
		cur_buffer->len--;
		break;
	case KEY_RESIZE:
		resize();
		break;
	case KEY_LEFT:
		chat_input_cursor_inc(-1);
		break;
	case KEY_RIGHT:
		chat_input_cursor_inc(+1);
		break;
	case CTRL('g'):
		set_focus(FOCUS_INDEX);
		return;
		break;
	case 10: /* LF */
	case 13: /* CR */
		if (streq(cur_buffer->buf, "/quit")) {
			set_focus(FOCUS_INDEX);
		} else {
			send_msg();
		}
		chat_input_clear();
		break;
	default:
		for (size_t i = cur_buffer->len; i > cur_buffer->pos; i--)
			cur_buffer->buf[i] = cur_buffer->buf[i-1];
		cur_buffer->buf[cur_buffer->pos] = c;
		cur_buffer->len++;
		chat_input_cursor_inc(+1);
		break;
	}
	cur_buffer->buf[cur_buffer->len] = '\0';
	chat_input_cursor_show();
	chat_input_redraw();
}

/*
 * Public functions
 */

void ui_curses_init() {
	/*
	 * TODO: we check if buffers is already allocated because our test
	 * main() can already have allocated it for testing purposes.
	 */
	if (!buffers)
		buffers = vector_new();

	initscr();
	clear();
	nonl();
	cbreak();
	noecho();

	int maxy, maxx;
	getmaxyx(stdscr, maxy, maxx);

	wchat = newwin(maxy, maxx, 0, 0);
	windex = newwin(maxy, maxx, 0, 0);
	wchat_msgs = subwin(wchat, maxy-1, maxx, 0, 0);
	wchat_input = subwin(wchat, 1, maxx, maxy-1, 0);
	keypad(windex, TRUE);
	keypad(wchat_input, TRUE);

	signal(SIGINT, handle_sigint);
}

void ui_curses_iter() {
	/* TODO: fix draw order */
	switch (focus) {
	case FOCUS_INDEX:
		/*
		 * TODO: having it here the buffer be sorted for ever keystroke.
		 * We should find a better place to have it.
		 */
		vector_sort(buffers, buffer_comparison);

		index_key();
		break;
	case FOCUS_CHAT_INPUT:
		chat_input_key();
		break;
	}
}

void ui_curses_room_new(Str *roomid) {
	struct buffer *b;
	b = malloc(sizeof(struct buffer));
	b->room = room_byid(roomid);
	b->pos = 0;
	b->len = 0;
	b->left = 0;
	b->right = 0;
	b->last_line = -1;
	vector_append(buffers, b);
}

void ui_curses_msg_new(Room *room, Str *sender, Str *msg) {
	if (focus == FOCUS_INDEX) {
		index_draw(); /* Update window */
		return;
	}
	/* TODO: what about other parameters? */
	if (cur_buffer && cur_buffer->room == room) {
		cur_buffer->room->unread_msgs = 0;
		chat_msgs_fill();
	}
}

/*
 * The main function is only used for testing purposes, if we want to test the
 * ui-curses frontend without having to connect to a Matrix server. It creates
 * fake rooms so we can mimic janechat behaviour. In order to enable this
 * window, macro UI_CURSES_TEST have to be defined.
 */
#ifdef UI_CURSES_TEST
int main(int argc, char *argv[]) {
	rooms_init();
	buffers = vector_new();

	Room *room;
	Str *name_s;
	Str *id_s;
#define new_room(id, name) \
	id_s = str_new_cstr(id); \
	name_s = str_new_cstr(name); \
	room = room_new(id_s); \
	room_set_name(room, name_s); \
	ui_curses_room_new(id_s);

	new_room("#test1:matrix.org", "Test A");
	new_room("#test2:matrix.org", "Test C");
	new_room("#test3:matrix.org", "Test B");

	/* Append a message to the last room */
	room_append_msg(room, str_new_cstr("sender"), str_new_cstr("text"));

	ui_curses_init();
	bottom = vector_len(buffers);

	resize();

	for (;;)
		ui_curses_iter();

	return 0;
}
#endif

