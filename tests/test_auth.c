#include "app/auth.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int logins;
	int resumes;
	int logouts;
	cb_app_status login_result;
	cb_app_status resume_result;
	cb_app_status logout_result;
} context_t;

static char *copy(const char *value) {
	char *result = malloc(strlen(value) + 1);
	assert(result);
	strcpy(result, value);
	return result;
}

static void make_session(cb_session_data *out, const char *access) {
	out->service = copy("https://bsky.social");
	out->access_jwt = copy(access);
	out->refresh_jwt = copy("refresh");
	out->handle = copy("alice.test");
	out->did = copy("did:plc:alice");
}

static cb_app_status login(void *opaque, const char *service,
	                       const char *identifier, const char *password,
	                       cb_session_data *out) {
	context_t *context = opaque;
	assert(strcmp(service, "https://bsky.social") == 0);
	assert(strcmp(identifier, "alice.test") == 0);
	assert(strcmp(password, "app-password") == 0);
	context->logins++;
	if (context->login_result != CB_APP_OK) return context->login_result;
	make_session(out, "access-1");
	return CB_APP_OK;
}

static cb_app_status resume(void *opaque, const cb_session_data *saved,
	                        cb_session_data *out) {
	context_t *context = opaque;
	assert(strcmp(saved->access_jwt, "access-1") == 0);
	context->resumes++;
	if (context->resume_result != CB_APP_OK) return context->resume_result;
	make_session(out, "access-2");
	return CB_APP_OK;
}

static cb_app_status logout(void *opaque) {
	context_t *context = opaque;
	context->logouts++;
	return context->logout_result;
}

int main(void) {
	const char *path = "/tmp/channel-blue-auth-test";
	cb_auth auth;
	cb_auth restored;
	context_t context = {0};
	cb_auth_backend backend = {login, resume, logout};

	cb_session_clear(path);

	/* happy path: login, resume (refresh), explicit logout */
	cb_auth_init(&auth);
	assert(cb_auth_login(&auth, &backend, &context, path,
	                     "https://bsky.social", "alice.test", "app-password") ==
	       CB_APP_OK);
	assert(auth.state == CB_AUTH_READY && context.logins == 1);
	cb_auth_init(&restored);
	assert(cb_auth_resume(&restored, &backend, &context, path) == CB_APP_OK);
	assert(strcmp(restored.session.access_jwt, "access-2") == 0);
	assert(context.resumes == 1);
	assert(cb_auth_logout(&restored, &backend, &context, path) == CB_APP_OK);
	assert(restored.state == CB_AUTH_SIGNED_OUT && context.logouts == 1);
	assert(cb_session_load(path, &restored.session) == CB_SESSION_NOT_FOUND);
	cb_auth_free(&restored);
	cb_auth_free(&auth);

	/* invalid/empty credentials never reach the backend */
	cb_session_clear(path);
	cb_auth_init(&auth);
	context = (context_t){0};
	assert(cb_auth_login(&auth, &backend, &context, path,
	                     "", "alice.test", "app-password") == CB_APP_INVALID);
	assert(cb_auth_login(&auth, &backend, &context, path,
	                     "https://bsky.social", "", "app-password") ==
	       CB_APP_INVALID);
	assert(cb_auth_login(&auth, &backend, &context, path,
	                     "https://bsky.social", "alice.test", "") ==
	       CB_APP_INVALID);
	assert(context.logins == 0);
	assert(auth.state == CB_AUTH_SIGNED_OUT);
	cb_auth_free(&auth);

	/* network-not-ready: login returns CB_APP_NETWORK and persists nothing */
	cb_session_clear(path);
	cb_auth_init(&auth);
	context = (context_t){0};
	context.login_result = CB_APP_NETWORK;
	assert(cb_auth_login(&auth, &backend, &context, path,
	                     "https://bsky.social", "alice.test", "app-password") ==
	       CB_APP_NETWORK);
	assert(auth.state == CB_AUTH_ERROR);
	assert(context.logins == 1);
	{
		cb_session_data leftover;
		memset(&leftover, 0, sizeof(leftover));
		assert(cb_session_load(path, &leftover) == CB_SESSION_NOT_FOUND);
	}
	cb_auth_free(&auth);

	/* missing session on resume is reported, not treated as an error state */
	cb_session_clear(path);
	cb_auth_init(&restored);
	context = (context_t){0};
	assert(cb_auth_resume(&restored, &backend, &context, path) ==
	       CB_APP_INVALID);
	assert(restored.state == CB_AUTH_SIGNED_OUT);
	assert(context.resumes == 0);
	cb_auth_free(&restored);

	/* repeated login overwrites the persisted session cleanly */
	cb_session_clear(path);
	cb_auth_init(&auth);
	context = (context_t){0};
	assert(cb_auth_login(&auth, &backend, &context, path,
	                     "https://bsky.social", "alice.test", "app-password") ==
	       CB_APP_OK);
	cb_auth_init(&restored);
	assert(cb_auth_login(&restored, &backend, &context, path,
	                     "https://bsky.social", "alice.test", "app-password") ==
	       CB_APP_OK);
	assert(strcmp(restored.session.access_jwt, "access-1") == 0);
	assert(context.logins == 2);
	cb_auth_free(&restored);
	cb_auth_free(&auth);

	/* session expiry: a failed refresh keeps the saved session for retry */
	cb_session_clear(path);
	cb_auth_init(&auth);
	context = (context_t){0};
	assert(cb_auth_login(&auth, &backend, &context, path,
	                     "https://bsky.social", "alice.test", "app-password") ==
	       CB_APP_OK);
	cb_auth_free(&auth);
	cb_auth_init(&restored);
	context = (context_t){0};
	context.resume_result = CB_APP_NETWORK;
	assert(cb_auth_resume(&restored, &backend, &context, path) ==
	       CB_APP_NETWORK);
	assert(restored.state == CB_AUTH_ERROR);
	assert(context.resumes == 1);
	{
		cb_session_data persisted;
		memset(&persisted, 0, sizeof(persisted));
		assert(cb_session_load(path, &persisted) == CB_SESSION_OK);
		assert(strcmp(persisted.access_jwt, "access-1") == 0);
		cb_session_data_free(&persisted);
	}
	cb_auth_free(&restored);

	cb_session_clear(path);
	return 0;
}
