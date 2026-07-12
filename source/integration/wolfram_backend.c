#include "wolfram_backend.h"

#include <stdlib.h>
#include <string.h>

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

static cb_app_status backend_login(void *opaque, const char *service,
	                               const char *identifier, const char *password,
	                               cb_session_data *out) {
	cb_wolfram_context *context = opaque;
	wf_status status;
	if (!context) return CB_APP_INVALID;
	if (!context->network_ready) return CB_APP_NETWORK;
	status = wf_bsky_agent_login(&context->agent, service, identifier, password);
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

static cb_app_status backend_fetch(void *opaque, const char *cursor, size_t limit,
	                               cb_timeline_page *out) {
	cb_wolfram_context *context = opaque;
	wf_agent_feed_list feed = {0};
	wf_status status;
	cb_app_status converted;
	if (!context || !context->agent.agent) return CB_APP_INVALID;
	if (!context->network_ready) return CB_APP_NETWORK;
	status = wf_bsky_agent_get_timeline(&context->agent, (int)limit, cursor, &feed);
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
