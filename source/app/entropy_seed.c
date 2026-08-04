#include "entropy_seed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int cb_entropy_seed_load(const char *path,
	unsigned char seed[CB_ENTROPY_SEED_SIZE]) {
	FILE *file;
	size_t count;
	long size;

	if (!path || !path[0] || !seed) return 0;
	file = fopen(path, "rb");
	if (!file) return 0;
	if (fseek(file, 0, SEEK_END) != 0 ||
	    (size = ftell(file)) != CB_ENTROPY_SEED_SIZE ||
	    fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		memset(seed, 0, CB_ENTROPY_SEED_SIZE);
		return 0;
	}
	count = fread(seed, 1, CB_ENTROPY_SEED_SIZE, file);
	if (fclose(file) != 0 || count != CB_ENTROPY_SEED_SIZE) {
		memset(seed, 0, CB_ENTROPY_SEED_SIZE);
		return 0;
	}
	return 1;
}

int cb_entropy_seed_save(const char *path,
	const unsigned char seed[CB_ENTROPY_SEED_SIZE]) {
	char *temporary;
	size_t path_length;
	FILE *file;
	int failed;

	if (!path || !path[0] || !seed) return 0;
	path_length = strlen(path);
	temporary = malloc(path_length + 5);
	if (!temporary) return 0;
	memcpy(temporary, path, path_length);
	memcpy(temporary + path_length, ".tmp", 5);
	file = fopen(temporary, "wb");
	if (!file) {
		free(temporary);
		return 0;
	}
	failed = fwrite(seed, 1, CB_ENTROPY_SEED_SIZE, file) !=
		CB_ENTROPY_SEED_SIZE;
	if (fflush(file) != 0) failed = 1;
	/*
	 * Push the bytes to the card before the rename makes them visible.
	 * fflush only clears stdio's buffer; without this barrier a power loss
	 * can land the rename while the data is still in the FAT cache, leaving
	 * a zero-length or stale seed. The next boot would then come up on the
	 * previous seed and regenerate identical key material — exactly what the
	 * rotate/save/commit cycle exists to prevent.
	 *
	 * libfat routes fsync through _FAT_fsync_r, so this is real on hardware.
	 * A failure is not treated as fatal: the data is already flushed, and a
	 * platform that cannot provide the barrier should not lose the seed
	 * entirely by refusing to save it.
	 */
	if (!failed) (void)fsync(fileno(file));
	if (fclose(file) != 0) failed = 1;
	if (!failed && rename(temporary, path) != 0) failed = 1;
	if (failed) remove(temporary);
	free(temporary);
	return !failed;
}
