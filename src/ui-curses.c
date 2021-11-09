/* curses front-end for janechat */

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>

#include "str.h"
#include "vector.h"

struct text_buffer {
	Vector *msgs;
	char buf[256];
	size_t pos; /* Cursor position */
	size_t len; /* String length - does not include the null byte*/
	size_t left;
	size_t right;
	int last_line;
};

WINDOW *windex;
WINDOW *windex_rooms;
WINDOW *windex_input;
WINDOW *wchat;
WINDOW *wchat_msgs;
WINDOW *wchat_input;

#define MAX_ROOMS 14
struct text_buffer b[MAX_ROOMS];

struct text_buffer *cur_buffer = &b[0];
struct text_buffer index_buffer;

void debug(const char *path, const char *format, ...) {
	FILE *f = fopen(path, "a");
	va_list args;
	va_start(args, format);
	vfprintf(f, format, args);
	fclose(f);
}

void redraw(WINDOW *w) {
	werase(w);
	wrefresh(w);
	mvwprintw(w, 0, 0, "%.*s",
		(int)(cur_buffer->right - cur_buffer->left + 1),
		&cur_buffer->buf[cur_buffer->left]);
	wmove(w, 0, cur_buffer->pos - cur_buffer->left);
}

WINDOW *new_window(int h, int w, int y, int x) {
	WINDOW *win;
	win = newwin(h, w, y, x);
	return win;
}

enum Focus {
	FOCUS_INDEX_ROOMS,
	FOCUS_INDEX_INPUT,
	FOCUS_CHAT_INPUT,
} focus = FOCUS_INDEX_ROOMS;

char *menu_entries[MAX_ROOMS] = {
	"first",
	"bar",
	"more",
	"lore",
	"lore",
	"lore",
	"lore",
	"lore",
	"lore",
	"lore",
	"baz",
	"baz",
	"baz",
	"last",
};

void change_cur_buffer(struct text_buffer *b) {
	cur_buffer = b;
	int maxy, maxx;
	getmaxyx(wchat_input, maxy, maxx);
	(void)maxy;
	cur_buffer->left = 0;
	cur_buffer->right = maxx-1;
}

void drawline(WINDOW *w, char ch) {
	int maxy, maxx;
	getmaxyx(w, maxy, maxx);
	mvwhline(w, maxy-2, 0, ch, maxx);
}

void fill_msgs() {
	werase(wchat_msgs);
	int last;
	if (cur_buffer->last_line == -1)
		last = cur_buffer->msgs->len-1;
	if (last < 0)
		return;
	int maxy, maxx;
	getmaxyx(wchat_msgs, maxy, maxx);
	/*
	 * We make a naive calculation of line height and print them backwards.
	 */
	int y = maxy;
	for (int i = last; i >= 0; i--) {
		size_t len = strlen(cur_buffer->msgs->elems[i]);
		int height = len / maxx;
		height++;
		y -= height;
		if (y < 0)
			break;
		mvwprintw(wchat_msgs, y, 0, "%s\n", (char *)cur_buffer->msgs->elems[i]);
	}
	wrefresh(wchat_msgs);
}

size_t idx = 0;
size_t top = 0;
size_t bottom = MAX_ROOMS-1;

void index_rooms_cursor_inc(int offset) {
	if (idx == 0 && offset < 0)
		return;
	if (idx >= MAX_ROOMS-1 && offset > 0)
		return;
	idx += offset;
}

void index_rooms_update_top_bottom() {
	int maxy, maxx;
	getmaxyx(windex_rooms, maxy, maxx);
	(void)maxx;
	top = 0;
	bottom = maxy-1;
	if (bottom >= MAX_ROOMS-1)
		bottom = MAX_ROOMS-1;
}

void index_rooms_cursor_show() {
	/* Cursor is below bottom */
	if (idx > bottom) {
		top += idx - bottom;
		bottom = idx;
	} else if (idx < top) {
		bottom -= top - idx;
		top = idx;
	}

	assert(top >= 0 && top < MAX_ROOMS);
	assert(bottom >= 0 && bottom < MAX_ROOMS);
	assert(idx >= top && idx <= bottom);

	werase(windex_rooms);
	for (size_t i = top; i <= bottom; i++) {
		if (idx == i)
			wattron(windex_rooms, A_REVERSE); 
		mvwprintw(windex_rooms, i-top, 0, menu_entries[i]);
		if (idx == i)
			wattroff(windex_rooms, A_REVERSE); 
	}
}

void input_cursor_inc(int offset) {
	if (cur_buffer->pos + offset < 0)
		return;
	if (cur_buffer->pos + offset > cur_buffer->len)
		return;
	cur_buffer->pos += offset;
}

void input_cursor_show() {
	//int maxy, maxx;
	//getmaxyx(wchat_input, maxy, maxx);
	//(void)maxy;
	size_t *pos = &cur_buffer->pos;
	size_t *left = &cur_buffer->left;
	size_t *right = &cur_buffer->right;
	if (*pos > *right) {
		*left += *pos - *right;
		*right = *pos;
	} else if (*pos < *left) {
		*right -= *left - *pos;
		*left = *pos;
	} else
		return;
}

void resize() {
	int maxy, maxx;
	clear();
	getmaxyx(stdscr, maxy, maxx);
	wresize(windex, maxy, maxx);
	wresize(wchat, maxy, maxx);
	wresize(wchat_msgs, maxy-2, maxx);
	wresize(windex_rooms, maxy-2, maxx);
	wresize(windex_input, 1, maxx);
	wresize(wchat_input, 1, maxx);
	mvwin(wchat_input, maxy-1, 0);
	mvwin(windex_input, maxy-1, 0);
	mvwin(windex_rooms, 0, 0);
	switch (focus) {
	case FOCUS_CHAT_INPUT:
		fill_msgs();
		break;
	case FOCUS_INDEX_INPUT:
	case FOCUS_INDEX_ROOMS:
		index_rooms_update_top_bottom();
		index_rooms_cursor_show();
		break;
	}
	drawline(windex, '~');
	drawline(wchat, '-');
	change_cur_buffer(cur_buffer);
}

void process_menu() {
	int c = wgetch(windex_rooms);
	switch (c) {
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
	case KEY_RESIZE:
		resize();
		break;
	case ':':
		change_cur_buffer(&index_buffer);
		focus = FOCUS_INDEX_INPUT;
		break;
	case 10:
	case 13:
		change_cur_buffer(&b[idx]);
		fill_msgs();
		focus = FOCUS_CHAT_INPUT;
		drawline(windex, '~');
		break;
	}
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
	case 10: /* LF */
	case 13: /* CR */
		if (focus == FOCUS_INDEX_INPUT) {
			if (streq(cur_buffer->buf, "quit")) {
				endwin();
				exit(0);
			}
			break;
		}
		if (streq(cur_buffer->buf, "/quit")) {
			cur_buffer->buf[0] = '\0';
			cur_buffer->len = 0;
			cur_buffer->pos = 0;
			erase();
			refresh();
			focus = FOCUS_INDEX_ROOMS;
			change_cur_buffer(&index_buffer);
			index_rooms_cursor_show();
		} else {
			vector_append(cur_buffer->msgs, strdup(cur_buffer->buf));
			fill_msgs();
		}
		cur_buffer->buf[0] = '\0';
		cur_buffer->pos = 0;
		cur_buffer->len = 0;
		break;
	case 27: /* Esc */
		if (focus == FOCUS_INDEX_INPUT)
			focus = FOCUS_INDEX_ROOMS;
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
}

#ifdef UI_CURSES_TEST
int main(int argc, char *argv[]) {

	for (size_t i = 0; i < MAX_ROOMS; i++) {
		b[i].msgs = vector_new();
		b[i].pos = 0;
		b[i].len = 0;
		b[i].last_line = -1;
		b[i].buf[0] = '\0';
	}

	initscr();
	clear();
	nonl();
	cbreak();
	noecho();

	int maxy, maxx;
	getmaxyx(stdscr, maxy, maxx);

	wchat = new_window(maxy, maxx, 0, 0);
	windex = new_window(maxy, maxx, 0, 0);
	windex_rooms = subwin(windex, maxy-2, maxx, 0, 0);
	windex_input = subwin(windex, 1, maxx, maxy-1, 0);
	wchat_msgs = subwin(wchat, maxy-1, maxx, 0, 0);
	wchat_input = subwin(wchat, 1, maxx, maxy-1, 0);
	keypad(windex_rooms, TRUE);
	//keypad(windex_input, TRUE);
	keypad(wchat_input, TRUE);
	resize();

	for (;;) {
		switch (focus) {
		case FOCUS_INDEX_ROOMS:
			wrefresh(windex);
			process_menu();
			break;
		case FOCUS_INDEX_INPUT:
			redraw(windex_input);
			wrefresh(windex);
			process_input(windex_input);
			break;
		case FOCUS_CHAT_INPUT:
			redraw(wchat_input);
			wrefresh(wchat);
			process_input(wchat_input);
			break;
		}
	}

	return 0;
}
#endif
