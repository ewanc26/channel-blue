/*
 * Header bar — screen title bar rendered below the tab bar.
 *
 * Shows a back arrow (if stack depth > 1) and the current screen title.
 */

#ifndef HEADER_BAR_H
#define HEADER_BAR_H

#include <gccore.h>
#include "../navigation/screens.h"

/* render the header bar at the given y offset */
void header_bar_render(u16 y, screen_id_t screen, u8 stack_depth);

#endif /* HEADER_BAR_H */
