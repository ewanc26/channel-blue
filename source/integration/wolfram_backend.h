#ifndef CHANNEL_BLUE_WOLFRAM_BACKEND_H
#define CHANNEL_BLUE_WOLFRAM_BACKEND_H

#include "../app/auth.h"
#include "../app/timeline.h"
#include "../app/notifications.h"
#include "../app/search.h"
#include "../app/profile.h"

#include <wolfram/bsky_agent.h>

typedef struct {
	wf_bsky_agent agent;
	int network_ready;
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

#endif /* CHANNEL_BLUE_WOLFRAM_BACKEND_H */
