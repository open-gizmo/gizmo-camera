#include <unity.h>
#include <string.h>
#include <string>

#include "mcp/base64_stream.h"
#include "mcp/mcp_protocol.h"

namespace {
	// Index of UXGA, the largest resolution of the OV2640, and of SVGA, the default for agents
	constexpr size_t UXGA_INDEX = 8;
	constexpr size_t SVGA_INDEX = 4;

	// The camera of the ESP32-CAM
	McpServerInfo info;

	// Headers of a 2026-07-28 request, and the last answer
	McpRequestHeaders headers;
	JsonDocument response;

	// Build the body of a 2026-07-28 request, with its _meta (the extra params are a JSON fragment, for example "\"name\":\"take_snapshot\"")
	std::string buildModernBody(const char *method, const char *extraParams = "", const char *id = "1") {
		std::string body = std::string("{\"jsonrpc\":\"2.0\",\"id\":") + id + ",\"method\":\"" + method + "\",\"params\":{";
		if (strlen(extraParams) > 0) {
			body += std::string(extraParams) + ",";
		}
		body += "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\",\"io.modelcontextprotocol/clientCapabilities\":{}}}}";
		return body;
	}

	// Send a body to the server
	McpReply send(const std::string &body) {
		return handleMcpMessage(body.c_str(), body.size(), headers, info, response);
	}

	// Send a 2026-07-28 request with the headers that match it
	McpReply sendModern(const char *method, const char *extraParams = "", const char *toolName = nullptr) {
		headers = { "2026-07-28", method, toolName };
		return send(buildModernBody(method, extraParams));
	}

	// Send a request of an earlier revision (no _meta, and no headers like a client opening with initialize)
	McpReply sendLegacy(const char *method, const char *params = "{}") {
		headers = { nullptr, nullptr, nullptr };
		return send(std::string("{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"") + method + "\",\"params\":" + params + "}");
	}

	// Call take_snapshot with arguments (a JSON object)
	McpReply callSnapshot(const char *arguments) {
		const std::string params = std::string("\"name\":\"take_snapshot\",\"arguments\":") + arguments;
		return sendModern("tools/call", params.c_str(), "take_snapshot");
	}

	// Text of the first content item of a tool result
	const char *getResultText() {
		return response["result"]["content"][0]["text"] | "";
	}
}

// Start each test from a working OV2640 and an empty answer
void setUp() {
	info = { "gizmo-camera", "Gizmo Camera", "v1.0.0", true, "OV2640", UXGA_INDEX, true };
	headers = { nullptr, nullptr, nullptr };
	response.clear();
}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Messages ********************************/

// A body that isn't JSON gets a parse error with a null id
void test_refuses_invalid_json() {
	const McpReply reply = send("{not json");
	TEST_ASSERT_EQUAL_UINT16(400, reply.httpStatus);
	TEST_ASSERT_EQUAL_INT(-32700, response["error"]["code"].as<int>());
	std::string json;
	serializeJson(response, json);
	TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"id\":null"));
}

// Batches are not part of the protocol anymore
void test_refuses_batches() {
	const McpReply reply = send("[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}]");
	TEST_ASSERT_EQUAL_UINT16(400, reply.httpStatus);
	TEST_ASSERT_EQUAL_INT(-32600, response["error"]["code"].as<int>());
}

// Notifications and responses of the client get 202 and no body
void test_accepts_notifications_and_client_responses() {
	TEST_ASSERT_EQUAL(McpReplyKind::Accepted, send("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}").kind);
	TEST_ASSERT_EQUAL(McpReplyKind::Accepted, send("{\"jsonrpc\":\"2.0\",\"id\":3,\"result\":{}}").kind);
	TEST_ASSERT_EQUAL_UINT16(202, send("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/cancelled\",\"params\":{}}").httpStatus);
}

// String ids come back as they are, and 0 is a valid id (not a missing one)
void test_keeps_the_request_id() {
	headers = { "2026-07-28", "tools/list", nullptr };
	send(buildModernBody("tools/list", "", "\"abc-1\""));
	TEST_ASSERT_EQUAL_STRING("abc-1", response["id"].as<const char *>());
	send(buildModernBody("tools/list", "", "0"));
	TEST_ASSERT_EQUAL_INT(0, response["id"].as<int>());
	TEST_ASSERT_FALSE(response["result"].isNull());
}

/******************************** 2026-07-28 ********************************/

// server/discover names the version, the tools capability and the server
void test_discovers_the_server() {
	const McpReply reply = sendModern("server/discover");
	TEST_ASSERT_EQUAL_UINT16(200, reply.httpStatus);
	JsonObjectConst result = response["result"];
	TEST_ASSERT_EQUAL_STRING("complete", result["resultType"]);
	TEST_ASSERT_EQUAL_STRING("2026-07-28", result["supportedVersions"][0]);
	TEST_ASSERT_TRUE(result["capabilities"]["tools"].is<JsonObjectConst>());
	TEST_ASSERT_EQUAL_STRING("gizmo-camera", result["_meta"]["io.modelcontextprotocol/serverInfo"]["name"]);
	TEST_ASSERT_EQUAL_STRING("v1.0.0", result["_meta"]["io.modelcontextprotocol/serverInfo"]["version"]);
	TEST_ASSERT_NOT_NULL(result["instructions"].as<const char *>());
	TEST_ASSERT_EQUAL_STRING("public", result["cacheScope"]);
}

// Another version is refused with the supported one, so the client can retry with it
void test_refuses_other_versions() {
	headers = { "1900-01-01", "tools/list", nullptr };
	std::string body = buildModernBody("tools/list");
	body.replace(body.find("2026-07-28"), 10, "1900-01-01");
	const McpReply reply = send(body);
	TEST_ASSERT_EQUAL_UINT16(400, reply.httpStatus);
	TEST_ASSERT_EQUAL_INT(-32022, response["error"]["code"].as<int>());
	TEST_ASSERT_EQUAL_STRING("2026-07-28", response["error"]["data"]["supported"][0]);
	TEST_ASSERT_EQUAL_STRING("1900-01-01", response["error"]["data"]["requested"]);
}

// The headers must repeat the version, the method and the tool name
void test_checks_the_headers() {
	// A missing Mcp-Name on a tool call
	TEST_ASSERT_EQUAL_UINT16(400, sendModern("tools/call", "\"name\":\"take_snapshot\"", nullptr).httpStatus);
	TEST_ASSERT_EQUAL_INT(-32020, response["error"]["code"].as<int>());

	// An Mcp-Method that doesn't match the body
	headers = { "2026-07-28", "tools/call", nullptr };
	TEST_ASSERT_EQUAL_UINT16(400, send(buildModernBody("tools/list")).httpStatus);
	TEST_ASSERT_EQUAL_INT(-32020, response["error"]["code"].as<int>());

	// A missing MCP-Protocol-Version
	headers = { nullptr, "tools/list", nullptr };
	TEST_ASSERT_EQUAL_UINT16(400, send(buildModernBody("tools/list")).httpStatus);
}

// Every request carries the client's capabilities
void test_needs_the_client_capabilities() {
	headers = { "2026-07-28", "tools/list", nullptr };
	const McpReply reply = send("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\"}}}");
	TEST_ASSERT_EQUAL_UINT16(400, reply.httpStatus);
	TEST_ASSERT_EQUAL_INT(-32602, response["error"]["code"].as<int>());
}

// An unknown method is a 404
void test_answers_unknown_methods_with_404() {
	TEST_ASSERT_EQUAL_UINT16(404, sendModern("resources/list").httpStatus);
	TEST_ASSERT_EQUAL_INT(-32601, response["error"]["code"].as<int>());
}

/******************************** Earlier revisions ********************************/

// initialize answers the client's version when the server speaks it
void test_initializes_with_the_client_version() {
	sendLegacy("initialize", "{\"protocolVersion\":\"2025-06-18\",\"capabilities\":{},\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
	TEST_ASSERT_EQUAL_STRING("2025-06-18", response["result"]["protocolVersion"]);
	TEST_ASSERT_EQUAL_STRING("Gizmo Camera", response["result"]["serverInfo"]["title"]);
	TEST_ASSERT_TRUE(response["result"]["capabilities"]["tools"].is<JsonObjectConst>());
}

// initialize with a version the server doesn't speak gets the newest earlier one
void test_initializes_unknown_versions_with_the_newest() {
	sendLegacy("initialize", "{\"protocolVersion\":\"2024-01-01\",\"capabilities\":{}}");
	TEST_ASSERT_EQUAL_STRING("2025-11-25", response["result"]["protocolVersion"]);
}

// ping answers an empty result
void test_answers_ping() {
	TEST_ASSERT_EQUAL_UINT16(200, sendLegacy("ping").httpStatus);
	TEST_ASSERT_TRUE(response["result"].is<JsonObjectConst>());
}

// Errors of earlier revisions keep a 200 status (a 404 there tells the client its session expired)
void test_keeps_legacy_errors_on_200() {
	TEST_ASSERT_EQUAL_UINT16(200, sendLegacy("resources/list").httpStatus);
	TEST_ASSERT_EQUAL_INT(-32601, response["error"]["code"].as<int>());
	TEST_ASSERT_EQUAL_UINT16(200, sendLegacy("tools/call", "{\"name\":\"zoom\"}").httpStatus);
	TEST_ASSERT_EQUAL_INT(-32602, response["error"]["code"].as<int>());
}

// A 2026-07-28 header without the _meta is a malformed modern request, not an earlier revision
void test_refuses_modern_requests_without_meta() {
	headers = { "2026-07-28", "tools/list", nullptr };
	const McpReply reply = send("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}");
	TEST_ASSERT_EQUAL_UINT16(400, reply.httpStatus);
	TEST_ASSERT_EQUAL_INT(-32602, response["error"]["code"].as<int>());
}

/******************************** Tools ********************************/

// The tools come in the same order, with the resolutions of the sensor
void test_lists_the_tools() {
	sendModern("tools/list");
	JsonArrayConst tools = response["result"]["tools"];
	TEST_ASSERT_EQUAL_UINT(2, tools.size());
	TEST_ASSERT_EQUAL_STRING("take_snapshot", tools[0]["name"]);
	TEST_ASSERT_EQUAL_STRING("get_camera_info", tools[1]["name"]);
	JsonArrayConst resolutions = tools[0]["inputSchema"]["properties"]["resolution"]["enum"];
	TEST_ASSERT_EQUAL_UINT(UXGA_INDEX + 1, resolutions.size());
	TEST_ASSERT_EQUAL_STRING("UXGA", resolutions[UXGA_INDEX]);
	TEST_ASSERT_FALSE(tools[0]["inputSchema"]["additionalProperties"].as<bool>());
	TEST_ASSERT_EQUAL_STRING("object", tools[1]["inputSchema"]["type"]);
	TEST_ASSERT_EQUAL_STRING("complete", response["result"]["resultType"]);
}

// Without arguments, a snapshot is SVGA at the default quality, without the flash
void test_snapshot_defaults_to_svga() {
	const McpReply reply = callSnapshot("{}");
	TEST_ASSERT_EQUAL(McpReplyKind::Snapshot, reply.kind);
	TEST_ASSERT_EQUAL_UINT(SVGA_INDEX, reply.snapshotOptions.resolutionIndex);
	TEST_ASSERT_EQUAL_UINT8(JPEG_QUALITY_DEFAULT, reply.snapshotOptions.quality);
	TEST_ASSERT_FALSE(reply.snapshotOptions.useFlash);
}

// Missing arguments are the same as none
void test_snapshot_without_arguments() {
	TEST_ASSERT_EQUAL(McpReplyKind::Snapshot, sendModern("tools/call", "\"name\":\"take_snapshot\"", "take_snapshot").kind);
}

// The arguments are read whatever the case of the resolution
void test_snapshot_reads_the_arguments() {
	const McpReply reply = callSnapshot("{\"resolution\":\"uxga\",\"quality\":20,\"flash\":true}");
	TEST_ASSERT_EQUAL(McpReplyKind::Snapshot, reply.kind);
	TEST_ASSERT_EQUAL_UINT(UXGA_INDEX, reply.snapshotOptions.resolutionIndex);
	TEST_ASSERT_EQUAL_UINT8(20, reply.snapshotOptions.quality);
	TEST_ASSERT_TRUE(reply.snapshotOptions.useFlash);
}

// Wrong argument values are tool errors the agent can read and fix, not protocol errors
void test_snapshot_refuses_wrong_arguments() {
	const char *wrongArguments[] = {
		"{\"resolution\":\"FHD\"}", // Bigger than the sensor
		"{\"resolution\":\"4K\"}", // Unknown
		"{\"resolution\":800}", // Not a name
		"{\"quality\":3}", // Out of range
		"{\"quality\":\"10\"}", // A string
		"{\"quality\":10.5}", // Not whole
		"{\"flash\":1}", // Not a boolean
		"{\"zoom\":2}" // Unknown argument
	};
	for (const char *arguments : wrongArguments) {
		const McpReply reply = callSnapshot(arguments);
		TEST_ASSERT_EQUAL_MESSAGE(McpReplyKind::Json, reply.kind, arguments);
		TEST_ASSERT_EQUAL_UINT16_MESSAGE(200, reply.httpStatus, arguments);
		TEST_ASSERT_TRUE_MESSAGE(response["result"]["isError"].as<bool>(), arguments);
	}

	// The message says what's wrong (it's built in a buffer the answer must not point to)
	callSnapshot("{\"quality\":3}");
	TEST_ASSERT_EQUAL_STRING("quality must be a whole number from 4 to 63", getResultText());
}

// Without a sensor, the agent is told why instead of getting a failing call
void test_snapshot_without_a_camera() {
	info.isCameraAvailable = false;
	TEST_ASSERT_EQUAL(McpReplyKind::Json, callSnapshot("{}").kind);
	TEST_ASSERT_TRUE(response["result"]["isError"].as<bool>());
	TEST_ASSERT_NOT_NULL(strstr(getResultText(), "ribbon cable"));
}

// An unknown tool is a protocol error
void test_refuses_unknown_tools() {
	TEST_ASSERT_EQUAL_UINT16(400, sendModern("tools/call", "\"name\":\"zoom\"", "zoom").httpStatus);
	TEST_ASSERT_EQUAL_INT(-32602, response["error"]["code"].as<int>());
	TEST_ASSERT_EQUAL_STRING("Unknown tool: zoom", response["error"]["message"]); // Built in a buffer the answer must not point to
}

// get_camera_info gives the camera as structured content and as the same JSON in text
void test_gets_the_camera_info() {
	sendModern("tools/call", "\"name\":\"get_camera_info\"", "get_camera_info");
	JsonObjectConst camera = response["result"]["structuredContent"];
	TEST_ASSERT_EQUAL_STRING("OV2640", camera["sensor"]);
	TEST_ASSERT_EQUAL_STRING("SVGA", camera["defaultResolution"]);
	TEST_ASSERT_TRUE(camera["hasFlash"].as<bool>());
	JsonDocument text;
	TEST_ASSERT_TRUE(deserializeJson(text, getResultText()) == DeserializationError::Ok);
	TEST_ASSERT_EQUAL_STRING("UXGA", text["resolutions"][UXGA_INDEX]["name"]);
}

/******************************** Snapshot result ********************************/

// The image is streamed into the serialized result: splitting it at the empty data and inserting the base64 gives a valid result
void test_streams_the_image_into_the_result() {
	// Prepare the result, and describe the snapshot
	callSnapshot("{\"flash\":true}");
	buildSnapshotResult(response, { 800, 600, 46080, 312, true });
	TEST_ASSERT_EQUAL_STRING("Snapshot 800x600, JPEG, 45 KB, taken in 312 ms with the flash.", getResultText());

	// Serialize it and split it around the image data
	std::string json;
	serializeJson(response, json);
	size_t dataPosition = 0;
	TEST_ASSERT_TRUE(findImageDataPosition(json.c_str(), json.size(), dataPosition));

	// Put the base64 of a few bytes between the two parts, like the web server
	const uint8_t image[] = { 0xFF, 0xD8, 0xFF, 0xE0 };
	char base64[16] = {};
	writeBase64Slice(image, sizeof(image), 0, base64, sizeof(base64));
	const std::string streamed = json.substr(0, dataPosition) + base64 + json.substr(dataPosition);

	// Expect a valid result with the image last
	JsonDocument parsed;
	TEST_ASSERT_TRUE(deserializeJson(parsed, streamed) == DeserializationError::Ok);
	TEST_ASSERT_EQUAL_STRING("/9j/4A==", parsed["result"]["content"][1]["data"]);
	TEST_ASSERT_EQUAL_STRING("image/jpeg", parsed["result"]["content"][1]["mimeType"]);
	TEST_ASSERT_FALSE(parsed["result"]["isError"].as<bool>());
}

// A snapshot the camera couldn't take becomes a tool error with only the message
void test_turns_a_failed_snapshot_into_a_tool_error() {
	callSnapshot("{}");
	buildSnapshotError(response, "The camera is busy");
	TEST_ASSERT_TRUE(response["result"]["isError"].as<bool>());
	TEST_ASSERT_EQUAL_UINT(1, response["result"]["content"].size());
	TEST_ASSERT_EQUAL_STRING("The camera is busy", getResultText());
}

// Without the marker there is nothing to split
void test_finds_no_marker_in_other_json() {
	size_t dataPosition = 0;
	const char *json = "{\"data\":\"x\"}";
	TEST_ASSERT_FALSE(findImageDataPosition(json, strlen(json), dataPosition));
}

// Run the tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_refuses_invalid_json);
	RUN_TEST(test_refuses_batches);
	RUN_TEST(test_accepts_notifications_and_client_responses);
	RUN_TEST(test_keeps_the_request_id);
	RUN_TEST(test_discovers_the_server);
	RUN_TEST(test_refuses_other_versions);
	RUN_TEST(test_checks_the_headers);
	RUN_TEST(test_needs_the_client_capabilities);
	RUN_TEST(test_answers_unknown_methods_with_404);
	RUN_TEST(test_initializes_with_the_client_version);
	RUN_TEST(test_initializes_unknown_versions_with_the_newest);
	RUN_TEST(test_answers_ping);
	RUN_TEST(test_keeps_legacy_errors_on_200);
	RUN_TEST(test_refuses_modern_requests_without_meta);
	RUN_TEST(test_lists_the_tools);
	RUN_TEST(test_snapshot_defaults_to_svga);
	RUN_TEST(test_snapshot_without_arguments);
	RUN_TEST(test_snapshot_reads_the_arguments);
	RUN_TEST(test_snapshot_refuses_wrong_arguments);
	RUN_TEST(test_snapshot_without_a_camera);
	RUN_TEST(test_refuses_unknown_tools);
	RUN_TEST(test_gets_the_camera_info);
	RUN_TEST(test_streams_the_image_into_the_result);
	RUN_TEST(test_turns_a_failed_snapshot_into_a_tool_error);
	RUN_TEST(test_finds_no_marker_in_other_json);
	return UNITY_END();
}