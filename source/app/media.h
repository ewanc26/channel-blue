#ifndef CB_MEDIA_H
#define CB_MEDIA_H

#include "timeline.h"
#include "../integration/wolfram_backend.h"
#include "../render/image.h"

/* Preview dimensions are deliberately below the 640x480 Wii display while
 * preserving enough detail to identify an attached image. */
#define CB_MEDIA_MAX_WIDTH 160
#define CB_MEDIA_MAX_HEIGHT 96

cb_app_status cb_media_ensure(cb_wolfram_context *context, const char *url);
int cb_media_draw(const char *url, f32 x, f32 y, f32 width, f32 height,
                  GXColor color);
cb_app_status cb_media_prefetch_timeline(const cb_timeline *timeline,
                                         cb_wolfram_context *context);
cb_app_status cb_media_prefetch_posts(const cb_post *posts, size_t count,
                                      cb_wolfram_context *context);

#endif
