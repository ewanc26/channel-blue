#include "app/search.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int searches;
	int fail;
} fake_context;

static char *copy(const char *value) {
	char *result = malloc(strlen(value) + 1);
	assert(result);
	strcpy(result, value);
	return result;
}

static cb_app_status search_actors(void *opaque, const char *query, size_t limit,
	                           cb_search_page *out) {
	fake_context *context = opaque;
	(void)limit;
	context->searches++;
	assert(strcmp(query, "alice") == 0);
	if (context->fail) return CB_APP_NETWORK;
	out->count = 2;
	out->results = calloc(out->count, sizeof(*out->results));
	assert(out->results);
	out->results[0].did = copy("did:plc:alice");
	out->results[0].handle = copy("alice.test");
	out->results[0].display_name = copy("Alice");
	out->results[1].did = copy("did:plc:alice2");
	out->results[1].handle = copy("alice2.test");
	return CB_APP_OK;
}

int main(void) {
	fake_context context = {0};
	cb_search search;
	cb_search_backend backend = {search_actors};

	cb_search_init(&search);
	assert(cb_search_insert(&search, 'a') == CB_APP_OK);
	assert(cb_search_insert(&search, 'l') == CB_APP_OK);
	assert(cb_search_insert(&search, 'i') == CB_APP_OK);
	assert(cb_search_insert(&search, 'c') == CB_APP_OK);
	assert(cb_search_insert(&search, 'e') == CB_APP_OK);
	assert(search.query_length == 5);
	cb_search_backspace(&search);
	assert(search.query_length == 4);
	assert(cb_search_insert(&search, 'e') == CB_APP_OK);
	assert(cb_search_insert(&search, 0x00e9) == CB_APP_OK);
	assert(search.query_length == 7);
	cb_search_backspace(&search);
	assert(search.query_length == 5 && strcmp(search.query, "alice") == 0);
	assert(cb_search_run(&search, &backend, &context) == CB_APP_OK);
	assert(search.loaded && search.count == 2 && context.searches == 1);
	assert(strcmp(search.query, "alice") == 0);
	assert(strcmp(cb_search_selected(&search)->handle, "alice.test") == 0);
	cb_search_move(&search, 1);
	assert(strcmp(cb_search_selected(&search)->handle, "alice2.test") == 0);
	context.fail = 1;
	assert(cb_search_run(&search, &backend, &context) == CB_APP_NETWORK);
	assert(!search.loaded && search.count == 0 && strcmp(search.query, "alice") == 0);
	/* empty query is rejected */
	cb_search_init(&search);
	assert(cb_search_run(&search, &backend, &context) == CB_APP_INVALID);
	cb_search_free(&search);
	return 0;
}
