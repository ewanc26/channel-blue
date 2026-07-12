#ifndef CHANNEL_BLUE_SESSION_MENU_H
#define CHANNEL_BLUE_SESSION_MENU_H

#include <stddef.h>

typedef enum {
	CB_SESSION_MENU_RESUME = 0,
	CB_SESSION_MENU_SIGN_OUT,
	CB_SESSION_MENU_EXIT,
	CB_SESSION_MENU_COUNT
} cb_session_menu_action;

typedef struct {
	size_t selected;
} cb_session_menu;

void cb_session_menu_init(cb_session_menu *menu);
void cb_session_menu_move(cb_session_menu *menu, int delta);
cb_session_menu_action cb_session_menu_selected(const cb_session_menu *menu);

#endif
