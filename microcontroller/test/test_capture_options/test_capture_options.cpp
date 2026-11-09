#include <unity.h>

#include "camera/capture_options.h"

namespace {
	// Index of UXGA, the largest resolution of the OV2640 (the sensor of the ESP32-CAM)
	constexpr size_t UXGA_INDEX = 8;

	// Options read by the test, filled with values no request can give so a refused request is seen not to change them
	CaptureOptions options;

	// Read the options of a request to an OV2640
	CaptureOptionsError parse(const char *resolution, const char *quality, const char *flash) {
		return parseCaptureOptions(resolution, quality, flash, UXGA_INDEX, options);
	}
}

// Start each test from options no request can give
void setUp() {
	options = { 99, 0, true };
}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Resolutions ********************************/

// The resolutions are listed from the smallest to the largest, so "up to the sensor's largest" is an index comparison
void test_resolutions_are_sorted_by_size() {
	for (size_t index = 1; index < RESOLUTION_COUNT; index++) {
		const uint32_t previousPixels = static_cast<uint32_t>(RESOLUTIONS[index - 1].width) * RESOLUTIONS[index - 1].height;
		const uint32_t pixels = static_cast<uint32_t>(RESOLUTIONS[index].width) * RESOLUTIONS[index].height;
		TEST_ASSERT_TRUE_MESSAGE(pixels > previousPixels, RESOLUTIONS[index].name);
	}
}

// UXGA is where the tests and the camera module expect it
void test_uxga_is_at_its_index() {
	TEST_ASSERT_EQUAL_STRING("UXGA", RESOLUTIONS[UXGA_INDEX].name);
}

// Names are found whatever their case
void test_finds_resolutions_ignoring_case() {
	TEST_ASSERT_EQUAL_INT(4, findResolutionIndex("SVGA"));
	TEST_ASSERT_EQUAL_INT(4, findResolutionIndex("svga"));
	TEST_ASSERT_EQUAL_INT(0, findResolutionIndex("QqVgA"));
}

// A name that only starts like a resolution is not one ("SVG" is not "SVGA", "VGA2" is not "VGA")
void test_refuses_partial_resolution_names() {
	TEST_ASSERT_EQUAL_INT(RESOLUTION_NOT_FOUND, findResolutionIndex("SVG"));
	TEST_ASSERT_EQUAL_INT(RESOLUTION_NOT_FOUND, findResolutionIndex("VGA2"));
	TEST_ASSERT_EQUAL_INT(RESOLUTION_NOT_FOUND, findResolutionIndex(""));
}

/******************************** Request options ********************************/

// A request without options gets the largest resolution, the default quality and no flash
void test_defaults_without_options() {
	TEST_ASSERT_EQUAL(CaptureOptionsError::None, parse(nullptr, nullptr, nullptr));
	TEST_ASSERT_EQUAL_UINT(UXGA_INDEX, options.resolutionIndex);
	TEST_ASSERT_EQUAL_UINT8(JPEG_QUALITY_DEFAULT, options.quality);
	TEST_ASSERT_FALSE(options.useFlash);
}

// Every option is read
void test_reads_every_option() {
	TEST_ASSERT_EQUAL(CaptureOptionsError::None, parse("xga", "12", "1"));
	TEST_ASSERT_EQUAL_UINT(5, options.resolutionIndex);
	TEST_ASSERT_EQUAL_UINT8(12, options.quality);
	TEST_ASSERT_TRUE(options.useFlash);
}

// A resolution bigger than the sensor's largest is refused, not quietly made smaller
void test_refuses_resolutions_above_the_sensor() {
	TEST_ASSERT_EQUAL(CaptureOptionsError::ResolutionNotSupported, parse("FHD", nullptr, nullptr));
	TEST_ASSERT_EQUAL(CaptureOptionsError::None, parse("UXGA", nullptr, nullptr));
}

// An unknown resolution is refused
void test_refuses_unknown_resolutions() {
	TEST_ASSERT_EQUAL(CaptureOptionsError::InvalidResolution, parse("4K", nullptr, nullptr));
}

// The quality range ends are allowed
void test_accepts_the_quality_range_ends() {
	TEST_ASSERT_EQUAL(CaptureOptionsError::None, parse(nullptr, "4", nullptr));
	TEST_ASSERT_EQUAL_UINT8(4, options.quality);
	TEST_ASSERT_EQUAL(CaptureOptionsError::None, parse(nullptr, "63", nullptr));
	TEST_ASSERT_EQUAL_UINT8(63, options.quality);
}

// Anything but a whole number in range is refused (atoi alone would read "abc" as 0 and "10x" as 10)
void test_refuses_invalid_qualities() {
	const char *invalidQualities[] = { "3", "64", "100", "", "abc", "10x", "-5", "+5", " 5", "5.5" };
	for (const char *quality : invalidQualities) {
		TEST_ASSERT_EQUAL_MESSAGE(CaptureOptionsError::InvalidQuality, parse(nullptr, quality, nullptr), quality);
	}
}

// The flash takes 0, 1, false and true, whatever their case
void test_reads_the_flash_switch() {
	TEST_ASSERT_EQUAL(CaptureOptionsError::None, parse(nullptr, nullptr, "TRUE"));
	TEST_ASSERT_TRUE(options.useFlash);
	TEST_ASSERT_EQUAL(CaptureOptionsError::None, parse(nullptr, nullptr, "0"));
	TEST_ASSERT_FALSE(options.useFlash);
	TEST_ASSERT_EQUAL(CaptureOptionsError::None, parse(nullptr, nullptr, "false"));
	TEST_ASSERT_FALSE(options.useFlash);
}

// Any other flash value is refused
void test_refuses_invalid_flash_values() {
	TEST_ASSERT_EQUAL(CaptureOptionsError::InvalidFlash, parse(nullptr, nullptr, "yes"));
	TEST_ASSERT_EQUAL(CaptureOptionsError::InvalidFlash, parse(nullptr, nullptr, ""));
	TEST_ASSERT_EQUAL(CaptureOptionsError::InvalidFlash, parse(nullptr, nullptr, "2"));
}

// A refused request leaves the options as they were, so no half-read options are ever used
void test_refused_request_keeps_the_options() {
	TEST_ASSERT_EQUAL(CaptureOptionsError::InvalidFlash, parse("VGA", "20", "maybe"));
	TEST_ASSERT_EQUAL_UINT(99, options.resolutionIndex);
	TEST_ASSERT_EQUAL_UINT8(0, options.quality);
	TEST_ASSERT_TRUE(options.useFlash);
}

// Run the tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_resolutions_are_sorted_by_size);
	RUN_TEST(test_uxga_is_at_its_index);
	RUN_TEST(test_finds_resolutions_ignoring_case);
	RUN_TEST(test_refuses_partial_resolution_names);
	RUN_TEST(test_defaults_without_options);
	RUN_TEST(test_reads_every_option);
	RUN_TEST(test_refuses_resolutions_above_the_sensor);
	RUN_TEST(test_refuses_unknown_resolutions);
	RUN_TEST(test_accepts_the_quality_range_ends);
	RUN_TEST(test_refuses_invalid_qualities);
	RUN_TEST(test_reads_the_flash_switch);
	RUN_TEST(test_refuses_invalid_flash_values);
	RUN_TEST(test_refused_request_keeps_the_options);
	return UNITY_END();
}