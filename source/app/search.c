#include "search.h"
#include "utf8.h"

#include <stdlib.h>
#include <string.h>

void cb_search_result_free(cb_search_result *result) {
	if (!result) return;
	free(result->did);
	free(result->handle);
	free(result->display_name);
	free(result->avatar_url);
	memset(result, 0, sizeof(*result));
}

void cb_search_page_free(cb_search_page *page) {
	size_t i;
	if (!page) return;
	for (i = 0; i < page->count; i++) cb_search_result_free(&page->results[i]);
	free(page->results);
	free(page->cursor);
	memset(page, 0, sizeof(*page));
}

void cb_search_init(cb_search *search) {
	if (search) memset(search, 0, sizeof(*search));
}

static void cb_search_clear_results(cb_search *search) {
	size_t i;
	if (!search) return;
	for (i = 0; i < search->count; i++) cb_search_result_free(&search->results[i]);
	free(search->results);
	search->results = NULL;
	search->count = 0;
	search->selected = 0;
	search->loaded = 0;
}

void cb_search_free(cb_search *search) {
	if (!search) return;
	cb_search_clear_results(search);
	memset(search, 0, sizeof(*search));
}

static int cb_search_result_valid(const cb_search_result *result) {
	return result && result->did && result->did[0] && result->handle &&
	       result->handle[0];
}

cb_app_status cb_search_run(cb_search *search, const cb_search_backend *backend,
	                    void *context) {
	cb_search_page page = {0};
	cb_app_status status;
	size_t i;

	if (!search || !backend || !backend->search_actors)
		return CB_APP_INVALID;
	if (!search->query_length) return CB_APP_INVALID;
	status = backend->search_actors(context, search->query,
	                                CB_SEARCH_CAPACITY, &page);
	if (status != CB_APP_OK) {
		cb_search_clear_results(search);
		search->last_status = status;
		cb_search_page_free(&page);
		return status;
	}
	if ((page.count && !page.results) || page.count > CB_SEARCH_CAPACITY) {
		cb_search_clear_results(search);
		cb_search_page_free(&page);
		search->last_status = CB_APP_INVALID;
		return CB_APP_INVALID;
	}
	for (i = 0; i < page.count; i++) {
		if (!cb_search_result_valid(&page.results[i])) {
			cb_search_clear_results(search);
			cb_search_page_free(&page);
			search->last_status = CB_APP_INVALID;
			return CB_APP_INVALID;
		}
	}
	cb_search_clear_results(search);
	search->count = page.count;
	search->selected = 0;
	search->results = page.results;
	page.results = NULL;
	search->loaded = 1;
	search->last_status = CB_APP_OK;
	free(page.cursor);
	return CB_APP_OK;
}

cb_app_status cb_search_insert(cb_search *search, unsigned int character) {
	if (!search) return CB_APP_INVALID;
	return cb_utf8_append(search->query, &search->query_length,
	                      CB_SEARCH_QUERY_MAX, character)
	     ? CB_APP_OK : CB_APP_INVALID;
}

void cb_search_backspace(cb_search *search) {
	if (!search || !search->query_length) return;
	cb_utf8_backspace(search->query, &search->query_length);
}

void cb_search_move(cb_search *search, int delta) {
	long selected;
	if (!search || !search->count) return;
	selected = (long)search->selected + delta;
	if (selected < 0) selected = 0;
	if ((size_t)selected >= search->count) selected = (long)search->count - 1;
	search->selected = (size_t)selected;
}

const cb_search_result *cb_search_selected(const cb_search *search) {
	if (!search || !search->count || search->selected >= search->count)
		return NULL;
	return &search->results[search->selected];
}
