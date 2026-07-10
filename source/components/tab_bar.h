/*
 * Tab bar — horizontal tab strip rendered at the top of the screen.
 *
 * Shows 4 tabs: Home, Search, Notifications, Profile.
 * Active tab is highlighted with a Bluesky blue underline.
 */

#ifndef TAB_BAR_H
#define TAB_BAR_H

#include <gccore.h>

/* render the tab bar at y=0, full width */
void tab_bar_render(u8 active_tab);

#endif /* TAB_BAR_H */
