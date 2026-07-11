#ifndef CHANNEL_BLUE_COMPOSE_H
#define CHANNEL_BLUE_COMPOSE_H

#include "timeline.h"

typedef struct {
	char text[CB_POST_TEXT_MAX + 1];
	size_t length;
	int replying;
	cb_app_status last_status;
} cb_compose;

void cb_compose_init(cb_compose *compose, int replying);

/* The Wii font pipeline is currently ASCII-only, so composition accepts the
 * same printable range. UTF-8 input can be enabled with the font upgrade. */
cb_app_status cb_compose_insert(cb_compose *compose, unsigned int character);
void cb_compose_backspace(cb_compose *compose);

/* Submit through the timeline backend. Text is cleared only after success so
 * flaky WiFi never destroys a draft. */
cb_app_status cb_compose_submit(cb_compose *compose, cb_timeline *timeline,
	                            const cb_timeline_backend *backend,
	                            void *context);

#endif /* CHANNEL_BLUE_COMPOSE_H */
