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

static nav_state_t nav;
static cb_timeline *feed;
static cb_compose *draft;
static const cb_timeline_backend *feed_backend;
static void *feed_context;

static void draw_quad(f32 x, f32 y, f32 w, f32 h, GXColor col) {
	(void)col;
	GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 6);
		GX_Position3f32(x, y, 0.0f);
		GX_Position3f32(x + w, y, 0.0f);
		GX_Position3f32(x + w, y + h, 0.0f);
		GX_Position3f32(x, y, 0.0f);
		GX_Position3f32(x + w, y + h, 0.0f);
		GX_Position3f32(x, y + h, 0.0f);
	GX_End();
}

static void draw_clipped_text(f32 x, f32 y, const char *text, size_t max_chars,
	                          GXColor color) {
	char line[96];
	size_t length;
	if (!text) return;
	length = strlen(text);
	if (length > max_chars) length = max_chars;
	if (length >= sizeof(line)) length = sizeof(line) - 1;
	memcpy(line, text, length);
	line[length] = '\0';
	font_draw_text(x, y, line, FONT_SIZE_HINTS, color);
}

static void nav_push(screen_id_t screen) {
	u8 tab = nav.active_tab;
	if (nav.stack_depth[tab] >= STACK_MAX_DEPTH) return;
	nav.stack[tab][nav.stack_depth[tab]++] = screen;
}

static void nav_pop(void) {
	u8 tab = nav.active_tab;
	if (nav.stack_depth[tab] > 1) nav.stack_depth[tab]--;
}

static void render_empty(const char *message) {
	GXColor text = {150, 150, 150, 255};
	draw_quad(0, CONTENT_Y_TOP, 640, CONTENT_HEIGHT, (GXColor){10, 10, 15, 255});
	draw_clipped_text(28, CONTENT_Y_TOP + 42, message, 78, text);
}

static void render_feed(void) {
	size_t first;
	size_t end;
	size_t i;
	GXColor normal = {18, 20, 26, 255};
	GXColor selected = {22, 56, 82, 255};
	GXColor author = {235, 235, 235, 255};
	GXColor body = {185, 185, 190, 255};
	GXColor counts = {115, 150, 175, 255};

	if (!feed || !feed->count) {
		render_empty(feed_backend && feed_backend->fetch_timeline
			? "No posts. Press + to refresh."
			: "Live feed unavailable until the Wii HTTPS backend is installed.");
		return;
	}
	first = (feed->selected / 4) * 4;
	end = first + 4 < feed->count ? first + 4 : feed->count;
	for (i = first; i < end; i++) {
		const cb_post *post = &feed->posts[i];
		f32 y = CONTENT_Y_TOP + (f32)(i - first) * 88.0f;
		char meta[72];
		draw_quad(8, y + 3, 624, 82, i == feed->selected ? selected : normal);
		draw_clipped_text(20, y + 10, post->display_name ? post->display_name : post->author,
		                  46, author);
		draw_clipped_text(20, y + 34, post->text, 78, body);
		snprintf(meta, sizeof(meta), "Replies %u   Reposts %u   Likes %u",
		         post->reply_count, post->repost_count, post->like_count);
		draw_clipped_text(20, y + 59, meta, 70, counts);
	}
}

static void render_thread(void) {
	const cb_post *post = cb_timeline_selected(feed);
	GXColor author = {245, 245, 245, 255};
	GXColor body = {195, 195, 200, 255};
	draw_quad(0, CONTENT_Y_TOP, 640, CONTENT_HEIGHT, (GXColor){10, 12, 16, 255});
	if (!post) {
		render_empty("This post is no longer available.");
		return;
	}
	draw_clipped_text(24, CONTENT_Y_TOP + 24,
	                  post->display_name ? post->display_name : post->author, 60, author);
	draw_clipped_text(24, CONTENT_Y_TOP + 64, post->text, 82, body);
	draw_clipped_text(24, CONTENT_Y_TOP + 112, "A: Reply   1: Like   2: Repost", 70,
	                  (GXColor){100, 165, 210, 255});
}

static void render_compose(void) {
	char counter[24];
	draw_quad(0, CONTENT_Y_TOP, 640, CONTENT_HEIGHT, (GXColor){12, 14, 19, 255});
	font_draw_text(22, CONTENT_Y_TOP + 18,
	               draft && draft->replying ? "Replying to selected post" : "New post",
	               FONT_SIZE_HINTS, (GXColor){110, 175, 220, 255});
	draw_quad(18, CONTENT_Y_TOP + 48, 604, 172, (GXColor){25, 28, 35, 255});
	draw_clipped_text(30, CONTENT_Y_TOP + 66,
	                  draft && draft->length ? draft->text : "Type with a USB keyboard...",
	                  82, (GXColor){225, 225, 228, 255});
	snprintf(counter, sizeof(counter), "%u / %u", draft ? (unsigned)draft->length : 0,
	         (unsigned)CB_POST_TEXT_MAX);
	font_draw_text(520, CONTENT_Y_TOP + 228, counter, FONT_SIZE_HINTS,
	               (GXColor){130, 130, 135, 255});
	if (draft && draft->last_status != CB_APP_OK)
		font_draw_text(22, CONTENT_Y_TOP + 270,
		               "Send failed; draft kept. Check network and retry.",
		               FONT_SIZE_HINTS, (GXColor){230, 120, 110, 255});
}

static void render_placeholder(screen_id_t screen) {
	static const char *messages[] = {
		"", "Search is not part of the first live-feed milestone.",
		"Notifications are not loaded yet.", "Sign in to view your profile."
	};
	render_empty(screen < 4 ? messages[screen] : "Not available.");
}

static void render_hints(screen_id_t screen) {
	const char *hints = "D-pad: Move   A: Open   -: Compose   1/2: Like/Repost";
	if (screen == SCREEN_THREAD) hints = "A: Reply   B: Back   1: Like   2: Repost";
	else if (screen == SCREEN_COMPOSE) hints = "Enter/A: Send   B/Esc: Cancel   USB keyboard: Type";
	draw_quad(0, 480 - HINTS_BAR_HEIGHT, 640, HINTS_BAR_HEIGHT,
	          (GXColor){20, 20, 20, 255});
	draw_clipped_text(14, 480 - HINTS_BAR_HEIGHT + 4, hints, 80,
	                  (GXColor){130, 130, 130, 255});
}

void nav_init(void) {
	memset(&nav, 0, sizeof(nav));
	nav.stack[0][0] = SCREEN_FEED;
	nav.stack[1][0] = SCREEN_SEARCH;
	nav.stack[2][0] = SCREEN_NOTIFICATIONS;
	nav.stack[3][0] = SCREEN_PROFILE;
	nav.stack_depth[0] = nav.stack_depth[1] = 1;
	nav.stack_depth[2] = nav.stack_depth[3] = 1;
}

void nav_bind_timeline(cb_timeline *timeline, cb_compose *compose,
	                   const cb_timeline_backend *backend, void *context) {
	feed = timeline;
	draft = compose;
	feed_backend = backend;
	feed_context = context;
	if (feed && backend && backend->fetch_timeline)
		cb_timeline_refresh(feed, backend, context);
}

void nav_handle_input(u32 pressed) {
	u8 tab = nav.active_tab;
	screen_id_t screen = nav.stack[tab][nav.stack_depth[tab] - 1];
	if (nav.stack_depth[tab] == 1) {
		if (pressed & WPAD_BUTTON_RIGHT) nav.active_tab = (tab + 1) % TAB_COUNT;
		if (pressed & WPAD_BUTTON_LEFT) nav.active_tab = (tab + TAB_COUNT - 1) % TAB_COUNT;
		if (nav.active_tab != tab) return;
	}
	if (screen == SCREEN_COMPOSE) {
		if (pressed & WPAD_BUTTON_A && draft &&
		    cb_compose_submit(draft, feed, feed_backend, feed_context) == CB_APP_OK)
			nav_pop();
		if (pressed & WPAD_BUTTON_B) nav_pop();
	} else if (screen == SCREEN_FEED || screen == SCREEN_THREAD) {
		if (pressed & WPAD_BUTTON_UP) cb_timeline_move(feed, -1);
		if (pressed & WPAD_BUTTON_DOWN) cb_timeline_move(feed, 1);
		if (pressed & WPAD_BUTTON_1)
			cb_timeline_like_selected(feed, feed_backend, feed_context);
		if (pressed & WPAD_BUTTON_2)
			cb_timeline_repost_selected(feed, feed_backend, feed_context);
		if (pressed & WPAD_BUTTON_MINUS) {
			cb_compose_init(draft, 0);
			nav_push(SCREEN_COMPOSE);
		}
		if (pressed & WPAD_BUTTON_A && cb_timeline_selected(feed)) {
			if (screen == SCREEN_FEED) nav_push(SCREEN_THREAD);
			else {
				cb_compose_init(draft, 1);
				nav_push(SCREEN_COMPOSE);
			}
		}
		if (pressed & WPAD_BUTTON_PLUS && screen == SCREEN_FEED && feed &&
		    feed_backend && feed_backend->fetch_timeline) {
			if (feed->has_more) cb_timeline_load_more(feed, feed_backend, feed_context);
			else cb_timeline_refresh(feed, feed_backend, feed_context);
		}
		if (pressed & WPAD_BUTTON_B) nav_pop();
	} else {
		if (pressed & WPAD_BUTTON_B) nav_pop();
	}
	if (pressed & WPAD_BUTTON_HOME) exit(0);
}

void nav_handle_key(unsigned int symbol) {
	if (nav_get_current_screen() != SCREEN_COMPOSE || !draft) return;
	if (symbol == 8 || symbol == 127) cb_compose_backspace(draft);
	else if (symbol == 13) {
		if (cb_compose_submit(draft, feed, feed_backend, feed_context) == CB_APP_OK)
			nav_pop();
	} else if (symbol == 27) nav_pop();
	else cb_compose_insert(draft, symbol);
}

void nav_render(void) {
	u8 tab = nav.active_tab;
	screen_id_t current = nav.stack[tab][nav.stack_depth[tab] - 1];
	tab_bar_render(tab);
	header_bar_render(TAB_BAR_HEIGHT, current, nav.stack_depth[tab]);
	if (current == SCREEN_FEED) render_feed();
	else if (current == SCREEN_THREAD) render_thread();
	else if (current == SCREEN_COMPOSE) render_compose();
	else render_placeholder(current);
	render_hints(current);
}

u8 nav_get_active_tab(void) { return nav.active_tab; }

screen_id_t nav_get_current_screen(void) {
	u8 tab = nav.active_tab;
	return nav.stack[tab][nav.stack_depth[tab] - 1];
}
