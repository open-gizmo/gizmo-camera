#ifndef BASE64_STREAM_H
#define BASE64_STREAM_H

#include <stddef.h>
#include <stdint.h>

// Base64 of a binary buffer, written a slice at a time (the web server asks for the response in chunks of any size, and a snapshot is too big to encode in one go), kept free of hardware headers so the native tests can use it

// Length of the base64 text of a number of bytes (with the = padding)
size_t getBase64Length(size_t dataLength);

// Write the base64 characters from a position of the encoded text, up to a maximum count, returning how many were written
size_t writeBase64Slice(const uint8_t *data, size_t dataLength, size_t textOffset, char *output, size_t maxLength);

#endif