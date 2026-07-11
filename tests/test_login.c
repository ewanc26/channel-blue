#include "app/login.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static char *copy(const char *value) {
	char *result = malloc(strlen(value) + 1);
	assert(result);
	strcpy(result, value);
	return result;
}

static cb_app_status login(void *context, const char *service,
	                       const char *identifier, const char *password,
	                       cb_session_data *out) {
	(void)context;
	assert(strcmp(service, "https://bsky.social") == 0);
	assert(strcmp(identifier, "alice.test") == 0);
	assert(strcmp(password, "secret") == 0);
	out->service = copy(service);
	out->access_jwt = copy("access");
	out->refresh_jwt = copy("refresh");
	out->handle = copy(identifier);
	out->did = copy("did:plc:alice");
	return CB_APP_OK;
}

int main(void) {
	const char *path = "/tmp/channel-blue-login-test";
	cb_login_form form;
	cb_auth auth;
	cb_auth_backend backend = {0};
	const char *identifier = "alice.test";
	const char *password = "secret";
	size_t i;

	backend.login = login;
	cb_session_clear(path);
	cb_login_form_init(&form);
	cb_auth_init(&auth);
	for (i = 0; identifier[i]; i++) cb_login_form_insert(&form, identifier[i]);
	cb_login_form_next_field(&form, 1);
	assert(form.active_field == CB_LOGIN_PASSWORD);
	for (i = 0; password[i]; i++) cb_login_form_insert(&form, password[i]);
	assert(cb_login_form_submit(&form, &auth, &backend, NULL, path) == CB_APP_OK);
	assert(auth.state == CB_AUTH_READY && form.password_length == 0);
	cb_auth_free(&auth);
	cb_session_clear(path);
	return 0;
}
