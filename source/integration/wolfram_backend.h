#ifndef CHANNEL_BLUE_WOLFRAM_BACKEND_H
#define CHANNEL_BLUE_WOLFRAM_BACKEND_H

#include "../app/auth.h"
#include "../app/timeline.h"
#include "../app/notifications.h"
#include "../app/search.h"
#include "../app/profile.h"
#include "../app/thread.h"

#include <wolfram/bsky_agent.h>
#include <wolfram/xrpc.h>

typedef struct {
	wf_bsky_agent agent;
	int network_ready;
	/* Reusable HTTPS client for fetching avatar bytes (cdn.bsky.app, etc.).
	 * Created lazily on first avatar fetch and owned by the context. The
	 * avatar CDN needs no auth, and wf_http_get uses the full URL regardless
	 * of this client's base URL, so a single shared client serves every
	 * avatar host. Freed by cb_wolfram_context_free. */
	wf_xrpc_client *avatar_client;
} cb_wolfram_context;

void cb_wolfram_context_init(cb_wolfram_context *context);
void cb_wolfram_context_free(cb_wolfram_context *context);
void cb_wolfram_context_set_network_ready(cb_wolfram_context *context,
	                                   int ready);

cb_auth_backend cb_wolfram_auth_backend(void);
cb_timeline_backend cb_wolfram_timeline_backend(void);
cb_notifications_backend cb_wolfram_notifications_backend(void);
cb_search_backend cb_wolfram_search_backend(void);
cb_profile_backend cb_wolfram_profile_backend(void);
cb_thread_backend cb_wolfram_thread_backend(void);

/* Exposed for offline fixture tests; each converts an owned Wolfram result
 * without transferring ownership from it. Free the result with the matching
 * cb_*_page_free / cb_profile_data_free. */
cb_app_status cb_wolfram_convert_feed(const wf_agent_feed_list *feed,
	                                  cb_timeline_page *out);
cb_app_status cb_wolfram_convert_notifications(
	const wf_agent_notification_list *source, cb_notifications_page *out);
cb_app_status cb_wolfram_convert_search(const wf_agent_actor_list *source,
	                                cb_search_page *out);
cb_app_status cb_wolfram_convert_profile(const wf_agent_profile *source,
	                                 cb_profile_data *out);
cb_app_status cb_wolfram_convert_thread(const wf_agent_thread *source,
	                                cb_timeline_page *out);

/* Fetch an avatar's raw image bytes over HTTPS via wolfram's transport.
 *
 * Gated by `context->network_ready` (returns CB_APP_NETWORK if not up) and
 * retried on transient errors with the same backoff as the other network
 * calls. On CB_APP_OK, `*out_bytes` points to a heap-owned buffer of
 * `*out_len` bytes (ASCII/PNG/JPEG octets) which the caller must free() with
 * free(); `*out_bytes` is left NULL on error. An empty or non-image body is
 * treated as a failure, never a silent success.
 *
 * Uses an honest error (with the network status mapped via status_from_wolfram)
 * for anything wolfram cannot satisfy — it never fabricates a successful
 * download. */
cb_app_status cb_wolfram_fetch_avatar(cb_wolfram_context *context,
	                                  const char *avatar_url,
	                                  unsigned char **out_bytes,
	                                  size_t *out_len);

#endif /* CHANNEL_BLUE_WOLFRAM_BACKEND_H */
