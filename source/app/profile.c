#include "profile.h"

#include <stdlib.h>
#include <string.h>

void cb_profile_data_free(cb_profile_data *profile) {
	if (!profile) return;
	free(profile->did);
	free(profile->handle);
	free(profile->display_name);
	free(profile->description);
	free(profile->avatar_url);
	free(profile->following_uri);
	memset(profile, 0, sizeof(*profile));
}

void cb_profile_init(cb_profile *controller) {
	if (controller) memset(controller, 0, sizeof(*controller));
}

void cb_profile_free(cb_profile *controller) {
	if (!controller) return;
	cb_profile_data_free(&controller->profile);
	memset(controller, 0, sizeof(*controller));
}

cb_app_status cb_profile_load(cb_profile *controller,
	                         const cb_profile_backend *backend, void *context,
	                         const char *actor) {
	cb_profile_data out = {0};
	cb_app_status status;

	if (!controller || !backend || !backend->fetch_profile || !actor || !actor[0])
		return CB_APP_INVALID;
	/* Do not retain a previously loaded account when replacing it. */
	cb_profile_free(controller);
	status = backend->fetch_profile(context, actor, &out);
	if (status != CB_APP_OK) {
		controller->last_status = status;
		cb_profile_data_free(&out);
		return status;
	}
	if (!out.did || !out.did[0] || !out.handle || !out.handle[0] ||
	    out.followed != (out.following_uri != NULL)) {
		controller->last_status = CB_APP_INVALID;
		cb_profile_data_free(&out);
		return CB_APP_INVALID;
	}
	cb_profile_data_free(&controller->profile);
	controller->profile = out;
	controller->loaded = 1;
	controller->last_status = CB_APP_OK;
	return CB_APP_OK;
}

cb_app_status cb_profile_toggle_follow(cb_profile *controller,
	                                  const cb_profile_backend *backend,
	                                  void *context) {
	int was_followed;
	if (!controller || !controller->loaded || !backend ||
	    !backend->toggle_follow) return CB_APP_INVALID;
	was_followed = controller->profile.followed;
	controller->last_status = backend->toggle_follow(context, &controller->profile);
	if (controller->last_status == CB_APP_OK &&
	    controller->profile.followed != was_followed) {
		if (controller->profile.followed) controller->profile.followers_count++;
		else if (controller->profile.followers_count > 0)
			controller->profile.followers_count--;
	}
	return controller->last_status;
}
