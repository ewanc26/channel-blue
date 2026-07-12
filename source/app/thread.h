#ifndef CHANNEL_BLUE_THREAD_H
#define CHANNEL_BLUE_THREAD_H

#include <stddef.h>

#include "timeline.h"

/*
 * Conversation / thread view controller.
 *
 * A thread is the focused post plus its ancestors and replies, flattened by
 * the wolfram backend into a bounded linear list of `cb_post` (reusing the
 * timeline post model). The controller owns a fixed-capacity buffer of posts
 * like `cb_timeline`; `loaded` flips to 1 once a fetch has succeeded for the
 * post named by `root_uri`.
 */

typedef struct {
	cb_app_status (*fetch_thread)(void *context, const char *uri,
	                              size_t limit, cb_timeline_page *out);
	cb_app_status (*like)(void *context, const cb_post *post);
	cb_app_status (*repost)(void *context, const cb_post *post);
	cb_app_status (*follow)(void *context, const char *actor);
} cb_thread_backend;

typedef struct {
	cb_post posts[CB_TIMELINE_CAPACITY];
	size_t count;
	size_t selected;
	char *root_uri;       /* URI of the post this thread was opened for */
	int loaded;
	cb_app_status last_status;
} cb_thread;

void cb_thread_init(cb_thread *thread);
void cb_thread_free(cb_thread *thread);

/* Load (or reload) the thread rooted at `uri`. The previous contents are freed
 * before the new page is ingested into the fixed post buffer. */
cb_app_status cb_thread_load(cb_thread *thread, const cb_thread_backend *backend,
	                     void *context, const char *uri);

void cb_thread_move(cb_thread *thread, int delta);
const cb_post *cb_thread_selected(const cb_thread *thread);

cb_app_status cb_thread_like_selected(cb_thread *thread,
	                              const cb_thread_backend *backend, void *context);
cb_app_status cb_thread_repost_selected(cb_thread *thread,
	                                 const cb_thread_backend *backend,
	                                 void *context);
cb_app_status cb_thread_follow_selected(cb_thread *thread,
	                                 const cb_thread_backend *backend,
	                                 void *context);

#endif /* CHANNEL_BLUE_THREAD_H */
