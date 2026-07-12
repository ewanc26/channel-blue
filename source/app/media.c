#include "media.h"

#include "../render/texcache.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void media_key(const char *url, char out[24]) {
	uint64_t hash = UINT64_C(1469598103934665603);
	const unsigned char *p;
	if (!url) { out[0] = '\0'; return; }
	for (p = (const unsigned char *)url; *p; p++) {
		hash ^= (uint64_t)*p;
		hash *= UINT64_C(1099511628211);
	}
	snprintf(out, 24, "md%016llx", (unsigned long long)hash);
}

cb_app_status cb_media_ensure(cb_wolfram_context *context, const char *url) {
	unsigned char *bytes = NULL;
	size_t length = 0;
	decoded_image_t image = {0};
	char key[24];
	cb_app_status status;
	if (!context || !url || !url[0]) return CB_APP_INVALID;
	media_key(url, key);
	if (texcache_get(key)) return CB_APP_OK;
	status = cb_wolfram_fetch_avatar(context, url, &bytes, &length);
	if (status != CB_APP_OK) return status;
	if (image_decode(bytes, (u32)length, CB_MEDIA_MAX_WIDTH,
	                 CB_MEDIA_MAX_HEIGHT, &image) != 0) {
		free(bytes);
		return CB_APP_INVALID;
	}
	free(bytes);
	if (texcache_put(key, &image) != 0) {
		image_free(&image);
		return CB_APP_ALLOC;
	}
	return CB_APP_OK;
}

int cb_media_draw(const char *url, f32 x, f32 y, f32 width, f32 height,
                  GXColor color) {
	char key[24];
	decoded_image_t *image;
	f32 scale;
	f32 draw_width;
	f32 draw_height;
	if (!url || !url[0]) return -1;
	if (width <= 0.0f || height <= 0.0f) return -1;
	media_key(url, key);
	image = texcache_get(key);
	if (!image || !image->image_width || !image->image_height) return -1;
	scale = width / (f32)image->image_width;
	if (height / (f32)image->image_height < scale)
		scale = height / (f32)image->image_height;
	draw_width = image->image_width * scale;
	draw_height = image->image_height * scale;
	image_draw_scaled(x + (width - draw_width) * 0.5f,
	                  y + (height - draw_height) * 0.5f,
	                  draw_width, draw_height, image, color);
	return 0;
}

cb_app_status cb_media_prefetch_timeline(const cb_timeline *timeline,
                                         cb_wolfram_context *context) {
	if (!timeline || !context) return CB_APP_INVALID;
	return cb_media_prefetch_posts(timeline->posts, timeline->count, context);
}

cb_app_status cb_media_prefetch_posts(const cb_post *posts, size_t count,
                                      cb_wolfram_context *context) {
	cb_app_status last = CB_APP_OK;
	size_t i;
	if (!posts || !context) return CB_APP_INVALID;
	for (i = 0; i < count; i++) {
		cb_app_status status;
		if (!posts[i].media_url) continue;
		status = cb_media_ensure(context, posts[i].media_url);
		if (status != CB_APP_OK) last = status;
	}
	return last;
}
