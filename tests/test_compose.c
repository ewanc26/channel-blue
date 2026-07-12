#include "app/compose.h"
#include "app/timeline.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int calls;
	int fail;
	int reply_seen;
} context_t;

static char *copy(const char *value) {
	char *result = malloc(strlen(value) + 1);
	assert(result);
	strcpy(result, value);
	return result;
}

static cb_app_status create(void *opaque, const char *text, const cb_post *reply) {
	context_t *context = opaque;
	context->calls++;
	context->reply_seen = reply != NULL;
	assert(strcmp(text, "hello") == 0);
	return context->fail ? CB_APP_NETWORK : CB_APP_OK;
}

static cb_app_status fetch(void *opaque, const char *cursor, size_t limit,
	                       cb_timeline_page *out) {
	(void)opaque;
	(void)cursor;
	(void)limit;
	out->count = 1;
	out->posts = calloc(out->count, sizeof(*out->posts));
	assert(out->posts);
	out->posts[0].uri = copy("at://post/1");
	out->posts[0].cid = copy("cid-1");
	out->posts[0].author = copy("alice.test");
	out->posts[0].text = copy("hello");
	return CB_APP_OK;
}

int main(void) {
	cb_compose compose;
	cb_timeline timeline;
	cb_timeline_backend backend = {0};
	context_t context = {0};
	const char *text = "hello";
	size_t i;

	backend.fetch_timeline = fetch;
	backend.create_post = create;
	cb_timeline_init(&timeline);

	/* top-level draft: append, fix a typo, fail, retry, then succeed */
	cb_compose_init(&compose, 0);
	for (i = 0; text[i]; i++)
		assert(cb_compose_insert(&compose, text[i]) == CB_APP_OK);
	cb_compose_backspace(&compose);
	assert(cb_compose_insert(&compose, 'o') == CB_APP_OK);
	context.fail = 1;
	assert(cb_compose_submit(&compose, &timeline, &backend, &context) ==
	       CB_APP_NETWORK);
	/* draft is retained across the failure */
	assert(strcmp(compose.text, "hello") == 0);
	context.fail = 0;
	assert(cb_compose_submit(&compose, &timeline, &backend, &context) ==
	       CB_APP_OK);
	assert(compose.length == 0 && compose.text[0] == '\0' && context.calls == 2);
	assert(context.reply_seen == 0);

	/* empty text is rejected without calling the backend */
	cb_compose_init(&compose, 0);
	assert(cb_compose_submit(&compose, &timeline, &backend, &context) ==
	       CB_APP_INVALID);
	assert(context.calls == 2);

	/* over-long text clamps at CB_POST_TEXT_MAX */
	cb_compose_init(&compose, 0);
	for (i = 0; i < CB_POST_TEXT_MAX; i++)
		assert(cb_compose_insert(&compose, 'a') == CB_APP_OK);
	assert(compose.length == CB_POST_TEXT_MAX);
	assert(cb_compose_insert(&compose, 'a') == CB_APP_INVALID);
	assert(compose.length == CB_POST_TEXT_MAX);

	/* reply mode targets the selected post */
	cb_timeline_free(&timeline);
	cb_timeline_init(&timeline);
	assert(cb_timeline_refresh(&timeline, &backend, &context) == CB_APP_OK);
	cb_compose_init(&compose, 1);
	for (i = 0; text[i]; i++)
		assert(cb_compose_insert(&compose, text[i]) == CB_APP_OK);
	assert(cb_compose_submit(&compose, &timeline, &backend, &context) ==
	       CB_APP_OK);
	assert(context.reply_seen == 1);

	cb_timeline_free(&timeline);
	return 0;
}
