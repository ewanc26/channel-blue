#include "app/avatar_key.h"

#include <assert.h>
#include <string.h>

int main(void) {
	char a[CB_AVATAR_KEY_LEN];
	char b[CB_AVATAR_KEY_LEN];
	char c[CB_AVATAR_KEY_LEN];

	/* Same URL maps to the same key (stable cache key). */
	cb_avatar_cache_key("https://cdn.bsky.app/img/abc.png", a, sizeof(a));
	cb_avatar_cache_key("https://cdn.bsky.app/img/abc.png", b, sizeof(b));
	assert(strcmp(a, b) == 0);
	assert(strncmp(a, "av", 2) == 0);
	assert(strlen(a) == 18);

	/* Different URLs map to different keys. */
	cb_avatar_cache_key("https://cdn.bsky.app/img/xyz.png", c, sizeof(c));
	assert(strcmp(a, c) != 0);

	/* NULL URL yields an empty key and never overruns. */
	cb_avatar_cache_key(NULL, a, sizeof(a));
	assert(a[0] == '\0');

	/* Truncated output is still NUL-terminated. */
	cb_avatar_cache_key("https://cdn.bsky.app/img/abc.png", b, 6);
	assert(strlen(b) < 6 && b[5] == '\0');

	return 0;
}
