#include "app/notifications.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int fetches;
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
	out->count = 2;
	out->notes = calloc(out->count, sizeof(*out->notes));
	assert(out->notes);
	out->notes[0].uri = copy(cursor ? "at://n/3" : "at://n/1");
	out->notes[0].cid = copy("cid-1");
	out->notes[0].author = copy("alice.test");
	out->notes[0].reason[0] = 'l';
	out->notes[0].reason[1] = 'i';
	out->notes[0].reason[2] = 'k';
	out->notes[0].reason[3] = 'e';
	out->notes[0].reason[4] = '\0';
	out->notes[0].is_read = 0;
	out->notes[0].text = copy("nice post");
	out->notes[1].uri = copy(cursor ? "at://n/4" : "at://n/2");
	out->notes[1].cid = copy("cid-2");
	out->notes[1].author = copy("bob.test");
	out->notes[1].is_read = 1;
	if (!cursor) out->cursor = copy("next");
	return CB_APP_OK;
}

int main(void) {
	fake_context context = {0};
	cb_notifications list;
	cb_notifications_backend backend = {fetch};

	cb_notifications_init(&list);
	assert(cb_notifications_refresh(&list, &backend, &context) == CB_APP_OK);
	assert(list.loaded && list.count == 2 && list.has_more);
	assert(strcmp(cb_notifications_selected(&list)->author, "alice.test") == 0);
	cb_notifications_move(&list, 1);
	assert(strcmp(cb_notifications_selected(&list)->author, "bob.test") == 0);
	assert(cb_notifications_load_more(&list, &backend, &context) == CB_APP_OK);
	assert(list.count == 4 && !list.has_more && context.fetches == 2);
	/* empty query not applicable here; verify invalid backend is rejected */
	assert(cb_notifications_refresh(&list, NULL, &context) == CB_APP_INVALID);
	cb_notifications_free(&list);
	return 0;
}
