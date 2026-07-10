/*
 * Navigation state machine.
 *
 * Manages:
 * - active tab index (0-3)
 * - per-tab screen stacks (push/pop)
 * - input dispatch (D-pad L/R for tabs, A to push, B to pop)
 * - rendering the full chrome (tab bar + header + content + hints)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#include "nav.h"
#include "screens.h"
#include "../components/tab_bar.h"
#include "../components/header_bar.h"

/* ---- internal state ---- */

static nav_state_t nav;

/* ---- content area placeholder renderers ---- */

/*
 * Each screen renders a placeholder coloured rectangle with label text bars.
 * These will be replaced with real content in later phases.
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

/* distinct background colours per screen for visual identification */
static const GXColor screen_bgs[SCREEN_COUNT] = {
    { 10, 10,  15, 255},  /* FEED — very dark blue-ish */
    { 12, 10,  10, 255},  /* SEARCH — very dark red-ish */
    { 10, 12,  10, 255},  /* NOTIFICATIONS — very dark green-ish */
    { 12, 10,  12, 255},  /* PROFILE — very dark purple-ish */
    { 10, 12,  12, 255},  /* THREAD — very dark cyan-ish */
};

/* placeholder text bar counts for each screen's content label */
static const u8 content_label_chars[SCREEN_COUNT] = {
    16, /* FEED — "Feed content" placeholder */
    16, /* SEARCH */
    16, /* NOTIFICATIONS */
    16, /* PROFILE */
    16, /* THREAD */
};

static void render_content_area(screen_id_t screen) {
    /* screen background */
    GXColor bg = screen_bgs[screen];
    draw_quad(0.0f, (f32)CONTENT_Y_TOP, 640.0f, (f32)CONTENT_HEIGHT, bg);

    /* placeholder label centred in content area */
    u8 chars = content_label_chars[screen];
    f32 text_w = (f32)chars * 5.0f;
    f32 text_x = 320.0f - text_w * 0.5f;
    f32 text_y = (f32)CONTENT_Y_TOP + (f32)CONTENT_HEIGHT * 0.5f - 6.0f;

    GXColor text_col = {100, 100, 100, 255};
    draw_text_bars(text_x, text_y, chars, text_col);
}

/* ---- control hints bar ---- */

/*
 * Bottom bar showing available actions.
 * Renders simple placeholder text bars for each hint.
 */
static void render_hints_bar(void) {
    u16 y = 480 - HINTS_BAR_HEIGHT;

    /* background */
    GXColor bg = {20, 20, 20, 255};
    draw_quad(0.0f, (f32)y, 640.0f, (f32)HINTS_BAR_HEIGHT, bg);

    /* top divider */
    GXColor divider = {60, 60, 60, 255};
    draw_quad(0.0f, (f32)y, 640.0f, 1.0f, divider);

    /* hint labels — 4 groups across the bar */
    GXColor hint_col = {120, 120, 120, 255};
    f32 hint_y = (f32)y + 5.0f;

    /* "A:Select" at left */
    draw_text_bars(20.0f, hint_y, 8, hint_col);
    /* "B:Back" */
    draw_text_bars(180.0f, hint_y, 6, hint_col);
    /* "D:Scroll" */
    draw_text_bars(320.0f, hint_y, 8, hint_col);
    /* "+/-:Page" */
    draw_text_bars(480.0f, hint_y, 8, hint_col);
}

/* ---- public API ---- */

void nav_init(void) {
    memset(&nav, 0, sizeof(nav));

    /* each tab starts at its root screen */
    nav.stack[0][0] = SCREEN_FEED;
    nav.stack_depth[0] = 1;

    nav.stack[1][0] = SCREEN_SEARCH;
    nav.stack_depth[1] = 1;

    nav.stack[2][0] = SCREEN_NOTIFICATIONS;
    nav.stack_depth[2] = 1;

    nav.stack[3][0] = SCREEN_PROFILE;
    nav.stack_depth[3] = 1;

    nav.active_tab = 0;
}

void nav_handle_input(u32 pressed) {
    u8 tab = nav.active_tab;

    /* tab switching — D-pad left/right */
    if (pressed & WPAD_BUTTON_RIGHT) {
        nav.active_tab = (tab + 1) % TAB_COUNT;
    }
    if (pressed & WPAD_BUTTON_LEFT) {
        nav.active_tab = (tab + TAB_COUNT - 1) % TAB_COUNT;
    }

    /* push screen — A button (placeholder: push SCREEN_THREAD onto current tab) */
    if (pressed & WPAD_BUTTON_A) {
        if (nav.stack_depth[tab] < STACK_MAX_DEPTH) {
            screen_id_t current = nav.stack[tab][nav.stack_depth[tab] - 1];
            /* push THREAD if on FEED; otherwise push PROFILE as demo */
            screen_id_t next = (current == SCREEN_FEED) ? SCREEN_THREAD : SCREEN_PROFILE;
            nav.stack[tab][nav.stack_depth[tab]] = next;
            nav.stack_depth[tab]++;
        }
    }

    /* pop screen — B button */
    if (pressed & WPAD_BUTTON_B) {
        if (nav.stack_depth[tab] > 1) {
            nav.stack_depth[tab]--;
        }
    }

    /* exit — Home button */
    if (pressed & WPAD_BUTTON_HOME) {
        exit(0);
    }
}

void nav_render(void) {
    u8 tab = nav.active_tab;
    screen_id_t current = nav.stack[tab][nav.stack_depth[tab] - 1];

    /* 1. tab bar (top 40px) */
    tab_bar_render(tab);

    /* 2. header bar (next 32px) */
    header_bar_render(TAB_BAR_HEIGHT, current, nav.stack_depth[tab]);

    /* 3. content area (remaining space) */
    render_content_area(current);

    /* 4. control hints bar (bottom 24px) */
    render_hints_bar();
}

u8 nav_get_active_tab(void) {
    return nav.active_tab;
}

screen_id_t nav_get_current_screen(void) {
    u8 tab = nav.active_tab;
    return nav.stack[tab][nav.stack_depth[tab] - 1];
}
