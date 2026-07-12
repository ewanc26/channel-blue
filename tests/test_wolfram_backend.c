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
	note->is_read = 0;
	note->indexed_at = copy("2024-01-01T00:00:00Z");
	note->record = cJSON_CreateObject();
	assert(note->record);
	assert(cJSON_AddStringToObject(note->record, "text", "liked your post") != NULL);

	assert(cb_wolfram_convert_notifications(&source, &page) == CB_APP_OK);
	assert(page.count == 1);
	assert(strcmp(page.notes[0].author, "alice.test") == 0);
	assert(strcmp(page.notes[0].reason, "like") == 0);
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
	source.followers_count = 7;
	source.follows_count = 8;
	source.posts_count = 9;

	assert(cb_wolfram_convert_profile(&source, &out) == CB_APP_OK);
	assert(strcmp(out.handle, "me.test") == 0);
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
	reply->post.author.display_name = copy("Carol");
	reply->post.record = cJSON_CreateObject();
	assert(reply->post.record);
	assert(cJSON_AddStringToObject(reply->post.record, "text", "reply post") != NULL);

	assert(cb_wolfram_convert_thread(&tree, &page) == CB_APP_OK);
	assert(page.count == 3);
	/* flattened in reading order: ancestor, focused post, reply */
	assert(strcmp(page.posts[0].author, "alice.test") == 0);
	assert(page.posts[0].like_count == 2);
	assert(strcmp(page.posts[1].author, "bob.test") == 0);
	assert(page.posts[1].reply_count == 1);
	assert(strcmp(page.posts[2].author, "carol.test") == 0);
	cb_timeline_page_free(&page);
	wf_agent_thread_free(&tree);
}

int main(void) {
	wf_agent_feed_list source = {0};
	cb_timeline_page page = {0};
	wf_agent_post_view *post;

	source.item_count = 1;
	source.items = calloc(1, sizeof(*source.items));
	assert(source.items);
	source.cursor = copy("cursor-2");
	post = &source.items[0].post;
	post->uri = copy("at://did:plc:alice/app.bsky.feed.post/one");
	post->cid = copy("bafycid");
	post->author.handle = copy("alice.test");
	post->author.display_name = copy("Alice");
	post->author.avatar = copy("https://cdn.example/avatar.png");
	post->record = cJSON_CreateObject();
	assert(post->record);
	assert(cJSON_AddStringToObject(post->record, "text", "Hello, Wii!") != NULL);
	post->like_count = 3;
	post->repost_count = 2;
	post->reply_count = 1;
	post->viewer.like = copy("at://like/one");

	assert(cb_wolfram_convert_feed(&source, &page) == CB_APP_OK);
	assert(page.count == 1 && strcmp(page.cursor, "cursor-2") == 0);
	assert(strcmp(page.posts[0].text, "Hello, Wii!") == 0);
	assert(strcmp(page.posts[0].display_name, "Alice") == 0);
	assert(page.posts[0].like_count == 3 && page.posts[0].liked);
	cb_timeline_page_free(&page);
	wf_agent_feed_list_free(&source);

	test_convert_notifications();
	test_convert_search();
	test_convert_profile();
	test_convert_thread();
	return 0;
}
