#ifndef CHANNEL_BLUE_NOTIFICATIONS_H
#define CHANNEL_BLUE_NOTIFICATIONS_H

#include <stddef.h>

#include "timeline.h"

#define CB_NOTIFICATIONS_CAPACITY 24
#define CB_NOTIF_REASON_MAX 24

typedef struct {
	char *uri;
	char *cid;
	char *author;          /* handle, e.g. alice.bsky.social */
	char *display_name;
	char *avatar_url;       /* avatar URL (not yet fetched to a texture) */
	char reason[CB_NOTIF_REASON_MAX + 1];
	int is_read;
	char *indexed_at;
	/* embedded post text for like/repost/reply/mention notifications */
	char *text;
} cb_notification;

typedef struct {
	cb_notification *notes;
	size_t count;
	char *cursor;
} cb_notifications_page;

typedef struct {
	cb_app_status (*fetch_notifications)(void *context, const char *cursor,
	                                      size_t limit, cb_notifications_page *out);
} cb_notifications_backend;

typedef struct {
	cb_notification notes[CB_NOTIFICATIONS_CAPACITY];
	size_t count;
	size_t selected;
	char *cursor;
	int has_more;
	/* loaded flips to 1 once a fetch has been attempted successfully. */
	int loaded;
	cb_app_status last_status;
} cb_notifications;

void cb_notification_free(cb_notification *note);
void cb_notifications_page_free(cb_notifications_page *page);
void cb_notifications_init(cb_notifications *list);
void cb_notifications_free(cb_notifications *list);

/* Refresh replaces the current list. Loading another page appends up to the
 * fixed Wii memory budget and preserves the current selection. */
cb_app_status cb_notifications_refresh(cb_notifications *list,
	                                   const cb_notifications_backend *backend,
	                                   void *context);
cb_app_status cb_notifications_load_more(cb_notifications *list,
	                                     const cb_notifications_backend *backend,
	                                     void *context);
void cb_notifications_move(cb_notifications *list, int delta);
const cb_notification *cb_notifications_selected(const cb_notifications *list);

#endif /* CHANNEL_BLUE_NOTIFICATIONS_H */
