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
	char buf[1024]; /* Input buffer. TODO: use Str? */
	size_t pos; /* Cursor position */
	size_t len; /* String length - does not include the null byte */
	size_t left; /* left-most character index showed in the input window */
	size_t right; /* right-most character index showed in the input window*/

	/*
	 * Updated whenever someone leaves the chat window to the index window,
	 * so when the user comes back, he gets a line dividing messages in
	 * already read messages and unread messages. It is -1 if disabled.
	 */
	int read_separator;

	/*
	 * User defined separator that is set with the /line command. -1 if
	 * disabled.
	 */
	int user_separator;

	/*
	 * As messages text are rendered backwards (on the wchat_msgs window),
	 * we need to store the index of the last line. If last_line == -1, then
	 * the last line is the most recent message received.
	 */
	int last_line;
};

bool curses_init = false; /* Did we started curses? */
bool autopilot = false;

WINDOW *windex; /* The index window - that shows rooms */
WINDOW *wchat; /* The chat window, that show room messages and the input field */
WINDOW *wchat_msgs; /* A subwindow for the chat window: show messages received */
WINDOW *wchat_status; /* A subwindow for the chat window: show room status bar */
WINDOW *winput; /* A window for input common to both windex and wchat */

enum Focus {
	/* Focus is on the index window */
	FOCUS_INDEX,
	/* wchat window is visible and focus is on its input subwindow */
	FOCUS_CHAT_INPUT,
	FOCUS_INDEX_INPUT,
} focus = FOCUS_INDEX;

/* A vector of struct buffer. Used in the index window */
Vector *buffers = NULL; /* Vector<struct buffer> */

/* Current buffer selected. NULL if focus is in index window */
struct buffer *cur_buffer = NULL;

struct buffer index_input_buffer = { .room = NULL };

/*
 * Index for the current selected buffer. This is used not only to index buffers
 * vector, but to correct draw cursor on the screen.
 */
size_t index_idx = 0;

/* The first and last rooms showed in the index window. */
size_t top = 0;
size_t bottom = 0;

void input_redraw(void);
void set_focus(enum Focus);
void index_draw(void);
void resize(void);
void index_update_top_bottom(void);
void input_clear(void);
void chat_draw_statusbar(void);
void chat_msgs_fill(void);

/*
 * A SIGINT handler. We use Ctrl-C to cleanup buffer input, so we need to
 * intercept SIGINT
 */
void handle_sigint(int sig) {
	(void)sig;
	input_clear();
}

/*
 * A SIGWINCH handler to redraw everything after terminal resize.
 */
void handle_sigwinch(int sig) {
	(void)sig;
	/*
	 * endwin(); refresh(); needs to be called in the correct order, in
	 * order to make curses initialize its internal data structures
	 * correctly.
	 */
	endwin();
	refresh();
	index_update_top_bottom();
	ui_curses_iter(); /* To handle KEY_RESIZE */
}

/*
 * Private helper functions
 */

int buffer_comparison(const void *a, const void *b) {
	const struct buffer **x = (const struct buffer **)a;
	const struct buffer **y = (const struct buffer **)b;
	int res;
	res = strcmp(str_buf(room_displayname((*x)->room)),
		str_buf(room_displayname((*y)->room)));
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
	getmaxyx(winput, maxy, maxx);
	(void)maxy;
	cur_buffer->left = 0;
	cur_buffer->right = maxx-1;
	if (cur_buffer->room) {
		cur_buffer->room->unread_msgs = 0;
		cur_buffer->last_line = -1;
	}
}

void set_focus(enum Focus f) {
	focus = f;
	switch (focus) {
	case FOCUS_INDEX:
		if (autopilot) {
			/* Find the first buffer with unread messages */
			struct buffer *b;
			size_t i;
			VECTOR_FOREACH(buffers, b, i)
				if (b->room->notify && b->room->unread_msgs) {
					index_idx = i;
					set_cur_buffer(b);
					set_focus(FOCUS_CHAT_INPUT);
					return;
				}

		}
		cur_buffer = &index_input_buffer;
		focus = FOCUS_INDEX;
		index_draw();
		wrefresh(windex);
		break;
	case FOCUS_INDEX_INPUT:
		wmove(winput, 0, 0);
		wrefresh(winput);
		break;
	case FOCUS_CHAT_INPUT:
		chat_msgs_fill();
		chat_draw_statusbar();
		wrefresh(wchat);
		break;
	}
	input_redraw();
}

void resize(void) {
	int maxy, maxx;

	clear();
	getmaxyx(stdscr, maxy, maxx);

	wresize(windex, maxy-1, maxx);
	wresize(wchat, maxy-1, maxx);
	wresize(wchat_msgs, maxy-2, maxx);
	mvwin(wchat_status, maxy-2, 0);
	wresize(wchat_status, 1, maxx);

	wresize(winput, 1, maxx);
	mvwin(winput, maxy-1, 0);

	switch (focus) {
	case FOCUS_CHAT_INPUT:
		chat_msgs_fill();
		chat_draw_statusbar();
		input_redraw();
		break;
	case FOCUS_INDEX_INPUT:
	case FOCUS_INDEX:
		index_update_top_bottom();
		index_draw();
		break;
	}
	if (cur_buffer)
		/* We force a input_redraw of the current buffer input window */
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
	size_t idx = index_idx;
	do  {
		index_cursor_inc(direction);
		struct buffer *b;
		b = vector_at(buffers, index_idx);
		if (b->room->unread_msgs > 0 && b->room->notify)
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
	if (bottom > vector_len(buffers))
		bottom = vector_len(buffers);
	if (bottom)
		bottom--;
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

	assert(top < vector_len(buffers));
	assert(bottom < vector_len(buffers));
	assert(index_idx >= top && index_idx <= bottom);

	/* Draw the window */
	werase(windex);
	for (size_t i = top; i <= bottom; i++) {
		struct buffer *tb;
		tb = vector_at(buffers, i);
		if (index_idx == i)
			wattron(windex, A_REVERSE); 
		if (tb->room->unread_msgs > 0 && tb->room->notify)
			wattron(windex, A_BOLD);
		mvwprintw(windex, i-top, 0, "%s (%zu)",
			str_buf(room_displayname(tb->room)),
			tb->room->unread_msgs);
		if (index_idx == i)
			wattroff(windex, A_REVERSE); 
		if (tb->room->unread_msgs > 0 && tb->room->notify)
			wattroff(windex, A_BOLD);
	}
	wrefresh(windex);
}

void index_key(void) {
	int c = wgetch(windex);
	switch (c) {
	case KEY_RESIZE:
		resize();
		break;
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
	case ':':
		set_cur_buffer(&index_input_buffer);
		set_focus(FOCUS_INDEX_INPUT);
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

void chat_draw_statusbar(void) {
	int maxy, maxx;
	(void)maxy;
	getmaxyx(wchat, maxy, maxx);
	Str *roomname = room_displayname(cur_buffer->room);
	mvwprintw(wchat_status, 0, 0, "%s", str_buf(roomname));
	mvwhline(wchat_status, 0, str_len(roomname), ' ', maxx);
	wrefresh(wchat_status);
}

int text_height(const Str *sender, const Str *text, int width) {
	/* TODO: still need to consider UTF-8 characters */
	int height = 1;
	int w = str_len(sender) + 2; /* +2 because of ": " */
	const char *c;
	for (c = str_buf(text); *c != '\0'; c++, w++)
		if (*c == '\n' || w >= width) {
			height++;
			w = 0;
		}
	return height;
}

void chat_msgs_fill(void) {
	werase(wchat_msgs);
	int last = cur_buffer->last_line;
	if (last == -1)
		last = vector_len(cur_buffer->room->msgs)-1;
	if (last < 0)
		return;
	int maxy, maxx;
	getmaxyx(wchat_msgs, maxy, maxx);
	int y = maxy;
	for (ssize_t i = last; i >= 0; i--) {
		Msg *msg = (Msg *)vector_at(cur_buffer->room->msgs, i);
		if (cur_buffer->read_separator == i+1
		&&  cur_buffer->read_separator != (int)vector_len(cur_buffer->room->msgs)) {
			y--;
			wattron(wchat_msgs, COLOR_PAIR(1));
			mvwhline(wchat_msgs, y, 0, '-', maxx);
			wattroff(wchat_msgs, COLOR_PAIR(1));
		}
		if (cur_buffer->user_separator == i) {
			y--;
			wattron(wchat_msgs, COLOR_PAIR(2));
			mvwhline(wchat_msgs, y, 0, '-', maxx);
			wattroff(wchat_msgs, COLOR_PAIR(2));
		}
		int height = text_height(msg->sender, msg->text, maxx);
		y -= height;
		if (y < 0)
			break;

		wattron(wchat_msgs, COLOR_PAIR(1));
		mvwprintw(wchat_msgs, y, 0, "%s",
			str_buf(user_name(msg->sender)));

		/* TODO: why does it set background to COLOR_BLACK? */
		wattroff(wchat_msgs, COLOR_PAIR(1));

		wprintw(wchat_msgs, ": %s", str_buf(msg->text));
	}
	wrefresh(wchat_msgs);
}

void input_clear(void) {
	if (!cur_buffer)
		return;
	cur_buffer->buf[0] = '\0';
	cur_buffer->pos = 0;
	cur_buffer->len = 0;
	input_redraw();
}

void input_redraw(void) {
	werase(winput);
	mvwprintw(winput, 0, 0, "%.*s",
		(int)(cur_buffer->right - cur_buffer->left + 1),
		&cur_buffer->buf[cur_buffer->left]);
	wmove(winput, 0, cur_buffer->pos - cur_buffer->left);
	wrefresh(winput);
}

void input_cursor_inc(int offset) {
	if ((int)cur_buffer->pos + offset < 0)
		return;
	if (cur_buffer->pos + offset > cur_buffer->len)
		return;
	cur_buffer->pos += offset;
}

void input_cursor_show(void) {
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

bool input_key_index(int c) {
	switch (c) {
	case 10:
	case 13:
		if (streq(cur_buffer->buf, "set autopilot"))
			autopilot = true;
		if (streq(cur_buffer->buf, "unset autopilot"))
			autopilot = false;
		/* FALLTHROUGH */
	case CTRL('g'):
		input_clear();
		focus = FOCUS_INDEX;
		return true;
	}
	return false;
}

bool input_key_chat(int c) {
	switch (c) {
	case CTRL('g'):
		cur_buffer->read_separator = vector_len(cur_buffer->room->msgs);
		set_focus(FOCUS_INDEX);
		return true;
		break;
	case CTRL('b'):
		{
			if (cur_buffer->last_line == -1)
				cur_buffer->last_line = vector_len(cur_buffer->room->msgs);
			int maxy, maxx;
			(void)maxx;
			getmaxyx(wchat_msgs, maxy, maxx);
			maxy /= 2;
			cur_buffer->last_line -= maxy;
			chat_msgs_fill();
			return true;
		}
		break;
	case CTRL('f'):
		{
			if (cur_buffer->last_line == -1)
				return true;
			int maxy, maxx;
			(void)maxx;
			getmaxyx(wchat_msgs, maxy, maxx);
			maxy /= 2;
			cur_buffer->last_line += maxy;
			if (cur_buffer->last_line >= (int)vector_len(cur_buffer->room->msgs))
				cur_buffer->last_line = -1;
			chat_msgs_fill();
			return true;
		}
		break;
	case 10: /* LF */
	case 13: /* CR */
		if (streq(cur_buffer->buf, "/quit")) {
			set_focus(FOCUS_INDEX);
		} else if (streq(cur_buffer->buf, "/line")) {
			cur_buffer->user_separator = vector_len(cur_buffer->room->msgs)-1;
			chat_msgs_fill();
		} else if (streq(cur_buffer->buf, "/disableautopilot")) {
			autopilot = false;
		} else {
			send_msg();
		}
		input_clear();
		return true;
		break;
	}
	return false;
}

void input_key_common(int c) {
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
	case KEY_LEFT:
		input_cursor_inc(-1);
		break;
	case KEY_RIGHT:
		input_cursor_inc(+1);
		break;
	default:
		for (size_t i = cur_buffer->len; i > cur_buffer->pos; i--)
			cur_buffer->buf[i] = cur_buffer->buf[i-1];
		cur_buffer->buf[cur_buffer->pos] = c;
		cur_buffer->len++;
		input_cursor_inc(+1);
		break;
	}
}

void input_key(void) {
	int c = wgetch(winput);
	if (c == KEY_RESIZE) {
		resize();
		return;
	}

	if (focus == FOCUS_INDEX_INPUT && input_key_index(c))
		return;
	else if (focus == FOCUS_CHAT_INPUT && input_key_chat(c))
		return;
	input_key_common(c);
	cur_buffer->buf[cur_buffer->len] = '\0';
	input_cursor_show();
	input_redraw();
}

/*
 * Public functions
 */

void ui_curses_setup(void) {
	if (!buffers)
		buffers = vector_new();
}

void ui_curses_init(void) {
	curses_init = true;
	initscr();
	clear();
	nonl();
	cbreak();
	noecho();

	int maxy, maxx;
	getmaxyx(stdscr, maxy, maxx);

	wchat = newwin(maxy-1, maxx, 0, 0);
	windex = newwin(maxy-1, maxx, 0, 0);
	wchat_msgs = subwin(wchat, maxy-2, maxx, 0, 0);
	wchat_status = subwin(wchat, 1, maxx, maxy-2, 0);
	winput = newwin(1, maxx, maxy-1, 0);
	keypad(windex, TRUE);
	keypad(winput, TRUE);

	start_color();
	use_default_colors();
	init_pair(1, COLOR_GREEN, -1);
	init_pair(2, COLOR_YELLOW, -1);
	init_pair(3, COLOR_WHITE, COLOR_BLUE);

	wattron(wchat_status, COLOR_PAIR(3));

	signal(SIGINT, handle_sigint);
	signal(SIGWINCH, handle_sigwinch);
	/*
	 * TODO: by having vector_sort() here, it doesn't sort new rooms that we
	 * can join after starting janechat.
	 */
	vector_sort(buffers, buffer_comparison);
	resize();
}

void ui_curses_iter(void) {
	if (!curses_init)
		return;
	/* TODO: fix draw order */
	switch (focus) {
	case FOCUS_INDEX:
		index_key();
		break;
	case FOCUS_INDEX_INPUT:
	case FOCUS_CHAT_INPUT:
		input_key();
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
	if (curses_init) {
		vector_sort(buffers, buffer_comparison);
		index_update_top_bottom();
	}
}

void ui_curses_msg_new(Room *room, Str *sender, Str *msg) {
	(void)sender; /* TODO: why is it unused? */
	(void)msg; /* TODO: why is it unused? */
	if (!curses_init)
		return;
	if (focus == FOCUS_INDEX) {
		index_draw(); /* Update window */
		if (autopilot) {
			/* Find the buffer `b` that holds `room` */
			struct buffer *b;
			size_t i;
			VECTOR_FOREACH(buffers, b, i)
				if (b->room == room && room->notify) {
					index_idx = i;
					set_cur_buffer(b);
					set_focus(FOCUS_CHAT_INPUT);
					break;
				}
		}
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
	ui_curses_setup();

	Room *room;
	Str *name_s;
	Str *id_s;
#define new_room(id, name) \
	id_s = str_new_cstr(id); \
	name_s = str_new_cstr(name); \
	room = room_new(id_s); \
	room_set_info(room, str_new_cstr("@example:matrix.org"), name_s); \
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

