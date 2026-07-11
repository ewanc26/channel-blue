#include "session_store.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CB_SESSION_MAGIC "channel-blue-session-v1"
#define CB_SESSION_MAX_FILE (32 * 1024)
#define CB_SESSION_MAX_VALUE (8 * 1024)

static char *cb_strdup(const char *value) {
	size_t length;
	char *copy;

	if (!value) return NULL;
	length = strlen(value);
	copy = malloc(length + 1);
	if (!copy) return NULL;
	memcpy(copy, value, length + 1);
	return copy;
}

static int cb_valid_value(const char *value) {
	return value && value[0] && strlen(value) <= CB_SESSION_MAX_VALUE &&
	       !strchr(value, '\n') && !strchr(value, '\r');
}

void cb_session_data_free(cb_session_data *data) {
	if (!data) return;
	free(data->service);
	free(data->access_jwt);
	free(data->refresh_jwt);
	free(data->handle);
	free(data->did);
	memset(data, 0, sizeof(*data));
}

static cb_session_status cb_copy_field(char **field, const char *value) {
	if (!cb_valid_value(value)) return CB_SESSION_INVALID;
	*field = cb_strdup(value);
	return *field ? CB_SESSION_OK : CB_SESSION_ALLOC;
}

cb_session_status cb_session_load(const char *path, cb_session_data *out) {
	FILE *file;
	char line[CB_SESSION_MAX_VALUE + 32];
	long size;
	cb_session_status status = CB_SESSION_INVALID;

	if (!path || !path[0] || !out) return CB_SESSION_INVALID;
	memset(out, 0, sizeof(*out));
	file = fopen(path, "rb");
	if (!file) return errno == ENOENT ? CB_SESSION_NOT_FOUND : CB_SESSION_IO;
	if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
	    size > CB_SESSION_MAX_FILE || fseek(file, 0, SEEK_SET) != 0) {
		status = CB_SESSION_IO;
		goto done;
	}
	if (!fgets(line, sizeof(line), file) || strcmp(line, CB_SESSION_MAGIC "\n") != 0)
		goto done;
	while (fgets(line, sizeof(line), file)) {
		char *separator = strchr(line, '=');
		char *value;
		char **field = NULL;
		if (!separator || !strchr(line, '\n')) goto done;
		*separator = '\0';
		value = separator + 1;
		value[strcspn(value, "\r\n")] = '\0';
		if (strcmp(line, "service") == 0) field = &out->service;
		else if (strcmp(line, "accessJwt") == 0) field = &out->access_jwt;
		else if (strcmp(line, "refreshJwt") == 0) field = &out->refresh_jwt;
		else if (strcmp(line, "handle") == 0) field = &out->handle;
		else if (strcmp(line, "did") == 0) field = &out->did;
		else goto done;
		if (*field || cb_copy_field(field, value) != CB_SESSION_OK) goto done;
	}
	if (ferror(file)) {
		status = CB_SESSION_IO;
		goto done;
	}
	if (!out->service || !out->access_jwt || !out->refresh_jwt ||
	    !out->handle || !out->did) goto done;
	status = CB_SESSION_OK;

done:
	fclose(file);
	if (status != CB_SESSION_OK) cb_session_data_free(out);
	return status;
}

cb_session_status cb_session_save(const char *path, const cb_session_data *data) {
	FILE *file;
	char *temporary;
	size_t path_length;
	int failed;

	if (!path || !path[0] || !data || !cb_valid_value(data->service) ||
	    !cb_valid_value(data->access_jwt) || !cb_valid_value(data->refresh_jwt) ||
	    !cb_valid_value(data->handle) || !cb_valid_value(data->did))
		return CB_SESSION_INVALID;
	path_length = strlen(path);
	temporary = malloc(path_length + 5);
	if (!temporary) return CB_SESSION_ALLOC;
	memcpy(temporary, path, path_length);
	memcpy(temporary + path_length, ".tmp", 5);
	file = fopen(temporary, "wb");
	if (!file) {
		free(temporary);
		return CB_SESSION_IO;
	}
	failed = fprintf(file, CB_SESSION_MAGIC "\nservice=%s\naccessJwt=%s\n"
	                 "refreshJwt=%s\nhandle=%s\ndid=%s\n", data->service,
	                 data->access_jwt, data->refresh_jwt, data->handle, data->did) < 0;
	if (fclose(file) != 0) failed = 1;
	if (!failed && rename(temporary, path) != 0) failed = 1;
	if (failed) remove(temporary);
	free(temporary);
	return failed ? CB_SESSION_IO : CB_SESSION_OK;
}

cb_session_status cb_session_clear(const char *path) {
	if (!path || !path[0]) return CB_SESSION_INVALID;
	if (remove(path) == 0 || errno == ENOENT) return CB_SESSION_OK;
	return CB_SESSION_IO;
}
