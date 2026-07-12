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
	status = backend->fetch_profile(context, actor, &out);
	if (status != CB_APP_OK) {
		controller->last_status = status;
		cb_profile_data_free(&out);
		return status;
	}
	if (!out.did || !out.did[0] || !out.handle || !out.handle[0]) {
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
