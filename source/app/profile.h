#ifndef CHANNEL_BLUE_PROFILE_H
#define CHANNEL_BLUE_PROFILE_H

#include <stddef.h>

#include "timeline.h"

typedef struct {
	char *did;
	char *handle;
	char *display_name;
	char *description;
	/* Profile-view avatar URL returned by the PDS. */
	char *avatar_url;
	char *following_uri;  /* viewer's follow record URI; NULL when not followed */
	int followed;
	int followers_count;
	int follows_count;
	int posts_count;
} cb_profile_data;

typedef struct {
	cb_app_status (*fetch_profile)(void *context, const char *actor,
	                               cb_profile_data *out);
	cb_app_status (*toggle_follow)(void *context, cb_profile_data *profile);
} cb_profile_backend;

typedef struct {
	cb_profile_data profile;
	int loaded;
	cb_app_status last_status;
} cb_profile;

void cb_profile_data_free(cb_profile_data *profile);
void cb_profile_init(cb_profile *controller);
void cb_profile_free(cb_profile *controller);

/* Load the profile for `actor` (a handle or DID). Frees any previously loaded
 * profile before populating the controller. */
cb_app_status cb_profile_load(cb_profile *controller,
	                         const cb_profile_backend *backend, void *context,
	                         const char *actor);
cb_app_status cb_profile_toggle_follow(cb_profile *controller,
	                                  const cb_profile_backend *backend,
	                                  void *context);

#endif /* CHANNEL_BLUE_PROFILE_H */
