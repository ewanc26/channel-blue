#include "wolfram_backend.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cJSON.h>
#include <wolfram/actor_typed.h>

#include "../app/retry.h"

static char *copy(const char *value) {
	size_t length;
	char *result;
	if (!value) return NULL;
	length = strlen(value);
	result = malloc(length + 1);
	if (result) memcpy(result, value, length + 1);
	return result;
}

static cb_app_status status_from_wolfram(wf_status status) {
	switch (status) {
	case WF_OK: return CB_APP_OK;
	case WF_ERR_ALLOC: return CB_APP_ALLOC;
	case WF_ERR_INVALID_ARG:
	case WF_ERR_PARSE:
	case WF_ERR_VALIDATION: return CB_APP_INVALID;
	case WF_ERR_NOT_IMPLEMENTED:
	case WF_ERR_UNSUPPORTED: return CB_APP_NOT_IMPLEMENTED;
	default: return CB_APP_NETWORK;
	}
}

static int set_string(char **out, const char *value, int required) {
	*out = copy(value);
	return *out || (!required && !value);
}

static cb_app_status session_from_wolfram(const wf_session_data *source,
	                                      const char *fallback_service,
	                                      cb_session_data *out) {
	memset(out, 0, sizeof(*out));
	if (!source || !source->access_jwt || !source->refresh_jwt ||
	    !source->handle || !source->did) return CB_APP_INVALID;
	if (!set_string(&out->service,
	                source->pds_url ? source->pds_url : fallback_service, 1) ||
	    !set_string(&out->access_jwt, source->access_jwt, 1) ||
	    !set_string(&out->refresh_jwt, source->refresh_jwt, 1) ||
	    !set_string(&out->handle, source->handle, 1) ||
	    !set_string(&out->did, source->did, 1)) {
		cb_session_data_free(out);
		return CB_APP_ALLOC;
	}
	return CB_APP_OK;
}

static cb_app_status export_session(cb_wolfram_context *context,
	                                const char *fallback_service,
	                                cb_session_data *out) {
	wf_session_data session = {0};
	wf_status status = wf_agent_get_session_data(context->agent.agent, &session);
	cb_app_status converted;
	if (status != WF_OK) return status_from_wolfram(status);
	converted = session_from_wolfram(&session, fallback_service, out);
	wf_agent_session_data_free(&session);
	return converted;
}

void cb_wolfram_context_init(cb_wolfram_context *context) {
	if (!context) return;
	memset(context, 0, sizeof(*context));
	wf_bsky_agent_init(&context->agent);
}

void cb_wolfram_context_free(cb_wolfram_context *context) {
	if (context) wf_bsky_agent_free(&context->agent);
}

void cb_wolfram_context_set_network_ready(cb_wolfram_context *context,
	                                       int ready) {
	if (context) context->network_ready = ready != 0;
}

/* Wii WiFi is 802.11b/g over IOS and transient failures are common, so network
 * calls retry with capped exponential backoff before surfacing an error. */
#define CB_NETWORK_MAX_ATTEMPTS 4

static int cb_wolfram_transient(cb_retry_status status) {
	switch ((wf_status)status) {
	case WF_ERR_NETWORK:
	case WF_ERR_TIMEOUT:
	case WF_ERR_WOULD_BLOCK:
	case WF_ERR_RATE_LIMIT:
	case WF_ERR_UNKNOWN:
		return 1;
	default:
		return 0;
	}
}

static unsigned cb_network_backoff_ms(int attempt) {
	unsigned delay = 500u << attempt;
	return delay > 8000u ? 8000u : delay;
}

static void cb_network_sleep_ms(unsigned ms) {
	usleep(ms * 1000);
}

typedef struct {
	wf_bsky_agent *agent;
	const char *service;
	const char *identifier;
	const char *password;
} login_ctx;

static cb_retry_status login_op(void *ctx) {
	login_ctx *c = ctx;
	return (cb_retry_status)wf_bsky_agent_login(c->agent, c->service,
	                                            c->identifier, c->password);
}

static cb_app_status backend_login(void *opaque, const char *service,
	                               const char *identifier, const char *password,
	                               cb_session_data *out) {
	cb_wolfram_context *context = opaque;
	login_ctx lc = {&context->agent, service, identifier, password};
	wf_status status;
	if (!context) return CB_APP_INVALID;
	if (!context->network_ready) return CB_APP_NETWORK;
	status = (wf_status)cb_retry(CB_NETWORK_MAX_ATTEMPTS, cb_wolfram_transient,
	                             login_op, &lc, cb_network_backoff_ms,
	                             cb_network_sleep_ms);
	if (status != WF_OK) return status_from_wolfram(status);
	return export_session(context, service, out);
}

static cb_app_status backend_resume(void *opaque, const cb_session_data *saved,
	                                cb_session_data *out) {
	cb_wolfram_context *context = opaque;
	wf_session_data session = {0};
	wf_status status;
	if (!context || !saved) return CB_APP_INVALID;
	if (!context->network_ready) return CB_APP_NETWORK;
	session.access_jwt = saved->access_jwt;
	session.refresh_jwt = saved->refresh_jwt;
	session.handle = saved->handle;
	session.did = saved->did;
	session.pds_url = saved->service;
	status = wf_bsky_agent_login_session(&context->agent, &session);
	if (status != WF_OK) return status_from_wolfram(status);
	return export_session(context, saved->service, out);
}

static cb_app_status backend_logout(void *opaque) {
	cb_wolfram_context *context = opaque;
	return context && context->network_ready
	             ? status_from_wolfram(wf_bsky_agent_logout(&context->agent))
	               : CB_APP_INVALID;
}

cb_auth_backend cb_wolfram_auth_backend(void) {
	cb_auth_backend backend = {backend_login, backend_resume, backend_logout};
	return backend;
}

cb_app_status cb_wolfram_convert_feed(const wf_agent_feed_list *feed,
	                                  cb_timeline_page *out) {
	size_t i;
	if (!feed || !out || (feed->item_count && !feed->items)) return CB_APP_INVALID;
	memset(out, 0, sizeof(*out));
	out->count = feed->item_count < CB_TIMELINE_CAPACITY
	           ? feed->item_count : CB_TIMELINE_CAPACITY;
	if (out->count) {
		out->posts = calloc(out->count, sizeof(*out->posts));
		if (!out->posts) return CB_APP_ALLOC;
	}
	for (i = 0; i < out->count; i++) {
		const wf_agent_post_view *source = &feed->items[i].post;
		cJSON *text = source->record
		            ? cJSON_GetObjectItemCaseSensitive(source->record, "text") : NULL;
		cb_post *post = &out->posts[i];
		if (!source->uri || !source->cid || !source->author.handle ||
		    !cJSON_IsString(text) || !text->valuestring ||
		    !set_string(&post->uri, source->uri, 1) ||
		    !set_string(&post->cid, source->cid, 1) ||
		    !set_string(&post->author, source->author.handle, 1) ||
		    !set_string(&post->display_name, source->author.display_name, 0) ||
		    !set_string(&post->text, text->valuestring, 1) ||
		    !set_string(&post->avatar_url, source->author.avatar, 0)) {
			cb_timeline_page_free(out);
			return CB_APP_INVALID;
		}
		post->reply_count = source->reply_count > 0 ? (unsigned)source->reply_count : 0;
		post->repost_count = source->repost_count > 0 ? (unsigned)source->repost_count : 0;
		post->like_count = source->like_count > 0 ? (unsigned)source->like_count : 0;
		post->liked = source->viewer.like != NULL;
		post->reposted = source->viewer.repost != NULL;
	}
	if (feed->cursor) {
		out->cursor = copy(feed->cursor);
		if (!out->cursor) {
			cb_timeline_page_free(out);
			return CB_APP_ALLOC;
		}
	}
	return CB_APP_OK;
}

typedef struct {
	wf_bsky_agent *agent;
	int limit;
	const char *cursor;
	wf_agent_feed_list *feed;
} fetch_ctx;

static cb_retry_status fetch_timeline_op(void *ctx) {
	fetch_ctx *c = ctx;
	return (cb_retry_status)wf_bsky_agent_get_timeline(c->agent, c->limit,
	                                                  c->cursor, c->feed);
}

static cb_app_status backend_fetch(void *opaque, const char *cursor, size_t limit,
	                               cb_timeline_page *out) {
	cb_wolfram_context *context = opaque;
	wf_agent_feed_list feed = {0};
	fetch_ctx fc = {&context->agent, (int)limit, cursor, &feed};
	wf_status status;
	cb_app_status converted;
	if (!context || !context->agent.agent) return CB_APP_INVALID;
	if (!context->network_ready) return CB_APP_NETWORK;
	status = (wf_status)cb_retry(CB_NETWORK_MAX_ATTEMPTS, cb_wolfram_transient,
	                             fetch_timeline_op, &fc, cb_network_backoff_ms,
	                             cb_network_sleep_ms);
	if (status != WF_OK) return status_from_wolfram(status);
	converted = cb_wolfram_convert_feed(&feed, out);
	wf_agent_feed_list_free(&feed);
	return converted;
}

static cb_app_status backend_create(void *opaque, const char *text,
	                                const cb_post *reply) {
	cb_wolfram_context *context = opaque;
	wf_agent_post_result result = {0};
	wf_status status;
	if (!context) return CB_APP_INVALID;
	if (!context->network_ready) return CB_APP_NETWORK;
	status = reply
	       ? wf_agent_reply(context->agent.agent, text, reply->uri, reply->cid, &result)
	       : wf_bsky_agent_post(&context->agent, text, &result);
	wf_agent_post_result_free(&result);
	return status_from_wolfram(status);
}

static cb_app_status backend_like(void *opaque, const cb_post *post) {
	cb_wolfram_context *context = opaque;
	wf_agent_post_result result = {0};
	wf_status status = context && context->network_ready
	                 ? wf_bsky_agent_like(&context->agent, post->uri, post->cid, &result)
	                 : WF_ERR_INVALID_ARG;
	wf_agent_post_result_free(&result);
	return status_from_wolfram(status);
}

static cb_app_status backend_repost(void *opaque, const cb_post *post) {
	cb_wolfram_context *context = opaque;
	wf_agent_post_result result = {0};
	wf_status status = context && context->network_ready
	                 ? wf_bsky_agent_repost(&context->agent, post->uri, post->cid, &result)
	                 : WF_ERR_INVALID_ARG;
	wf_agent_post_result_free(&result);
	return status_from_wolfram(status);
}

static cb_app_status backend_follow(void *opaque, const char *actor) {
	cb_wolfram_context *context = opaque;
	wf_agent_post_result result = {0};
	wf_status status = context && context->network_ready
	                 ? wf_bsky_agent_follow(&context->agent, actor, &result)
	                 : WF_ERR_INVALID_ARG;
	wf_agent_post_result_free(&result);
	return status_from_wolfram(status);
}

cb_timeline_backend cb_wolfram_timeline_backend(void) {
	cb_timeline_backend backend = {
		backend_fetch, backend_create, backend_like, backend_repost, backend_follow
	};
	return backend;
}

cb_app_status cb_wolfram_convert_notifications(
	const wf_agent_notification_list *source, cb_notifications_page *out) {
	size_t i;
	if (!source || !out || (source->notification_count && !source->notifications))
		return CB_APP_INVALID;
	memset(out, 0, sizeof(*out));
	out->count = source->notification_count < CB_NOTIFICATIONS_CAPACITY
	           ? source->notification_count : CB_NOTIFICATIONS_CAPACITY;
	if (out->count) {
		out->notes = calloc(out->count, sizeof(*out->notes));
		if (!out->notes) return CB_APP_ALLOC;
	}
	for (i = 0; i < out->count; i++) {
		const wf_agent_notification *src = &source->notifications[i];
		const char *text = src->record
		                 ? cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(
		                       src->record, "text")) : NULL;
		cb_notification *note = &out->notes[i];
		size_t rlen;
		if (!set_string(&note->uri, src->uri, 1) ||
		    !set_string(&note->cid, src->cid, 1) ||
		    !set_string(&note->author, src->author.handle, 1) ||
		    !set_string(&note->display_name, src->author.display_name, 0) ||
		    !set_string(&note->avatar_url, src->author.avatar, 0) ||
		    !set_string(&note->indexed_at, src->indexed_at, 0) ||
		    !set_string(&note->text, text, 0)) {
			cb_notifications_page_free(out);
			return CB_APP_ALLOC;
		}
		rlen = src->reason ? strlen(src->reason) : 0;
		if (rlen > CB_NOTIF_REASON_MAX) rlen = CB_NOTIF_REASON_MAX;
		if (rlen) memcpy(note->reason, src->reason, rlen);
		note->reason[rlen] = '\0';
		note->is_read = src->is_read;
	}
	if (source->cursor) {
		out->cursor = copy(source->cursor);
		if (!out->cursor) {
			cb_notifications_page_free(out);
			return CB_APP_ALLOC;
		}
	}
	return CB_APP_OK;
}

cb_app_status cb_wolfram_convert_search(const wf_agent_actor_list *source,
	                                cb_search_page *out) {
	size_t i;
	if (!source || !out || (source->actor_count && !source->actors))
		return CB_APP_INVALID;
	memset(out, 0, sizeof(*out));
	out->count = source->actor_count < CB_SEARCH_CAPACITY
	           ? source->actor_count : CB_SEARCH_CAPACITY;
	if (out->count) {
		out->results = calloc(out->count, sizeof(*out->results));
		if (!out->results) return CB_APP_ALLOC;
	}
	for (i = 0; i < out->count; i++) {
		const wf_agent_profile_view *src = &source->actors[i];
		cb_search_result *result = &out->results[i];
		if (!set_string(&result->did, src->did, 1) ||
		    !set_string(&result->handle, src->handle, 1) ||
		    !set_string(&result->display_name, src->display_name, 0) ||
		    !set_string(&result->avatar_url, src->avatar, 0)) {
			cb_search_page_free(out);
			return CB_APP_ALLOC;
		}
	}
	if (source->cursor) {
		out->cursor = copy(source->cursor);
		if (!out->cursor) {
			cb_search_page_free(out);
			return CB_APP_ALLOC;
		}
	}
	return CB_APP_OK;
}

cb_app_status cb_wolfram_convert_profile(const wf_agent_profile *source,
	                                 cb_profile_data *out) {
	memset(out, 0, sizeof(*out));
	if (!source || !source->did || !source->handle) return CB_APP_INVALID;
	if (!set_string(&out->did, source->did, 1) ||
	    !set_string(&out->handle, source->handle, 1) ||
	    !set_string(&out->display_name, source->display_name, 0) ||
	    !set_string(&out->description, source->description, 0) ||
	    !set_string(&out->avatar_url, source->avatar_cid, 0)) {
		cb_profile_data_free(out);
		return CB_APP_ALLOC;
	}
	out->followers_count = source->followers_count;
	out->follows_count = source->follows_count;
	out->posts_count = source->posts_count;
	return CB_APP_OK;
}

typedef struct {
	wf_bsky_agent *agent;
	int limit;
	const char *cursor;
	wf_agent_notification_list *list;
} notifications_ctx;

static cb_retry_status fetch_notifications_op(void *ctx) {
	notifications_ctx *c = ctx;
	return (cb_retry_status)wf_bsky_agent_get_notifications(c->agent, c->limit,
	                                                       c->cursor, c->list);
}

static cb_app_status backend_notifications_fetch(void *opaque,
	                                         const char *cursor, size_t limit,
	                                         cb_notifications_page *out) {
	cb_wolfram_context *context = opaque;
	wf_agent_notification_list list = {0};
	notifications_ctx nc = {&context->agent, (int)limit, cursor, &list};
	wf_status status;
	cb_app_status converted;
	if (!context || !context->agent.agent) return CB_APP_INVALID;
	if (!context->network_ready) return CB_APP_NETWORK;
	status = (wf_status)cb_retry(CB_NETWORK_MAX_ATTEMPTS, cb_wolfram_transient,
	                             fetch_notifications_op, &nc, cb_network_backoff_ms,
	                             cb_network_sleep_ms);
	if (status != WF_OK) return status_from_wolfram(status);
	converted = cb_wolfram_convert_notifications(&list, out);
	wf_agent_notification_list_free(&list);
	return converted;
}

typedef struct {
	wf_bsky_agent *agent;
	const char *query;
	int limit;
	const char *cursor;
	wf_agent_actor_list *list;
} search_ctx;

static cb_retry_status search_actors_op(void *ctx) {
	search_ctx *c = ctx;
	return (cb_retry_status)wf_bsky_agent_search_actors(c->agent, c->query,
	                                                   c->limit, c->cursor,
	                                                   c->list);
}

static cb_app_status backend_search_fetch(void *opaque, const char *query,
	                                  size_t limit, cb_search_page *out) {
	cb_wolfram_context *context = opaque;
	wf_agent_actor_list list = {0};
	search_ctx sc = {&context->agent, query, (int)limit, NULL, &list};
	wf_status status;
	cb_app_status converted;
	if (!context || !context->agent.agent) return CB_APP_INVALID;
	if (!context->network_ready) return CB_APP_NETWORK;
	status = (wf_status)cb_retry(CB_NETWORK_MAX_ATTEMPTS, cb_wolfram_transient,
	                             search_actors_op, &sc, cb_network_backoff_ms,
	                             cb_network_sleep_ms);
	if (status != WF_OK) return status_from_wolfram(status);
	converted = cb_wolfram_convert_search(&list, out);
	wf_agent_actor_list_free(&list);
	return converted;
}

typedef struct {
	wf_bsky_agent *agent;
	const char *actor;
	wf_agent_profile *profile;
} profile_ctx;

static cb_retry_status fetch_profile_op(void *ctx) {
	profile_ctx *c = ctx;
	return (cb_retry_status)wf_bsky_agent_get_profile(c->agent, c->actor,
	                                                 c->profile);
}

static cb_app_status backend_profile_fetch(void *opaque, const char *actor,
	                               cb_profile_data *out) {
	cb_wolfram_context *context = opaque;
	wf_agent_profile profile = {0};
	profile_ctx pc = {&context->agent, actor, &profile};
	wf_status status;
	cb_app_status converted;
	if (!context || !context->agent.agent) return CB_APP_INVALID;
	if (!context->network_ready) return CB_APP_NETWORK;
	status = (wf_status)cb_retry(CB_NETWORK_MAX_ATTEMPTS, cb_wolfram_transient,
	                             fetch_profile_op, &pc, cb_network_backoff_ms,
	                             cb_network_sleep_ms);
	if (status != WF_OK) return status_from_wolfram(status);
	converted = cb_wolfram_convert_profile(&profile, out);
	wf_agent_profile_free(&profile);
	return converted;
}

cb_notifications_backend cb_wolfram_notifications_backend(void) {
	cb_notifications_backend backend = {backend_notifications_fetch};
	return backend;
}

cb_search_backend cb_wolfram_search_backend(void) {
	cb_search_backend backend = {backend_search_fetch};
	return backend;
}

cb_profile_backend cb_wolfram_profile_backend(void) {
	cb_profile_backend backend = {backend_profile_fetch};
	return backend;
}
