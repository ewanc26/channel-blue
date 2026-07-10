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
#include "../render/font.h"

/* ---- internal state ---- */

static nav_state_t nav;

/* ---- content area placeholder renderers ---- */

/*
 * Each screen renders a placeholder coloured rectangle with a label.
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

/* distinct background colours per screen for visual identification */
static const GXColor screen_bgs[SCREEN_COUNT] = {
    { 10, 10,  15, 255},  /* FEED — very dark blue-ish */
    { 12, 10,  10, 255},  /* SEARCH — very dark red-ish */
    { 10, 12,  10, 255},  /* NOTIFICATIONS — very dark green-ish */
    { 12, 10,  12, 255},  /* PROFILE — very dark purple-ish */
    { 10, 12,  12, 255},  /* THREAD — very dark cyan-ish */
};

/* content area labels */
static const char *content_labels[SCREEN_COUNT] = {
    "Feed content",       /* SCREEN_FEED */
    "Search content",     /* SCREEN_SEARCH */
    "Notifications",      /* SCREEN_NOTIFICATIONS */
    "Profile content",    /* SCREEN_PROFILE */
    "Thread content",     /* SCREEN_THREAD */
};

static void render_content_area(screen_id_t screen) {
    /* screen background */
    GXColor bg = screen_bgs[screen];
    draw_quad(0.0f, (f32)CONTENT_Y_TOP, 640.0f, (f32)CONTENT_HEIGHT, bg);

    /* label centred in content area */
    const char *label = content_labels[screen];
    int tw = font_text_width(label, FONT_SIZE_HEADER);
    f32 text_x = 320.0f - (f32)tw * 0.5f;
    f32 text_y = (f32)CONTENT_Y_TOP + (f32)CONTENT_HEIGHT * 0.5f - 9.0f;

    GXColor text_col = {100, 100, 100, 255};
    font_draw_text(text_x, text_y, label, FONT_SIZE_HEADER, text_col);
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

    /* hint labels */
    GXColor hint_col = {120, 120, 120, 255};
    f32 hint_y = (f32)y + 4.0f;

    /* "A: Select" at left */
    font_draw_text(20.0f, hint_y, "A: Select", FONT_SIZE_HINTS, hint_col);
    /* "B: Back" */
    font_draw_text(180.0f, hint_y, "B: Back", FONT_SIZE_HINTS, hint_col);
    /* "D-pad: Scroll" */
    font_draw_text(320.0f, hint_y, "D-pad: Scroll", FONT_SIZE_HINTS, hint_col);
    /* "+/-: Page" */
    font_draw_text(500.0f, hint_y, "+/-: Page", FONT_SIZE_HINTS, hint_col);
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
