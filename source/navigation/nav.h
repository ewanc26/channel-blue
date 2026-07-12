/*
 * Navigation API — tab switching and screen stack push/pop.
 *
 * The nav system owns the top-level state: which tab is active,
 * which screen stack is visible, and which tab bar / header to render.
 */

#ifndef NAV_H
#define NAV_H

#include <gccore.h>
#include "screens.h"
#include "../app/compose.h"
#include "../app/login.h"
#include "../app/notifications.h"
#include "../app/search.h"
#include "../app/profile.h"

/* layout constants (pixels) */
#define TAB_BAR_HEIGHT    40
#define HEADER_BAR_HEIGHT 32
#define HINTS_BAR_HEIGHT  24
#define SCREEN_WIDTH      640

/* content area = screen height minus chrome */
#define CONTENT_Y_TOP     (TAB_BAR_HEIGHT + HEADER_BAR_HEIGHT)
#define CONTENT_Y_BOTTOM  (480 - HINTS_BAR_HEIGHT)
#define CONTENT_HEIGHT    (CONTENT_Y_BOTTOM - CONTENT_Y_TOP)

/* max depth of screen stack per tab */
#define STACK_MAX_DEPTH   8

/* navigation state — module-internal, exposed for render helpers */
typedef struct {
    u8 active_tab;
    u8 stack_depth[TAB_COUNT];
    screen_id_t stack[TAB_COUNT][STACK_MAX_DEPTH];
} nav_state_t;

/* initialise navigation to default state (all tabs at root) */
void nav_init(void);

/* Bind the application controller and its transport adapter. The pointed-to
 * objects must outlive the navigation loop. */
void nav_bind_timeline(cb_timeline *timeline, cb_compose *compose,
                       const cb_timeline_backend *backend, void *context);

/* Bind authentication after nav_bind_timeline. Signed-out users are routed to
 * the login screen; resumed users load the feed immediately. */
void nav_bind_auth(cb_auth *auth, cb_login_form *login,
                    const cb_auth_backend *backend, const char *session_path);

/* Bind the discovery controllers (notifications / search / profile) and their
 * transport adapters. The pointed-to objects must outlive the navigation loop.
 * The context is the shared wolfram context already bound to the timeline. */
void nav_bind_discovery(cb_notifications *notes, cb_search *search_ctrl,
                        cb_profile *profile_ctrl,
                        const cb_notifications_backend *notes_backend,
                        const cb_search_backend *search_backend,
                        const cb_profile_backend *profile_backend,
                        void *context);

/* handle one frame of input; called before render */
void nav_handle_input(u32 pressed);

/* Feed one translated USB keyboard symbol into the compose screen. */
void nav_handle_key(unsigned int symbol);

/* render the full navigation chrome + current screen content */
void nav_render(void);

/* read-only accessor for the active tab (used by tab_bar) */
u8 nav_get_active_tab(void);

/* read-only accessor for current screen on the active tab */
screen_id_t nav_get_current_screen(void);

#endif /* NAV_H */
