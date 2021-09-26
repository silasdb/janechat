#ifndef JANECHAT_UI_CLI_H
#define JANECHAT_UI_CLI_H

#include "ui.h"
#include "rooms.h"

void ui_cli_iter();
void ui_set_event_handler(void (*callback)(UiEvent)); /* TODO: move to ui.h */
void ui_cli_new_msg(Room *room, StrBuf *sender, StrBuf *msg);

#endif
