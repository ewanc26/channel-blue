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
#include "../app/avatar.h"
#include "../integration/wolfram_backend.h"

static nav_state_t nav;
static cb_timeline *feed;
static cb_compose *draft;
static const cb_timeline_backend *feed_backend;
static void *feed_context;
static cb_auth *authentication;
static cb_login_form *login_form;
static const cb_auth_backend *authentication_backend;
static const char *authentication_path;

static void draw_quad(f32 x, f32 y, f32 w, f32 h, GXColor col) {
	GX_SetChanMatColor(GX_COLOR0A0, col);
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
		/* Avatar thumbnail on the left; text is indented to clear it. */
		cb_avatar_draw(post->avatar_url, 18, y + 10, 64,
		               (GXColor){255, 255, 255, 255});
		draw_clipped_text(92, y + 10, post->display_name ? post->display_name : post->author,
		                  34, author);
		draw_clipped_text(92, y + 34, post->text, 66, body);
		snprintf(meta, sizeof(meta), "Replies %u   Reposts %u   Likes %u",
		         post->reply_count, post->repost_count, post->like_count);
		draw_clipped_text(92, y + 59, meta, 58, counts);
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
	cb_avatar_draw(post->avatar_url, 24, CONTENT_Y_TOP + 24, 64,
	               (GXColor){255, 255, 255, 255});
	draw_clipped_text(100, CONTENT_Y_TOP + 28,
	                  post->display_name ? post->display_name : post->author, 42, author);
	draw_clipped_text(100, CONTENT_Y_TOP + 100, post->text, 56, body);
	draw_clipped_text(24, CONTENT_Y_TOP + 112,
	                  "A: Reply   +: Follow   1: Like   2: Repost", 70,
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

static void render_login_field(f32 y, const char *label, const char *value,
	                           int selected, int secret) {
	char masked[CB_LOGIN_PASSWORD_MAX + 1];
	size_t length = value ? strlen(value) : 0;
	if (secret) {
		if (length > CB_LOGIN_PASSWORD_MAX) length = CB_LOGIN_PASSWORD_MAX;
		memset(masked, '*', length);
		masked[length] = '\0';
		value = masked;
	}
	font_draw_text(54, y, label, FONT_SIZE_HINTS,
	               selected ? (GXColor){80, 175, 235, 255}
	                        : (GXColor){150, 150, 155, 255});
	draw_quad(50, y + 24, 540, 38,
	          selected ? (GXColor){25, 55, 75, 255} : (GXColor){28, 30, 36, 255});
	draw_clipped_text(62, y + 33, value && value[0] ? value : "(empty)", 68,
	                  (GXColor){225, 225, 228, 255});
}

static void render_login(void) {
	draw_quad(0, CONTENT_Y_TOP, 640, CONTENT_HEIGHT, (GXColor){10, 12, 17, 255});
	if (!login_form) return;
	render_login_field(CONTENT_Y_TOP + 12, "PDS service", login_form->service,
	                   login_form->active_field == CB_LOGIN_SERVICE, 0);
	render_login_field(CONTENT_Y_TOP + 88, "Handle or email", login_form->identifier,
	                   login_form->active_field == CB_LOGIN_IDENTIFIER, 0);
	render_login_field(CONTENT_Y_TOP + 164, "App password", login_form->password,
	                   login_form->active_field == CB_LOGIN_PASSWORD, 1);
	font_draw_text(54, CONTENT_Y_TOP + 258,
	               "Use an app password. It is never saved to SD.",
	               FONT_SIZE_HINTS, (GXColor){140, 165, 180, 255});
	if (login_form->last_status != CB_APP_OK)
		font_draw_text(54, CONTENT_Y_TOP + 292,
		               login_form->last_status == CB_APP_CONFIGURATION
		               ? "Install a valid unique entropy.bin on the SD card."
		               : login_form->last_status == CB_APP_NOT_IMPLEMENTED
		               ? "This operation is not available on Wii yet."
		               : "Sign-in failed. Check details/network and retry.",
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
	if (screen == SCREEN_THREAD) hints = "A: Reply   +: Follow   B: Back   1: Like   2: Repost";
	else if (screen == SCREEN_COMPOSE) hints = "Enter/A: Send   B/Esc: Cancel   USB keyboard: Type";
	else if (screen == SCREEN_LOGIN) hints = "Up/Down/Tab: Field   Enter/A: Sign in   USB keyboard: Type";
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
}

void nav_bind_auth(cb_auth *auth, cb_login_form *login,
	               const cb_auth_backend *backend, const char *session_path) {
	authentication = auth;
	login_form = login;
	authentication_backend = backend;
	authentication_path = session_path;
	if (auth && auth->state == CB_AUTH_READY) {
		if (nav_get_current_screen() == SCREEN_LOGIN)
			nav_pop();
		if (feed && feed_backend && feed_backend->fetch_timeline) {
			cb_timeline_refresh(feed, feed_backend, feed_context);
			cb_avatar_prefetch_feed(feed, (cb_wolfram_context *)feed_context);
		}
	} else {
		nav_push(SCREEN_LOGIN);
	}
}

static void submit_login(void) {
	if (!login_form || !authentication || !authentication_backend ||
	    !authentication_path) return;
	if (cb_login_form_submit(login_form, authentication, authentication_backend,
	                         feed_context, authentication_path) == CB_APP_OK) {
		nav_pop();
		if (feed && feed_backend && feed_backend->fetch_timeline) {
			cb_timeline_refresh(feed, feed_backend, feed_context);
			cb_avatar_prefetch_feed(feed, (cb_wolfram_context *)feed_context);
		}
	}
}

void nav_handle_input(u32 pressed) {
	u8 tab = nav.active_tab;
	screen_id_t screen = nav.stack[tab][nav.stack_depth[tab] - 1];
	if (nav.stack_depth[tab] == 1) {
		if (pressed & WPAD_BUTTON_RIGHT) nav.active_tab = (tab + 1) % TAB_COUNT;
		if (pressed & WPAD_BUTTON_LEFT) nav.active_tab = (tab + TAB_COUNT - 1) % TAB_COUNT;
		if (nav.active_tab != tab) return;
	}
	if (screen == SCREEN_LOGIN) {
		if (pressed & WPAD_BUTTON_UP) cb_login_form_next_field(login_form, -1);
		if (pressed & WPAD_BUTTON_DOWN) cb_login_form_next_field(login_form, 1);
		if (pressed & WPAD_BUTTON_A) submit_login();
	} else if (screen == SCREEN_COMPOSE) {
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
			cb_avatar_prefetch_feed(feed, (cb_wolfram_context *)feed_context);
		}
		if (pressed & WPAD_BUTTON_PLUS && screen == SCREEN_THREAD)
			cb_timeline_follow_selected(feed, feed_backend, feed_context);
		if (pressed & WPAD_BUTTON_B) nav_pop();
	} else {
		if (pressed & WPAD_BUTTON_B) nav_pop();
	}
	if (pressed & WPAD_BUTTON_HOME) exit(0);
}

void nav_handle_key(unsigned int symbol) {
	if (nav_get_current_screen() == SCREEN_LOGIN && login_form) {
		if (symbol == 8 || symbol == 127) cb_login_form_backspace(login_form);
		else if (symbol == 9) cb_login_form_next_field(login_form, 1);
		else if (symbol == 13) {
			if (login_form->active_field == CB_LOGIN_PASSWORD) submit_login();
			else cb_login_form_next_field(login_form, 1);
		} else cb_login_form_insert(login_form, symbol);
		return;
	}
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
	else if (current == SCREEN_LOGIN) render_login();
	else render_placeholder(current);
	render_hints(current);
}

u8 nav_get_active_tab(void) { return nav.active_tab; }

screen_id_t nav_get_current_screen(void) {
	u8 tab = nav.active_tab;
	return nav.stack[tab][nav.stack_depth[tab] - 1];
}
