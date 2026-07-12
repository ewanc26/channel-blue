#include "app/session_menu.h"

#include <assert.h>

int main(void) {
	cb_session_menu menu;
	cb_session_menu_init(&menu);
	assert(cb_session_menu_selected(&menu) == CB_SESSION_MENU_RESUME);
	cb_session_menu_move(&menu, 1);
	assert(cb_session_menu_selected(&menu) == CB_SESSION_MENU_SIGN_OUT);
	cb_session_menu_move(&menu, 1);
	assert(cb_session_menu_selected(&menu) == CB_SESSION_MENU_EXIT);
	cb_session_menu_move(&menu, 1);
	assert(cb_session_menu_selected(&menu) == CB_SESSION_MENU_RESUME);
	cb_session_menu_move(&menu, -1);
	assert(cb_session_menu_selected(&menu) == CB_SESSION_MENU_EXIT);
	assert(cb_session_menu_selected(NULL) == CB_SESSION_MENU_RESUME);
	return 0;
}
