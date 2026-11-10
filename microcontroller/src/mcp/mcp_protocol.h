#ifndef MCP_PROTOCOL_H
#define MCP_PROTOCOL_H

#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>
#include "../camera/capture_options.h"

// The Model Context Protocol server of the camera (Streamable HTTP, JSON responses only, stateless), kept free of hardware headers so the native tests can use it
// It speaks the 2026-07-28 revision (every request carries its version in _meta) and the earlier ones, which open with initialize

// Who the server is and what the camera can do
struct McpServerInfo {
	const char *name; // Machine name, for example "gizmo-camera"
	const char *title; // Name for people, for example "Gizmo Camera"
	const char *version; // Firmware version, for example "v1.0.0"
	bool isCameraAvailable;
	const char *sensorName;
	size_t maxResolutionIndex; // Index in RESOLUTIONS of the largest resolution of the sensor
	bool hasFlash;
};

// The standard MCP headers of the HTTP request (nullptr when missing)
struct McpRequestHeaders {
	const char *protocolVersion; // MCP-Protocol-Version
	const char *method; // Mcp-Method
	const char *name; // Mcp-Name
};

// What the web server must send back
enum class McpReplyKind : uint8_t {
	Json, // The JSON document, with the HTTP status
	Accepted, // 202 with no body (a notification, or a response from the client)
	Snapshot // Take a snapshot with the options, then send the result with buildSnapshotResult
};

// Answer of the protocol to one HTTP message
struct McpReply {
	McpReplyKind kind;
	uint16_t httpStatus;
	CaptureOptions snapshotOptions;
};

// Details of a snapshot, for the text the agent reads next to the image
struct SnapshotDetails {
	uint16_t width;
	uint16_t height;
	size_t sizeBytes;
	uint32_t captureTimeMs;
	bool usedFlash;
};

// Resolution of a snapshot when the agent doesn't choose one (SVGA: enough to see a scene, quick to send, few tokens)
size_t getDefaultMcpResolutionIndex(size_t maxResolutionIndex);

// Read one JSON-RPC message from the body of a POST, and write the answer into the response (for a snapshot, the result without its image)
McpReply handleMcpMessage(const char *body, size_t length, const McpRequestHeaders &headers, const McpServerInfo &info, JsonDocument &response);

// Fill a snapshot result with its details (the response handleMcpMessage prepared, whose image data is still empty)
void buildSnapshotResult(JsonDocument &response, const SnapshotDetails &details);

// Turn a prepared result into a tool error (the camera was busy or gave no frame)
void buildSnapshotError(JsonDocument &response, const char *message);

// Split a serialized snapshot result around its empty image data, so the image can be streamed between the two parts (false if the marker isn't there)
bool findImageDataPosition(const char *json, size_t length, size_t &dataPosition);

#endif