#include "app/session_store.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* The on-disk magic is private to session_store.c; mirror it for corruption
 * tests that hand-craft a session file. */
#define MAGIC "channel-blue-session-v1"

static void write_file(const char *path, const char *contents) {
	FILE *file = fopen(path, "wb");
	assert(file);
	assert(fputs(contents, file) >= 0);
	assert(fclose(file) == 0);
}

static int temp_exists(const char *tmp_path) {
	FILE *file = fopen(tmp_path, "rb");
	if (file) {
		fclose(file);
		return 1;
	}
	return 0;
}

int main(void) {
	const char *path = "/tmp/channel-blue-session-test";
	char tmp_path[512];
	cb_session_data input = {
		"https://bsky.social", "access-token", "refresh-token",
		"alice.test", "did:plc:alice"
	};
	cb_session_data loaded;

	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

	/* ---- round trip ---- */
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
	/* atomic save must leave no temp file behind */
	assert(!temp_exists(tmp_path));

	/* ---- save rejects invalid input and writes nothing ---- */
	{
		cb_session_data bad = input;
		bad.service = "";
		assert(cb_session_save(path, &bad) == CB_SESSION_INVALID);
		/* the previously saved file is untouched */
		assert(cb_session_load(path, &loaded) == CB_SESSION_OK);
		cb_session_data_free(&loaded);
	}

	/* ---- corruption detection: bad magic ---- */
	write_file(path, "this-is-not-the-session-magic\n");
	assert(cb_session_load(path, &loaded) == CB_SESSION_INVALID);

	/* ---- corruption detection: missing required field ---- */
	write_file(path, MAGIC "\nservice=https://bsky.social\n"
	                   "accessJwt=access\nrefreshJwt=refresh\n");
	assert(cb_session_load(path, &loaded) == CB_SESSION_INVALID);

	/* ---- corruption detection: empty (zero-length) file ---- */
	write_file(path, "");
	assert(cb_session_load(path, &loaded) == CB_SESSION_INVALID);

	/* ---- invalid arguments ---- */
	assert(cb_session_load(NULL, &loaded) == CB_SESSION_INVALID);
	assert(cb_session_save(NULL, &input) == CB_SESSION_INVALID);
	assert(cb_session_clear(NULL) == CB_SESSION_INVALID);

	/* ---- a failed write must not corrupt the target nor leave a temp ---- */
	cb_session_clear(path);
	assert(mkdir(path, 0755) == 0);
	assert(cb_session_save(path, &input) == CB_SESSION_IO);
	assert(!temp_exists(tmp_path));
	/* the original target is still intact (here, still a directory) */
	assert(temp_exists(path));
	assert(rmdir(path) == 0);

	/* ---- clear is idempotent ---- */
	assert(cb_session_clear(path) == CB_SESSION_OK);
	assert(cb_session_clear(path) == CB_SESSION_OK);
	puts("session store tests passed");
	return 0;
}
