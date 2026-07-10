/*
 * Texture cache — fixed-size pool for decoded image thumbnails.
 *
 * Caches decoded_image_t entries indexed by a string key (typically a URL
 * hash or DID). LRU eviction when the cache is full.
 *
 * Memory budget: each slot holds a decoded_image_t with up to 48×48 RGBA8
 * texture data (~32 KB per slot). With 8 slots, total budget is ~256 KB.
 *
 * Usage:
 *   1. Call texcache_init() once at startup.
 *   2. Call texcache_get() to retrieve a cached image (returns NULL if miss).
 *   3. On cache miss, decode the image and call texcache_put() to store it.
 *   4. Call texcache_render() to draw all visible cached images.
 *   5. Call texcache_shutdown() to free all cached textures.
 */

#ifndef CB_TEXCACHE_H
#define CB_TEXCACHE_H

#include <gccore.h>
#include "image.h"

/* cache configuration */
#define TEXCACHE_MAX_SLOTS   8
#define TEXCACHE_KEY_MAX_LEN 32

/* a single cache slot */
typedef struct {
    char             key[TEXCACHE_KEY_MAX_LEN];
    decoded_image_t  image;
    u32              last_access;  /* frame counter for LRU eviction */
    u8               in_use;       /* 1 if slot is occupied */
} texcache_slot_t;

/* cache state */
typedef struct {
    texcache_slot_t slots[TEXCACHE_MAX_SLOTS];
    u32             frame_counter;  /* incremented each frame */
} texcache_t;

/*
 * texcache_init — initialize the texture cache.
 *
 * Must be called after image_init() but before any get/put calls.
 */
void texcache_init(void);

/*
 * texcache_shutdown — free all cached textures and reset the cache.
 */
void texcache_shutdown(void);

/*
 * texcache_get — retrieve a cached image by key.
 *
 * Returns pointer to the decoded_image_t if found, NULL on cache miss.
 * Updates the LRU access timestamp.
 */
decoded_image_t *texcache_get(const char *key);

/*
 * texcache_put — store a decoded image in the cache.
 *
 * If the key already exists, the old entry is replaced.
 * If the cache is full, the least recently used entry is evicted.
 * Takes ownership of img->texture_data (do not free after calling).
 * Returns 0 on success, -1 on failure.
 */
int texcache_put(const char *key, const decoded_image_t *img);

/*
 * texcache_remove — remove a specific entry from the cache.
 */
void texcache_remove(const char *key);

/*
 * texcache_begin_frame — increment frame counter for LRU tracking.
 *
 * Call once per frame before rendering.
 */
void texcache_begin_frame(void);

/*
 * texcache_render_slot — draw a cached image at (x, y).
 *
 * Returns 0 if the image was drawn, -1 if not found.
 */
int texcache_render_slot(const char *key, f32 x, f32 y,
                         GXColor color);

#endif /* CB_TEXCACHE_H */
