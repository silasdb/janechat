/* curses front-end for janechat */

#include <assert.h>
#include <ctype.h>
#include <locale.h>
#include <regex.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>

#include "ui.h"
#include "ui-curses.h"
#include "rooms.h"
#include "str.h"
#include "vector.h"
#include "utils.h"

/* We can detect Ctrl+key sequences by masking the return of getch() */
#define CTRL(x) (x & 037)

#define MAXY 1000 /* Max lines we allow to be shown in the terminal */

/*
 * We call the data structure that holds the room information for the UI as
 * "buffer".  We have very few UI elements, the only thing that changes is the
 * buffer, that is unique for each room.
 */
struct buffer {
	Room *room;
	Str *buf; /* Input buffer. TODO: use Str? */
	size_t pos; /* Cursor position - UTF-8 index. */

	/* Left-most character index showed in the input window - UTF-8 index */
	size_t left; 

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
};

bool curses_init = false; /* Did we started curses? */
bool autopilot = false;

WINDOW *windex; /* The index window - that shows rooms */
WINDOW *wmsgs; /* A window that show messages received */
WINDOW *wstatus; /* A window to: show room status bar */
WINDOW *winput; /* A window for input common to both windex and wmsgs */

enum Focus {
	/* Focus is on the index window */
	FOCUS_INDEX,
	/* chat windows are visible and focus is on its input subwindow */
	FOCUS_CHAT_INPUT,
	FOCUS_INDEX_INPUT,
} focus = FOCUS_INDEX;

/* A vector of struct buffer. Used in the index window */
Vector *buffers = NULL; /* Vector<struct buffer> */

/* Current buffer selected. NULL if focus is in index window */
struct buffer *cur_buffer = NULL;

struct buffer index_input_buffer;

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
	cur_buffer->left = 0;
	if (cur_buffer->room)
		cur_buffer->room->unread_msgs = 0;
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
		index_update_top_bottom();
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
		break;
	}
	input_redraw();
}

void resize(void) {
	int maxy, maxx;

	clear();
	getmaxyx(stdscr, maxy, maxx);

	wresize(windex, maxy-1, maxx);
	mvwin(wstatus, maxy-2, 0);
	wresize(wstatus, 1, maxx);

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
	const char *c = str_buf(cur_buffer->buf);
	while (isspace(*c))
		c++;
	if (*c == '\0')
		return;

	struct UiEvent ev;
	ev.type = UIEVENTTYPE_SENDMSG,
	ev.msg.roomid = str_incref(cur_buffer->room->id);
	ev.msg.text = str_incref(cur_buffer->buf);
	ui_event_handler_callback(ev);
	str_decref(ev.msg.roomid);
	str_decref(ev.msg.text);
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

regex_t re;

void index_find_next(int direction) {
	size_t idx = index_idx;
	do  {
		index_cursor_inc(direction);
		struct buffer *b;
		b = vector_at(buffers, index_idx);
		const char *s = str_buf(room_displayname(b->room));
		if (regexec(&re, s, 0, NULL, 0) == 0)
			break;
	} while (idx != index_idx);
}

void set_buffer_mute(bool mute) {
	struct buffer *b = vector_at(buffers, index_idx);
	if (!b)
		return;
	b->room->notify = !mute;
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
		/* TODO: only redraw if we found a valid item */
		index_next_unread(-1);
		index_draw();
		break;
	case 'J':
		/* TODO: only redraw if we found a valid item */
		index_next_unread(+1);
		index_draw();
		break;
	case 'm':
		set_buffer_mute(true);
		index_draw();
		break;
	case 'M':
		set_buffer_mute(false);
		index_draw();
		break;
	case 'N':
		/* TODO: only redraw if we found a valid item */
		index_find_next(-1);
		index_draw();
		break;
	case 'n':
		/* TODO: only redraw if we found a valid item */
		index_find_next(+1);
		index_draw();
		break;;
	case '/':
		str_reset(index_input_buffer.buf);
		str_append_cstr(index_input_buffer.buf, "/");
		/* FALLTHROUGH */
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
 * Private functions for chat windows.
 */

void chat_draw_statusbar(void) {
	int maxy, maxx;
	(void)maxy;
	getmaxyx(stdscr, maxy, maxx);
	Str *roomname = room_displayname(cur_buffer->room);
	mvwprintw(wstatus, 0, 0, "%s", str_buf(roomname));
	mvwhline(wstatus, 0, str_bytelen(roomname), ' ', maxx);
	wrefresh(wstatus);
}

int text_height(const Str *sender, const Str *text, int width) {
	/* TODO: still need to consider UTF-8 characters */
	int height = 1;
	int w = str_bytelen(sender) + 2; /* +2 because of ": " */
	const char *c;
	for (c = str_buf(text); *c != '\0'; c++, w++)
		if (*c == '\n' || w >= width) {
			height++;
			w = 0;
		}
	return height;
}

void chat_msgs_fill(void) {
	werase(wmsgs);
	wrefresh(wmsgs);

	Msg *msg;
	size_t i;
	ROOM_MESSAGES_FOREACH(cur_buffer->room, msg, i) {
		if (cur_buffer->read_separator == i+1
		&&  cur_buffer->read_separator != (int)vector_len(cur_buffer->room->msgs)) {
			wattron(wmsgs, COLOR_PAIR(1));
			waddstr(wmsgs, "-----");
			wattroff(wmsgs, COLOR_PAIR(1));
		}
		if (cur_buffer->user_separator == i) {
			wattron(wmsgs, COLOR_PAIR(2));
			waddstr(wmsgs, "-----");
			wattroff(wmsgs, COLOR_PAIR(2));
		}

		wattron(wmsgs, COLOR_PAIR(1));
		wprintw(wmsgs, "[%zu] %s", i,
			str_buf(user_name(msg->sender)));

		/* TODO: why does it set background to COLOR_BLACK? */
		wattroff(wmsgs, COLOR_PAIR(1));

		if (msg->type == MSGTYPE_TEXT)
			wprintw(wmsgs, ": %s\n", str_buf(msg->text.content));
		else
			wprintw(wmsgs, ": %s: %s\n",
				str_buf(msg->fileinfo.mimetype),
				str_buf(msg->fileinfo.uri));

	}
	int y = getcury(wmsgs);
	int maxy, maxx;
	getmaxyx(stdscr, maxy, maxx);
	maxy -= 2;
	int top = y - maxy;
	if (top < 0)
		top = 0;
	prefresh(wmsgs, top, 0, 0, 0, y, 10);
}

void input_clear(void) {
	if (!cur_buffer)
		return;

	cur_buffer->pos = 0;
	cur_buffer->left = 0;
	str_reset(cur_buffer->buf);
	input_redraw();
}

void input_redraw(void) {
	size_t maxy, maxx, right;
	(void)maxy;
	getmaxyx(winput, maxy, maxx);

	werase(winput);

	size_t *pos = &cur_buffer->pos;
	size_t *left = &cur_buffer->left;

	/* TODO: work with Str type instead of the inner buffer */
	const char *buf = str_buf(cur_buffer->buf);

	if (*pos < *left)
		*left = *pos;

	/* Now discover largest possible right */
	size_t screenwidth = 0;
	right = *left;
	while (right < str_utf8len(cur_buffer->buf)) {
		size_t bytepos = utf8_char_bytepos(buf, right, str_bytelen(cur_buffer->buf));
		screenwidth += utf8_char_width(&buf[bytepos]);
		if (screenwidth >= maxx)
			break;
		right++;
	}

	if (*pos > right) {
		right = *pos;

		assert(right <= str_utf8len(cur_buffer->buf));

		/*
		 * Discover new left: we need to walk backwards from `right`, to
		 * discover the right left position from where we should start
		 * printing our string.
		 */
		*left = right;
		if (*left == str_utf8len(cur_buffer->buf))
			(*left)--;
		screenwidth = 0;
		for (; *left > 0; (*left)--) {
			size_t bytepos = utf8_char_bytepos(buf, *left, str_bytelen(cur_buffer->buf));
			screenwidth += utf8_char_width(&buf[bytepos]);
			if (screenwidth >= maxx)
				break;
		}
		*left += 1;
	}

	/* Draw string in input window */
	int screenpos = 0;
	for (size_t i = cur_buffer->left; i < right; i++) {
		size_t bytepos = utf8_char_bytepos(buf, i, str_bytelen(cur_buffer->buf));
		if (buf[bytepos] == '\0')
			break;
		size_t chsize = utf8_char_size(buf[bytepos]);
		mvwprintw(winput, 0, screenpos,
			"%.*s", chsize, &buf[bytepos]);
	 	screenpos += utf8_char_width(&buf[bytepos]);
	}

	/*
	 * Discover where to position cursor. Some characters may fullfil more
	 * than one terminal column, (e.g. chinese characters).
	 */
	screenpos = 0;
	for (size_t i = cur_buffer->left; i < cur_buffer->pos; i++) {
		size_t bytepos = utf8_char_bytepos(buf, i, str_bytelen(cur_buffer->buf));
		screenpos += utf8_char_width(&buf[bytepos]);
	}
	wmove(winput, 0, screenpos);

	wrefresh(winput);
}

void input_cursor_inc(int offset) {
	if ((int)cur_buffer->pos + offset < 0)
		return;
	if (cur_buffer->pos + offset > str_utf8len(cur_buffer->buf))
		return;
	cur_buffer->pos += offset;
}

bool input_key_index(int c) {
	switch (c) {
	case 10:
	case 13:
		if (str_sc_eq(cur_buffer->buf, "set autopilot"))
			autopilot = true;
		else if (str_sc_eq(cur_buffer->buf, "unset autopilot"))
			autopilot = false;
		else if (str_sc_eq(cur_buffer->buf, "set mute"))
			set_buffer_mute(true);
		else if (str_sc_eq(cur_buffer->buf, "unset mute"))
			set_buffer_mute(false);
		else {
			Utf8Char uc;
			uc = str_utf8char_at(cur_buffer->buf, UTF8_INDEX(0));
			if (uc.c[0] == '/') {
				regcomp(&re, &(str_buf(cur_buffer->buf)[1]),
					REG_EXTENDED|REG_ICASE|REG_NOSUB);
				/* TODO: only redraw if we found a valid item */
				index_find_next(+1);
				index_draw();
			}
		}
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
		wscrl(wmsgs, -5);
		wrefresh(wmsgs);
		return true;
		break;
	case CTRL('f'):
		wscrl(wmsgs, 5);
		wrefresh(wmsgs);
		return true;
		break;
	case 10: /* LF */
	case 13: /* CR */
		if (str_sc_eq(cur_buffer->buf, "/quit")) {
			set_focus(FOCUS_INDEX);
		} else if (str_sc_eq(cur_buffer->buf, "/line")) {
			cur_buffer->user_separator = vector_len(cur_buffer->room->msgs)-1;
			chat_msgs_fill();
		} else if (str_sc_eq(cur_buffer->buf, "/disableautopilot")) {
			autopilot = false;
		} else if (str_starts_with_cstr(cur_buffer->buf, "/open ")) {
			const char *number = str_buf(cur_buffer->buf) + strlen("/open ");
			long int id;
			if (str2li(number, &id) &&
			   id >= 0 && (size_t)id < vector_len(cur_buffer->room->msgs)) {
				Msg *msg = vector_at(cur_buffer->room->msgs, id);
				assert(msg);
				if (msg->type == MSGTYPE_FILE) {
					struct UiEvent ev;
					ev.type = UIEVENTTYPE_OPENATTACHMENT;
					ev.openattachment.fileinfo = msg->fileinfo;
					ui_event_handler_callback(ev);
				}
			}
		} else {
			Utf8Char uc;
			uc = str_utf8char_at(cur_buffer->buf, UTF8_INDEX(0));
			if (uc.c[0] == '/') {
				/* TODO: show in the UI invalid command */
				/*
				 * TODO: still need to allow user to send messages that
				 * start with '/'.  Maybe by double '/' like IRC clients
				 * and Element do?
				 */
			} else {
				send_msg();
			}
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
	case KEY_BACKSPACE: {
		if (cur_buffer->pos == 0)
			break;
		str_remove_utf8char_at(cur_buffer->buf,
			UTF8_INDEX(cur_buffer->pos-1));
		cur_buffer->pos--;
		break;
	}
	case KEY_LEFT:
		input_cursor_inc(-1);
		break;
	case KEY_RIGHT:
		input_cursor_inc(+1);
		break;
	default: {
		Utf8Char uc = {.c = '\0' };
		uc.c[0] = c;
		size_t sz = utf8_char_size(c);
		for (size_t i = 1; i < sz; i++) {
			c = wgetch(winput);
			uc.c[i] = c;
		}

		str_insert_utf8char_at(cur_buffer->buf, uc,
			UTF8_INDEX(cur_buffer->pos));
		input_cursor_inc(+1);
		break;
	}
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

	input_redraw();
}

/*
 * Public functions
 */

void ui_curses_setup(void) {
	if (!buffers)
		buffers = vector_new();

 	index_input_buffer = (struct buffer){
 		.buf = str_new(),
 		.room = NULL,
 	};
}

void ui_curses_init(void) {
	curses_init = true;

	/*
	 * TODO: it seems to solve the problem, but we should investigate why. See (https://stackoverflow.com/questions/9922528/how-to-make-ncurses-display-utf-8-chars-correctly-in-c)
	 */
	setlocale(LC_CTYPE, "");

	initscr();
	clear();
	nonl();
	cbreak();
	noecho();

	int maxy, maxx;
	getmaxyx(stdscr, maxy, maxx);

	windex = newwin(maxy-1, maxx, 0, 0);
	wmsgs = newpad(MAXY, maxx);
	wstatus = newwin(1, maxx, maxy-2, 0);
	winput = newwin(1, maxx, maxy-1, 0);
	keypad(windex, TRUE);
	keypad(winput, TRUE);

	start_color();
	use_default_colors();
	init_pair(1, COLOR_GREEN, -1);
	init_pair(2, COLOR_YELLOW, -1);
	init_pair(3, COLOR_WHITE, COLOR_BLUE);

	wattron(wstatus, COLOR_PAIR(3));

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
	b->buf = str_new();
	b->room = room_byid(roomid);
	b->pos = 0;
	b->left = 0;
	b->read_separator = -1;
	b->user_separator = -1;
	vector_append(buffers, b);
	if (curses_init) {
		vector_sort(buffers, buffer_comparison);
		index_update_top_bottom();
	}
}

void ui_curses_msg_new(Room *room, Msg msg) {
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
