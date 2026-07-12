#ifndef CB_AVATAR_H
#define CB_AVATAR_H

#include "../render/image.h"   /* f32, GXColor, decoded_image_t */
#include "timeline.h"           /* cb_timeline, cb_post */
#include "../integration/wolfram_backend.h"  /* cb_wolfram_context, fetch helper */

/*
 * avatar.h — Wii-side avatar fetch + thumbnail rendering.
 *
 * Bridges wolfram's HTTPS avatar download to the fixed texture cache and the
 * GX textured-quad drawing path. This module is Wii-only: it pulls in libogc
 * (via render/image.h) and must not be compiled into host unit tests. The
 * URL→key mapping it relies on lives in the pure module avatar_key.{c,h}.
 */

/* Longest edge (in pixels) an avatar is decoded to. Kept small per AGENTS.md
 * TMEM budget; the fixed cache stores up to TEXCACHE_MAX_SLOTS of these. */
#define CB_AVATAR_THUMB 48

/*
 * cb_avatar_ensure — make sure `url`'s decoded thumbnail is in the cache.
 *
 * If the URL is already cached, returns CB_APP_OK immediately. Otherwise it
 * fetches the bytes through wolfram (gated by network_ready, with retry on
 * transient errors), decodes the PNG/JPEG into a GX texture, and stores it in
 * the fixed texture cache keyed by cb_avatar_cache_key(url). The caller owns
 * nothing extra — the cache owns the texture until evicted.
 *
 * Returns CB_APP_OK on a cache hit or a successful fetch+decode, or an error
 * status (typically CB_APP_NETWORK / CB_APP_INVALID) on failure. Never fakes a
 * success: a fetch or decode failure surfaces as an error.
 */
cb_app_status cb_avatar_ensure(cb_wolfram_context *context, const char *url);

/*
 * cb_avatar_draw — draw the cached avatar for `url` as a `size`×`size` textured
 * quad at (x, y). Returns 0 if drawn, -1 if the URL is not (yet) cached.
 */
int cb_avatar_draw(const char *url, f32 x, f32 y, f32 size, GXColor color);

/*
 * cb_avatar_prefetch_feed — best-effort warm the cache for every post in
 * `feed`. Intended to be called from a user-driven refresh/load event (not
 * from the per-frame render path, since the fetch performs blocking network
 * I/O). Per-URL failures are non-fatal; returns the last error seen, or
 * CB_APP_OK if every avatar resolved.
 */
cb_app_status cb_avatar_prefetch_feed(const cb_timeline *feed,
                                      cb_wolfram_context *context);

#endif /* CB_AVATAR_H */
