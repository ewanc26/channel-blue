#ifndef CHANNEL_BLUE_WOLFRAM_BACKEND_H
#define CHANNEL_BLUE_WOLFRAM_BACKEND_H

#include "../app/auth.h"
#include "../app/timeline.h"

#include <wolfram/bsky_agent.h>

typedef struct {
	wf_bsky_agent agent;
} cb_wolfram_context;

void cb_wolfram_context_init(cb_wolfram_context *context);
void cb_wolfram_context_free(cb_wolfram_context *context);

cb_auth_backend cb_wolfram_auth_backend(void);
cb_timeline_backend cb_wolfram_timeline_backend(void);

/* Exposed for offline fixture tests; converts an owned Wolfram feed without
 * transferring ownership from it. Free the result with cb_timeline_page_free. */
cb_app_status cb_wolfram_convert_feed(const wf_agent_feed_list *feed,
	                                  cb_timeline_page *out);

#endif /* CHANNEL_BLUE_WOLFRAM_BACKEND_H */
