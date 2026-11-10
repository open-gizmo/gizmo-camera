#include "mcp_protocol.h"
#include <stdio.h>
#include <string.h>
#include <string>

namespace {
	/******************************** Protocol constants ********************************/

	// Revision with the version in every request (_meta), and the earlier ones that open with initialize (newest first)
	constexpr const char *MODERN_PROTOCOL_VERSION = "2026-07-28";
	constexpr const char *LEGACY_PROTOCOL_VERSIONS[] = { "2025-11-25", "2025-06-18", "2025-03-26" };

	// Keys of the per-request _meta of the 2026-07-28 revision
	constexpr const char *META_PROTOCOL_VERSION = "io.modelcontextprotocol/protocolVersion";
	constexpr const char *META_CLIENT_CAPABILITIES = "io.modelcontextprotocol/clientCapabilities";
	constexpr const char *META_SERVER_INFO = "io.modelcontextprotocol/serverInfo";

	// JSON-RPC and MCP error codes
	constexpr int PARSE_ERROR = -32700;
	constexpr int INVALID_REQUEST = -32600;
	constexpr int METHOD_NOT_FOUND = -32601;
	constexpr int INVALID_PARAMS = -32602;
	constexpr int HEADER_MISMATCH = -32020;
	constexpr int UNSUPPORTED_PROTOCOL_VERSION = -32022;

	// How long clients may cache the tools and the server details (the firmware only changes them with an update)
	constexpr uint32_t CACHE_TTL_MS = 3600000; // 1 hour

	// Index in RESOLUTIONS of SVGA, the default resolution for agents
	constexpr size_t SVGA_RESOLUTION_INDEX = 4;

	// Names of the tools
	constexpr const char *TAKE_SNAPSHOT_TOOL = "take_snapshot";
	constexpr const char *GET_CAMERA_INFO_TOOL = "get_camera_info";

	// What the server tells agents about itself
	constexpr const char *SERVER_INSTRUCTIONS = "A camera (ESP32-CAM with an OV2640 sensor) on the local network. Call take_snapshot to see what it sees right now: every call takes a new photo. The default SVGA resolution is enough to understand a scene; ask for UXGA only to read small details, it is slower and costs more tokens. Turn the flash on in the dark. The camera takes one snapshot at a time: if it is busy, wait a moment and try again.";

	// Era of a request: the 2026-07-28 revision, or an earlier one
	enum class ProtocolEra : uint8_t {
		Modern,
		Legacy
	};

	/******************************** Helpers ********************************/

	// Copy a text into the document (ArduinoJson only keeps a pointer to a const char *, which would dangle once a buffer or the request is gone)
	JsonString copyText(const char *text) {
		return JsonString(text, JsonString::Copied);
	}

	// Check whether a version is one of the earlier revisions the server speaks
	bool isLegacyProtocolVersion(const char *version) {
		for (const char *legacyVersion : LEGACY_PROTOCOL_VERSIONS) {
			if (strcmp(version, legacyVersion) == 0) {
				return true;
			}
		}
		return false;
	}

	// Compare a header with the value it must repeat (a missing header never matches)
	bool isHeaderEqual(const char *header, const char *value) {
		return header != nullptr && value != nullptr && strcmp(header, value) == 0;
	}

	// Start a JSON-RPC answer with the id of the request
	JsonObject startAnswer(JsonDocument &response, JsonVariantConst id) {
		response.clear();
		response["jsonrpc"] = "2.0";
		if (id.isNull()) {
			response["id"] = nullptr; // A request whose id couldn't be read is answered with a null id
		} else {
			response["id"] = id;
		}
		return response.as<JsonObject>();
	}

	// Write a JSON-RPC error, returning the reply with its HTTP status
	McpReply writeError(JsonDocument &response, JsonVariantConst id, uint16_t httpStatus, int code, const char *message) {
		JsonObject answer = startAnswer(response, id);
		JsonObject error = answer["error"].to<JsonObject>();
		error["code"] = code;
		error["message"] = copyText(message);
		return { McpReplyKind::Json, httpStatus, {} };
	}

	// Start a successful result, with the server's identity in its _meta
	JsonObject startResult(JsonDocument &response, JsonVariantConst id, const McpServerInfo &info) {
		JsonObject result = startAnswer(response, id)["result"].to<JsonObject>();
		result["resultType"] = "complete";
		JsonObject serverInfo = result["_meta"][META_SERVER_INFO].to<JsonObject>();
		serverInfo["name"] = info.name;
		serverInfo["title"] = info.title;
		serverInfo["version"] = info.version;
		return result;
	}

	// Reply with the JSON document and a 200 status
	McpReply replyJson() {
		return { McpReplyKind::Json, 200, {} };
	}

	// Write a tool result with one text item, as an error the agent can read and correct
	McpReply writeToolError(JsonDocument &response, JsonVariantConst id, const McpServerInfo &info, const char *message) {
		JsonObject result = startResult(response, id, info);
		JsonObject text = result["content"].add<JsonObject>();
		text["type"] = "text";
		text["text"] = copyText(message);
		result["isError"] = true;
		return replyJson();
	}

	/******************************** Server details ********************************/

	// Write the capabilities of the server: tools only, whose list never changes while it runs
	void writeCapabilities(JsonObject result) {
		result["capabilities"]["tools"].to<JsonObject>();
	}

	// Answer server/discover (2026-07-28): versions, capabilities and instructions
	McpReply answerDiscover(JsonDocument &response, JsonVariantConst id, const McpServerInfo &info) {
		JsonObject result = startResult(response, id, info);
		result["supportedVersions"].add(MODERN_PROTOCOL_VERSION);
		writeCapabilities(result);
		result["instructions"] = SERVER_INSTRUCTIONS;
		result["ttlMs"] = CACHE_TTL_MS;
		result["cacheScope"] = "public";
		return replyJson();
	}

	// Answer initialize (earlier revisions): the client's version when the server speaks it, the newest earlier one otherwise
	McpReply answerInitialize(JsonDocument &response, JsonVariantConst id, JsonVariantConst params, const McpServerInfo &info) {
		const char *requestedVersion = params["protocolVersion"] | "";
		JsonObject result = startResult(response, id, info);
		result["protocolVersion"] = copyText(isLegacyProtocolVersion(requestedVersion) ? requestedVersion : LEGACY_PROTOCOL_VERSIONS[0]);
		writeCapabilities(result);
		JsonObject serverInfo = result["serverInfo"].to<JsonObject>();
		serverInfo["name"] = info.name;
		serverInfo["title"] = info.title;
		serverInfo["version"] = info.version;
		result["instructions"] = SERVER_INSTRUCTIONS;
		return replyJson();
	}

	/******************************** Tools ********************************/

	// Write the description of take_snapshot, with the resolutions this sensor can take
	void writeTakeSnapshotTool(JsonArray tools, const McpServerInfo &info) {
		JsonObject tool = tools.add<JsonObject>();
		tool["name"] = TAKE_SNAPSHOT_TOOL;
		tool["title"] = "Take snapshot";
		tool["description"] = "Take a new JPEG photo with the camera and return it. Every call takes a new photo, a few hundred ms after it's asked for.";
		tool["annotations"]["readOnlyHint"] = true;

		// Input: every option is optional
		JsonObject schema = tool["inputSchema"].to<JsonObject>();
		schema["type"] = "object";
		schema["additionalProperties"] = false;
		JsonObject properties = schema["properties"].to<JsonObject>();

		// Resolution, up to the sensor's largest
		JsonObject resolution = properties["resolution"].to<JsonObject>();
		resolution["type"] = "string";
		resolution["description"] = "Size of the photo. SVGA (800x600, the default) shows a scene well; larger ones are slower and cost more tokens.";
		JsonArray resolutionNames = resolution["enum"].to<JsonArray>();
		for (size_t index = 0; index <= info.maxResolutionIndex; index++) {
			resolutionNames.add(RESOLUTIONS[index].name);
		}

		// JPEG quality
		JsonObject quality = properties["quality"].to<JsonObject>();
		quality["type"] = "integer";
		quality["minimum"] = JPEG_QUALITY_MIN;
		quality["maximum"] = JPEG_QUALITY_MAX;
		quality["description"] = "JPEG quality: lower numbers are sharper and bigger. Default 10.";

		// Flash LED
		if (info.hasFlash) {
			JsonObject flash = properties["flash"].to<JsonObject>();
			flash["type"] = "boolean";
			flash["description"] = "Turn on the white flash LED for the photo (for dark scenes). Default false.";
		}
	}

	// Write the description of get_camera_info
	void writeGetCameraInfoTool(JsonArray tools) {
		JsonObject tool = tools.add<JsonObject>();
		tool["name"] = GET_CAMERA_INFO_TOOL;
		tool["title"] = "Get camera info";
		tool["description"] = "Describe the camera: its sensor, the resolutions it can take, the JPEG quality range, and whether it has a flash.";
		tool["annotations"]["readOnlyHint"] = true;
		JsonObject schema = tool["inputSchema"].to<JsonObject>();
		schema["type"] = "object";
		schema["additionalProperties"] = false;
	}

	// Answer tools/list (always in the same order)
	McpReply answerToolsList(JsonDocument &response, JsonVariantConst id, const McpServerInfo &info) {
		JsonObject result = startResult(response, id, info);
		JsonArray tools = result["tools"].to<JsonArray>();
		writeTakeSnapshotTool(tools, info);
		writeGetCameraInfoTool(tools);
		result["ttlMs"] = CACHE_TTL_MS;
		result["cacheScope"] = "public";
		return replyJson();
	}

	// Find an argument the tool doesn't take (nullptr if there is none)
	const char *findUnknownArgument(JsonObjectConst arguments, const char *const *knownNames, size_t knownCount) {
		for (JsonPairConst argument : arguments) {
			bool isKnown = false;
			for (size_t index = 0; index < knownCount; index++) {
				isKnown = isKnown || strcmp(argument.key().c_str(), knownNames[index]) == 0;
			}
			if (!isKnown) {
				return argument.key().c_str();
			}
		}
		return nullptr;
	}

	// Read the arguments of take_snapshot, writing the problem into message when one is wrong
	bool readSnapshotArguments(JsonObjectConst arguments, const McpServerInfo &info, CaptureOptions &options, char *message, size_t messageSize) {
		options = { getDefaultMcpResolutionIndex(info.maxResolutionIndex), JPEG_QUALITY_DEFAULT, false };

		// Refuse arguments the tool doesn't take
		const char *const knownNames[] = { "resolution", "quality", "flash" };
		const char *unknownArgument = findUnknownArgument(arguments, knownNames, info.hasFlash ? 3 : 2);
		if (unknownArgument != nullptr) {
			snprintf(message, messageSize, "Unknown argument \"%s\": take_snapshot takes resolution, quality and flash", unknownArgument);
			return false;
		}

		// Read the resolution, which the sensor must be able to take
		JsonVariantConst resolution = arguments["resolution"];
		if (!resolution.isNull()) {
			const int resolutionIndex = resolution.is<const char *>() ? findResolutionIndex(resolution.as<const char *>()) : RESOLUTION_NOT_FOUND;
			if (resolutionIndex == RESOLUTION_NOT_FOUND || static_cast<size_t>(resolutionIndex) > info.maxResolutionIndex) {
				snprintf(message, messageSize, "resolution must be one of QQVGA to %s (the largest this sensor can take)", RESOLUTIONS[info.maxResolutionIndex].name);
				return false;
			}
			options.resolutionIndex = static_cast<size_t>(resolutionIndex);
		}

		// Read the JPEG quality (a whole number in range: 10.5 or "10" are refused)
		JsonVariantConst quality = arguments["quality"];
		if (!quality.isNull()) {
			if (!quality.is<int>() || quality.as<int>() < JPEG_QUALITY_MIN || quality.as<int>() > JPEG_QUALITY_MAX) {
				snprintf(message, messageSize, "quality must be a whole number from %u to %u", JPEG_QUALITY_MIN, JPEG_QUALITY_MAX);
				return false;
			}
			options.quality = static_cast<uint8_t>(quality.as<int>());
		}

		// Read the flash switch
		JsonVariantConst flash = arguments["flash"];
		if (!flash.isNull()) {
			if (!flash.is<bool>()) {
				snprintf(message, messageSize, "flash must be true or false");
				return false;
			}
			options.useFlash = flash.as<bool>();
		}

		// All the arguments are valid
		return true;
	}

	// Call take_snapshot: check the arguments, and prepare the result the web server fills once the photo is taken
	McpReply callTakeSnapshot(JsonDocument &response, JsonVariantConst id, JsonObjectConst arguments, const McpServerInfo &info) {
		// Nothing to take without a sensor
		if (!info.isCameraAvailable) {
			return writeToolError(response, id, info, "The camera sensor did not start, so no snapshot can be taken. Someone has to check its ribbon cable and restart the device.");
		}

		// Read the arguments, telling the agent what's wrong so it can try again
		CaptureOptions options;
		char message[128];
		if (!readSnapshotArguments(arguments, info, options, message, sizeof(message))) {
			return writeToolError(response, id, info, message);
		}

		// Prepare the result: a text with the details, then the image (its data is streamed in by the web server)
		JsonObject result = startResult(response, id, info);
		JsonArray content = result["content"].to<JsonArray>();
		JsonObject text = content.add<JsonObject>();
		text["type"] = "text";
		text["text"] = "";
		JsonObject image = content.add<JsonObject>();
		image["type"] = "image";
		image["mimeType"] = "image/jpeg";
		image["data"] = "";
		result["isError"] = false;

		// Ask the web server for the snapshot
		return { McpReplyKind::Snapshot, 200, options };
	}

	// Call get_camera_info: the camera as structured content, and the same JSON as text
	McpReply callGetCameraInfo(JsonDocument &response, JsonVariantConst id, JsonObjectConst arguments, const McpServerInfo &info) {
		// The tool takes no arguments
		const char *unknownArgument = findUnknownArgument(arguments, nullptr, 0);
		if (unknownArgument != nullptr) {
			return writeToolError(response, id, info, "get_camera_info takes no arguments");
		}

		// Describe the camera
		JsonObject result = startResult(response, id, info);
		JsonObject camera = result["structuredContent"].to<JsonObject>();
		camera["available"] = info.isCameraAvailable;
		camera["sensor"] = info.sensorName;
		JsonArray resolutions = camera["resolutions"].to<JsonArray>();
		for (size_t index = 0; index <= info.maxResolutionIndex; index++) {
			JsonObject resolution = resolutions.add<JsonObject>();
			resolution["name"] = RESOLUTIONS[index].name;
			resolution["width"] = RESOLUTIONS[index].width;
			resolution["height"] = RESOLUTIONS[index].height;
		}
		camera["defaultResolution"] = RESOLUTIONS[getDefaultMcpResolutionIndex(info.maxResolutionIndex)].name;
		camera["quality"]["min"] = JPEG_QUALITY_MIN;
		camera["quality"]["max"] = JPEG_QUALITY_MAX;
		camera["quality"]["default"] = JPEG_QUALITY_DEFAULT;
		camera["hasFlash"] = info.hasFlash;
		camera["firmwareVersion"] = info.version;

		// Repeat it as text, for clients that don't read structured content
		JsonObject text = result["content"].add<JsonObject>();
		text["type"] = "text";
		std::string cameraJson;
		serializeJson(camera, cameraJson);
		text["text"] = cameraJson;
		result["isError"] = false;
		return replyJson();
	}

	// Answer tools/call: unknown tools and malformed params are protocol errors, wrong argument values are tool errors
	McpReply answerToolsCall(JsonDocument &response, JsonVariantConst id, JsonVariantConst params, const McpServerInfo &info, ProtocolEra era) {
		const uint16_t errorStatus = era == ProtocolEra::Modern ? 400 : 200;

		// Read the tool name and its arguments (missing arguments are none)
		JsonVariantConst name = params["name"];
		JsonVariantConst arguments = params["arguments"];
		if (!name.is<const char *>() || !(arguments.isNull() || arguments.is<JsonObjectConst>())) {
			return writeError(response, id, errorStatus, INVALID_PARAMS, "tools/call needs a tool name, and its arguments as an object");
		}

		// Call the tool
		const char *toolName = name.as<const char *>();
		if (strcmp(toolName, TAKE_SNAPSHOT_TOOL) == 0) {
			return callTakeSnapshot(response, id, arguments.as<JsonObjectConst>(), info);
		}
		if (strcmp(toolName, GET_CAMERA_INFO_TOOL) == 0) {
			return callGetCameraInfo(response, id, arguments.as<JsonObjectConst>(), info);
		}

		// Unknown tool
		char message[96];
		snprintf(message, sizeof(message), "Unknown tool: %s", toolName);
		return writeError(response, id, errorStatus, INVALID_PARAMS, message);
	}

	/******************************** Eras ********************************/

	// Answer a request of the 2026-07-28 revision: the version, the headers and the capabilities are checked first
	McpReply answerModernRequest(JsonDocument &response, JsonVariantConst id, const char *method, JsonVariantConst params, const char *version, const McpRequestHeaders &headers, const McpServerInfo &info) {
		// Refuse other versions, naming the supported one
		if (strcmp(version, MODERN_PROTOCOL_VERSION) != 0) {
			McpReply reply = writeError(response, id, 400, UNSUPPORTED_PROTOCOL_VERSION, "Unsupported protocol version");
			JsonObject data = response["error"]["data"].to<JsonObject>();
			data["supported"].add(MODERN_PROTOCOL_VERSION);
			data["requested"] = copyText(version);
			return reply;
		}

		// The headers must repeat the version, the method and (for a tool call) the tool name
		const bool isToolCall = strcmp(method, "tools/call") == 0;
		const bool areHeadersValid = isHeaderEqual(headers.protocolVersion, version) && isHeaderEqual(headers.method, method) && (!isToolCall || isHeaderEqual(headers.name, params["name"].as<const char *>()));
		if (!areHeadersValid) {
			return writeError(response, id, 400, HEADER_MISMATCH, "The MCP-Protocol-Version, Mcp-Method and Mcp-Name headers must match the request");
		}

		// Every request carries the client's capabilities
		if (!params["_meta"][META_CLIENT_CAPABILITIES].is<JsonObjectConst>()) {
			return writeError(response, id, 400, INVALID_PARAMS, "Missing _meta io.modelcontextprotocol/clientCapabilities");
		}

		// Answer the method
		if (strcmp(method, "server/discover") == 0) {
			return answerDiscover(response, id, info);
		}
		if (strcmp(method, "tools/list") == 0) {
			return answerToolsList(response, id, info);
		}
		if (isToolCall) {
			return answerToolsCall(response, id, params, info, ProtocolEra::Modern);
		}
		return writeError(response, id, 404, METHOD_NOT_FOUND, "Method not found");
	}

	// Answer a request of an earlier revision (errors keep a 200 status: there, a 404 means "session expired" and makes the client start over)
	McpReply answerLegacyRequest(JsonDocument &response, JsonVariantConst id, const char *method, JsonVariantConst params, const McpRequestHeaders &headers, const McpServerInfo &info) {
		// A 2026-07-28 request without its _meta, or an unknown version in the header
		if (headers.protocolVersion != nullptr && !isLegacyProtocolVersion(headers.protocolVersion)) {
			if (strcmp(headers.protocolVersion, MODERN_PROTOCOL_VERSION) == 0) {
				return writeError(response, id, 400, INVALID_PARAMS, "Missing _meta io.modelcontextprotocol/protocolVersion");
			}
			return writeError(response, id, 400, INVALID_REQUEST, "Unsupported MCP-Protocol-Version");
		}

		// Answer the method
		if (strcmp(method, "initialize") == 0) {
			return answerInitialize(response, id, params, info);
		}
		if (strcmp(method, "ping") == 0) {
			startAnswer(response, id)["result"].to<JsonObject>();
			return replyJson();
		}
		if (strcmp(method, "tools/list") == 0) {
			return answerToolsList(response, id, info);
		}
		if (strcmp(method, "tools/call") == 0) {
			return answerToolsCall(response, id, params, info, ProtocolEra::Legacy);
		}
		if (strcmp(method, "server/discover") == 0) {
			return writeError(response, id, 400, INVALID_PARAMS, "Missing _meta io.modelcontextprotocol/protocolVersion");
		}
		return writeError(response, id, 200, METHOD_NOT_FOUND, "Method not found");
	}
}

// Resolution of a snapshot when the agent doesn't choose one
size_t getDefaultMcpResolutionIndex(size_t maxResolutionIndex) {
	return maxResolutionIndex < SVGA_RESOLUTION_INDEX ? maxResolutionIndex : SVGA_RESOLUTION_INDEX;
}

// Read one JSON-RPC message from the body of a POST, and write the answer into the response
McpReply handleMcpMessage(const char *body, size_t length, const McpRequestHeaders &headers, const McpServerInfo &info, JsonDocument &response) {
	// Parse the body (a const body is copied, so it can be freed right after)
	JsonDocument request;
	if (deserializeJson(request, body, length) != DeserializationError::Ok) {
		return writeError(response, JsonVariantConst(), 400, PARSE_ERROR, "Parse error");
	}

	// One JSON-RPC 2.0 message per request (no batches)
	JsonVariantConst id = request["id"];
	if (!request.is<JsonObjectConst>() || strcmp(request["jsonrpc"] | "", "2.0") != 0) {
		return writeError(response, JsonVariantConst(), 400, INVALID_REQUEST, "Expected one JSON-RPC 2.0 message");
	}

	// A response from the client (it has no method): accepted with no body
	if (!request["method"].is<const char *>()) {
		if (!request["result"].isNull() || !request["error"].isNull()) {
			return { McpReplyKind::Accepted, 202, {} };
		}
		return writeError(response, JsonVariantConst(), 400, INVALID_REQUEST, "Expected a method");
	}

	// A notification (it has no id): accepted with no body
	if (id.isNull()) {
		return { McpReplyKind::Accepted, 202, {} };
	}

	// The id is a string or a whole number
	if (!id.is<const char *>() && !id.is<long long>()) {
		return writeError(response, JsonVariantConst(), 400, INVALID_REQUEST, "The id must be a string or a whole number");
	}

	// Answer by era: a version in _meta means the 2026-07-28 revision
	const char *method = request["method"].as<const char *>();
	JsonVariantConst params = request["params"];
	const char *version = params["_meta"][META_PROTOCOL_VERSION];
	if (version != nullptr) {
		return answerModernRequest(response, id, method, params, version, headers, info);
	}
	return answerLegacyRequest(response, id, method, params, headers, info);
}

// Fill a snapshot result with its details
void buildSnapshotResult(JsonDocument &response, const SnapshotDetails &details) {
	// Describe the snapshot, so the agent knows what it's looking at
	char description[128];
	snprintf(description, sizeof(description), "Snapshot %ux%u, JPEG, %u KB, taken in %lu ms%s.", details.width, details.height, static_cast<unsigned>((details.sizeBytes + 512) / 1024), static_cast<unsigned long>(details.captureTimeMs), details.usedFlash ? " with the flash" : "");

	// Put the description in the text item (the image data stays empty, the web server streams it in)
	response["result"]["content"][0]["text"] = copyText(description);
}

// Turn a prepared result into a tool error
void buildSnapshotError(JsonDocument &response, const char *message) {
	// Keep only the text item, with the message
	JsonArray content = response["result"]["content"];
	content.remove(1);
	content[0]["text"] = copyText(message);
	response["result"]["isError"] = true;
}

// Find where the image data goes in a serialized snapshot result
bool findImageDataPosition(const char *json, size_t length, size_t &dataPosition) {
	// The image is the last item, so its empty data is the last "data":"" of the text (strings in the JSON are escaped, so their quotes can't match)
	constexpr const char *MARKER = "\"data\":\"\"";
	const size_t markerLength = strlen(MARKER);
	for (size_t position = length >= markerLength ? length - markerLength + 1 : 0; position > 0; position--) {
		if (memcmp(json + position - 1, MARKER, markerLength) == 0) {
			dataPosition = position - 1 + markerLength - 1; // Between the two quotes
			return true;
		}
	}
	return false;
}