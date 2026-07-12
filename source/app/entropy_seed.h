#ifndef CB_ENTROPY_SEED_H
#define CB_ENTROPY_SEED_H

#include <stddef.h>

#define CB_ENTROPY_SEED_SIZE 64

int cb_entropy_seed_load(const char *path,
	unsigned char seed[CB_ENTROPY_SEED_SIZE]);
int cb_entropy_seed_save(const char *path,
	const unsigned char seed[CB_ENTROPY_SEED_SIZE]);

#endif
