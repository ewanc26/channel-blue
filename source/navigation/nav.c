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
#include "../app/notifications.h"
#include "../app/search.h"
#include "../app/profile.h"
#include "../app/thread.h"
#include "../app/session_menu.h"
#include "../app/utf8.h"
#include "../app/media.h"
#include "../render/texcache.h"

static nav_state_t nav;
static u8 nav_prev_tab;
static cb_timeline *feed;
static cb_compose *draft;
static const cb_timeline_backend *feed_backend;
static void *feed_context;
static cb_auth *authentication;
static cb_login_form *login_form;
static const cb_auth_backend *authentication_backend;
static const char *authentication_path;
static cb_notifications *notifications;
static cb_search *search;
static cb_profile *profile;
static const cb_notifications_backend *notifications_backend;
static const cb_search_backend *search_backend;
static const cb_profile_backend *profile_backend;
static void *discovery_context;
static cb_thread *thread_ctrl;
static const cb_thread_backend *thread_backend;
static void *thread_context;
static cb_session_menu session_menu;
static int exit_requested;

static void cache_avatar(const char *url) {
	if (url && discovery_context)
		cb_avatar_ensure((cb_wolfram_context *)discovery_context, url);
}

static void cache_search_avatars(void) {
	size_t i;
	if (!search) return;
	for (i = 0; i < search->count; i++)
		cache_avatar(search->results[i].avatar_url);
}

static void notifications_loaded(void) {
	size_t i;
	if (!notifications) return;
	if (notifications_backend && notifications_backend->mark_seen)
		cb_notifications_mark_seen(notifications, notifications_backend,
		                           discovery_context);
	for (i = 0; i < notifications->count; i++)
		cache_avatar(notifications->notes[i].avatar_url);
}

static cb_app_status load_profile(const char *actor) {
	cb_app_status status;
	if (!profile || !profile_backend || !actor) return CB_APP_INVALID;
	status = cb_profile_load(profile, profile_backend, discovery_context, actor);
	if (status == CB_APP_OK) cache_avatar(profile->profile.avatar_url);
	return status;
}

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
	length = cb_utf8_prefix_bytes(text, max_chars, sizeof(line) - 1);
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
		cb_media_draw(post->media_url, 548, y + 10, 68, 64,
		              (GXColor){255, 255, 255, 255});
		draw_clipped_text(92, y + 10, post->display_name ? post->display_name : post->author,
		                  30, author);
		draw_clipped_text(92, y + 34, post->text, 54, body);
		snprintf(meta, sizeof(meta), "Replies %u   Reposts %u   Likes %u",
		         post->reply_count, post->repost_count, post->like_count);
		draw_clipped_text(92, y + 59, meta, 48, counts);
	}
}

static void render_thread(void) {
	size_t first;
	size_t end;
	size_t i;
	GXColor normal = {18, 20, 26, 255};
	GXColor selected = {22, 56, 82, 255};
	GXColor author = {235, 235, 235, 255};
	GXColor body = {185, 185, 190, 255};
	GXColor counts = {115, 150, 175, 255};

	if (!thread_ctrl || !thread_ctrl->loaded) {
		render_empty(thread_ctrl && thread_ctrl->last_status != CB_APP_OK
		             ? "Thread unavailable. Check connection and retry."
		             : "Loading thread. Press B to go back.");
		return;
	}
	if (!thread_ctrl->count) {
		render_empty("This thread is unavailable.");
		return;
	}
	first = (thread_ctrl->selected / 4) * 4;
	end = first + 4 < thread_ctrl->count ? first + 4 : thread_ctrl->count;
	for (i = first; i < end; i++) {
		const cb_post *post = &thread_ctrl->posts[i];
		f32 y = CONTENT_Y_TOP + (f32)(i - first) * 88.0f;
		char meta[72];
		draw_quad(8, y + 3, 624, 82, i == thread_ctrl->selected ? selected : normal);
		cb_avatar_draw(post->avatar_url, 18, y + 10, 64,
		               (GXColor){255, 255, 255, 255});
		cb_media_draw(post->media_url, 548, y + 10, 68, 64,
		              (GXColor){255, 255, 255, 255});
		draw_clipped_text(92, y + 10, post->display_name ? post->display_name : post->author,
		                  30, author);
		draw_clipped_text(92, y + 34, post->text, 54, body);
		snprintf(meta, sizeof(meta), "Replies %u   Reposts %u   Likes %u",
		         post->reply_count, post->repost_count, post->like_count);
		draw_clipped_text(92, y + 59, meta, 48, counts);
	}
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
	snprintf(counter, sizeof(counter), "%u / %u",
	         draft ? (unsigned)cb_utf8_count(draft->text) : 0,
	         (unsigned)CB_POST_TEXT_GRAPHEMES_MAX);
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

static void render_notifications(void) {
	size_t first;
	size_t end;
	size_t i;
	GXColor normal = {18, 20, 26, 255};
	GXColor selected = {22, 56, 82, 255};
	GXColor author = {235, 235, 235, 255};
	GXColor body = {185, 185, 190, 255};
	GXColor reason = {130, 200, 240, 255};

	if (!notifications || !notifications->loaded) {
		if (notifications && notifications->last_status == CB_APP_NETWORK)
			render_empty("Network unavailable. Press + to retry.");
		else if (notifications &&
		         notifications->last_status == CB_APP_NOT_IMPLEMENTED)
			render_empty("Notifications are not available on Wii yet.");
		else
			render_empty("No notifications. Press + to load.");
		return;
	}
	if (!notifications->count) {
		render_empty("You have no notifications.");
		return;
	}
	first = (notifications->selected / 4) * 4;
	end = first + 4 < notifications->count ? first + 4 : notifications->count;
	for (i = first; i < end; i++) {
		const cb_notification *note = &notifications->notes[i];
		f32 y = CONTENT_Y_TOP + (f32)(i - first) * 88.0f;
		char meta[96];
		draw_quad(8, y + 3, 624, 82, i == notifications->selected ? selected : normal);
		cb_avatar_draw(note->avatar_url, 18, y + 10, 64,
		               (GXColor){255, 255, 255, 255});
		snprintf(meta, sizeof(meta), "%s  %s",
		         note->display_name ? note->display_name
		                            : (note->author ? note->author : ""),
		         note->reason[0] ? note->reason : "");
		draw_clipped_text(92, y + 10, meta, 68, author);
		draw_clipped_text(92, y + 36, note->text, 68, body);
		if (!note->is_read)
			draw_clipped_text(92, y + 60, "[unread]", 12, reason);
	}
}

static void render_search(void) {
	GXColor normal = {18, 20, 26, 255};
	GXColor selected = {22, 56, 82, 255};
	GXColor author = {235, 235, 235, 255};
	GXColor body = {185, 185, 190, 255};
	GXColor field = {25, 28, 35, 255};
	f32 list_top = CONTENT_Y_TOP + 56.0f;
	size_t first;
	size_t end;
	size_t i;

	draw_quad(0, CONTENT_Y_TOP, 640, CONTENT_HEIGHT, (GXColor){10, 12, 16, 255});
	/* query field */
	draw_quad(18, CONTENT_Y_TOP + 18, 604, 34, field);
	draw_clipped_text(30, CONTENT_Y_TOP + 27,
	                  search && search->query_length ? search->query
	                                                 : "Type a name, then + to search",
	                  78, (GXColor){225, 225, 228, 255});

	if (!search || !search->loaded) {
		if (search && search->last_status == CB_APP_NETWORK)
			draw_clipped_text(28, CONTENT_Y_TOP + 92,
			                  "Network unavailable. Check connection and retry.", 76,
			                  (GXColor){150, 150, 150, 255});
		else
			draw_clipped_text(28, CONTENT_Y_TOP + 92,
			                  "Enter a search above, then press + to search.", 76,
			                  (GXColor){150, 150, 150, 255});
		return;
	}
	if (!search->count) {
		draw_clipped_text(28, CONTENT_Y_TOP + 92,
		                  "No accounts matched your search.", 76,
		                  (GXColor){150, 150, 150, 255});
		return;
	}
	first = (search->selected / 4) * 4;
	end = first + 4 < search->count ? first + 4 : search->count;
	for (i = first; i < end; i++) {
		const cb_search_result *result = &search->results[i];
		f32 y = list_top + (f32)(i - first) * 76.0f;
		char meta[96];
		draw_quad(8, y + 3, 624, 70, i == search->selected ? selected : normal);
		cb_avatar_draw(result->avatar_url, 18, y + 9, 54,
		               (GXColor){255, 255, 255, 255});
		draw_clipped_text(82, y + 10,
		                  result->display_name ? result->display_name
		                                       : result->handle, 72, author);
		snprintf(meta, sizeof(meta), "@%s", result->handle ? result->handle : "");
		draw_clipped_text(82, y + 36, meta, 68, body);
	}
}

static void render_profile(void) {
	GXColor label = {150, 150, 155, 255};
	GXColor value = {225, 225, 228, 255};
	GXColor accent = {29, 155, 240, 255};
	char counts[96];

	if (!profile || !profile->loaded) {
		if (profile && profile->last_status == CB_APP_NETWORK)
			render_empty("Network unavailable. Press + to reload.");
		else if (profile && profile->last_status == CB_APP_NOT_IMPLEMENTED)
			render_empty("Profile is not available on Wii yet.");
		else
			render_empty("Sign in to view your profile. Press + to load.");
		return;
	}
	draw_quad(0, CONTENT_Y_TOP, 640, CONTENT_HEIGHT, (GXColor){10, 12, 16, 255});
	cb_avatar_draw(profile->profile.avatar_url, 28, CONTENT_Y_TOP + 18, 72,
	               (GXColor){255, 255, 255, 255});
	draw_clipped_text(118, CONTENT_Y_TOP + 20,
	                  profile->profile.display_name ? profile->profile.display_name
	                                               : profile->profile.handle,
	                  70, accent);
	draw_clipped_text(118, CONTENT_Y_TOP + 52, profile->profile.handle, 60, label);
	if (authentication && authentication->session.did &&
	    strcmp(profile->profile.did, authentication->session.did) != 0)
		draw_clipped_text(118, CONTENT_Y_TOP + 76,
		                  profile->profile.followed ? "[Following]" : "[Not following]",
		                  22, profile->profile.followed ? accent : label);
	if (profile->profile.description)
		draw_clipped_text(28, CONTENT_Y_TOP + 108, profile->profile.description,
		                  84, value);
	snprintf(counts, sizeof(counts), "Followers %d   Following %d   Posts %d",
	         profile->profile.followers_count, profile->profile.follows_count,
	         profile->profile.posts_count);
	draw_clipped_text(28, CONTENT_Y_TOP + 164, counts, 78, value);
}

static void render_session_menu(void) {
	static const char *labels[CB_SESSION_MENU_COUNT] = {
		"Resume Channel Blue", "Sign out and clear session", "Exit to Wii Menu"
	};
	size_t i;
	draw_quad(0, CONTENT_Y_TOP, 640, CONTENT_HEIGHT, (GXColor){10, 12, 16, 255});
	if (authentication && authentication->state == CB_AUTH_READY &&
	    authentication->session.handle)
		draw_clipped_text(28, CONTENT_Y_TOP + 24,
		                  authentication->session.handle, 72,
		                  (GXColor){130, 190, 230, 255});
	for (i = 0; i < CB_SESSION_MENU_COUNT; i++) {
		f32 y = CONTENT_Y_TOP + 70.0f + (f32)i * 64.0f;
		draw_quad(22, y, 596, 50,
		          i == session_menu.selected ? (GXColor){22, 56, 82, 255}
		                                     : (GXColor){25, 28, 35, 255});
		draw_clipped_text(40, y + 15, labels[i], 72,
		                  (GXColor){225, 225, 228, 255});
	}
}

static void render_placeholder(screen_id_t screen) {
	render_empty(screen < SCREEN_COUNT ? "Not available." : "Not available.");
}

static void render_hints(screen_id_t screen) {
	const char *hints = "D-pad: Move   A: Open   -: Compose   1/2: Like/Repost";
	if (screen == SCREEN_THREAD) hints = "A: Reply   +: Follow   B: Back   1: Like   2: Repost";
	else if (screen == SCREEN_COMPOSE) hints = "Enter/A: Send   B/Esc: Cancel   USB keyboard: Type";
	else if (screen == SCREEN_LOGIN) hints = "Up/Down/Tab: Field   Enter/A: Sign in   USB keyboard: Type";
	else if (screen == SCREEN_SEARCH) hints = "Type: Search   +: Go   A: Profile   Up/Down: Pick";
	else if (screen == SCREEN_NOTIFICATIONS) hints = "Up/Down: Move   A: Open   +: More/Refresh   B: Back";
	else if (screen == SCREEN_PROFILE) hints = "1: Follow/Unfollow   +: Reload   B: Back";
	else if (screen == SCREEN_SESSION_MENU) hints = "Up/Down: Choose   A: Select   Home/B: Resume";
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
	nav_prev_tab = 0;
	exit_requested = 0;
	cb_session_menu_init(&session_menu);
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
				cb_media_prefetch_timeline(feed, (cb_wolfram_context *)feed_context);
		}
	} else {
		nav_push(SCREEN_LOGIN);
	}
}

void nav_bind_discovery(cb_notifications *notes, cb_search *search_ctrl,
	                    cb_profile *profile_ctrl,
	                    const cb_notifications_backend *notes_backend,
	                    const cb_search_backend *search_b,
	                    const cb_profile_backend *profile_b, void *context) {
	notifications = notes;
	search = search_ctrl;
	profile = profile_ctrl;
	notifications_backend = notes_backend;
	search_backend = search_b;
	profile_backend = profile_b;
	discovery_context = context;
}

void nav_bind_thread(cb_thread *thread_c, const cb_thread_backend *backend,
	                 void *context) {
	thread_ctrl = thread_c;
	thread_backend = backend;
	thread_context = context;
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
			cb_media_prefetch_timeline(feed, (cb_wolfram_context *)feed_context);
		}
	}
}

static void clear_account_views(void) {
	if (feed) {
		cb_timeline_free(feed);
		cb_timeline_init(feed);
	}
	if (draft) cb_compose_begin(draft, NULL);
	if (notifications) {
		cb_notifications_free(notifications);
		cb_notifications_init(notifications);
	}
	if (search) {
		cb_search_free(search);
		cb_search_init(search);
	}
	if (profile) {
		cb_profile_free(profile);
		cb_profile_init(profile);
	}
	if (thread_ctrl) {
		cb_thread_free(thread_ctrl);
		cb_thread_init(thread_ctrl);
	}
	if (login_form) cb_login_form_init(login_form);
}

static void activate_session_menu(void) {
	switch (cb_session_menu_selected(&session_menu)) {
	case CB_SESSION_MENU_RESUME:
		nav_pop();
		break;
	case CB_SESSION_MENU_SIGN_OUT:
		if (authentication && authentication->state == CB_AUTH_READY &&
		    authentication_backend && authentication_path) {
			cb_auth_logout(authentication, authentication_backend, feed_context,
			               authentication_path);
			clear_account_views();
			texcache_clear();
			nav_init();
			nav_push(SCREEN_LOGIN);
		} else nav_pop();
		break;
	case CB_SESSION_MENU_EXIT:
		exit_requested = 1;
		break;
	default:
		nav_pop();
		break;
	}
}

int nav_exit_requested(void) {
	return exit_requested;
}

void nav_handle_input(u32 pressed) {
	u8 tab = nav.active_tab;
	screen_id_t screen = nav.stack[tab][nav.stack_depth[tab] - 1];
	if (pressed & WPAD_BUTTON_HOME) {
		if (screen == SCREEN_SESSION_MENU) nav_pop();
		else {
			cb_session_menu_init(&session_menu);
			nav_push(SCREEN_SESSION_MENU);
		}
		return;
	}
	if (nav.stack_depth[tab] == 1) {
		if (pressed & WPAD_BUTTON_RIGHT) nav.active_tab = (tab + 1) % TAB_COUNT;
		if (pressed & WPAD_BUTTON_LEFT) nav.active_tab = (tab + TAB_COUNT - 1) % TAB_COUNT;
		if (nav.active_tab != tab) {
			/* First open of a discovery tab triggers an initial load so the
			 * user does not need to press + (search still needs a query). */
			u8 switched = nav.active_tab;
			nav_prev_tab = switched;
			if (switched == 2 && notifications && notifications_backend &&
			    !notifications->loaded && authentication &&
			    authentication->state == CB_AUTH_READY) {
				if (cb_notifications_refresh(notifications, notifications_backend,
				                             discovery_context) == CB_APP_OK)
					notifications_loaded();
			}
			else if (switched == 3 && profile && profile_backend &&
			         authentication && authentication->session.handle &&
			         authentication->session.handle[0] && authentication->session.did &&
			         (!profile->loaded ||
			          strcmp(profile->profile.did, authentication->session.did) != 0))
				load_profile(authentication->session.handle);
			return;
		}
	}
	if (screen == SCREEN_SESSION_MENU) {
		if (pressed & WPAD_BUTTON_UP) cb_session_menu_move(&session_menu, -1);
		if (pressed & WPAD_BUTTON_DOWN) cb_session_menu_move(&session_menu, 1);
		if (pressed & WPAD_BUTTON_A) activate_session_menu();
		if (pressed & WPAD_BUTTON_B) nav_pop();
	} else if (screen == SCREEN_LOGIN) {
		if (pressed & WPAD_BUTTON_UP) cb_login_form_next_field(login_form, -1);
		if (pressed & WPAD_BUTTON_DOWN) cb_login_form_next_field(login_form, 1);
		if (pressed & WPAD_BUTTON_A) submit_login();
	} else if (screen == SCREEN_COMPOSE) {
		if (pressed & WPAD_BUTTON_A && draft &&
		    cb_compose_submit(draft, feed, feed_backend, feed_context) == CB_APP_OK)
			nav_pop();
		if (pressed & WPAD_BUTTON_B) nav_pop();
	} else if (screen == SCREEN_FEED) {
		if (pressed & WPAD_BUTTON_UP) cb_timeline_move(feed, -1);
		if (pressed & WPAD_BUTTON_DOWN) cb_timeline_move(feed, 1);
		if (pressed & WPAD_BUTTON_1)
			cb_timeline_like_selected(feed, feed_backend, feed_context);
		if (pressed & WPAD_BUTTON_2)
			cb_timeline_repost_selected(feed, feed_backend, feed_context);
		if (pressed & WPAD_BUTTON_MINUS) {
			if (cb_compose_begin(draft, NULL) == CB_APP_OK)
				nav_push(SCREEN_COMPOSE);
		}
		if (pressed & WPAD_BUTTON_A && cb_timeline_selected(feed)) {
			const cb_post *picked = cb_timeline_selected(feed);
			nav_push(SCREEN_THREAD);
				if (thread_ctrl && thread_backend)
					cb_thread_load(thread_ctrl, thread_backend, thread_context,
					               picked->uri);
				if (thread_ctrl) {
					cb_avatar_prefetch_posts(thread_ctrl->posts, thread_ctrl->count,
					                         (cb_wolfram_context *)thread_context);
					cb_media_prefetch_posts(thread_ctrl->posts, thread_ctrl->count,
					                       (cb_wolfram_context *)thread_context);
				}
		}
		if (pressed & WPAD_BUTTON_PLUS && feed &&
		    feed_backend && feed_backend->fetch_timeline) {
			if (feed->has_more) cb_timeline_load_more(feed, feed_backend, feed_context);
				else cb_timeline_refresh(feed, feed_backend, feed_context);
				cb_avatar_prefetch_feed(feed, (cb_wolfram_context *)feed_context);
				cb_media_prefetch_timeline(feed, (cb_wolfram_context *)feed_context);
		}
		if (pressed & WPAD_BUTTON_B) nav_pop();
	} else if (screen == SCREEN_THREAD) {
		if (pressed & WPAD_BUTTON_UP) cb_thread_move(thread_ctrl, -1);
		if (pressed & WPAD_BUTTON_DOWN) cb_thread_move(thread_ctrl, 1);
		if (pressed & WPAD_BUTTON_1)
			cb_thread_like_selected(thread_ctrl, thread_backend, thread_context);
		if (pressed & WPAD_BUTTON_2)
			cb_thread_repost_selected(thread_ctrl, thread_backend, thread_context);
		if (pressed & WPAD_BUTTON_MINUS) {
			if (cb_compose_begin(draft, NULL) == CB_APP_OK)
				nav_push(SCREEN_COMPOSE);
		}
		if (pressed & WPAD_BUTTON_A && cb_thread_selected(thread_ctrl) &&
		    cb_compose_begin(draft, cb_thread_selected(thread_ctrl)) == CB_APP_OK) {
			nav_push(SCREEN_COMPOSE);
		}
		if (pressed & WPAD_BUTTON_PLUS && thread_ctrl && thread_backend &&
		    thread_ctrl->root_uri)
			cb_thread_follow_selected(thread_ctrl, thread_backend, thread_context);
		if (pressed & WPAD_BUTTON_B) nav_pop();
	} else if (screen == SCREEN_NOTIFICATIONS) {
		if (pressed & WPAD_BUTTON_UP) {
			cb_notifications_move(notifications, -1);
			if (cb_notifications_selected(notifications))
				cache_avatar(cb_notifications_selected(notifications)->avatar_url);
		}
		if (pressed & WPAD_BUTTON_DOWN) {
			cb_notifications_move(notifications, 1);
			if (cb_notifications_selected(notifications))
				cache_avatar(cb_notifications_selected(notifications)->avatar_url);
		}
		if (pressed & WPAD_BUTTON_A && cb_notifications_selected(notifications)) {
			const cb_notification *note = cb_notifications_selected(notifications);
			if (note->reason_subject && thread_ctrl && thread_backend) {
				nav_push(SCREEN_THREAD);
				cb_thread_load(thread_ctrl, thread_backend, thread_context,
				               note->reason_subject);
			} else if (note->author && profile && profile_backend &&
			           load_profile(note->author) == CB_APP_OK) {
				nav_push(SCREEN_PROFILE);
			}
		}
		if (pressed & WPAD_BUTTON_PLUS && notifications_backend) {
			cb_app_status status = notifications && notifications->has_more
			                     ? cb_notifications_load_more(
			                           notifications, notifications_backend,
			                           discovery_context)
			                     : cb_notifications_refresh(
			                           notifications, notifications_backend,
			                           discovery_context);
			if (status == CB_APP_OK) notifications_loaded();
		}
		if (pressed & WPAD_BUTTON_B) nav_pop();
	} else if (screen == SCREEN_SEARCH) {
		if (pressed & WPAD_BUTTON_UP) {
			cb_search_move(search, -1);
			if (cb_search_selected(search)) cache_avatar(cb_search_selected(search)->avatar_url);
		}
		if (pressed & WPAD_BUTTON_DOWN) {
			cb_search_move(search, 1);
			if (cb_search_selected(search)) cache_avatar(cb_search_selected(search)->avatar_url);
		}
		if (pressed & WPAD_BUTTON_A && cb_search_selected(search) && profile &&
		    profile_backend &&
		    load_profile(cb_search_selected(search)->did) == CB_APP_OK)
			nav_push(SCREEN_PROFILE);
		if (pressed & WPAD_BUTTON_PLUS && search_backend &&
		    cb_search_run(search, search_backend, discovery_context) == CB_APP_OK &&
		    cb_search_selected(search))
			cache_search_avatars();
		if (pressed & WPAD_BUTTON_B) nav_pop();
	} else if (screen == SCREEN_PROFILE) {
		if (pressed & WPAD_BUTTON_1 && profile && profile_backend &&
		    authentication && authentication->session.did && profile->profile.did &&
		    strcmp(authentication->session.did, profile->profile.did) != 0)
			cb_profile_toggle_follow(profile, profile_backend, discovery_context);
		if (pressed & WPAD_BUTTON_PLUS && profile_backend && profile &&
		    profile->loaded && profile->profile.did && nav.stack_depth[tab] > 1)
			load_profile(profile->profile.did);
		else if (pressed & WPAD_BUTTON_PLUS && profile_backend && authentication &&
		         authentication->session.handle && authentication->session.handle[0])
			load_profile(authentication->session.handle);
		if (pressed & WPAD_BUTTON_B) nav_pop();
	} else {
		if (pressed & WPAD_BUTTON_B) nav_pop();
	}
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
	if (nav_get_current_screen() == SCREEN_SEARCH && search) {
		if (symbol == 8 || symbol == 127) cb_search_backspace(search);
		else if (symbol == 13) {
			if (search_backend &&
			    cb_search_run(search, search_backend, discovery_context) == CB_APP_OK)
				cache_search_avatars();
		} else cb_search_insert(search, symbol);
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
	else if (current == SCREEN_NOTIFICATIONS) render_notifications();
	else if (current == SCREEN_SEARCH) render_search();
	else if (current == SCREEN_PROFILE) render_profile();
	else if (current == SCREEN_SESSION_MENU) render_session_menu();
	else render_placeholder(current);
	render_hints(current);
}

u8 nav_get_active_tab(void) { return nav.active_tab; }

screen_id_t nav_get_current_screen(void) {
	u8 tab = nav.active_tab;
	return nav.stack[tab][nav.stack_depth[tab] - 1];
}
