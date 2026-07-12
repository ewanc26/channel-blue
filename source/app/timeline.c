#include "timeline.h"

#include <stdlib.h>
#include <string.h>

static char *cb_strdup(const char *value) {
	size_t length;
	char *copy;
	if (!value) return NULL;
	length = strlen(value);
	copy = malloc(length + 1);
	if (copy) memcpy(copy, value, length + 1);
	return copy;
}

void cb_post_free(cb_post *post) {
	if (!post) return;
	free(post->uri);
	free(post->cid);
	free(post->root_uri);
	free(post->root_cid);
	free(post->author);
	free(post->author_did);
	free(post->display_name);
	free(post->text);
	free(post->avatar_url);
	free(post->media_url);
	free(post->like_uri);
	free(post->repost_uri);
	memset(post, 0, sizeof(*post));
}

void cb_timeline_page_free(cb_timeline_page *page) {
	size_t i;
	if (!page) return;
	for (i = 0; i < page->count; i++) cb_post_free(&page->posts[i]);
	free(page->posts);
	free(page->cursor);
	memset(page, 0, sizeof(*page));
}

void cb_timeline_init(cb_timeline *timeline) {
	if (timeline) memset(timeline, 0, sizeof(*timeline));
}

void cb_timeline_free(cb_timeline *timeline) {
	size_t i;
	if (!timeline) return;
	for (i = 0; i < timeline->count; i++) cb_post_free(&timeline->posts[i]);
	free(timeline->cursor);
	memset(timeline, 0, sizeof(*timeline));
}

static int cb_post_valid(const cb_post *post) {
	return post && post->uri && post->uri[0] && post->cid && post->cid[0] &&
	       post->root_uri && post->root_uri[0] &&
	       post->root_cid && post->root_cid[0] &&
	       post->author && post->author[0] && post->author_did &&
	       post->author_did[0] && post->text &&
	       strlen(post->text) <= CB_POST_TEXT_BYTES_MAX &&
	       post->liked == (post->like_uri != NULL) &&
	       post->reposted == (post->repost_uri != NULL);
}

static cb_app_status cb_timeline_fetch(cb_timeline *timeline,
	                                   const cb_timeline_backend *backend,
	                                   void *context, int append) {
	cb_timeline_page page = {0};
	cb_app_status status;
	size_t available;
	size_t accepted;
	size_t i;
	char *cursor = NULL;

	if (!timeline || !backend || !backend->fetch_timeline)
		return CB_APP_INVALID;
	status = backend->fetch_timeline(context, append ? timeline->cursor : NULL,
	                                 CB_TIMELINE_CAPACITY, &page);
	if (status != CB_APP_OK) {
		timeline->last_status = status;
		cb_timeline_page_free(&page);
		return status;
	}
	if ((page.count && !page.posts) || page.count > CB_TIMELINE_CAPACITY) {
		cb_timeline_page_free(&page);
		timeline->last_status = CB_APP_INVALID;
		return CB_APP_INVALID;
	}
	for (i = 0; i < page.count; i++) {
		if (!cb_post_valid(&page.posts[i])) {
			cb_timeline_page_free(&page);
			timeline->last_status = CB_APP_INVALID;
			return CB_APP_INVALID;
		}
	}
	if (page.cursor) {
		cursor = cb_strdup(page.cursor);
		if (!cursor) {
			cb_timeline_page_free(&page);
			timeline->last_status = CB_APP_ALLOC;
			return CB_APP_ALLOC;
		}
	}
	if (!append) cb_timeline_free(timeline);
	available = CB_TIMELINE_CAPACITY - timeline->count;
	accepted = page.count < available ? page.count : available;
	for (i = 0; i < accepted; i++) {
		timeline->posts[timeline->count++] = page.posts[i];
		memset(&page.posts[i], 0, sizeof(page.posts[i]));
	}
	free(timeline->cursor);
	timeline->cursor = cursor;
	timeline->has_more = cursor != NULL && timeline->count < CB_TIMELINE_CAPACITY;
	timeline->last_status = CB_APP_OK;
	cb_timeline_page_free(&page);
	return CB_APP_OK;
}

cb_app_status cb_timeline_refresh(cb_timeline *timeline,
	                              const cb_timeline_backend *backend,
	                              void *context) {
	return cb_timeline_fetch(timeline, backend, context, 0);
}

cb_app_status cb_timeline_load_more(cb_timeline *timeline,
	                                const cb_timeline_backend *backend,
	                                void *context) {
	if (!timeline || !timeline->has_more) return CB_APP_INVALID;
	return cb_timeline_fetch(timeline, backend, context, 1);
}

void cb_timeline_move(cb_timeline *timeline, int delta) {
	long selected;
	if (!timeline || !timeline->count) return;
	selected = (long)timeline->selected + delta;
	if (selected < 0) selected = 0;
	if ((size_t)selected >= timeline->count) selected = (long)timeline->count - 1;
	timeline->selected = (size_t)selected;
}

const cb_post *cb_timeline_selected(const cb_timeline *timeline) {
	if (!timeline || !timeline->count || timeline->selected >= timeline->count)
		return NULL;
	return &timeline->posts[timeline->selected];
}

cb_app_status cb_timeline_create_post(cb_timeline *timeline,
	                                  const cb_timeline_backend *backend,
	                                  void *context, const char *text,
	                                  int reply_to_selected) {
	const cb_post *reply = reply_to_selected ? cb_timeline_selected(timeline) : NULL;
	if (!timeline || !backend || !backend->create_post || !text || !text[0] ||
	    strlen(text) > CB_POST_TEXT_BYTES_MAX || (reply_to_selected && !reply))
		return CB_APP_INVALID;
	timeline->last_status = backend->create_post(context, text, reply);
	return timeline->last_status;
}

cb_app_status cb_timeline_like_selected(cb_timeline *timeline,
	                                    const cb_timeline_backend *backend,
	                                    void *context) {
	cb_post *post;
	int was_liked;
	if (!timeline || !backend || !backend->toggle_like ||
	    !(post = (cb_post *)cb_timeline_selected(timeline))) return CB_APP_INVALID;
	was_liked = post->liked;
	timeline->last_status = backend->toggle_like(context, post);
	if (timeline->last_status == CB_APP_OK && post->liked != was_liked) {
		if (post->liked) post->like_count++;
		else if (post->like_count) post->like_count--;
	}
	return timeline->last_status;
}

cb_app_status cb_timeline_repost_selected(cb_timeline *timeline,
	                                      const cb_timeline_backend *backend,
	                                      void *context) {
	cb_post *post;
	int was_reposted;
	if (!timeline || !backend || !backend->toggle_repost ||
	    !(post = (cb_post *)cb_timeline_selected(timeline))) return CB_APP_INVALID;
	was_reposted = post->reposted;
	timeline->last_status = backend->toggle_repost(context, post);
	if (timeline->last_status == CB_APP_OK && post->reposted != was_reposted) {
		if (post->reposted) post->repost_count++;
		else if (post->repost_count) post->repost_count--;
	}
	return timeline->last_status;
}

cb_app_status cb_timeline_follow_selected(cb_timeline *timeline,
	                                      const cb_timeline_backend *backend,
	                                      void *context) {
	const cb_post *post = cb_timeline_selected(timeline);
	if (!timeline || !backend || !backend->follow || !post)
		return CB_APP_INVALID;
	timeline->last_status = backend->follow(context, post->author_did);
	return timeline->last_status;
}
