#include "app/utf8.h"

#include <assert.h>
#include <string.h>

int main(void) {
	char buffer[32] = {0};
	size_t length = 0;
	const char *cursor;
	assert(cb_utf8_append(buffer, &length, sizeof(buffer) - 1, 'A'));
	assert(cb_utf8_append(buffer, &length, sizeof(buffer) - 1, 0x00e9));
	assert(cb_utf8_append(buffer, &length, sizeof(buffer) - 1, 0x1f30d));
	assert(strcmp(buffer, "A\xc3\xa9\xf0\x9f\x8c\x8d") == 0);
	assert(length == 7 && cb_utf8_count(buffer) == 3);
	cursor = buffer;
	assert(cb_utf8_next(&cursor) == 'A');
	assert(cb_utf8_next(&cursor) == 0x00e9);
	assert(cb_utf8_next(&cursor) == 0x1f30d);
	assert(cb_utf8_prefix_bytes(buffer, 2, sizeof(buffer)) == 3);
	assert(cb_utf8_prefix_bytes(buffer, 3, 6) == 3);
	cb_utf8_backspace(buffer, &length);
	assert(strcmp(buffer, "A\xc3\xa9") == 0 && length == 3);
	cb_utf8_backspace(buffer, &length);
	assert(strcmp(buffer, "A") == 0 && length == 1);
	assert(!cb_utf8_append(buffer, &length, sizeof(buffer) - 1, 0xd800));
	assert(!cb_utf8_append(buffer, &length, sizeof(buffer) - 1, '\n'));
	return 0;
}
