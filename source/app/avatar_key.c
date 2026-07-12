/*
 * avatar_key.c — URL-to-cache-key mapping for avatar thumbnails.
 *
 * Pure module: uses only the C standard library so it links into host tests
 * as well as the Wii DOL. See avatar_key.h.
 */

#include "avatar_key.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* FNV-1a 64-bit, the same digest family wolfram's offline fixtures rely on
 * for stable short hashes. We only need a collision-resistant-enough
 * fingerprint to key a small fixed texture cache, not cryptographic strength. */
#define CB_FNV_OFFSET 1469598103934665603ULL
#define CB_FNV_PRIME  1099511628211ULL

void cb_avatar_cache_key(const char *url, char *out, size_t out_len) {
	uint64_t hash = CB_FNV_OFFSET;
	const unsigned char *p;
	if (!out || out_len == 0) return;
	if (!url) { out[0] = '\0'; return; }
	for (p = (const unsigned char *)url; *p; p++) {
		hash ^= (uint64_t)*p;
		hash *= CB_FNV_PRIME;
	}
	snprintf(out, out_len, "av%016llx", (unsigned long long)hash);
}
