/*
 * Header bar — GX-rendered screen title bar.
 *
 * Shows a back arrow indicator (when stack depth > 1) on the left,
 * and the screen title as placeholder text bars in the centre.
 */

#include <gccore.h>
#include "header_bar.h"
#include "../navigation/screens.h"

/* Bluesky brand colours */
#define BLUE_R 0x1d
#define BLUE_G 0x9b
#define BLUE_B 0xf0

/* layout */
#define HEADER_WIDTH  640
#define HEADER_HEIGHT 32

/* screen titles — placeholder text bar counts */
static const u8 title_char_counts[SCREEN_COUNT] = {
    4,  /* SCREEN_FEED — "Feed" */
    6,  /* SCREEN_SEARCH — "Search" */
    6,  /* SCREEN_NOTIFICATIONS — "Notifs" */
    7,  /* SCREEN_PROFILE — "Profile" */
    6,  /* SCREEN_THREAD — "Thread" */
};

static const char *screen_title_labels[SCREEN_COUNT] = {
    "Feed",
    "Search",
    "Notifs",
    "Profile",
    "Thread",
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
 * draw_text_bars — placeholder text (same pattern as tab_bar.c).
 */
static void draw_text_bars(f32 x, f32 y, u8 chars, GXColor col) {
    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, chars * 6);
    for (u8 i = 0; i < chars; i++) {
        f32 bx = x + (f32)i * 5.0f;
        GX_Position3f32(bx,     y,      0.0f);
        GX_Position3f32(bx + 3, y,      0.0f);
        GX_Position3f32(bx + 3, y + 12, 0.0f);
        GX_Position3f32(bx,     y,      0.0f);
        GX_Position3f32(bx + 3, y + 12, 0.0f);
        GX_Position3f32(bx,     y + 12, 0.0f);
    }
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

    /* screen title — centred placeholder text */
    if (screen < SCREEN_COUNT) {
        u8 chars = title_char_counts[screen];
        f32 text_w = (f32)chars * 5.0f;
        f32 text_x = 320.0f - text_w * 0.5f;
        f32 text_y = (f32)y + 8.0f;

        GXColor text_col = {255, 255, 255, 255};
        draw_text_bars(text_x, text_y, chars, text_col);
    }
}
