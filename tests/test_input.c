#include "app/input.h"

#include <assert.h>

#define CLASSIC_UP       (UINT32_C(0x0001) << 16)
#define CLASSIC_X        (UINT32_C(0x0008) << 16)
#define CLASSIC_A        (UINT32_C(0x0010) << 16)
#define CLASSIC_Y        (UINT32_C(0x0020) << 16)
#define CLASSIC_B        (UINT32_C(0x0040) << 16)
#define CLASSIC_FULL_R   (UINT32_C(0x0200) << 16)
#define CLASSIC_FULL_L   (UINT32_C(0x2000) << 16)

int main(void) {
	cb_input_repeat repeat;
	uint32_t out;
	unsigned int i;

	cb_input_repeat_init(&repeat);
	out = cb_input_translate(CB_INPUT_A | CLASSIC_X, 1, 0.0f, 0.0f, &repeat);
	assert((out & CB_INPUT_A) && (out & CB_INPUT_1));
	out = cb_input_translate(CLASSIC_A | CLASSIC_B | CLASSIC_Y, 1,
	                         0.0f, 0.0f, &repeat);
	assert((out & CB_INPUT_A) && (out & CB_INPUT_B) && (out & CB_INPUT_2));
	out = cb_input_translate(CLASSIC_FULL_R | CLASSIC_FULL_L, 1,
	                         0.0f, 0.0f, &repeat);
	assert((out & CB_INPUT_PLUS) && (out & CB_INPUT_MINUS));
	out = cb_input_translate(CLASSIC_UP, 1, 0.0f, 0.0f, &repeat);
	assert(out & CB_INPUT_UP);

	/* Analog emits immediately, pauses, then repeats at the bounded cadence. */
	cb_input_repeat_init(&repeat);
	assert(cb_input_translate(0, 1, 0.8f, 180.0f, &repeat) & CB_INPUT_DOWN);
	for (i = 0; i < 17; i++)
		assert(!(cb_input_translate(0, 1, 0.8f, 180.0f, &repeat) & CB_INPUT_DOWN));
	assert(cb_input_translate(0, 1, 0.8f, 180.0f, &repeat) & CB_INPUT_DOWN);
	for (i = 0; i < 4; i++)
		assert(!(cb_input_translate(0, 1, 0.8f, 180.0f, &repeat) & CB_INPUT_DOWN));
	assert(cb_input_translate(0, 1, 0.8f, 180.0f, &repeat) & CB_INPUT_DOWN);

	/* Returning to the dead zone resets repeat and changing direction emits. */
	assert(!cb_input_translate(0, 1, 0.2f, 180.0f, &repeat));
	assert(cb_input_translate(0, 1, 0.8f, 90.0f, &repeat) & CB_INPUT_RIGHT);
	assert(!cb_input_translate(0, 0, 0.8f, 0.0f, &repeat));
	return 0;
}
