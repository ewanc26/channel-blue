#include "app/notifications.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int fetches;
	int seen_calls;
	int seen_fail;
	int empty;
} fake_context;

static char *copy(const char *value) {
	char *result = malloc(strlen(value) + 1);
	assert(result);
	strcpy(result, value);
	return result;
}

static cb_app_status fetch(void *opaque, const char *cursor, size_t limit,
	                       cb_notifications_page *out) {
	fake_context *context = opaque;
	(void)limit;
	context->fetches++;
	if (context->empty) {
		out->count = 0;
		return CB_APP_OK;
	}
	out->count = 2;
	out->notes = calloc(out->count, sizeof(*out->notes));
	assert(out->notes);
	out->notes[0].uri = copy(cursor ? "at://n/3" : "at://n/1");
	out->notes[0].cid = copy("cid-1");
	out->notes[0].reason_subject = copy("at://post/subject");
	out->notes[0].author = copy("alice.test");
	out->notes[0].reason[0] = 'l';
	out->notes[0].reason[1] = 'i';
	out->notes[0].reason[2] = 'k';
	out->notes[0].reason[3] = 'e';
	out->notes[0].reason[4] = '\0';
	out->notes[0].is_read = 0;
	out->notes[0].indexed_at = copy("2026-07-12T12:00:00Z");
	out->notes[0].text = copy("nice post");
	out->notes[1].uri = copy(cursor ? "at://n/4" : "at://n/2");
	out->notes[1].cid = copy("cid-2");
	out->notes[1].author = copy("bob.test");
	out->notes[1].is_read = 1;
	out->notes[1].indexed_at = copy("2026-07-12T13:00:00Z");
	if (!cursor) out->cursor = copy("next");
	return CB_APP_OK;
}

static cb_app_status mark_seen(void *opaque, const char *seen_at) {
	fake_context *context = opaque;
	context->seen_calls++;
	assert(strcmp(seen_at, "2026-07-12T13:00:00Z") == 0);
	return context->seen_fail ? CB_APP_NETWORK : CB_APP_OK;
}

int main(void) {
	fake_context context = {0};
	cb_notifications list;
	cb_notifications_backend backend = {fetch, mark_seen};

	cb_notifications_init(&list);
	assert(cb_notifications_refresh(&list, &backend, &context) == CB_APP_OK);
	assert(list.loaded && list.count == 2 && list.has_more);
	assert(strcmp(cb_notifications_selected(&list)->author, "alice.test") == 0);
	assert(strcmp(cb_notifications_selected(&list)->reason_subject,
	              "at://post/subject") == 0);
	assert(cb_notifications_mark_seen(&list, &backend, &context) == CB_APP_OK);
	assert(list.notes[0].is_read && list.notes[1].is_read && context.seen_calls == 1);
	cb_notifications_move(&list, 1);
	assert(strcmp(cb_notifications_selected(&list)->author, "bob.test") == 0);
	assert(cb_notifications_load_more(&list, &backend, &context) == CB_APP_OK);
	assert(list.count == 4 && !list.has_more && context.fetches == 2);
	context.empty = 1;
	assert(cb_notifications_refresh(&list, &backend, &context) == CB_APP_OK);
	assert(list.count == 0 && list.selected == 0);
	context.empty = 0;
	assert(cb_notifications_refresh(&list, &backend, &context) == CB_APP_OK);
	assert(list.count == 2 && list.selected == 0);
	/* A failed seen update keeps the successfully fetched page and unread state. */
	context.seen_fail = 1;
	assert(cb_notifications_refresh(&list, &backend, &context) == CB_APP_OK);
	assert(!list.notes[0].is_read && list.count == 2);
	assert(cb_notifications_mark_seen(&list, &backend, &context) == CB_APP_NETWORK);
	assert(!list.notes[0].is_read && list.last_status == CB_APP_OK &&
	       list.seen_status == CB_APP_NETWORK);
	/* empty query not applicable here; verify invalid backend is rejected */
	assert(cb_notifications_refresh(&list, NULL, &context) == CB_APP_INVALID);
	cb_notifications_free(&list);
	return 0;
}
