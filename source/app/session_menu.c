#include "session_menu.h"

#include <string.h>

void cb_session_menu_init(cb_session_menu *menu) {
	if (menu) memset(menu, 0, sizeof(*menu));
}

void cb_session_menu_move(cb_session_menu *menu, int delta) {
	long selected;
	if (!menu || !delta) return;
	selected = (long)menu->selected + delta;
	while (selected < 0) selected += CB_SESSION_MENU_COUNT;
	while (selected >= CB_SESSION_MENU_COUNT) selected -= CB_SESSION_MENU_COUNT;
	menu->selected = (size_t)selected;
}

cb_session_menu_action cb_session_menu_selected(const cb_session_menu *menu) {
	if (!menu || menu->selected >= CB_SESSION_MENU_COUNT)
		return CB_SESSION_MENU_RESUME;
	return (cb_session_menu_action)menu->selected;
}
