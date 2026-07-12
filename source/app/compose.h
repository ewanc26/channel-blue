#ifndef CHANNEL_BLUE_COMPOSE_H
#define CHANNEL_BLUE_COMPOSE_H

#include "timeline.h"

typedef struct {
	char text[CB_POST_TEXT_BYTES_MAX + 1];
	size_t length;
	int replying;
	char *reply_uri;
	char *reply_cid;
	char *reply_root_uri;
	char *reply_root_cid;
	cb_app_status last_status;
} cb_compose;

/* Initialise once, then call cb_compose_begin for each new draft. The reply
 * URI/CID are copied so retries cannot accidentally follow a changed feed or
 * thread selection. */
void cb_compose_init(cb_compose *compose);
cb_app_status cb_compose_begin(cb_compose *compose, const cb_post *reply_to);
void cb_compose_free(cb_compose *compose);

/* The Wii font pipeline is currently ASCII-only, so composition accepts the
 * same printable range. UTF-8 input can be enabled with the font upgrade. */
cb_app_status cb_compose_insert(cb_compose *compose, unsigned int character);
void cb_compose_backspace(cb_compose *compose);

/* Submit through the timeline backend. Text and the owned reply target are
 * cleared only after success so flaky WiFi never destroys a draft. */
cb_app_status cb_compose_submit(cb_compose *compose, cb_timeline *timeline,
	                            const cb_timeline_backend *backend,
	                            void *context);

#endif /* CHANNEL_BLUE_COMPOSE_H */
