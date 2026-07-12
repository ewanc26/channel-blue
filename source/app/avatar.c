/*
 * avatar.c — Wii-side avatar fetch + thumbnail rendering.
 *
 * Fetches an author's avatar bytes through wolfram, decodes them with the
 * existing PNG/JPEG path, and uploads the result into the fixed texture cache
 * keyed by a hash of the URL. Drawing reuses image_draw_scaled via the cache.
 *
 * See avatar.h for the public API and ownership rules.
 */

#include "avatar.h"
#include "avatar_key.h"
#include "../integration/wolfram_backend.h"
#include "../render/texcache.h"

#include <stdlib.h>

/* ---- ensure a URL's thumbnail is cached ------------------------------- */

cb_app_status cb_avatar_ensure(cb_wolfram_context *context, const char *url) {
	unsigned char *bytes = NULL;
	size_t len = 0;
	decoded_image_t img;
	cb_app_status status;
	char key[CB_AVATAR_KEY_LEN];

	if (!context || !url) return CB_APP_INVALID;

	cb_avatar_cache_key(url, key, sizeof(key));

	/* Already resident: nothing to do (does not bump LRU here; texcache_get
	 * in cb_avatar_draw does that at draw time). */
	if (texcache_get(key)) return CB_APP_OK;

	status = cb_wolfram_fetch_avatar(context, url, &bytes, &len);
	if (status != CB_APP_OK) return status;

	if (image_decode((const u8 *)bytes, (u32)len, CB_AVATAR_THUMB,
	                 CB_AVATAR_THUMB, &img) != 0) {
		free(bytes);
		return CB_APP_INVALID;
	}
	free(bytes);

	/* texcache_put takes ownership of img.texture_data; do not image_free it
	 * on success. On failure we still own it and must release it. */
	if (texcache_put(key, &img) != 0) {
		image_free(&img);
		return CB_APP_ALLOC;
	}
	return CB_APP_OK;
}

/* ---- draw a cached avatar --------------------------------------------- */

int cb_avatar_draw(const char *url, f32 x, f32 y, f32 size, GXColor color) {
	decoded_image_t *img;
	char key[CB_AVATAR_KEY_LEN];
	if (!url) return -1;
	cb_avatar_cache_key(url, key, sizeof(key));
	img = texcache_get(key);
	if (!img) return -1;
	image_draw_scaled(x, y, size, size, img, color);
	return 0;
}

/* ---- warm the cache for a whole feed ---------------------------------- */

cb_app_status cb_avatar_prefetch_feed(const cb_timeline *feed,
                                      cb_wolfram_context *context) {
	size_t i;
	cb_app_status last = CB_APP_OK;
	if (!feed || !context) return CB_APP_INVALID;
	for (i = 0; i < feed->count; i++) {
		const char *url = feed->posts[i].avatar_url;
		cb_app_status status;
		if (!url) continue;
		status = cb_avatar_ensure(context, url);
		if (status != CB_APP_OK) last = status;
	}
	return last;
}
