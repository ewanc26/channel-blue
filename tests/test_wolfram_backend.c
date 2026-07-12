#include "integration/wolfram_backend.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <wolfram/actor_typed.h>
#include <wolfram/thread_typed.h>

static char *copy(const char *value) {
	char *result = malloc(strlen(value) + 1);
	assert(result);
	strcpy(result, value);
	return result;
}

static void test_convert_notifications(void) {
	wf_agent_notification_list source = {0};
	cb_notifications_page page = {0};
	wf_agent_notification *note;

	source.notification_count = 1;
	source.notifications = calloc(1, sizeof(*source.notifications));
	assert(source.notifications);
	note = &source.notifications[0];
	note->uri = copy("at://did:plc:alice/app.bsky.notification.receive/1");
	note->cid = copy("bafycid");
	note->author.handle = copy("alice.test");
	note->author.display_name = copy("Alice");
	note->author.avatar = copy("https://cdn.example/a.png");
	note->reason = copy("like");
	note->reason_subject = copy("at://did:plc:me/app.bsky.feed.post/subject");
	note->is_read = 0;
	note->indexed_at = copy("2024-01-01T00:00:00Z");
	note->record = cJSON_CreateObject();
	assert(note->record);
	assert(cJSON_AddStringToObject(note->record, "text", "liked your post") != NULL);

	assert(cb_wolfram_convert_notifications(&source, &page) == CB_APP_OK);
	assert(page.count == 1);
	assert(strcmp(page.notes[0].author, "alice.test") == 0);
	assert(strcmp(page.notes[0].reason, "like") == 0);
	assert(strcmp(page.notes[0].reason_subject,
	              "at://did:plc:me/app.bsky.feed.post/subject") == 0);
	assert(strcmp(page.notes[0].text, "liked your post") == 0);
	assert(page.notes[0].is_read == 0);
	cb_notifications_page_free(&page);
	wf_agent_notification_list_free(&source);
}

static void test_convert_search(void) {
	wf_agent_actor_list source = {0};
	cb_search_page page = {0};

	source.actor_count = 1;
	source.actors = calloc(1, sizeof(*source.actors));
	assert(source.actors);
	source.actors[0].did = copy("did:plc:alice");
	source.actors[0].handle = copy("alice.test");
	source.actors[0].display_name = copy("Alice");
	source.actors[0].avatar = copy("https://cdn.example/a.png");

	assert(cb_wolfram_convert_search(&source, &page) == CB_APP_OK);
	assert(page.count == 1);
	assert(strcmp(page.results[0].handle, "alice.test") == 0);
	assert(strcmp(page.results[0].display_name, "Alice") == 0);
	cb_search_page_free(&page);
	wf_agent_actor_list_free(&source);
}

static void test_convert_profile(void) {
	wf_agent_profile source = {0};
	cb_profile_data out = {0};

	source.did = copy("did:plc:me");
	source.handle = copy("me.test");
	source.display_name = copy("Me");
	source.description = copy("hello");
	source.avatar_cid = copy("https://cdn.example/profile.png");
	source.following = copy("at://did:plc:me/app.bsky.graph.follow/alice");
	source.followers_count = 7;
	source.follows_count = 8;
	source.posts_count = 9;

	assert(cb_wolfram_convert_profile(&source, &out) == CB_APP_OK);
	assert(strcmp(out.handle, "me.test") == 0);
	assert(strcmp(out.avatar_url, "https://cdn.example/profile.png") == 0);
	assert(out.followed && out.following_uri &&
	       strstr(out.following_uri, "app.bsky.graph.follow/alice"));
	assert(out.followers_count == 7 && out.posts_count == 9);
	cb_profile_data_free(&out);
	wf_agent_profile_free(&source);
}

static void test_convert_thread(void) {
	wf_agent_thread tree = {0};
	cb_timeline_page page = {0};
	wf_agent_thread_node *parent;
	wf_agent_thread_node *reply;

	/* ancestor */
	tree.root.parent = parent = calloc(1, sizeof(*parent));
	assert(parent);
	parent->kind = WF_AGENT_THREAD_KIND_POST;
	parent->post.uri = copy("at://did:plc:alice/app.bsky.feed.post/parent");
	parent->post.cid = copy("cid-p");
	parent->post.author.handle = copy("alice.test");
	parent->post.author.did = copy("did:plc:alice");
	parent->post.author.display_name = copy("Alice");
	parent->post.record = cJSON_CreateObject();
	assert(parent->post.record);
	assert(cJSON_AddStringToObject(parent->post.record, "text", "parent post") != NULL);
	parent->post.like_count = 2;

	/* focused post */
	tree.root.kind = WF_AGENT_THREAD_KIND_POST;
	tree.root.post.uri = copy("at://did:plc:bob/app.bsky.feed.post/root");
	tree.root.post.cid = copy("cid-r");
	tree.root.post.author.handle = copy("bob.test");
	tree.root.post.author.did = copy("did:plc:bob");
	tree.root.post.author.display_name = copy("Bob");
	tree.root.post.record = cJSON_CreateObject();
	assert(tree.root.post.record);
	assert(cJSON_AddStringToObject(tree.root.post.record, "text", "root post") != NULL);
	tree.root.post.reply_count = 1;

	/* a reply */
	tree.root.replies = reply = calloc(1, sizeof(*reply));
	assert(reply);
	tree.root.replies_count = 1;
	reply->kind = WF_AGENT_THREAD_KIND_POST;
	reply->post.uri = copy("at://did:plc:carol/app.bsky.feed.post/reply");
	reply->post.cid = copy("cid-c");
	reply->post.author.handle = copy("carol.test");
	reply->post.author.did = copy("did:plc:carol");
	reply->post.author.display_name = copy("Carol");
	reply->post.record = cJSON_CreateObject();
	reply->post.viewer_like = copy("at://did:plc:me/app.bsky.feed.like/thread-like");
	reply->post.viewer_repost = copy("at://did:plc:me/app.bsky.feed.repost/thread-repost");
	assert(reply->post.record);
	assert(cJSON_AddStringToObject(reply->post.record, "text", "reply post") != NULL);
	{
		cJSON *reply_ref = cJSON_CreateObject();
		cJSON *root_ref = cJSON_CreateObject();
		assert(reply_ref && root_ref);
		assert(cJSON_AddStringToObject(root_ref, "uri", tree.root.post.uri));
		assert(cJSON_AddStringToObject(root_ref, "cid", tree.root.post.cid));
		assert(cJSON_AddItemToObject(reply_ref, "root", root_ref));
		assert(cJSON_AddItemToObject(reply->post.record, "reply", reply_ref));
	}

	assert(cb_wolfram_convert_thread(&tree, &page) == CB_APP_OK);
	assert(page.count == 3);
	/* flattened in reading order: ancestor, focused post, reply */
	assert(strcmp(page.posts[0].author, "alice.test") == 0);
	assert(strcmp(page.posts[0].author_did, "did:plc:alice") == 0);
	assert(page.posts[0].like_count == 2);
	assert(strcmp(page.posts[1].author, "bob.test") == 0);
	assert(strcmp(page.posts[1].author_did, "did:plc:bob") == 0);
	assert(page.posts[1].reply_count == 1);
	assert(strcmp(page.posts[2].author, "carol.test") == 0);
	assert(strcmp(page.posts[2].author_did, "did:plc:carol") == 0);
	assert(strcmp(page.posts[2].root_uri,
	              "at://did:plc:bob/app.bsky.feed.post/root") == 0);
	assert(strcmp(page.posts[2].root_cid, "cid-r") == 0);
	assert(page.posts[2].liked && page.posts[2].like_uri &&
	       strstr(page.posts[2].like_uri, "thread-like"));
	assert(page.posts[2].reposted && page.posts[2].repost_uri &&
	       strstr(page.posts[2].repost_uri, "thread-repost"));
	cb_timeline_page_free(&page);
	wf_agent_thread_free(&tree);
}

int main(void) {
	cb_timeline_backend timeline_backend = cb_wolfram_timeline_backend();
	cb_notifications_backend notifications_backend = cb_wolfram_notifications_backend();
	cb_search_backend search_backend = cb_wolfram_search_backend();
	cb_profile_backend profile_backend = cb_wolfram_profile_backend();
	cb_thread_backend thread_backend = cb_wolfram_thread_backend();
	cb_auth_backend auth_backend = cb_wolfram_auth_backend();
	wf_agent_feed_list source = {0};
	cb_timeline_page page = {0};
	wf_agent_post_view *post;
	cb_notifications_page notes = {0};
	cb_search_page search = {0};
	cb_profile_data profile = {0};
	wf_agent_profile empty_profile = {0};
	assert(timeline_backend.fetch_timeline(NULL, NULL, 1, &page) == CB_APP_INVALID);
	assert(notifications_backend.fetch_notifications(NULL, NULL, 1, &notes) == CB_APP_INVALID);
	assert(search_backend.search_actors(NULL, "x", 1, &search) == CB_APP_INVALID);
	assert(profile_backend.fetch_profile(NULL, "did:plc:x", &profile) == CB_APP_INVALID);
	assert(cb_wolfram_convert_profile(&empty_profile, NULL) == CB_APP_INVALID);
	assert(thread_backend.fetch_thread(NULL, "at://x", 1, &page) == CB_APP_INVALID);
	assert(auth_backend.login(NULL, "https://bsky.social", "x", "y", NULL) == CB_APP_INVALID);

	source.item_count = 1;
	source.items = calloc(1, sizeof(*source.items));
	assert(source.items);
	source.cursor = copy("cursor-2");
	post = &source.items[0].post;
	post->uri = copy("at://did:plc:alice/app.bsky.feed.post/one");
	post->cid = copy("bafycid");
	post->author.handle = copy("alice.test");
	post->author.did = copy("did:plc:alice");
	post->author.display_name = copy("Alice");
	post->author.avatar = copy("https://cdn.example/avatar.png");
	post->record = cJSON_CreateObject();
	assert(post->record);
	assert(cJSON_AddStringToObject(post->record, "text", "Hello, Wii!") != NULL);
	post->embed = cJSON_CreateObject();
	assert(post->embed);
	assert(cJSON_AddStringToObject(post->embed, "$type",
	                              "app.bsky.embed.images#view") != NULL);
	{
		cJSON *images = cJSON_CreateArray();
		cJSON *image = cJSON_CreateObject();
		assert(images && image);
		assert(cJSON_AddStringToObject(image, "thumb",
	                               "https://cdn.example/post-thumb.jpg") != NULL);
		assert(cJSON_AddItemToArray(images, image));
		assert(cJSON_AddItemToObject(post->embed, "images", images));
	}
	post->like_count = 3;
	post->repost_count = 2;
	post->reply_count = 1;
	post->viewer.like = copy("at://like/one");
	post->viewer.repost = copy("at://repost/one");

	assert(cb_wolfram_convert_feed(&source, &page) == CB_APP_OK);
	assert(page.count == 1 && strcmp(page.cursor, "cursor-2") == 0);
	assert(strcmp(page.posts[0].text, "Hello, Wii!") == 0);
	assert(strcmp(page.posts[0].media_url,
	              "https://cdn.example/post-thumb.jpg") == 0);
	assert(strcmp(page.posts[0].display_name, "Alice") == 0);
	assert(strcmp(page.posts[0].author_did, "did:plc:alice") == 0);
	assert(strcmp(page.posts[0].root_uri, post->uri) == 0);
	assert(strcmp(page.posts[0].root_cid, post->cid) == 0);
	assert(page.posts[0].like_count == 3 && page.posts[0].liked);
	assert(strcmp(page.posts[0].like_uri, "at://like/one") == 0);
	assert(page.posts[0].reposted &&
	       strcmp(page.posts[0].repost_uri, "at://repost/one") == 0);
	cb_timeline_page_free(&page);
	cJSON_Delete(post->embed);
	post->embed = cJSON_CreateObject();
	assert(post->embed);
	assert(cJSON_AddStringToObject(post->embed, "$type",
	                              "app.bsky.embed.external#view") != NULL);
	assert(cJSON_AddStringToObject(post->embed, "thumb",
	                              "https://cdn.example/external.jpg") != NULL);
	assert(cb_wolfram_convert_feed(&source, &page) == CB_APP_OK);
	assert(strcmp(page.posts[0].media_url,
	              "https://cdn.example/external.jpg") == 0);
	cb_timeline_page_free(&page);
	wf_agent_feed_list_free(&source);

	test_convert_notifications();
	test_convert_search();
	test_convert_profile();
	test_convert_thread();
	return 0;
}
