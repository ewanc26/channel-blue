#include "thread.h"

#include <stdlib.h>
#include <string.h>

static char *copy(const char *value) {
	size_t length;
	char *result;
	if (!value) return NULL;
	length = strlen(value);
	result = malloc(length + 1);
	if (result) memcpy(result, value, length + 1);
	return result;
}

void cb_thread_init(cb_thread *thread) {
	if (thread) memset(thread, 0, sizeof(*thread));
}

void cb_thread_free(cb_thread *thread) {
	size_t i;
	if (!thread) return;
	for (i = 0; i < thread->count; i++) cb_post_free(&thread->posts[i]);
	free(thread->root_uri);
	memset(thread, 0, sizeof(*thread));
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

cb_app_status cb_thread_load(cb_thread *thread, const cb_thread_backend *backend,
	                     void *context, const char *uri) {
	cb_timeline_page page = {0};
	cb_app_status status;
	size_t i;
	char *root = NULL;

	if (!thread || !backend || !backend->fetch_thread || !uri || !uri[0])
		return CB_APP_INVALID;
	/* Do not leave a previous conversation visible while a new URI is being
	 * fetched. A failed reload must render as unavailable, not stale content. */
	cb_thread_free(thread);
	status = backend->fetch_thread(context, uri, CB_TIMELINE_CAPACITY, &page);
	if (status != CB_APP_OK) {
		thread->last_status = status;
		cb_timeline_page_free(&page);
		return status;
	}
	for (i = 0; i < page.count; i++) {
		if (!cb_post_valid(&page.posts[i])) {
			cb_timeline_page_free(&page);
			thread->last_status = CB_APP_INVALID;
			return CB_APP_INVALID;
		}
	}
	root = copy(uri);
	if (!root) {
		cb_timeline_page_free(&page);
		thread->last_status = CB_APP_ALLOC;
		return CB_APP_ALLOC;
	}
	thread->count = page.count < CB_TIMELINE_CAPACITY
	              ? page.count : CB_TIMELINE_CAPACITY;
	for (i = 0; i < thread->count; i++) {
		thread->posts[i] = page.posts[i];
		memset(&page.posts[i], 0, sizeof(page.posts[i]));
	}
	thread->selected = 0;
	thread->root_uri = root;
	thread->loaded = 1;
	thread->last_status = CB_APP_OK;
	cb_timeline_page_free(&page);
	return CB_APP_OK;
}

void cb_thread_move(cb_thread *thread, int delta) {
	long selected;
	if (!thread || !thread->count) return;
	selected = (long)thread->selected + delta;
	if (selected < 0) selected = 0;
	if ((size_t)selected >= thread->count) selected = (long)thread->count - 1;
	thread->selected = (size_t)selected;
}

const cb_post *cb_thread_selected(const cb_thread *thread) {
	if (!thread || !thread->count || thread->selected >= thread->count)
		return NULL;
	return &thread->posts[thread->selected];
}

cb_app_status cb_thread_like_selected(cb_thread *thread,
	                              const cb_thread_backend *backend,
	                              void *context) {
	cb_post *post;
	int was_liked;
	if (!thread || !backend ||
	    !(post = (cb_post *)cb_thread_selected(thread))) return CB_APP_INVALID;
	if (backend->toggle_like) {
		was_liked = post->liked;
		thread->last_status = backend->toggle_like(context, post);
		if (thread->last_status == CB_APP_OK && post->liked != was_liked) {
			if (post->liked) post->like_count++;
			else if (post->like_count) post->like_count--;
		}
		return thread->last_status;
	}
	return CB_APP_NOT_IMPLEMENTED;
}

cb_app_status cb_thread_repost_selected(cb_thread *thread,
	                                 const cb_thread_backend *backend,
	                                 void *context) {
	cb_post *post;
	int was_reposted;
	if (!thread || !backend ||
	    !(post = (cb_post *)cb_thread_selected(thread))) return CB_APP_INVALID;
	if (backend->toggle_repost) {
		was_reposted = post->reposted;
		thread->last_status = backend->toggle_repost(context, post);
		if (thread->last_status == CB_APP_OK && post->reposted != was_reposted) {
			if (post->reposted) post->repost_count++;
			else if (post->repost_count) post->repost_count--;
		}
		return thread->last_status;
	}
	return CB_APP_NOT_IMPLEMENTED;
}

cb_app_status cb_thread_follow_selected(cb_thread *thread,
	                                 const cb_thread_backend *backend,
	                                 void *context) {
	const cb_post *post = cb_thread_selected(thread);
	if (!thread || !backend || !post)
		return CB_APP_INVALID;
	if (backend->follow) {
		thread->last_status = backend->follow(context, post->author_did);
		return thread->last_status;
	}
	return CB_APP_NOT_IMPLEMENTED;
}
