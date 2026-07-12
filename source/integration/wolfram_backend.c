#include "wolfram_backend.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
	if (context) {
		if (context->avatar_client)
			wf_xrpc_client_free(context->avatar_client);
		wf_bsky_agent_free(&context->agent);
	}
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

/* ------------------------------------------------------------------ */
/* Avatar image fetch                                                  */
/* ------------------------------------------------------------------ */

/* Avatars are served from the Bluesky CDN (cdn.bsky.app) and peers. The fetch
 * client never attaches auth, and wf_http_get ignores the client's base URL
 * (it is given a complete URL), so any HTTPS origin works through one client. */
#define CB_AVATAR_CLIENT_BASE "https://cdn.bsky.app"

typedef struct {
	wf_xrpc_client *client;
	const char *url;
	wf_response resp;   /* owned by the op only on a successful attempt */
	int captured;
} avatar_fetch_ctx;

static cb_retry_status avatar_fetch_op(void *ctx) {
	avatar_fetch_ctx *c = ctx;
	wf_response resp = {0};
	wf_status status = wf_http_get(c->client, c->url, &resp);
	if (status != WF_OK) return (cb_retry_status)status;
	/* An empty body is not a usable avatar image, even on a 2xx. Surface it
	 * as a generic failure so the retry primitive can still back off once. */
	if (!resp.body || resp.body_len == 0) {
		wf_response_free(&resp);
		return (cb_retry_status)WF_ERR_UNKNOWN;
	}
	c->resp = resp;
	c->captured = 1;
	return (cb_retry_status)WF_OK;
}

cb_app_status cb_wolfram_fetch_avatar(cb_wolfram_context *context,
	                                  const char *avatar_url,
	                                  unsigned char **out_bytes,
	                                  size_t *out_len) {
	avatar_fetch_ctx fc = {NULL, avatar_url, {0}, 0};
	wf_status status;
	unsigned char *buf;

	if (!context || !avatar_url || !out_bytes || !out_len)
		return CB_APP_INVALID;
	if (!context->network_ready) return CB_APP_NETWORK;

	if (!context->avatar_client) {
		context->avatar_client =
			wf_xrpc_client_new(CB_AVATAR_CLIENT_BASE);
		if (!context->avatar_client) return CB_APP_ALLOC;
	}
	fc.client = context->avatar_client;

	status = (wf_status)cb_retry(CB_NETWORK_MAX_ATTEMPTS, cb_wolfram_transient,
	                             avatar_fetch_op, &fc, cb_network_backoff_ms,
	                             cb_network_sleep_ms);
	if (status != WF_OK || !fc.captured) return status_from_wolfram(status);

	buf = malloc(fc.resp.body_len ? fc.resp.body_len : 1);
	if (!buf) {
		wf_response_free(&fc.resp);
		return CB_APP_ALLOC;
	}
	memcpy(buf, fc.resp.body, fc.resp.body_len);
	*out_len = fc.resp.body_len;
	wf_response_free(&fc.resp);

	*out_bytes = buf;
	return CB_APP_OK;
}
