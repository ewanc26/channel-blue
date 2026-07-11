#include "app/session_store.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
	const char *path = "/tmp/channel-blue-session-test";
	cb_session_data input = {
		"https://bsky.social", "access-token", "refresh-token",
		"alice.test", "did:plc:alice"
	};
	cb_session_data loaded;

	cb_session_clear(path);
	assert(cb_session_load(path, &loaded) == CB_SESSION_NOT_FOUND);
	assert(cb_session_save(path, &input) == CB_SESSION_OK);
	assert(cb_session_load(path, &loaded) == CB_SESSION_OK);
	assert(strcmp(loaded.service, input.service) == 0);
	assert(strcmp(loaded.access_jwt, input.access_jwt) == 0);
	assert(strcmp(loaded.refresh_jwt, input.refresh_jwt) == 0);
	assert(strcmp(loaded.handle, input.handle) == 0);
	assert(strcmp(loaded.did, input.did) == 0);
	cb_session_data_free(&loaded);
	assert(cb_session_clear(path) == CB_SESSION_OK);
	assert(cb_session_clear(path) == CB_SESSION_OK);
	puts("session store tests passed");
	return 0;
}
