#include "input.h"

#include <string.h>

#define CLASSIC_UP       (UINT32_C(0x0001) << 16)
#define CLASSIC_LEFT     (UINT32_C(0x0002) << 16)
#define CLASSIC_X        (UINT32_C(0x0008) << 16)
#define CLASSIC_A        (UINT32_C(0x0010) << 16)
#define CLASSIC_Y        (UINT32_C(0x0020) << 16)
#define CLASSIC_B        (UINT32_C(0x0040) << 16)
#define CLASSIC_FULL_R   (UINT32_C(0x0200) << 16)
#define CLASSIC_PLUS     (UINT32_C(0x0400) << 16)
#define CLASSIC_HOME     (UINT32_C(0x0800) << 16)
#define CLASSIC_MINUS    (UINT32_C(0x1000) << 16)
#define CLASSIC_FULL_L   (UINT32_C(0x2000) << 16)
#define CLASSIC_DOWN     (UINT32_C(0x4000) << 16)
#define CLASSIC_RIGHT    (UINT32_C(0x8000) << 16)

#define ANALOG_THRESHOLD 0.55f
#define REPEAT_DELAY 18u
#define REPEAT_RATE 5u

void cb_input_repeat_init(cb_input_repeat *state) {
	if (state) memset(state, 0, sizeof(*state));
}

static int analog_direction(float magnitude, float angle) {
	if (magnitude < ANALOG_THRESHOLD) return 0;
	if (angle < 45.0f || angle >= 315.0f) return (int)CB_INPUT_UP;
	if (angle < 135.0f) return (int)CB_INPUT_RIGHT;
	if (angle < 225.0f) return (int)CB_INPUT_DOWN;
	return (int)CB_INPUT_LEFT;
}

static int should_emit_analog(cb_input_repeat *state, int direction) {
	if (!state || !direction) {
		if (state) {
			state->analog_direction = 0;
			state->held_frames = 0;
		}
		return 0;
	}
	if (state->analog_direction != direction) {
		state->analog_direction = direction;
		state->held_frames = 0;
		return 1;
	}
	state->held_frames++;
	return state->held_frames == REPEAT_DELAY ||
	       (state->held_frames > REPEAT_DELAY &&
	        (state->held_frames - REPEAT_DELAY) % REPEAT_RATE == 0);
}

uint32_t cb_input_translate(uint32_t raw_down, int classic_connected,
	                        float classic_mag, float classic_angle,
	                        cb_input_repeat *repeat) {
	uint32_t out = raw_down & UINT32_C(0x1f9f);
	int analog = 0;
	if (!classic_connected) {
		should_emit_analog(repeat, 0);
		return out;
	}
	if (raw_down & CLASSIC_UP) out |= CB_INPUT_UP;
	if (raw_down & CLASSIC_DOWN) out |= CB_INPUT_DOWN;
	if (raw_down & CLASSIC_LEFT) out |= CB_INPUT_LEFT;
	if (raw_down & CLASSIC_RIGHT) out |= CB_INPUT_RIGHT;
	if (raw_down & CLASSIC_A) out |= CB_INPUT_A;
	if (raw_down & CLASSIC_B) out |= CB_INPUT_B;
	if (raw_down & CLASSIC_X) out |= CB_INPUT_1;
	if (raw_down & CLASSIC_Y) out |= CB_INPUT_2;
	if (raw_down & (CLASSIC_PLUS | CLASSIC_FULL_R)) out |= CB_INPUT_PLUS;
	if (raw_down & (CLASSIC_MINUS | CLASSIC_FULL_L)) out |= CB_INPUT_MINUS;
	if (raw_down & CLASSIC_HOME) out |= CB_INPUT_HOME;

	if (!(out & (CB_INPUT_UP | CB_INPUT_DOWN | CB_INPUT_LEFT | CB_INPUT_RIGHT)))
		analog = analog_direction(classic_mag, classic_angle);
	if (should_emit_analog(repeat, analog)) out |= (uint32_t)analog;
	return out;
}
