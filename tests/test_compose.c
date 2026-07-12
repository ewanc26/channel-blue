#include "app/compose.h"
#include "app/timeline.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int calls;
	int fail;
	int reply_seen;
	char reply_uri[128];
	char reply_root_uri[128];
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
	if (reply) {
		assert(reply->uri && reply->cid);
		strncpy(context->reply_uri, reply->uri, sizeof(context->reply_uri) - 1);
		context->reply_uri[sizeof(context->reply_uri) - 1] = '\0';
		strncpy(context->reply_root_uri, reply->root_uri,
		        sizeof(context->reply_root_uri) - 1);
		context->reply_root_uri[sizeof(context->reply_root_uri) - 1] = '\0';
	}
	assert(strcmp(text, "hello") == 0);
	return context->fail ? CB_APP_NETWORK : CB_APP_OK;
}

static cb_app_status fetch(void *opaque, const char *cursor, size_t limit,
	                       cb_timeline_page *out) {
	(void)opaque;
	(void)cursor;
	(void)limit;
	out->count = 2;
	out->posts = calloc(out->count, sizeof(*out->posts));
	assert(out->posts);
	out->posts[0].uri = copy("at://post/1");
	out->posts[0].cid = copy("cid-1");
	out->posts[0].root_uri = copy("at://thread/root");
	out->posts[0].root_cid = copy("root-cid");
	out->posts[0].author = copy("alice.test");
	out->posts[0].author_did = copy("did:plc:alice");
	out->posts[0].text = copy("hello");
	out->posts[1].uri = copy("at://post/2");
	out->posts[1].cid = copy("cid-2");
	out->posts[1].root_uri = copy("at://post/2");
	out->posts[1].root_cid = copy("cid-2");
	out->posts[1].author = copy("bob.test");
	out->posts[1].author_did = copy("did:plc:bob");
	out->posts[1].text = copy("another post");
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
	cb_compose_init(&compose);
	assert(cb_compose_begin(&compose, NULL) == CB_APP_OK);
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
	assert(cb_compose_begin(&compose, NULL) == CB_APP_OK);
	assert(cb_compose_submit(&compose, &timeline, &backend, &context) ==
	       CB_APP_INVALID);
	assert(context.calls == 2);

	/* The fixed buffer follows the lexicon's 3,000-byte maxLength. */
	assert(cb_compose_begin(&compose, NULL) == CB_APP_OK);
	for (i = 0; i < CB_POST_TEXT_BYTES_MAX; i++)
		assert(cb_compose_insert(&compose, 'a') == CB_APP_OK);
	assert(compose.length == CB_POST_TEXT_BYTES_MAX);
	assert(cb_compose_insert(&compose, 'a') == CB_APP_INVALID);
	assert(compose.length == CB_POST_TEXT_BYTES_MAX);

	/* Keyboard codepoints are encoded as UTF-8 and removed atomically. */
	assert(cb_compose_begin(&compose, NULL) == CB_APP_OK);
	assert(cb_compose_insert(&compose, 0x1f30d) == CB_APP_OK);
	assert(compose.length == 4);
	cb_compose_backspace(&compose);
	assert(compose.length == 0 && compose.text[0] == '\0');

	/* Three hundred four-byte codepoints fit without confusing bytes with the
	 * separate 300-grapheme protocol limit. */
	for (i = 0; i < CB_POST_TEXT_GRAPHEMES_MAX; i++)
		assert(cb_compose_insert(&compose, 0x1f30d) == CB_APP_OK);
	assert(compose.length == CB_POST_TEXT_GRAPHEMES_MAX * 4);

	/* reply mode targets the selected post */
	cb_timeline_free(&timeline);
	cb_timeline_init(&timeline);
	assert(cb_timeline_refresh(&timeline, &backend, &context) == CB_APP_OK);
	assert(cb_compose_begin(&compose, cb_timeline_selected(&timeline)) == CB_APP_OK);
	/* Move the feed after opening the composer. The captured target must not
	 * change with the controller's current selection. */
	cb_timeline_move(&timeline, 1);
	assert(strcmp(cb_timeline_selected(&timeline)->uri, "at://post/2") == 0);
	for (i = 0; text[i]; i++)
		assert(cb_compose_insert(&compose, text[i]) == CB_APP_OK);
	assert(cb_compose_submit(&compose, &timeline, &backend, &context) ==
	       CB_APP_OK);
	assert(context.reply_seen == 1);
	assert(strcmp(context.reply_uri, "at://post/1") == 0);
	assert(strcmp(context.reply_root_uri, "at://thread/root") == 0);

	cb_compose_free(&compose);
	cb_timeline_free(&timeline);
	return 0;
}
