#ifndef CHANNEL_BLUE_LOGIN_H
#define CHANNEL_BLUE_LOGIN_H

#include "auth.h"

#define CB_LOGIN_SERVICE_MAX 255
#define CB_LOGIN_IDENTIFIER_MAX 253
#define CB_LOGIN_PASSWORD_MAX 128

typedef enum {
	CB_LOGIN_SERVICE = 0,
	CB_LOGIN_IDENTIFIER,
	CB_LOGIN_PASSWORD,
	CB_LOGIN_FIELD_COUNT
} cb_login_field;

typedef struct {
	char service[CB_LOGIN_SERVICE_MAX + 1];
	char identifier[CB_LOGIN_IDENTIFIER_MAX + 1];
	char password[CB_LOGIN_PASSWORD_MAX + 1];
	size_t service_length;
	size_t identifier_length;
	size_t password_length;
	cb_login_field active_field;
	cb_app_status last_status;
} cb_login_form;

void cb_login_form_init(cb_login_form *form);
void cb_login_form_next_field(cb_login_form *form, int direction);
cb_app_status cb_login_form_insert(cb_login_form *form, unsigned int character);
void cb_login_form_backspace(cb_login_form *form);

/* Password is erased after every successful login and remains available after
 * failure so the user can correct a transient/network problem and retry. */
cb_app_status cb_login_form_submit(cb_login_form *form, cb_auth *auth,
	                               const cb_auth_backend *backend,
	                               void *context, const char *session_path);

#endif /* CHANNEL_BLUE_LOGIN_H */
