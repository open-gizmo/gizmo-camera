#include <unity.h>
#include <string.h>
#include <string>

#include "mcp/agent_guide.h"

namespace {
	// Collect the pieces of the guide into a string
	void appendPart(const char *text, size_t length, void *context) {
		static_cast<std::string *>(context)->append(text, length);
	}

	// Build the whole guide for an address
	std::string buildGuide(const char *host) {
		std::string guide;
		writeAgentGuide(host, appendPart, &guide);
		return guide;
	}

	// Count the times a text appears in another
	size_t countOccurrences(const std::string &text, const char *part) {
		size_t count = 0;
		for (size_t position = text.find(part); position != std::string::npos; position = text.find(part, position + 1)) {
			count++;
		}
		return count;
	}
}

// Nothing to set up before each test
void setUp() {}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Guide ********************************/

// Every placeholder becomes the address the agent used, so the URLs in the guide work as they are
void test_fills_in_the_address() {
	const std::string guide = buildGuide("192.168.1.42");
	TEST_ASSERT_EQUAL_UINT(0, countOccurrences(guide, "{host}"));
	TEST_ASSERT_NOT_EQUAL(std::string::npos, guide.find("http://192.168.1.42/mcp"));
	TEST_ASSERT_NOT_EQUAL(std::string::npos, guide.find("claude mcp add --transport http gizmo-camera http://192.168.1.42/mcp"));
	TEST_ASSERT_NOT_EQUAL(std::string::npos, guide.find("http://192.168.1.42/api/capture?resolution=SVGA&flash=1"));
}

// The guide names both tools and the plain API
void test_describes_the_tools_and_the_api() {
	const std::string guide = buildGuide("gizmo-camera.local");
	TEST_ASSERT_EQUAL_UINT(0, guide.find("# Gizmo Camera"));
	TEST_ASSERT_NOT_EQUAL(std::string::npos, guide.find("`take_snapshot`"));
	TEST_ASSERT_NOT_EQUAL(std::string::npos, guide.find("`get_camera_info`"));
	TEST_ASSERT_NOT_EQUAL(std::string::npos, guide.find("GET http://gizmo-camera.local/api/camera"));
}

/******************************** Accept header ********************************/

// Markdown is sent only to clients that ask for it before HTML
void test_prefers_markdown_only_when_asked() {
	TEST_ASSERT_TRUE(isMarkdownPreferred("text/markdown"));
	TEST_ASSERT_TRUE(isMarkdownPreferred("text/markdown, text/html;q=0.9, */*;q=0.8"));
	TEST_ASSERT_TRUE(isMarkdownPreferred("Text/Markdown"));
	TEST_ASSERT_FALSE(isMarkdownPreferred("text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"));
	TEST_ASSERT_FALSE(isMarkdownPreferred("text/html, text/markdown"));
	TEST_ASSERT_FALSE(isMarkdownPreferred("*/*"));
	TEST_ASSERT_FALSE(isMarkdownPreferred(""));
	TEST_ASSERT_FALSE(isMarkdownPreferred(nullptr));
}

// Run the tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_fills_in_the_address);
	RUN_TEST(test_describes_the_tools_and_the_api);
	RUN_TEST(test_prefers_markdown_only_when_asked);
	return UNITY_END();
}