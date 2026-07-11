#ifndef CHANNEL_BLUE_TIMELINE_H
#define CHANNEL_BLUE_TIMELINE_H

#include <stddef.h>

#define CB_TIMELINE_CAPACITY 24
#define CB_POST_TEXT_MAX 300

typedef enum {
	CB_APP_OK = 0,
	CB_APP_INVALID,
	CB_APP_ALLOC,
	CB_APP_NETWORK,
	CB_APP_NOT_IMPLEMENTED
} cb_app_status;

typedef struct {
	char *uri;
	char *cid;
	char *author;
	char *display_name;
	char *text;
	char *avatar_url;
	unsigned int like_count;
	unsigned int repost_count;
	unsigned int reply_count;
	unsigned char liked;
	unsigned char reposted;
} cb_post;

typedef struct {
	cb_post *posts;
	size_t count;
	char *cursor;
} cb_timeline_page;

typedef struct {
	cb_post posts[CB_TIMELINE_CAPACITY];
	size_t count;
	size_t selected;
	char *cursor;
	int has_more;
	cb_app_status last_status;
} cb_timeline;

typedef struct {
	cb_app_status (*fetch_timeline)(void *context, const char *cursor,
	                                size_t limit, cb_timeline_page *out);
	cb_app_status (*create_post)(void *context, const char *text,
	                             const cb_post *reply_to);
	cb_app_status (*like)(void *context, const cb_post *post);
	cb_app_status (*repost)(void *context, const cb_post *post);
	cb_app_status (*follow)(void *context, const char *actor);
} cb_timeline_backend;

void cb_post_free(cb_post *post);
void cb_timeline_page_free(cb_timeline_page *page);
void cb_timeline_init(cb_timeline *timeline);
void cb_timeline_free(cb_timeline *timeline);

/* Refresh replaces the current feed. Loading another page appends up to the
 * fixed Wii memory budget and preserves the current selection. */
cb_app_status cb_timeline_refresh(cb_timeline *timeline,
	                              const cb_timeline_backend *backend,
	                              void *context);
cb_app_status cb_timeline_load_more(cb_timeline *timeline,
	                                const cb_timeline_backend *backend,
	                                void *context);
void cb_timeline_move(cb_timeline *timeline, int delta);
const cb_post *cb_timeline_selected(const cb_timeline *timeline);

cb_app_status cb_timeline_create_post(cb_timeline *timeline,
	                                  const cb_timeline_backend *backend,
	                                  void *context, const char *text,
	                                  int reply_to_selected);
cb_app_status cb_timeline_like_selected(cb_timeline *timeline,
	                                    const cb_timeline_backend *backend,
	                                    void *context);
cb_app_status cb_timeline_repost_selected(cb_timeline *timeline,
	                                      const cb_timeline_backend *backend,
	                                      void *context);
cb_app_status cb_timeline_follow_selected(cb_timeline *timeline,
	                                      const cb_timeline_backend *backend,
	                                      void *context);

#endif /* CHANNEL_BLUE_TIMELINE_H */
