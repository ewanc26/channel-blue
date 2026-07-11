#ifndef CHANNEL_BLUE_AUTH_H
#define CHANNEL_BLUE_AUTH_H

#include "session_store.h"
#include "timeline.h"

typedef enum {
	CB_AUTH_SIGNED_OUT = 0,
	CB_AUTH_READY,
	CB_AUTH_ERROR
} cb_auth_state;

typedef struct {
	cb_auth_state state;
	cb_app_status last_status;
	cb_session_data session;
} cb_auth;

typedef struct {
	cb_app_status (*login)(void *context, const char *service,
	                       const char *identifier, const char *password,
	                       cb_session_data *out);
	cb_app_status (*resume)(void *context, const cb_session_data *saved,
	                        cb_session_data *out_refreshed);
	cb_app_status (*logout)(void *context);
} cb_auth_backend;

void cb_auth_init(cb_auth *auth);
void cb_auth_free(cb_auth *auth);

/* Resume a saved session and persist refreshed tokens. A failed refresh keeps
 * the saved session file so transient WiFi failure does not sign the user out. */
cb_app_status cb_auth_resume(cb_auth *auth, const cb_auth_backend *backend,
	                         void *context, const char *session_path);

/* Password is borrowed for this call and is never copied into cb_auth or the
 * session file. */
cb_app_status cb_auth_login(cb_auth *auth, const cb_auth_backend *backend,
	                        void *context, const char *session_path,
	                        const char *service, const char *identifier,
	                        const char *password);

/* Invalidates remotely first. Local credentials are cleared even when the
 * network call fails, matching an explicit user sign-out request. */
cb_app_status cb_auth_logout(cb_auth *auth, const cb_auth_backend *backend,
	                         void *context, const char *session_path);

#endif /* CHANNEL_BLUE_AUTH_H */
