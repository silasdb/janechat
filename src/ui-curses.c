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

#define CTRL(x) (x & 037)

struct buffer {
	Room *room;
	char buf[256]; /* TODO: use Str? */
	size_t pos; /* Cursor position */
	size_t len; /* String length - does not include the null byte*/
	size_t left;
	size_t right;
	int last_line;
};

WINDOW *windex;
WINDOW *wchat;
WINDOW *wchat_msgs;
WINDOW *wchat_input;

Vector *buffers = NULL; /* Vector<struct buffer> */

struct buffer *cur_buffer = NULL;

enum Focus {
	FOCUS_INDEX,
	FOCUS_CHAT_INPUT,
} focus = FOCUS_INDEX;

void redraw(WINDOW *w);
void change_focus(enum Focus);

void clear_cur_buffer_input() {
	if (!cur_buffer)
		return;
	cur_buffer->buf[0] = '\0';
	cur_buffer->pos = 0;
	cur_buffer->len = 0;
	redraw(wchat_input);
}

void handle_sigint(int sig) {
	(void)sig;
	clear_cur_buffer_input();
}

void redraw(WINDOW *w) {
	werase(w);
	mvwprintw(w, 0, 0, "%.*s",
		(int)(cur_buffer->right - cur_buffer->left + 1),
		&cur_buffer->buf[cur_buffer->left]);
	wmove(w, 0, cur_buffer->pos - cur_buffer->left);
	wrefresh(w);
}

void change_cur_buffer(struct buffer *buffers) {
	cur_buffer = buffers;
	int maxy, maxx;
	getmaxyx(wchat_input, maxy, maxx);
	(void)maxy;
	cur_buffer->left = 0;
	cur_buffer->right = maxx-1;
	cur_buffer->room->unread_msgs = 0;
}

void drawline(WINDOW *w, char ch) {
	int maxy, maxx;
	getmaxyx(w, maxy, maxx);
	mvwhline(w, maxy-2, 0, ch, maxx);
}

void fill_msgs() {
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

size_t idx = 0;
size_t top = 0;
size_t bottom = 0;

void index_rooms_cursor_inc(int offset) {
	/* TODO: test case vector_len(buffers) == 0 */
	if (idx == 0 && offset < 0)
		idx = vector_len(buffers);
	if (idx >= vector_len(buffers)-1 && offset > 0)
		idx = -1;
	idx += offset;
}

/* Jump to the next room with unread message */
void index_rooms_next_unread(int direction) {
	int idx2 = idx;
	do  {
		index_rooms_cursor_inc(direction);
		struct buffer *b;
		b = vector_at(buffers, idx);
		if (b->room->unread_msgs > 0)
			break;
	} while (idx2 != idx);
}

void index_rooms_update_top_bottom() {
	int maxy, maxx;
	getmaxyx(windex, maxy, maxx);
	(void)maxx;
	top = 0;
	bottom = maxy-1;
	if (bottom >= vector_len(buffers)-1)
		bottom = vector_len(buffers)-1;
}

void index_rooms_cursor_show() {
	if (vector_len(buffers) == 0)
		return;

	/* Cursor is below bottom */
	if (idx > bottom) {
		top += idx - bottom;
		bottom = idx;
	} else if (idx < top) {
		bottom -= top - idx;
		top = idx;
	}

	assert(top >= 0 && top < vector_len(buffers));
	assert(bottom >= 0 && bottom < vector_len(buffers));
	assert(idx >= top && idx <= bottom);

	werase(windex);
	for (size_t i = top; i <= bottom; i++) {
		struct buffer *tb;
		tb = vector_at(buffers, i);
		if (idx == i)
			wattron(windex, A_REVERSE); 
		if (tb->room->unread_msgs > 0)
			wattron(windex, A_BOLD);
		mvwprintw(windex, i-top, 0, "%s (%zu)",
			tb->room->name->buf, tb->room->unread_msgs);
		if (idx == i)
			wattroff(windex, A_REVERSE); 
		if (tb->room->unread_msgs > 0)
			wattroff(windex, A_BOLD);
	}
	wrefresh(windex);
}

void input_cursor_inc(int offset) {
	if (cur_buffer->pos + offset < 0)
		return;
	if (cur_buffer->pos + offset > cur_buffer->len)
		return;
	cur_buffer->pos += offset;
}

void input_cursor_show() {
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

void change_focus(enum Focus f) {
	focus = f;
	switch (focus) {
	case FOCUS_INDEX:
		cur_buffer = NULL;
		focus = FOCUS_INDEX;
		index_rooms_cursor_show();
		wrefresh(windex);
		break;
	case FOCUS_CHAT_INPUT:
		fill_msgs();
		drawline(wchat, '-');
		redraw(wchat_input);
		wrefresh(wchat);
		break;
	}
}

void resize() {
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
		fill_msgs();
		break;
	case FOCUS_INDEX:
		index_rooms_update_top_bottom();
		index_rooms_cursor_show();
		break;
	}
	drawline(wchat, '-');
	if (cur_buffer)
		/* We force a redraw of the current buffer input window */
		change_cur_buffer(cur_buffer);
}

void process_menu() {
	int c = wgetch(windex);
	switch (c) {
	case 'Q':
		endwin();
		exit(0);
		break;
	case 'k':
	case KEY_UP:
		index_rooms_cursor_inc(-1);
		index_rooms_cursor_show();
		break;
	case 'j':
	case KEY_DOWN:
		index_rooms_cursor_inc(+1);
		index_rooms_cursor_show();
		break;
	case 'K':
		index_rooms_next_unread(-1);
		index_rooms_cursor_show();
		break;
	case 'J':
		index_rooms_next_unread(+1);
		index_rooms_cursor_show();
		break;
	case KEY_RESIZE:
		resize();
		break;
	case 10:
	case 13:
		/* TODO: what if buffers is empty? */
		change_cur_buffer(vector_at(buffers, idx));
		change_focus(FOCUS_CHAT_INPUT);
		break;
	}
}

void send_msg() {
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
	fill_msgs();
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

void process_input(WINDOW *w) {
	int c = wgetch(w);

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
		input_cursor_inc(-1);
		break;
	case KEY_RIGHT:
		input_cursor_inc(+1);
		break;
	case CTRL('g'):
		change_focus(FOCUS_INDEX);
		return;
		break;
	case 10: /* LF */
	case 13: /* CR */
		if (streq(cur_buffer->buf, "/quit")) {
			change_focus(FOCUS_INDEX);
		} else {
			send_msg();
		}
		clear_cur_buffer_input();
		break;
	default:
		for (size_t i = cur_buffer->len; i > cur_buffer->pos; i--)
			cur_buffer->buf[i] = cur_buffer->buf[i-1];
		cur_buffer->buf[cur_buffer->pos] = c;
		cur_buffer->len++;
		input_cursor_inc(+1);
		break;
	}
	cur_buffer->buf[cur_buffer->len] = '\0';
	input_cursor_show();
	redraw(wchat_input);
}

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

void ui_curses_iter() {
	/* TODO: fix draw order */
	switch (focus) {
	case FOCUS_INDEX:
		/*
		 * TODO: having it here the buffer be sorted for ever keystroke.
		 * We should find a better place to have it.
		 */
		vector_sort(buffers, buffer_comparison);

		process_menu();
		break;
	case FOCUS_CHAT_INPUT:
		process_input(wchat_input);
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
		index_rooms_cursor_show(); /* Update window */
		return;
	}
	/* TODO: what about other parameters? */
	if (cur_buffer && cur_buffer->room == room) {
		cur_buffer->room->unread_msgs = 0;
		fill_msgs();
	}
}

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
