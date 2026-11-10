#include <unity.h>
#include <string.h>

#include "mcp/base64_stream.h"

namespace {
	// Encode a whole text in one slice
	void encodeAll(const char *text, char *output, size_t outputSize) {
		const size_t length = writeBase64Slice(reinterpret_cast<const uint8_t *>(text), strlen(text), 0, output, outputSize - 1);
		output[length] = '\0';
	}

	// Bytes 0 to 255 then a few more, so every value and every padding case shows up
	uint8_t allBytes[259];

	// Fill the test bytes
	void fillAllBytes() {
		for (size_t index = 0; index < sizeof(allBytes); index++) {
			allBytes[index] = static_cast<uint8_t>(index * 7 + 3);
		}
	}
}

// Fill the test bytes before each test
void setUp() {
	fillAllBytes();
}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Whole text ********************************/

// The examples of RFC 4648, with every padding case
void test_encodes_the_rfc_examples() {
	const char *cases[][2] = {
		{ "", "" },
		{ "f", "Zg==" },
		{ "fo", "Zm8=" },
		{ "foo", "Zm9v" },
		{ "foob", "Zm9vYg==" },
		{ "fooba", "Zm9vYmE=" },
		{ "foobar", "Zm9vYmFy" }
	};
	for (const auto &testCase : cases) {
		char output[16];
		encodeAll(testCase[0], output, sizeof(output));
		TEST_ASSERT_EQUAL_STRING(testCase[1], output);
	}
}

// The length counts the padding (4 characters for every started group of 3 bytes)
void test_counts_the_text_length() {
	TEST_ASSERT_EQUAL_UINT(0, getBase64Length(0));
	TEST_ASSERT_EQUAL_UINT(4, getBase64Length(1));
	TEST_ASSERT_EQUAL_UINT(4, getBase64Length(3));
	TEST_ASSERT_EQUAL_UINT(8, getBase64Length(4));
	TEST_ASSERT_EQUAL_UINT(400000, getBase64Length(300000));
}

// High bytes use the last characters of the alphabet (+ and /)
void test_uses_the_whole_alphabet() {
	const uint8_t data[] = { 0xFB, 0xFF, 0xBF };
	char output[5] = {};
	writeBase64Slice(data, sizeof(data), 0, output, 4);
	TEST_ASSERT_EQUAL_STRING("+/+/", output);
}

/******************************** Slices ********************************/

// Slices of every size give the same text as one go (the web server asks for chunks of any size, cutting groups in the middle)
void test_slices_match_the_whole_text() {
	const size_t textLength = getBase64Length(sizeof(allBytes));
	char whole[400] = {};
	TEST_ASSERT_EQUAL_UINT(textLength, writeBase64Slice(allBytes, sizeof(allBytes), 0, whole, sizeof(whole)));

	// Rebuild the text slice by slice, for every slice size up to 9
	for (size_t sliceSize = 1; sliceSize <= 9; sliceSize++) {
		char rebuilt[400] = {};
		size_t offset = 0;
		while (offset < textLength) {
			offset += writeBase64Slice(allBytes, sizeof(allBytes), offset, rebuilt + offset, sliceSize);
		}
		TEST_ASSERT_EQUAL_MEMORY(whole, rebuilt, textLength);
	}
}

// A slice stops at the end of the text, and past it nothing is written
void test_stops_at_the_end_of_the_text() {
	const uint8_t data[] = { 'f', 'o' };
	char output[8] = {};
	TEST_ASSERT_EQUAL_UINT(2, writeBase64Slice(data, sizeof(data), 2, output, 8));
	TEST_ASSERT_EQUAL_MEMORY("8=", output, 2);
	TEST_ASSERT_EQUAL_UINT(0, writeBase64Slice(data, sizeof(data), 4, output, 8));
	TEST_ASSERT_EQUAL_UINT(0, writeBase64Slice(data, sizeof(data), 100, output, 8));
}

// Run the tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_encodes_the_rfc_examples);
	RUN_TEST(test_counts_the_text_length);
	RUN_TEST(test_uses_the_whole_alphabet);
	RUN_TEST(test_slices_match_the_whole_text);
	RUN_TEST(test_stops_at_the_end_of_the_text);
	return UNITY_END();
}