#ifndef CHANNEL_BLUE_SESSION_STORE_H
#define CHANNEL_BLUE_SESSION_STORE_H

#include <stddef.h>

/* Persistent subset of wf_session_data. Every string is heap-owned. */
typedef struct {
	char *service;
	char *access_jwt;
	char *refresh_jwt;
	char *handle;
	char *did;
} cb_session_data;

typedef enum {
	CB_SESSION_OK = 0,
	CB_SESSION_NOT_FOUND,
	CB_SESSION_INVALID,
	CB_SESSION_IO,
	CB_SESSION_ALLOC
} cb_session_status;

/* Load a session file. On success, free all fields with cb_session_data_free. */
cb_session_status cb_session_load(const char *path, cb_session_data *out);

/* Write through <path>.tmp and rename so a power loss cannot leave half a file. */
cb_session_status cb_session_save(const char *path, const cb_session_data *data);

/* Remove a persisted session. A missing file is treated as success. */
cb_session_status cb_session_clear(const char *path);

void cb_session_data_free(cb_session_data *data);

#endif /* CHANNEL_BLUE_SESSION_STORE_H */
