#include "compose.h"
#include "utf8.h"

#include <stdlib.h>
#include <string.h>

static char *copy(const char *value) {
	size_t length;
	char *result;
	if (!value) return NULL;
	length = strlen(value);
	result = malloc(length + 1);
	if (result) memcpy(result, value, length + 1);
	return result;
}

void cb_compose_init(cb_compose *compose) {
	if (!compose) return;
	memset(compose, 0, sizeof(*compose));
}

void cb_compose_free(cb_compose *compose) {
	if (!compose) return;
	free(compose->reply_uri);
	free(compose->reply_cid);
	free(compose->reply_root_uri);
	free(compose->reply_root_cid);
	memset(compose, 0, sizeof(*compose));
}

cb_app_status cb_compose_begin(cb_compose *compose, const cb_post *reply_to) {
	char *uri = NULL;
	char *cid = NULL;
	char *root_uri = NULL;
	char *root_cid = NULL;
	if (!compose) return CB_APP_INVALID;
	if (reply_to) {
		if (!reply_to->uri || !reply_to->uri[0] ||
		    !reply_to->cid || !reply_to->cid[0] ||
		    !reply_to->root_uri || !reply_to->root_uri[0] ||
		    !reply_to->root_cid || !reply_to->root_cid[0]) return CB_APP_INVALID;
		uri = copy(reply_to->uri);
		cid = copy(reply_to->cid);
		root_uri = copy(reply_to->root_uri);
		root_cid = copy(reply_to->root_cid);
		if (!uri || !cid || !root_uri || !root_cid) {
			free(uri);
			free(cid);
			free(root_uri);
			free(root_cid);
			return CB_APP_ALLOC;
		}
	}
	cb_compose_free(compose);
	compose->reply_uri = uri;
	compose->reply_cid = cid;
	compose->reply_root_uri = root_uri;
	compose->reply_root_cid = root_cid;
	compose->replying = reply_to != NULL;
	return CB_APP_OK;
}

cb_app_status cb_compose_insert(cb_compose *compose, unsigned int character) {
	if (!compose) return CB_APP_INVALID;
	return cb_utf8_append(compose->text, &compose->length, CB_POST_TEXT_BYTES_MAX,
	                      character) ? CB_APP_OK : CB_APP_INVALID;
}

void cb_compose_backspace(cb_compose *compose) {
	if (!compose || !compose->length) return;
	cb_utf8_backspace(compose->text, &compose->length);
}

cb_app_status cb_compose_submit(cb_compose *compose, cb_timeline *timeline,
	                            const cb_timeline_backend *backend,
	                            void *context) {
	cb_post reply = {0};
	const cb_post *reply_ptr = NULL;
	if (!compose || !timeline || !backend || !backend->create_post ||
	    !compose->length) return CB_APP_INVALID;
	if (compose->replying) {
		if (!compose->reply_uri || !compose->reply_cid ||
		    !compose->reply_root_uri || !compose->reply_root_cid)
			return CB_APP_INVALID;
		reply.uri = compose->reply_uri;
		reply.cid = compose->reply_cid;
		reply.root_uri = compose->reply_root_uri;
		reply.root_cid = compose->reply_root_cid;
		reply_ptr = &reply;
	}
	compose->last_status = backend->create_post(context, compose->text, reply_ptr);
	timeline->last_status = compose->last_status;
	if (compose->last_status == CB_APP_OK) {
		cb_compose_free(compose);
	}
	return compose->last_status;
}
