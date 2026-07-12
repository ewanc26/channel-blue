#include "app/thread.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int fetches;
	int likes;
	int reposts;
	int follows;
} fake_context;

static char *copy(const char *value) {
	char *result = malloc(strlen(value) + 1);
	assert(result);
	strcpy(result, value);
	return result;
}

/* A thread page shaped like a flattened conversation: an ancestor, the focused
 * post, then two replies. */
static cb_app_status fetch_thread(void *opaque, const char *uri, size_t limit,
	                          cb_timeline_page *out) {
	fake_context *context = opaque;
	(void)limit;
	(void)uri;
	context->fetches++;
	out->count = 3;
	out->posts = calloc(out->count, sizeof(*out->posts));
	assert(out->posts);
	out->posts[0].uri = copy("at://did:plc:alice/app.bsky.feed.post/ancestor");
	out->posts[0].cid = copy("cid-a");
	out->posts[0].author = copy("alice.test");
	out->posts[0].text = copy("ancestor post");
	out->posts[1].uri = copy("at://did:plc:bob/app.bsky.feed.post/root");
	out->posts[1].cid = copy("cid-r");
	out->posts[1].author = copy("bob.test");
	out->posts[1].text = copy("the focused post");
	out->posts[2].uri = copy("at://did:plc:carol/app.bsky.feed.post/reply");
	out->posts[2].cid = copy("cid-c");
	out->posts[2].author = copy("carol.test");
	out->posts[2].text = copy("a reply");
	return CB_APP_OK;
}

static cb_app_status like(void *opaque, const cb_post *post) {
	assert(post->cid);
	((fake_context *)opaque)->likes++;
	return CB_APP_OK;
}

static cb_app_status repost(void *opaque, const cb_post *post) {
	assert(post->uri);
	((fake_context *)opaque)->reposts++;
	return CB_APP_OK;
}

static cb_app_status follow(void *opaque, const char *actor) {
	assert(strcmp(actor, "bob.test") == 0);
	((fake_context *)opaque)->follows++;
	return CB_APP_OK;
}

int main(void) {
	fake_context context = {0};
	cb_thread thread;
	cb_thread_backend backend = {fetch_thread, like, repost, follow};

	cb_thread_init(&thread);
	assert(cb_thread_load(&thread, &backend, &context,
	                      "at://did:plc:bob/app.bsky.feed.post/root") ==
	       CB_APP_OK);
	assert(thread.count == 3 && thread.loaded);
	assert(strcmp(thread.root_uri, "at://did:plc:bob/app.bsky.feed.post/root") == 0);
	assert(strcmp(cb_thread_selected(&thread)->author, "alice.test") == 0);
	assert(context.fetches == 1);

	cb_thread_move(&thread, 1);
	assert(strcmp(cb_thread_selected(&thread)->author, "bob.test") == 0);
	cb_thread_move(&thread, 1);
	assert(strcmp(cb_thread_selected(&thread)->author, "carol.test") == 0);
	cb_thread_move(&thread, 100);
	assert(thread.selected == 2);
	cb_thread_move(&thread, -100);
	assert(thread.selected == 0);

	assert(cb_thread_like_selected(&thread, &backend, &context) == CB_APP_OK);
	assert(cb_thread_selected(&thread)->liked == 1 && context.likes == 1);
	assert(cb_thread_repost_selected(&thread, &backend, &context) == CB_APP_OK);
	assert(context.reposts == 1);
	cb_thread_move(&thread, 1);
	assert(cb_thread_follow_selected(&thread, &backend, &context) == CB_APP_OK);
	assert(context.follows == 1);

	/* Reloading resets the selection and replaces the contents. */
	assert(cb_thread_load(&thread, &backend, &context,
	                      "at://did:plc:bob/app.bsky.feed.post/root") ==
	       CB_APP_OK);
	assert(thread.selected == 0 && context.fetches == 2);

	cb_thread_free(&thread);
	return 0;
}
