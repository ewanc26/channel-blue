#include "login.h"

#include <string.h>

static void active_buffer(cb_login_form *form, char **buffer, size_t **length,
	                      size_t *capacity) {
	if (form->active_field == CB_LOGIN_SERVICE) {
		*buffer = form->service;
		*length = &form->service_length;
		*capacity = CB_LOGIN_SERVICE_MAX;
	} else if (form->active_field == CB_LOGIN_IDENTIFIER) {
		*buffer = form->identifier;
		*length = &form->identifier_length;
		*capacity = CB_LOGIN_IDENTIFIER_MAX;
	} else {
		*buffer = form->password;
		*length = &form->password_length;
		*capacity = CB_LOGIN_PASSWORD_MAX;
	}
}

void cb_login_form_init(cb_login_form *form) {
	static const char service[] = "https://bsky.social";
	if (!form) return;
	memset(form, 0, sizeof(*form));
	memcpy(form->service, service, sizeof(service));
	form->service_length = sizeof(service) - 1;
	form->active_field = CB_LOGIN_IDENTIFIER;
}

void cb_login_form_next_field(cb_login_form *form, int direction) {
	int field;
	if (!form) return;
	field = (int)form->active_field + (direction < 0 ? -1 : 1);
	if (field < 0) field = CB_LOGIN_FIELD_COUNT - 1;
	if (field >= CB_LOGIN_FIELD_COUNT) field = 0;
	form->active_field = (cb_login_field)field;
}

cb_app_status cb_login_form_insert(cb_login_form *form, unsigned int character) {
	char *buffer;
	size_t *length;
	size_t capacity;
	if (!form || character < 32 || character > 126) return CB_APP_INVALID;
	active_buffer(form, &buffer, &length, &capacity);
	if (*length >= capacity) return CB_APP_INVALID;
	buffer[(*length)++] = (char)character;
	buffer[*length] = '\0';
	return CB_APP_OK;
}

void cb_login_form_backspace(cb_login_form *form) {
	char *buffer;
	size_t *length;
	size_t capacity;
	if (!form) return;
	active_buffer(form, &buffer, &length, &capacity);
	(void)capacity;
	if (*length) buffer[--(*length)] = '\0';
}

cb_app_status cb_login_form_submit(cb_login_form *form, cb_auth *auth,
	                               const cb_auth_backend *backend,
	                               void *context, const char *session_path) {
	if (!form || !form->service_length || !form->identifier_length ||
	    !form->password_length) return CB_APP_INVALID;
	form->last_status = cb_auth_login(auth, backend, context, session_path,
	                                 form->service, form->identifier,
	                                 form->password);
	if (form->last_status == CB_APP_OK) {
		memset(form->password, 0, sizeof(form->password));
		form->password_length = 0;
	}
	return form->last_status;
}
