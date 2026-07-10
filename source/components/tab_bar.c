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

/* approximate character widths for placeholder text bars */
static const u8 tab_label_widths[TAB_COUNT] = {
    4,  /* Home */
    5,  /* Search */
    5,  /* Notifs */
    7,  /* Profile */
};

/*
 * draw_quad — draw a solid coloured rectangle via GX immediate mode.
 *
 * Position is (x, y) with y increasing downward (screen coords).
 * Vertices are emitted as a triangle list (6 indices for 2 triangles).
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
 * draw_text_bars — placeholder text rendered as a series of thin vertical bars.
 *
 * Each bar is 3px wide with 2px gap, giving a rough "word" shape.
 * Total width = chars * 5.
 */
static void draw_text_bars(f32 x, f32 y, u8 chars, GXColor col) {
    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, chars * 6);
    for (u8 i = 0; i < chars; i++) {
        f32 bx = x + (f32)i * 5.0f;
        GX_Position3f32(bx,     y,      0.0f);
        GX_Position3f32(bx + 3, y,      0.0f);
        GX_Position3f32(bx + 3, y + 14, 0.0f);
        GX_Position3f32(bx,     y,      0.0f);
        GX_Position3f32(bx + 3, y + 14, 0.0f);
        GX_Position3f32(bx,     y + 14, 0.0f);
    }
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

        /* label text (placeholder bars) */
        u8 chars = tab_label_widths[i];
        f32 text_w = (f32)chars * 5.0f;
        f32 text_x = tab_center_x - text_w * 0.5f;
        f32 text_y = 10.0f;

        GXColor text_col;
        if (i == active_tab) {
            text_col.r = 255; text_col.g = 255; text_col.b = 255; text_col.a = 255;
        } else {
            text_col.r = 140; text_col.g = 140; text_col.b = 140; text_col.a = 255;
        }
        draw_text_bars(text_x, text_y, chars, text_col);

        /* active underline */
        if (i == active_tab) {
            GXColor underline = {BLUE_R, BLUE_G, BLUE_B, 255};
            draw_quad(tab_x + 20.0f, (f32)(TAB_HEIGHT - UNDERLINE_H),
                      (f32)TAB_WIDTH - 40.0f, (f32)UNDERLINE_H, underline);
        }
    }
}
