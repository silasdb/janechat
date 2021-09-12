#ifndef JANECHAT_UI_CLI_H
#define JANECHAT_UI_CLI_H

#include "ui.h"
#include "rooms.h"

UiEvent ui_cli_iter();
void ui_cli_new_msg(Room *room, const char *sender, const char *msg);

#endif
