#ifndef CB_AVATAR_KEY_H
#define CB_AVATAR_KEY_H

#include <stddef.h>

/*
 * avatar_key.h — URL-to-cache-key mapping for avatar thumbnails.
 *
 * Pure, host-compilable module (no libogc / GX dependencies) so the key
 * derivation can be unit-tested without Wii hardware. The rest of the avatar
 * pipeline (network fetch + GX upload) lives in avatar.c, which is Wii-only.
 */

/* Length of a rendered cache key, including the NUL terminator. Must fit
 * within TEXCACHE_KEY_MAX_LEN (32) in texcache.h. */
#define CB_AVATAR_KEY_LEN 24

/*
 * cb_avatar_cache_key — map an avatar URL to a stable, fixed-width cache key.
 *
 * The fixed texture cache is indexed by a short string key (it cannot store
 * full URLs), so each avatar URL is hashed to an "av" + 16-hex-digit key
 * (~18 chars). The same URL always yields the same key, so a cached texture
 * is reused across timeline rows and the post/thread view.
 *
 * `out` receives a NUL-terminated key; at most `out_len` bytes are written
 * (including the terminator). Safe to call with a NULL `url` or `out`: the
 * key is left empty.
 */
void cb_avatar_cache_key(const char *url, char *out, size_t out_len);

#endif /* CB_AVATAR_KEY_H */
