#include "compose.h"

#include <string.h>

void cb_compose_init(cb_compose *compose, int replying) {
	if (!compose) return;
	memset(compose, 0, sizeof(*compose));
	compose->replying = replying != 0;
}

cb_app_status cb_compose_insert(cb_compose *compose, unsigned int character) {
	if (!compose || character < 32 || character > 126) return CB_APP_INVALID;
	if (compose->length >= CB_POST_TEXT_MAX) return CB_APP_INVALID;
	compose->text[compose->length++] = (char)character;
	compose->text[compose->length] = '\0';
	return CB_APP_OK;
}

void cb_compose_backspace(cb_compose *compose) {
	if (!compose || !compose->length) return;
	compose->text[--compose->length] = '\0';
}

cb_app_status cb_compose_submit(cb_compose *compose, cb_timeline *timeline,
	                            const cb_timeline_backend *backend,
	                            void *context) {
	if (!compose || !compose->length) return CB_APP_INVALID;
	compose->last_status = cb_timeline_create_post(timeline, backend, context,
	                                              compose->text,
	                                              compose->replying);
	if (compose->last_status == CB_APP_OK) {
		compose->length = 0;
		compose->text[0] = '\0';
	}
	return compose->last_status;
}
