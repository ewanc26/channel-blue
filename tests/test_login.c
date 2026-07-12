#include "app/login.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	cb_app_status result;
	int calls;
} context_t;

static char *copy(const char *value) {
	char *result = malloc(strlen(value) + 1);
	assert(result);
	strcpy(result, value);
	return result;
}

static cb_app_status login(void *context, const char *service,
	                       const char *identifier, const char *password,
	                       cb_session_data *out) {
	context_t *ctx = context;
	ctx->calls++;
	assert(strcmp(service, "https://bsky.social") == 0);
	assert(strcmp(identifier, "alice.test") == 0);
	assert(strcmp(password, "secret") == 0);
	if (ctx->result != CB_APP_OK) return ctx->result;
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
	context_t context = {0};
	const char *identifier = "alice.test";
	const char *password = "secret";
	size_t i;

	backend.login = login;
	cb_session_clear(path);

	/* happy path: fill identifier + password, submit, password erased */
	cb_login_form_init(&form);
	cb_auth_init(&auth);
	for (i = 0; identifier[i]; i++) cb_login_form_insert(&form, identifier[i]);
	cb_login_form_next_field(&form, 1);
	assert(form.active_field == CB_LOGIN_PASSWORD);
	for (i = 0; password[i]; i++) cb_login_form_insert(&form, password[i]);
	assert(cb_login_form_submit(&form, &auth, &backend, &context, path) ==
	       CB_APP_OK);
	assert(auth.state == CB_AUTH_READY && form.password_length == 0);
	cb_auth_free(&auth);
	cb_session_clear(path);

	/* empty credentials are rejected without a backend call */
	cb_login_form_init(&form);
	cb_auth_init(&auth);
	context = (context_t){0};
	assert(cb_login_form_submit(&form, &auth, &backend, &context, path) ==
	       CB_APP_INVALID);
	assert(context.calls == 0);
	for (i = 0; identifier[i]; i++) cb_login_form_insert(&form, identifier[i]);
	assert(cb_login_form_submit(&form, &auth, &backend, &context, path) ==
	       CB_APP_INVALID);
	assert(context.calls == 0);
	cb_auth_free(&auth);

	/* out-of-range characters are rejected; valid ones are accepted */
	assert(cb_login_form_insert(&form, '\n') == CB_APP_INVALID);
	assert(cb_login_form_insert(&form, 127) == CB_APP_INVALID);
	assert(cb_login_form_insert(&form, 'a') == CB_APP_OK);
	cb_login_form_backspace(&form);
	assert(form.identifier_length == strlen(identifier));
	assert(cb_login_form_insert(&form, 0x00e9) == CB_APP_OK);
	assert(form.identifier_length == strlen(identifier) + 2);
	cb_login_form_backspace(&form);
	assert(form.identifier_length == strlen(identifier));

	/* password field clamps at its capacity */
	cb_login_form_init(&form);
	cb_login_form_next_field(&form, 1);
	for (i = 0; i < CB_LOGIN_PASSWORD_MAX; i++)
		assert(cb_login_form_insert(&form, 'x') == CB_APP_OK);
	assert(form.password_length == CB_LOGIN_PASSWORD_MAX);
	assert(cb_login_form_insert(&form, 'x') == CB_APP_INVALID);
	assert(form.password_length == CB_LOGIN_PASSWORD_MAX);

	/* identifier field clamps at its capacity */
	cb_login_form_init(&form);
	for (i = 0; i < CB_LOGIN_IDENTIFIER_MAX; i++)
		assert(cb_login_form_insert(&form, 'y') == CB_APP_OK);
	assert(form.identifier_length == CB_LOGIN_IDENTIFIER_MAX);
	assert(cb_login_form_insert(&form, 'y') == CB_APP_INVALID);
	assert(form.identifier_length == CB_LOGIN_IDENTIFIER_MAX);

	/* field navigation wraps around both ends */
	cb_login_form_init(&form);
	assert(form.active_field == CB_LOGIN_IDENTIFIER);
	cb_login_form_next_field(&form, -1);
	assert(form.active_field == CB_LOGIN_SERVICE);
	cb_login_form_next_field(&form, -1);
	assert(form.active_field == CB_LOGIN_PASSWORD);
	cb_login_form_next_field(&form, -1);
	assert(form.active_field == CB_LOGIN_IDENTIFIER);
	cb_login_form_next_field(&form, 1);
	assert(form.active_field == CB_LOGIN_PASSWORD);
	cb_login_form_next_field(&form, 1);
	assert(form.active_field == CB_LOGIN_SERVICE);

	/* network-not-ready: error returned and the password is kept for retry */
	cb_login_form_init(&form);
	cb_auth_init(&auth);
	for (i = 0; identifier[i]; i++) cb_login_form_insert(&form, identifier[i]);
	cb_login_form_next_field(&form, 1);
	for (i = 0; password[i]; i++) cb_login_form_insert(&form, password[i]);
	context = (context_t){0};
	context.result = CB_APP_NETWORK;
	assert(cb_login_form_submit(&form, &auth, &backend, &context, path) ==
	       CB_APP_NETWORK);
	assert(form.password_length == strlen(password));
	assert(strcmp(form.password, password) == 0);

	/* a retry after the network recovers succeeds and clears the password */
	context = (context_t){0};
	assert(cb_login_form_submit(&form, &auth, &backend, &context, path) ==
	       CB_APP_OK);
	assert(form.password_length == 0);
	cb_auth_free(&auth);

	cb_session_clear(path);
	return 0;
}
