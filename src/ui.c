#include "ui.h"

void (*ui_event_handler_callback)(UiEvent) = NULL;

void ui_set_event_handler(void (*callback)(UiEvent)) {
	ui_event_handler_callback = callback;
}
