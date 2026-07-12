#include "app/profile.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int loads;
	int toggles;
	int fail;
} fake_context;

static char *copy(const char *value) {
	char *result = malloc(strlen(value) + 1);
	assert(result);
	strcpy(result, value);
	return result;
}

static cb_app_status toggle_follow(void *opaque, cb_profile_data *profile) {
	fake_context *context = opaque;
	context->toggles++;
	if (context->fail) return CB_APP_NETWORK;
	if (profile->followed) {
		free(profile->following_uri);
		profile->following_uri = NULL;
		profile->followed = 0;
	} else {
		profile->following_uri = copy("at://did:plc:me/app.bsky.graph.follow/one");
		profile->followed = 1;
	}
	return CB_APP_OK;
}

static cb_app_status fetch_profile(void *opaque, const char *actor,
	                           cb_profile_data *out) {
	fake_context *context = opaque;
	context->loads++;
	assert(strcmp(actor, "me.test") == 0);
	if (context->fail) return CB_APP_NETWORK;
	out->did = copy("did:plc:me");
	out->handle = copy("me.test");
	out->display_name = copy("Me");
	out->description = copy("Wii bluesky user");
	out->followers_count = 12;
	out->follows_count = 34;
	out->posts_count = 56;
	return CB_APP_OK;
}

int main(void) {
	fake_context context = {0};
	cb_profile controller;
	cb_profile_backend backend = {fetch_profile, toggle_follow};

	cb_profile_init(&controller);
	assert(cb_profile_load(&controller, &backend, &context, "me.test") == CB_APP_OK);
	assert(controller.loaded && context.loads == 1);
	assert(strcmp(controller.profile.handle, "me.test") == 0);
	assert(controller.profile.followers_count == 12);
	assert(controller.profile.posts_count == 56);
	assert(cb_profile_toggle_follow(&controller, &backend, &context) == CB_APP_OK);
	assert(controller.profile.followed && controller.profile.followers_count == 13);
	assert(cb_profile_toggle_follow(&controller, &backend, &context) == CB_APP_OK);
	assert(!controller.profile.followed && controller.profile.followers_count == 12);
	context.fail = 1;
	assert(cb_profile_toggle_follow(&controller, &backend, &context) ==
	       CB_APP_NETWORK);
	assert(!controller.profile.followed && controller.profile.followers_count == 12);
	context.fail = 0;
	/* reload replaces the previous profile */
	assert(cb_profile_load(&controller, &backend, &context, "me.test") == CB_APP_OK);
	assert(context.loads == 2);
	context.fail = 1;
	assert(cb_profile_load(&controller, &backend, &context, "me.test") == CB_APP_NETWORK);
	assert(!controller.loaded && controller.last_status == CB_APP_NETWORK);
	assert(cb_profile_load(&controller, &backend, &context, "") == CB_APP_INVALID);
	cb_profile_free(&controller);
	return 0;
}
