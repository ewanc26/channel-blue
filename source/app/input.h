#ifndef CHANNEL_BLUE_INPUT_H
#define CHANNEL_BLUE_INPUT_H

#include <stdint.h>

/* Canonical masks intentionally match libogc's WPAD_BUTTON_* values so the
 * translated result can be passed directly to nav_handle_input. */
#define CB_INPUT_2      UINT32_C(0x0001)
#define CB_INPUT_1      UINT32_C(0x0002)
#define CB_INPUT_B      UINT32_C(0x0004)
#define CB_INPUT_A      UINT32_C(0x0008)
#define CB_INPUT_MINUS  UINT32_C(0x0010)
#define CB_INPUT_HOME   UINT32_C(0x0080)
#define CB_INPUT_LEFT   UINT32_C(0x0100)
#define CB_INPUT_RIGHT  UINT32_C(0x0200)
#define CB_INPUT_DOWN   UINT32_C(0x0400)
#define CB_INPUT_UP     UINT32_C(0x0800)
#define CB_INPUT_PLUS   UINT32_C(0x1000)

typedef struct {
	int analog_direction;
	unsigned int held_frames;
} cb_input_repeat;

void cb_input_repeat_init(cb_input_repeat *state);

/* Translate one frame of WPAD buttons and optional Classic left-stick state.
 * `classic_mag` is 0..1 and `classic_angle` uses wiiuse's convention:
 * 0=up, 90=right, 180=down, 270=left. Analog directions repeat after a short
 * delay; discrete button-down events are never repeated here. */
uint32_t cb_input_translate(uint32_t raw_down, int classic_connected,
	                        float classic_mag, float classic_angle,
	                        cb_input_repeat *repeat);

#endif
