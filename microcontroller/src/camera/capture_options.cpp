#include "capture_options.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace {
	// Compare two names, ignoring the case
	bool isSameName(const char *first, const char *second) {
		// Compare the characters one by one
		while (*first != '\0' && *second != '\0') {
			if (tolower(static_cast<unsigned char>(*first)) != tolower(static_cast<unsigned char>(*second))) {
				return false;
			}
			first++;
			second++;
		}

		// Same name only if both ended together
		return *first == *second;
	}

	// Read a JPEG quality: a whole number of one or two digits, in range (false otherwise)
	bool parseQuality(const char *text, uint8_t &quality) {
		// Check the length and that every character is a digit (no sign, no spaces, no decimals)
		const size_t length = strlen(text);
		if (length == 0 || length > 2) {
			return false;
		}
		for (size_t index = 0; index < length; index++) {
			if (!isdigit(static_cast<unsigned char>(text[index]))) {
				return false;
			}
		}

		// Check the range
		const int value = atoi(text);
		if (value < JPEG_QUALITY_MIN || value > JPEG_QUALITY_MAX) {
			return false;
		}

		// Return the quality
		quality = static_cast<uint8_t>(value);
		return true;
	}

	// Read the flash switch: 1 or true turns it on, 0 or false leaves it off (false for anything else)
	bool parseFlash(const char *text, bool &useFlash) {
		if (strcmp(text, "1") == 0 || isSameName(text, "true")) {
			useFlash = true;
		} else if (strcmp(text, "0") == 0 || isSameName(text, "false")) {
			useFlash = false;
		} else {
			return false;
		}
		return true;
	}
}

// Find a resolution by name, ignoring the case
int findResolutionIndex(const char *name) {
	// Look for the name in the list
	for (size_t index = 0; index < RESOLUTION_COUNT; index++) {
		if (isSameName(RESOLUTIONS[index].name, name)) {
			return static_cast<int>(index);
		}
	}

	// Not a resolution name
	return RESOLUTION_NOT_FOUND;
}

// Read the options of a snapshot request
CaptureOptionsError parseCaptureOptions(const char *resolution, const char *quality, const char *flash, size_t maxResolutionIndex, CaptureOptions &options) {
	// Start from the defaults: the largest resolution of the sensor, the default quality and no flash
	CaptureOptions parsedOptions = { maxResolutionIndex, JPEG_QUALITY_DEFAULT, false };

	// Read the resolution, which the sensor must be able to take
	if (resolution != nullptr) {
		const int resolutionIndex = findResolutionIndex(resolution);
		if (resolutionIndex == RESOLUTION_NOT_FOUND) {
			return CaptureOptionsError::InvalidResolution;
		}
		if (static_cast<size_t>(resolutionIndex) > maxResolutionIndex) {
			return CaptureOptionsError::ResolutionNotSupported;
		}
		parsedOptions.resolutionIndex = static_cast<size_t>(resolutionIndex);
	}

	// Read the JPEG quality
	if (quality != nullptr && !parseQuality(quality, parsedOptions.quality)) {
		return CaptureOptionsError::InvalidQuality;
	}

	// Read the flash switch
	if (flash != nullptr && !parseFlash(flash, parsedOptions.useFlash)) {
		return CaptureOptionsError::InvalidFlash;
	}

	// Return the options only once they are all valid
	options = parsedOptions;
	return CaptureOptionsError::None;
}