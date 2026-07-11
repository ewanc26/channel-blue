#include "integration/wolfram_backend.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static char *copy(const char *value) {
	char *result = malloc(strlen(value) + 1);
	assert(result);
	strcpy(result, value);
	return result;
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
	return 0;
}
