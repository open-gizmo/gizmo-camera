#ifndef CAPTURE_OPTIONS_H
#define CAPTURE_OPTIONS_H

#include <stddef.h>
#include <stdint.h>

// Snapshot options, kept free of hardware headers so the native tests can use them

// A resolution the camera can take snapshots at
struct Resolution {
	const char *name; // Name used by the web API, for example "SVGA"
	uint16_t width;
	uint16_t height;
};

// Resolutions, smallest to largest (camera.cpp maps each one to the sensor's frame size, in the same order)
constexpr Resolution RESOLUTIONS[] = {
	{ "QQVGA", 160, 120 },
	{ "QVGA", 320, 240 },
	{ "CIF", 400, 296 },
	{ "VGA", 640, 480 },
	{ "SVGA", 800, 600 },
	{ "XGA", 1024, 768 },
	{ "HD", 1280, 720 },
	{ "SXGA", 1280, 1024 },
	{ "UXGA", 1600, 1200 },
	{ "FHD", 1920, 1080 },
	{ "QXGA", 2048, 1536 },
	{ "QHD", 2560, 1440 },
	{ "WQXGA", 2560, 1600 },
	{ "QSXGA", 2560, 1920 }
};
constexpr size_t RESOLUTION_COUNT = sizeof(RESOLUTIONS) / sizeof(RESOLUTIONS[0]);

// Value returned when a resolution is not in the list
constexpr int RESOLUTION_NOT_FOUND = -1;

// JPEG quality of the sensor: lower numbers mean better quality and bigger files
constexpr uint8_t JPEG_QUALITY_MIN = 4; // Below 4 the frame buffer can overflow at the highest resolution
constexpr uint8_t JPEG_QUALITY_MAX = 63;
constexpr uint8_t JPEG_QUALITY_DEFAULT = 10;

// Why the options of a snapshot request were refused
enum class CaptureOptionsError : uint8_t {
	None,
	InvalidResolution, // Not a resolution name
	ResolutionNotSupported, // Bigger than the sensor can take
	InvalidQuality, // Not a whole number from JPEG_QUALITY_MIN to JPEG_QUALITY_MAX
	InvalidFlash // Not 0, 1, false or true
};

// Options of one snapshot
struct CaptureOptions {
	size_t resolutionIndex; // Index in RESOLUTIONS
	uint8_t quality;
	bool useFlash;
};

// Find a resolution by name, ignoring the case (RESOLUTION_NOT_FOUND if there is none)
int findResolutionIndex(const char *name);

// Read the options of a snapshot request: each value is the text of its query parameter, or nullptr when the request leaves it out (the largest resolution, the default quality and no flash)
CaptureOptionsError parseCaptureOptions(const char *resolution, const char *quality, const char *flash, size_t maxResolutionIndex, CaptureOptions &options);

#endif