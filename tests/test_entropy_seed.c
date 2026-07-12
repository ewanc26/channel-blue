#include "app/entropy_seed.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
	const char *path = "/tmp/channel-blue-entropy-test.bin";
	unsigned char expected[CB_ENTROPY_SEED_SIZE];
	unsigned char actual[CB_ENTROPY_SEED_SIZE];

	for (size_t i = 0; i < sizeof(expected); i++) expected[i] = (unsigned char)i;
	remove(path);
	assert(!cb_entropy_seed_load(path, actual));
	assert(cb_entropy_seed_save(path, expected));
	assert(cb_entropy_seed_load(path, actual));
	assert(memcmp(expected, actual, sizeof(expected)) == 0);

	FILE *file = fopen(path, "ab");
	assert(file);
	fputc(0, file);
	fclose(file);
	assert(!cb_entropy_seed_load(path, actual));
	remove(path);
	puts("entropy seed tests passed");
	return 0;
}
