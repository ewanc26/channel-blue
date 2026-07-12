/*
 * Screen IDs for Channel Blue.
 *
 * Each screen has a render callback that draws into the content area
 * (below the header bar, above the control hints bar).
 */

#ifndef SCREENS_H
#define SCREENS_H

typedef enum {
    SCREEN_FEED = 0,
    SCREEN_SEARCH,
    SCREEN_NOTIFICATIONS,
    SCREEN_PROFILE,
    SCREEN_THREAD,
    SCREEN_COMPOSE,
    SCREEN_LOGIN,
    SCREEN_SESSION_MENU,
    SCREEN_COUNT,
} screen_id_t;

/* number of tabs (top-level screens reachable via D-pad L/R) */
#define TAB_COUNT 4

#endif /* SCREENS_H */
