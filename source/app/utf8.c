#include "utf8.h"

#include <string.h>

static int scalar_valid(unsigned int cp) {
	return cp <= 0x10ffffu && !(cp >= 0xd800u && cp <= 0xdfffu);
}

size_t cb_utf8_encode(unsigned int cp, char out[4]) {
	if (!out || !scalar_valid(cp)) return 0;
	if (cp <= 0x7fu) {
		out[0] = (char)cp;
		return 1;
	}
	if (cp <= 0x7ffu) {
		out[0] = (char)(0xc0u | (cp >> 6));
		out[1] = (char)(0x80u | (cp & 0x3fu));
		return 2;
	}
	if (cp <= 0xffffu) {
		out[0] = (char)(0xe0u | (cp >> 12));
		out[1] = (char)(0x80u | ((cp >> 6) & 0x3fu));
		out[2] = (char)(0x80u | (cp & 0x3fu));
		return 3;
	}
	out[0] = (char)(0xf0u | (cp >> 18));
	out[1] = (char)(0x80u | ((cp >> 12) & 0x3fu));
	out[2] = (char)(0x80u | ((cp >> 6) & 0x3fu));
	out[3] = (char)(0x80u | (cp & 0x3fu));
	return 4;
}

int cb_utf8_append(char *buffer, size_t *length, size_t capacity,
	               unsigned int cp) {
	char encoded[4];
	size_t bytes;
	if (!buffer || !length || cp < 32u || cp == 0x7fu) return 0;
	bytes = cb_utf8_encode(cp, encoded);
	if (!bytes || *length > capacity || bytes > capacity - *length) return 0;
	memcpy(buffer + *length, encoded, bytes);
	*length += bytes;
	buffer[*length] = '\0';
	return 1;
}

void cb_utf8_backspace(char *buffer, size_t *length) {
	size_t pos;
	if (!buffer || !length || !*length) return;
	pos = *length - 1;
	while (pos > 0 && ((unsigned char)buffer[pos] & 0xc0u) == 0x80u) pos--;
	*length = pos;
	buffer[pos] = '\0';
}

unsigned int cb_utf8_next(const char **cursor) {
	const unsigned char *p;
	unsigned int cp;
	unsigned int min;
	int extra;
	if (!cursor || !*cursor || !**cursor) return 0;
	p = (const unsigned char *)*cursor;
	if (p[0] < 0x80u) {
		(*cursor)++;
		return p[0];
	}
	if ((p[0] & 0xe0u) == 0xc0u) {
		cp = p[0] & 0x1fu; min = 0x80u; extra = 1;
	} else if ((p[0] & 0xf0u) == 0xe0u) {
		cp = p[0] & 0x0fu; min = 0x800u; extra = 2;
	} else if ((p[0] & 0xf8u) == 0xf0u) {
		cp = p[0] & 0x07u; min = 0x10000u; extra = 3;
	} else {
		(*cursor)++;
		return 0xfffdu;
	}
	for (int i = 1; i <= extra; i++) {
		if (!p[i] || (p[i] & 0xc0u) != 0x80u) {
			(*cursor)++;
			return 0xfffdu;
		}
		cp = (cp << 6) | (p[i] & 0x3fu);
	}
	if (cp < min || !scalar_valid(cp)) {
		(*cursor)++;
		return 0xfffdu;
	}
	*cursor += extra + 1;
	return cp;
}

size_t cb_utf8_count(const char *text) {
	size_t count = 0;
	if (!text) return 0;
	while (*text) {
		cb_utf8_next(&text);
		count++;
	}
	return count;
}

size_t cb_utf8_prefix_bytes(const char *text, size_t max_codepoints,
	                        size_t max_bytes) {
	const char *cursor = text;
	const char *accepted = text;
	size_t count = 0;
	if (!text) return 0;
	while (*cursor && count < max_codepoints) {
		const char *next = cursor;
		cb_utf8_next(&next);
		if ((size_t)(next - text) > max_bytes) break;
		accepted = next;
		cursor = next;
		count++;
	}
	return (size_t)(accepted - text);
}
