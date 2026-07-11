#include "app/compose.h"

#include <assert.h>
#include <string.h>

typedef struct { int calls; int fail; } context_t;

static cb_app_status create(void *opaque, const char *text, const cb_post *reply) {
	context_t *context = opaque;
	context->calls++;
	assert(strcmp(text, "hello") == 0);
	assert(reply == NULL);
	return context->fail ? CB_APP_NETWORK : CB_APP_OK;
}

int main(void) {
	cb_compose compose;
	cb_timeline timeline;
	cb_timeline_backend backend = {0};
	context_t context = {0};
	const char *text = "hellp";
	size_t i;

	backend.create_post = create;
	cb_timeline_init(&timeline);
	cb_compose_init(&compose, 0);
	for (i = 0; text[i]; i++) assert(cb_compose_insert(&compose, text[i]) == CB_APP_OK);
	cb_compose_backspace(&compose);
	assert(cb_compose_insert(&compose, 'o') == CB_APP_OK);
	context.fail = 1;
	assert(cb_compose_submit(&compose, &timeline, &backend, &context) == CB_APP_NETWORK);
	assert(strcmp(compose.text, "hello") == 0);
	context.fail = 0;
	assert(cb_compose_submit(&compose, &timeline, &backend, &context) == CB_APP_OK);
	assert(compose.length == 0 && compose.text[0] == '\0' && context.calls == 2);
	cb_timeline_free(&timeline);
	return 0;
}
