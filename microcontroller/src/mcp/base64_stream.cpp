#include "base64_stream.h"

namespace {
	// Characters of the base64 alphabet
	constexpr char BASE64_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	// Get one character of the base64 text (each group of 3 bytes becomes 4 characters, the last group padded with =)
	char getBase64Character(const uint8_t *data, size_t dataLength, size_t textPosition) {
		const size_t groupStart = (textPosition / 4) * 3;
		const size_t characterIndex = textPosition % 4;
		const size_t groupLength = dataLength - groupStart < 3 ? dataLength - groupStart : 3;

		// Padding past the end of the data
		if (characterIndex > groupLength) {
			return '=';
		}

		// Join the bytes of the group into 24 bits (missing bytes count as zero)
		const uint32_t first = data[groupStart];
		const uint32_t second = groupLength > 1 ? data[groupStart + 1] : 0;
		const uint32_t third = groupLength > 2 ? data[groupStart + 2] : 0;
		const uint32_t bits = (first << 16) | (second << 8) | third;

		// Return the 6 bits of this character
		return BASE64_ALPHABET[(bits >> (18 - characterIndex * 6)) & 0x3F];
	}
}

// Length of the base64 text of a number of bytes
size_t getBase64Length(size_t dataLength) {
	return ((dataLength + 2) / 3) * 4;
}

// Write the base64 characters from a position of the encoded text
size_t writeBase64Slice(const uint8_t *data, size_t dataLength, size_t textOffset, char *output, size_t maxLength) {
	const size_t textLength = getBase64Length(dataLength);

	// Nothing left to write
	if (textOffset >= textLength) {
		return 0;
	}

	// Write the characters that fit
	const size_t count = textLength - textOffset < maxLength ? textLength - textOffset : maxLength;
	for (size_t index = 0; index < count; index++) {
		output[index] = getBase64Character(data, dataLength, textOffset + index);
	}

	// Return how many were written
	return count;
}