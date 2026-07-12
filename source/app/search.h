#ifndef CHANNEL_BLUE_SEARCH_H
#define CHANNEL_BLUE_SEARCH_H

#include <stddef.h>

#include "timeline.h"

#define CB_SEARCH_CAPACITY 24
#define CB_SEARCH_QUERY_MAX 127

typedef struct {
	char *did;
	char *handle;
	char *display_name;
	char *avatar_url;
} cb_search_result;

typedef struct {
	cb_search_result *results;
	size_t count;
	char *cursor;
} cb_search_page;

typedef struct {
	cb_app_status (*search_actors)(void *context, const char *query,
	                               size_t limit, cb_search_page *out);
} cb_search_backend;

typedef struct {
	char query[CB_SEARCH_QUERY_MAX + 1];
	size_t query_length;
	cb_search_result *results;
	size_t count;
	size_t selected;
	int loaded;
	cb_app_status last_status;
} cb_search;

void cb_search_result_free(cb_search_result *result);
void cb_search_page_free(cb_search_page *page);
void cb_search_init(cb_search *search);
void cb_search_free(cb_search *search);

/* Run a search for the current query. An empty query is rejected as invalid
 * rather than issuing a wildcard request to the PDS. */
cb_app_status cb_search_run(cb_search *search, const cb_search_backend *backend,
	                    void *context);
cb_app_status cb_search_insert(cb_search *search, unsigned int character);
void cb_search_backspace(cb_search *search);
void cb_search_move(cb_search *search, int delta);
const cb_search_result *cb_search_selected(const cb_search *search);

#endif /* CHANNEL_BLUE_SEARCH_H */
