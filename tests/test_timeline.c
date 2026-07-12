#include "app/timeline.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int fetches;
	int posts;
	int likes;
	int reposts;
	int follows;
	int replied;
	int big;
	int full;
	int empty;
	int malformed;
	int action_fail;
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
	size_t n = context->empty ? 0
	          : (context->big ? (CB_TIMELINE_CAPACITY + 16)
	             : (context->full ? CB_TIMELINE_CAPACITY : 2));
	size_t i;
	(void)limit;
	context->fetches++;
	out->count = n;
	if (n == 0) {
		out->posts = NULL;
		return CB_APP_OK;
	}
	out->posts = calloc(n, sizeof(*out->posts));
	assert(out->posts);
	for (i = 0; i < n; i++) {
		if (context->big || context->full) {
			char uri[64];
			snprintf(uri, sizeof(uri), "at://post/%zu",
			         cursor ? 1000 + i : i);
			out->posts[i].uri = copy(uri);
			out->posts[i].cid = copy("cid");
			out->posts[i].root_uri = copy(uri);
			out->posts[i].root_cid = copy("cid");
			out->posts[i].author = copy("author.test");
			out->posts[i].author_did = copy("did:plc:author");
			out->posts[i].text = copy("post text");
			continue;
		}
		if (i == 0)
			out->posts[i].uri = copy(cursor ? "at://post/3"
			                               : "at://post/1");
		else
			out->posts[i].uri = copy(cursor ? "at://post/4"
			                               : "at://post/2");
		out->posts[i].cid = copy(i == 0 ? "cid-1" : "cid-2");
		out->posts[i].root_uri = copy(out->posts[i].uri);
		out->posts[i].root_cid = copy(i == 0 ? "cid-1" : "cid-2");
		out->posts[i].author = copy(i == 0 ? "alice.test" : "bob.test");
		out->posts[i].author_did = copy(i == 0 ? "did:plc:alice"
		                                             : "did:plc:bob");
		out->posts[i].text = copy(i == 0 ? "hello from Wii"
		                                 : "second post");
	}
	if (context->malformed) {
		free(out->posts[0].uri);
		out->posts[0].uri = NULL;
	}
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

static cb_app_status like(void *opaque, cb_post *post) {
	assert(post->cid);
	((fake_context *)opaque)->likes++;
	if (((fake_context *)opaque)->action_fail) return CB_APP_NETWORK;
	if (post->liked) {
		free(post->like_uri);
		post->like_uri = NULL;
		post->liked = 0;
	} else {
		post->like_uri = copy("at://did:plc:me/app.bsky.feed.like/one");
		post->liked = 1;
	}
	return CB_APP_OK;
}

static cb_app_status repost(void *opaque, cb_post *post) {
	assert(post->uri);
	((fake_context *)opaque)->reposts++;
	if (post->reposted) {
		free(post->repost_uri);
		post->repost_uri = NULL;
		post->reposted = 0;
	} else {
		post->repost_uri = copy("at://did:plc:me/app.bsky.feed.repost/one");
		post->reposted = 1;
	}
	return CB_APP_OK;
}

static cb_app_status follow(void *opaque, const char *actor_did) {
	assert(strcmp(actor_did, "did:plc:bob") == 0);
	((fake_context *)opaque)->follows++;
	return CB_APP_OK;
}

int main(void) {
	fake_context context = {0};
	cb_timeline timeline;
	cb_timeline_backend backend = {fetch, create, like, repost, follow};

	/* ---- happy path (existing coverage) ---- */
	cb_timeline_init(&timeline);
	assert(cb_timeline_refresh(&timeline, &backend, &context) == CB_APP_OK);
	assert(timeline.count == 2 && timeline.has_more);
	cb_timeline_move(&timeline, 1);
	assert(strcmp(cb_timeline_selected(&timeline)->author, "bob.test") == 0);
	assert(cb_timeline_like_selected(&timeline, &backend, &context) == CB_APP_OK);
	assert(cb_timeline_selected(&timeline)->liked == 1);
	assert(cb_timeline_selected(&timeline)->like_uri != NULL);
	assert(cb_timeline_like_selected(&timeline, &backend, &context) == CB_APP_OK);
	assert(cb_timeline_selected(&timeline)->liked == 0);
	assert(cb_timeline_selected(&timeline)->like_uri == NULL);
	context.action_fail = 1;
	assert(cb_timeline_like_selected(&timeline, &backend, &context) ==
	       CB_APP_NETWORK);
	assert(!cb_timeline_selected(&timeline)->liked &&
	       cb_timeline_selected(&timeline)->like_count == 0);
	context.action_fail = 0;
	assert(cb_timeline_repost_selected(&timeline, &backend, &context) ==
	       CB_APP_OK);
	assert(cb_timeline_selected(&timeline)->reposted == 1);
	assert(cb_timeline_repost_selected(&timeline, &backend, &context) ==
	       CB_APP_OK);
	assert(cb_timeline_selected(&timeline)->reposted == 0);
	assert(cb_timeline_follow_selected(&timeline, &backend, &context) ==
	       CB_APP_OK);
	assert(cb_timeline_create_post(&timeline, &backend, &context, "reply", 1) ==
	       CB_APP_OK);
	assert(context.likes == 3 && context.reposts == 2 && context.follows == 1);
	assert(context.posts == 1 && context.replied);
	assert(cb_timeline_load_more(&timeline, &backend, &context) == CB_APP_OK);
	assert(timeline.count == 4 && !timeline.has_more && context.fetches == 2);
	/* no further pages once the cursor is exhausted */
	assert(cb_timeline_load_more(&timeline, &backend, &context) ==
	       CB_APP_INVALID);

	/* a single page larger than the budget is rejected as malformed */
	cb_timeline_free(&timeline);
	cb_timeline_init(&timeline);
	context = (fake_context){0};
	context.big = 1;
	assert(cb_timeline_refresh(&timeline, &backend, &context) == CB_APP_INVALID);
	assert(timeline.count == 0);

	/* appending clamps to the fixed budget across pages */
	cb_timeline_free(&timeline);
	cb_timeline_init(&timeline);
	context = (fake_context){0};
	assert(cb_timeline_refresh(&timeline, &backend, &context) == CB_APP_OK);
	assert(timeline.count == 2 && timeline.has_more);
	context.full = 1;
	assert(cb_timeline_load_more(&timeline, &backend, &context) == CB_APP_OK);
	assert(timeline.count == CB_TIMELINE_CAPACITY);
	assert(!timeline.has_more);

	/* ---- cursor is persisted and used to page forward ---- */
	cb_timeline_free(&timeline);
	cb_timeline_init(&timeline);
	context = (fake_context){0};
	assert(cb_timeline_refresh(&timeline, &backend, &context) == CB_APP_OK);
	assert(timeline.cursor && strcmp(timeline.cursor, "next") == 0);
	assert(context.fetches == 1);
	assert(cb_timeline_load_more(&timeline, &backend, &context) == CB_APP_OK);
	assert(context.fetches == 2);

	/* ---- empty feed ---- */
	cb_timeline_free(&timeline);
	cb_timeline_init(&timeline);
	context = (fake_context){0};
	context.empty = 1;
	assert(cb_timeline_refresh(&timeline, &backend, &context) == CB_APP_OK);
	assert(timeline.count == 0);
	assert(cb_timeline_selected(&timeline) == NULL);

	/* ---- malformed post (missing uri) is rejected ---- */
	cb_timeline_free(&timeline);
	cb_timeline_init(&timeline);
	context = (fake_context){0};
	context.malformed = 1;
	assert(cb_timeline_refresh(&timeline, &backend, &context) == CB_APP_INVALID);
	assert(timeline.count == 0);

	/* ---- create_post validation ---- */
	cb_timeline_free(&timeline);
	cb_timeline_init(&timeline);
	context = (fake_context){0};
	/* empty text never reaches the backend */
	assert(cb_timeline_create_post(&timeline, &backend, &context, "", 0) ==
	       CB_APP_INVALID);
	assert(context.posts == 0);
	/* reply requested but nothing is selected */
	assert(cb_timeline_create_post(&timeline, &backend, &context, "hi", 1) ==
	       CB_APP_INVALID);
	assert(context.posts == 0);

	/* ---- social actions require a selection ---- */
	assert(cb_timeline_like_selected(&timeline, &backend, &context) ==
	       CB_APP_INVALID);
	assert(cb_timeline_repost_selected(&timeline, &backend, &context) ==
	       CB_APP_INVALID);
	assert(cb_timeline_follow_selected(&timeline, &backend, &context) ==
	       CB_APP_INVALID);

	/* ---- movement clamps at both ends ---- */
	cb_timeline_init(&timeline);
	context = (fake_context){0};
	assert(cb_timeline_refresh(&timeline, &backend, &context) == CB_APP_OK);
	cb_timeline_move(&timeline, 100);
	assert(timeline.selected == timeline.count - 1);
	cb_timeline_move(&timeline, -100);
	assert(timeline.selected == 0);

	cb_timeline_free(&timeline);
	return 0;
}
