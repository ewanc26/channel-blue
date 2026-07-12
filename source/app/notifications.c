#include "notifications.h"

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

void cb_notification_free(cb_notification *note) {
	if (!note) return;
	free(note->uri);
	free(note->cid);
	free(note->author);
	free(note->display_name);
	free(note->avatar_url);
	free(note->indexed_at);
	free(note->text);
	memset(note, 0, sizeof(*note));
}

void cb_notifications_page_free(cb_notifications_page *page) {
	size_t i;
	if (!page) return;
	for (i = 0; i < page->count; i++) cb_notification_free(&page->notes[i]);
	free(page->notes);
	free(page->cursor);
	memset(page, 0, sizeof(*page));
}

void cb_notifications_init(cb_notifications *list) {
	if (list) memset(list, 0, sizeof(*list));
}

void cb_notifications_free(cb_notifications *list) {
	size_t i;
	if (!list) return;
	for (i = 0; i < list->count; i++) cb_notification_free(&list->notes[i]);
	free(list->cursor);
	memset(list, 0, sizeof(*list));
}

static int cb_notification_valid(const cb_notification *note) {
	return note && note->uri && note->uri[0] && note->cid && note->cid[0] &&
	       note->author && note->author[0];
}

static cb_app_status cb_notifications_fetch(cb_notifications *list,
	                                       const cb_notifications_backend *backend,
	                                       void *context, int append) {
	cb_notifications_page page = {0};
	cb_app_status status;
	size_t available;
	size_t accepted;
	size_t i;
	char *cursor = NULL;

	if (!list || !backend || !backend->fetch_notifications)
		return CB_APP_INVALID;
	status = backend->fetch_notifications(context,
	                                      append ? list->cursor : NULL,
	                                      CB_NOTIFICATIONS_CAPACITY, &page);
	if (status != CB_APP_OK) {
		list->last_status = status;
		cb_notifications_page_free(&page);
		return status;
	}
	if ((page.count && !page.notes) || page.count > CB_NOTIFICATIONS_CAPACITY) {
		cb_notifications_page_free(&page);
		list->last_status = CB_APP_INVALID;
		return CB_APP_INVALID;
	}
	for (i = 0; i < page.count; i++) {
		if (!cb_notification_valid(&page.notes[i])) {
			cb_notifications_page_free(&page);
			list->last_status = CB_APP_INVALID;
			return CB_APP_INVALID;
		}
	}
	if (page.cursor) {
		cursor = cb_strdup(page.cursor);
		if (!cursor) {
			cb_notifications_page_free(&page);
			list->last_status = CB_APP_ALLOC;
			return CB_APP_ALLOC;
		}
	}
	if (!append) {
		size_t j;
		for (j = 0; j < list->count; j++) cb_notification_free(&list->notes[j]);
		list->count = 0;
	}
	available = CB_NOTIFICATIONS_CAPACITY - list->count;
	accepted = page.count < available ? page.count : available;
	for (i = 0; i < accepted; i++) {
		list->notes[list->count++] = page.notes[i];
		memset(&page.notes[i], 0, sizeof(page.notes[i]));
	}
	free(list->cursor);
	list->cursor = cursor;
	list->has_more = cursor != NULL && list->count < CB_NOTIFICATIONS_CAPACITY;
	list->loaded = 1;
	list->last_status = CB_APP_OK;
	cb_notifications_page_free(&page);
	return CB_APP_OK;
}

cb_app_status cb_notifications_refresh(cb_notifications *list,
	                                   const cb_notifications_backend *backend,
	                                   void *context) {
	return cb_notifications_fetch(list, backend, context, 0);
}

cb_app_status cb_notifications_load_more(cb_notifications *list,
	                                     const cb_notifications_backend *backend,
	                                     void *context) {
	if (!list || !list->has_more) return CB_APP_INVALID;
	return cb_notifications_fetch(list, backend, context, 1);
}

void cb_notifications_move(cb_notifications *list, int delta) {
	long selected;
	if (!list || !list->count) return;
	selected = (long)list->selected + delta;
	if (selected < 0) selected = 0;
	if ((size_t)selected >= list->count) selected = (long)list->count - 1;
	list->selected = (size_t)selected;
}

const cb_notification *cb_notifications_selected(const cb_notifications *list) {
	if (!list || !list->count || list->selected >= list->count)
		return NULL;
	return &list->notes[list->selected];
}
