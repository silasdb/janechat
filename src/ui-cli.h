#ifndef JANECHAT_UI_CLI_H
#define JANECHAT_UI_CLI_H

#include "ui.h"
#include "rooms.h"

void ui_cli_iter();
void ui_cli_msg_new(Room *room, Str *sender, Str *msg);

#endif
