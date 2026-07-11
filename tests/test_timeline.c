#include "app/timeline.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int fetches;
	int posts;
	int likes;
	int reposts;
	int follows;
	int replied;
} fake_context;

static char *copy(const char *value) {
	char *result = malloc(strlen(value) + 1);
	assert(result);
	strcpy(result, value);
	return result;
}

static cb_app_status fetch(void *opaque, const char *cursor, size_t limit,
	                       cb_timeline_page *out) {
	fake_context *context = opaque;
	(void)limit;
	context->fetches++;
	out->count = 2;
	out->posts = calloc(out->count, sizeof(*out->posts));
	assert(out->posts);
	out->posts[0].uri = copy(cursor ? "at://post/3" : "at://post/1");
	out->posts[0].cid = copy("cid-1");
	out->posts[0].author = copy("alice.test");
	out->posts[0].text = copy("hello from Wii");
	out->posts[1].uri = copy(cursor ? "at://post/4" : "at://post/2");
	out->posts[1].cid = copy("cid-2");
	out->posts[1].author = copy("bob.test");
	out->posts[1].text = copy("second post");
	if (!cursor) out->cursor = copy("next");
	return CB_APP_OK;
}

static cb_app_status create(void *opaque, const char *text, const cb_post *reply) {
	fake_context *context = opaque;
	assert(strcmp(text, "reply") == 0);
	context->posts++;
	context->replied = reply != NULL;
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
	cb_timeline timeline;
	cb_timeline_backend backend = {fetch, create, like, repost, follow};

	cb_timeline_init(&timeline);
	assert(cb_timeline_refresh(&timeline, &backend, &context) == CB_APP_OK);
	assert(timeline.count == 2 && timeline.has_more);
	cb_timeline_move(&timeline, 1);
	assert(strcmp(cb_timeline_selected(&timeline)->author, "bob.test") == 0);
	assert(cb_timeline_like_selected(&timeline, &backend, &context) == CB_APP_OK);
	assert(cb_timeline_selected(&timeline)->liked == 1);
	assert(cb_timeline_repost_selected(&timeline, &backend, &context) == CB_APP_OK);
	assert(cb_timeline_follow_selected(&timeline, &backend, &context) == CB_APP_OK);
	assert(cb_timeline_create_post(&timeline, &backend, &context, "reply", 1) ==
	       CB_APP_OK);
	assert(context.likes == 1 && context.reposts == 1 && context.follows == 1);
	assert(context.posts == 1 && context.replied);
	assert(cb_timeline_load_more(&timeline, &backend, &context) == CB_APP_OK);
	assert(timeline.count == 4 && !timeline.has_more && context.fetches == 2);
	cb_timeline_free(&timeline);
	return 0;
}
