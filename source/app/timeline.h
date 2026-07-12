#ifndef CHANNEL_BLUE_TIMELINE_H
#define CHANNEL_BLUE_TIMELINE_H

#include <stddef.h>

#define CB_TIMELINE_CAPACITY 24
/* app.bsky.feed.post: maxLength is UTF-8 bytes; maxGraphemes is the
 * user-visible composition limit enforced when the record is validated. */
#define CB_POST_TEXT_BYTES_MAX 3000
#define CB_POST_TEXT_GRAPHEMES_MAX 300

typedef enum {
	CB_APP_OK = 0,
	CB_APP_INVALID,
	CB_APP_ALLOC,
	CB_APP_NETWORK,
	CB_APP_CONFIGURATION,
	CB_APP_NOT_IMPLEMENTED
} cb_app_status;

typedef struct {
	char *uri;
	char *cid;
	/* Thread root strong reference. Equal to uri/cid for a top-level post. */
	char *root_uri;
	char *root_cid;
	char *author;       /* display handle */
	char *author_did;   /* stable DID required by graph operations */
	char *display_name;
	char *text;
	char *avatar_url;
	char *media_url;       /* first renderable image/thumbnail URL, owned */
	unsigned int like_count;
	unsigned int repost_count;
	unsigned int reply_count;
	char *like_uri;       /* viewer's like record URI, owned; NULL when unliked */
	char *repost_uri;     /* viewer's repost record URI, owned; NULL otherwise */
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
	/* Toggle callbacks update post->*_uri and the matching boolean only after
	 * the remote create/delete succeeds. */
	cb_app_status (*toggle_like)(void *context, cb_post *post);
	cb_app_status (*toggle_repost)(void *context, cb_post *post);
	cb_app_status (*follow)(void *context, const char *actor_did);
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
