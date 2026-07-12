#ifndef CHANNEL_BLUE_UTF8_H
#define CHANNEL_BLUE_UTF8_H

#include <stddef.h>

/* Encode a Unicode scalar value. Returns 1..4, or 0 for an invalid scalar. */
size_t cb_utf8_encode(unsigned int codepoint, char out[4]);

/* Append one printable scalar to a bounded NUL-terminated buffer. `length`
 * counts bytes excluding NUL and is updated only on success. */
int cb_utf8_append(char *buffer, size_t *length, size_t capacity,
	               unsigned int codepoint);

/* Delete one complete trailing UTF-8 sequence. Malformed trailing bytes are
 * removed conservatively until the remaining buffer is a valid prefix. */
void cb_utf8_backspace(char *buffer, size_t *length);

/* Decode the next scalar. Invalid input consumes one byte and returns U+FFFD. */
unsigned int cb_utf8_next(const char **cursor);

size_t cb_utf8_count(const char *text);

/* Largest byte prefix containing at most max_codepoints and max_bytes without
 * splitting a UTF-8 sequence. */
size_t cb_utf8_prefix_bytes(const char *text, size_t max_codepoints,
	                        size_t max_bytes);

#endif
