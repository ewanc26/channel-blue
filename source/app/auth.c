#include "auth.h"

#include <string.h>

void cb_auth_init(cb_auth *auth) {
	if (auth) memset(auth, 0, sizeof(*auth));
}

void cb_auth_free(cb_auth *auth) {
	if (!auth) return;
	cb_session_data_free(&auth->session);
	memset(auth, 0, sizeof(*auth));
}

static cb_app_status cb_store_status(cb_session_status status) {
	switch (status) {
	case CB_SESSION_OK: return CB_APP_OK;
	case CB_SESSION_ALLOC: return CB_APP_ALLOC;
	case CB_SESSION_NOT_FOUND: return CB_APP_INVALID;
	default: return CB_APP_NETWORK;
	}
}

static cb_app_status cb_auth_accept(cb_auth *auth, cb_session_data *session,
	                                const char *path) {
	cb_app_status status;
	if (cb_session_save(path, session) != CB_SESSION_OK) {
		cb_session_data_free(session);
		auth->state = CB_AUTH_ERROR;
		return CB_APP_NETWORK;
	}
	cb_session_data_free(&auth->session);
	auth->session = *session;
	memset(session, 0, sizeof(*session));
	auth->state = CB_AUTH_READY;
	status = CB_APP_OK;
	auth->last_status = status;
	return status;
}

cb_app_status cb_auth_resume(cb_auth *auth, const cb_auth_backend *backend,
	                         void *context, const char *session_path) {
	cb_session_data saved = {0};
	cb_session_data refreshed = {0};
	cb_session_status loaded;
	if (!auth || !backend || !backend->resume || !session_path)
		return CB_APP_INVALID;
	loaded = cb_session_load(session_path, &saved);
	if (loaded != CB_SESSION_OK) {
		auth->last_status = cb_store_status(loaded);
		auth->state = loaded == CB_SESSION_NOT_FOUND ? CB_AUTH_SIGNED_OUT : CB_AUTH_ERROR;
		return auth->last_status;
	}
	auth->last_status = backend->resume(context, &saved, &refreshed);
	cb_session_data_free(&saved);
	if (auth->last_status != CB_APP_OK) {
		cb_session_data_free(&refreshed);
		auth->state = CB_AUTH_ERROR;
		return auth->last_status;
	}
	return cb_auth_accept(auth, &refreshed, session_path);
}

cb_app_status cb_auth_login(cb_auth *auth, const cb_auth_backend *backend,
	                        void *context, const char *session_path,
	                        const char *service, const char *identifier,
	                        const char *password) {
	cb_session_data session = {0};
	if (!auth || !backend || !backend->login || !session_path || !service ||
	    !service[0] || !identifier || !identifier[0] || !password || !password[0])
		return CB_APP_INVALID;
	auth->last_status = backend->login(context, service, identifier, password,
	                                  &session);
	if (auth->last_status != CB_APP_OK) {
		cb_session_data_free(&session);
		auth->state = CB_AUTH_ERROR;
		return auth->last_status;
	}
	return cb_auth_accept(auth, &session, session_path);
}

cb_app_status cb_auth_logout(cb_auth *auth, const cb_auth_backend *backend,
	                         void *context, const char *session_path) {
	cb_app_status remote = CB_APP_OK;
	cb_session_status local;
	if (!auth || !backend || !session_path) return CB_APP_INVALID;
	if (backend->logout) remote = backend->logout(context);
	local = cb_session_clear(session_path);
	cb_session_data_free(&auth->session);
	auth->state = CB_AUTH_SIGNED_OUT;
	auth->last_status = local == CB_SESSION_OK ? remote : cb_store_status(local);
	return auth->last_status;
}
