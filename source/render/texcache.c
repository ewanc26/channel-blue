/*
 * Texture cache implementation.
 *
 * Fixed-size LRU cache for decoded images. When the cache is full,
 * the least recently accessed slot is evicted to make room for new entries.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>

#include "texcache.h"

static texcache_t cache;

void texcache_init(void) {
    memset(&cache, 0, sizeof(cache));
    cache.frame_counter = 1;
}

void texcache_shutdown(void) {
    for (int i = 0; i < TEXCACHE_MAX_SLOTS; i++) {
        if (cache.slots[i].in_use) {
            image_free(&cache.slots[i].image);
            cache.slots[i].in_use = 0;
        }
    }
    memset(&cache, 0, sizeof(cache));
}

decoded_image_t *texcache_get(const char *key) {
    if (!key) return NULL;

    for (int i = 0; i < TEXCACHE_MAX_SLOTS; i++) {
        if (cache.slots[i].in_use &&
            strncmp(cache.slots[i].key, key, TEXCACHE_KEY_MAX_LEN) == 0) {
            cache.slots[i].last_access = cache.frame_counter;
            return &cache.slots[i].image;
        }
    }

    return NULL;
}

/*
 * evict_lru — find and evict the least recently used slot.
 *
 * Returns the slot index, or -1 if all slots are free (shouldn't happen
 * since we only call this when the cache is full).
 */
static int evict_lru(void) {
    int lru_idx = 0;
    u32 lru_time = 0xFFFFFFFF;

    for (int i = 0; i < TEXCACHE_MAX_SLOTS; i++) {
        if (!cache.slots[i].in_use)
            return i;

        if (cache.slots[i].last_access < lru_time) {
            lru_time = cache.slots[i].last_access;
            lru_idx = i;
        }
    }

    /* evict the LRU slot */
    image_free(&cache.slots[lru_idx].image);
    cache.slots[lru_idx].in_use = 0;

    return lru_idx;
}

int texcache_put(const char *key, const decoded_image_t *img) {
    if (!key || !img || !img->texture_data) return -1;

    /* check if key already exists — replace */
    for (int i = 0; i < TEXCACHE_MAX_SLOTS; i++) {
        if (cache.slots[i].in_use &&
            strncmp(cache.slots[i].key, key, TEXCACHE_KEY_MAX_LEN) == 0) {
            image_free(&cache.slots[i].image);
            cache.slots[i].image = *img;
            cache.slots[i].last_access = cache.frame_counter;
            return 0;
        }
    }

    /* find a free slot or evict */
    int idx = -1;
    for (int i = 0; i < TEXCACHE_MAX_SLOTS; i++) {
        if (!cache.slots[i].in_use) {
            idx = i;
            break;
        }
    }

    if (idx < 0)
        idx = evict_lru();

    /* store the entry */
    strncpy(cache.slots[idx].key, key, TEXCACHE_KEY_MAX_LEN - 1);
    cache.slots[idx].key[TEXCACHE_KEY_MAX_LEN - 1] = '\0';
    cache.slots[idx].image = *img;
    cache.slots[idx].last_access = cache.frame_counter;
    cache.slots[idx].in_use = 1;

    return 0;
}

void texcache_remove(const char *key) {
    if (!key) return;

    for (int i = 0; i < TEXCACHE_MAX_SLOTS; i++) {
        if (cache.slots[i].in_use &&
            strncmp(cache.slots[i].key, key, TEXCACHE_KEY_MAX_LEN) == 0) {
            image_free(&cache.slots[i].image);
            cache.slots[i].in_use = 0;
            return;
        }
    }
}

void texcache_begin_frame(void) {
    cache.frame_counter++;
}

int texcache_render_slot(const char *key, f32 x, f32 y,
                         GXColor color) {
    decoded_image_t *img = texcache_get(key);
    if (!img) return -1;

    image_draw(x, y, img, color);
    return 0;
}
