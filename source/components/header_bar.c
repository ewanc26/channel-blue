/*
 * Header bar — GX-rendered screen title bar.
 *
 * Shows a back arrow indicator (when stack depth > 1) on the left,
 * and the screen title as placeholder text bars in the centre.
 */

#include <gccore.h>
#include "header_bar.h"
#include "../navigation/screens.h"
#include "../render/font.h"

/* Bluesky brand colours */
#define BLUE_R 0x1d
#define BLUE_G 0x9b
#define BLUE_B 0xf0

/* layout */
#define HEADER_WIDTH  640
#define HEADER_HEIGHT 32

/* screen titles */
static const char *screen_title_labels[SCREEN_COUNT] = {
    "Feed",
    "Search",
    "Notifs",
    "Profile",
    "Thread",
    "Compose",
};

/*
 * draw_quad — draw a solid coloured rectangle (same as tab_bar.c).
 *
 * TODO(gx): extract into a shared gx_draw.c helper to avoid duplication.
 */
static void draw_quad(f32 x, f32 y, f32 w, f32 h, GXColor col) {
    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 6);
        GX_Position3f32(x,     y,     0.0f);
        GX_Position3f32(x + w, y,     0.0f);
        GX_Position3f32(x + w, y + h, 0.0f);
        GX_Position3f32(x,     y,     0.0f);
        GX_Position3f32(x + w, y + h, 0.0f);
        GX_Position3f32(x,     y + h, 0.0f);
    GX_End();
}

/*
 * draw_back_arrow — small left-pointing arrow as placeholder.
 *
 * Drawn as a thin vertical bar + two small diagonal quads.
 * Positioned at x=8, vertically centred in the header.
 */
static void draw_back_arrow(f32 x, f32 y_center) {
    GXColor arrow = {180, 180, 180, 255};
    f32 bar_w = 3.0f;
    f32 bar_h = 14.0f;
    f32 y = y_center - bar_h * 0.5f;

    /* vertical bar of the arrow */
    draw_quad(x + 8.0f, y, bar_w, bar_h, arrow);
}

void header_bar_render(u16 y, screen_id_t screen, u8 stack_depth) {
    /* background */
    GXColor bg = {40, 40, 40, 255};
    draw_quad(0.0f, (f32)y, (f32)HEADER_WIDTH, (f32)HEADER_HEIGHT, bg);

    /* bottom divider */
    GXColor divider = {60, 60, 60, 255};
    draw_quad(0.0f, (f32)(y + HEADER_HEIGHT - 1), (f32)HEADER_WIDTH, 1.0f, divider);

    /* back arrow (only if we can go back) */
    if (stack_depth > 1) {
        draw_back_arrow(0.0f, (f32)y + (f32)HEADER_HEIGHT * 0.5f);
    }

    /* screen title — centred text */
    if (screen < SCREEN_COUNT) {
        int tw = font_text_width(screen_title_labels[screen], FONT_SIZE_HEADER);
        f32 text_x = 320.0f - (f32)tw * 0.5f;
        f32 text_y = (f32)y + 6.0f;

        GXColor text_col = {255, 255, 255, 255};
        font_draw_text(text_x, text_y, screen_title_labels[screen],
                       FONT_SIZE_HEADER, text_col);
    }
}
