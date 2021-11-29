#ifndef JANECHAT_UI_CURSES_H
#define JANECHAT_UI_CURSES_H

void ui_curses_setup(void);
void ui_curses_init(void);
void ui_curses_iter(void);
void ui_curses_msg_new(Room *room, Str *sender, Str *msg);
void ui_curses_room_new(Str *roomid);

#endif /* !JANECHAT_UI_CURSES_H */
