/*
 * Tab bar — GX-rendered horizontal tab strip.
 *
 * 4 tabs evenly spaced across 640px = 160px each.
 * Active tab: Bluesky blue underline + brighter text.
 * Inactive tabs: dimmer text.
 *
 * Text is rendered as small filled rectangles (placeholder until FreeType).
 * Each tab label is represented by a series of thin vertical bars whose
 * count approximates the character count of the label.
 */

#include <gccore.h>
#include "tab_bar.h"
#include "../navigation/screens.h"
#include "../render/font.h"

/* Bluesky brand colours */
#define BLUE_R 0x1d
#define BLUE_G 0x9b
#define BLUE_B 0xf0

/* layout */
#define TAB_WIDTH  160
#define TAB_HEIGHT 40
#define UNDERLINE_H 4

/* tab label metadata */
static const char *tab_labels[TAB_COUNT] = {
    "Home",
    "Search",
    "Notifs",
    "Profile",
};

/*
 * draw_quad — draw a solid coloured rectangle via GX immediate mode.
 *
 * Position is (x, y) with y increasing downward (screen coords).
 * Vertices are emitted as a triangle list (6 indices for 2 triangles).
 */
static void draw_quad(f32 x, f32 y, f32 w, f32 h, GXColor col) {
    GX_SetChanMatColor(GX_COLOR0A0, col);
    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 6);
        GX_Position3f32(x,     y,     0.0f);
        GX_Position3f32(x + w, y,     0.0f);
        GX_Position3f32(x + w, y + h, 0.0f);
        GX_Position3f32(x,     y,     0.0f);
        GX_Position3f32(x + w, y + h, 0.0f);
        GX_Position3f32(x,     y + h, 0.0f);
    GX_End();
}

void tab_bar_render(u8 active_tab) {
    /* dark background */
    GXColor bg = {30, 30, 30, 255};
    draw_quad(0.0f, 0.0f, 640.0f, (f32)TAB_HEIGHT, bg);

    /* divider line below tabs */
    GXColor divider = {60, 60, 60, 255};
    draw_quad(0.0f, (f32)(TAB_HEIGHT - 1), 640.0f, 1.0f, divider);

    for (u8 i = 0; i < TAB_COUNT; i++) {
        f32 tab_x = (f32)i * (f32)TAB_WIDTH;
        f32 tab_center_x = tab_x + (f32)TAB_WIDTH * 0.5f;

        /* label text */
        f32 text_y = 10.0f;

        GXColor text_col;
        if (i == active_tab) {
            text_col.r = 255; text_col.g = 255; text_col.b = 255; text_col.a = 255;
        } else {
            text_col.r = 190; text_col.g = 190; text_col.b = 195; text_col.a = 255;
        }

        /* centre-align text within tab */
        int tw = font_text_width(tab_labels[i], FONT_SIZE_TAB_BAR);
        f32 text_x = tab_center_x - (f32)tw * 0.5f;
        font_draw_text(text_x, text_y, tab_labels[i], FONT_SIZE_TAB_BAR, text_col);

        /* active underline */
        if (i == active_tab) {
            GXColor underline = {BLUE_R, BLUE_G, BLUE_B, 255};
            draw_quad(tab_x + 20.0f, (f32)(TAB_HEIGHT - UNDERLINE_H),
                      (f32)TAB_WIDTH - 40.0f, (f32)UNDERLINE_H, underline);
        }
    }
}
