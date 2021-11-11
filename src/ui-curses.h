#ifndef JANECHAT_UI_CURSES_H
#define JANECHAT_UI_CURSES_H

void ui_curses_init();
void ui_curses_iter();
void ui_curses_new_msg(Room *room, Str *sender, Str *msg);

#endif /* !JANECHAT_UI_CURSES_H */
