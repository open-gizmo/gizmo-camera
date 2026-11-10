#include "../server.h"
#include <WString.h>
#include "../../camera/camera.h"
#include "../../mcp/base64_stream.h"
#include "../../mcp/mcp_protocol.h"

namespace {
	// JSON-RPC errors the web server sends before the protocol reads the message
	constexpr int PARSE_ERROR = -32700;
	constexpr int INVALID_REQUEST = -32600;
	constexpr int INTERNAL_ERROR = -32603;

	// JSON response of a snapshot result, with the image streamed as base64 straight from the frame buffer (no copy of the photo), giving the frame back to the camera when the request ends (sent, or the client gone)
	class McpSnapshotResponse : public AsyncAbstractResponse {
	public:
		McpSnapshotResponse(camera_fb_t *frame, String json, size_t dataPosition)
			: frame(frame), json(json), dataPosition(dataPosition), imageLength(getBase64Length(frame->len)) {
			_code = 200;
			_contentType = "application/json";
			_contentLength = json.length() + imageLength;
			_sendContentLength = true;
			_chunked = false;
		}

		// Give the snapshot back to the camera
		~McpSnapshotResponse() override {
			releaseSnapshot(frame);
		}

		// The frame is there until the response ends
		bool _sourceValid() const override {
			return true;
		}

		// Fill the next chunk: the JSON before the image, the base64 of the image, then the JSON after it
		size_t _fillBuffer(uint8_t *buffer, size_t maxLength) override {
			size_t written = 0;
			while (written < maxLength && position < _contentLength) {
				const size_t room = maxLength - written;
				char *output = reinterpret_cast<char *>(buffer + written);
				size_t count = 0;

				// Copy the JSON before the image
				if (position < dataPosition) {
					count = min(room, dataPosition - position);
					memcpy(output, json.c_str() + position, count);
				} else if (position < dataPosition + imageLength) {
					// Encode the next part of the image
					count = writeBase64Slice(frame->buf, frame->len, position - dataPosition, output, room);
				} else {
					// Copy the JSON after the image
					const size_t jsonPosition = position - imageLength;
					count = min(room, json.length() - jsonPosition);
					memcpy(output, json.c_str() + jsonPosition, count);
				}

				// Move on
				written += count;
				position += count;
			}
			return written;
		}

	private:
		camera_fb_t *frame;
		String json; // The whole result with an empty image data
		size_t dataPosition; // Where the image data goes in the JSON
		size_t imageLength; // Length of the base64 of the image
		size_t position = 0; // Bytes of the response already given to the web server
	};

	// Send a JSON document with a status
	void sendJson(AsyncWebServerRequest *request, uint16_t statusCode, const JsonDocument &doc) {
		AsyncResponseStream *response = request->beginResponseStream("application/json");
		response->setCode(statusCode);
		response->addHeader("Cache-Control", "no-store");
		serializeJson(doc, *response);
		request->send(response);
	}

	// Send a JSON-RPC error before the message is read (its id is unknown)
	void sendJsonRpcError(AsyncWebServerRequest *request, uint16_t statusCode, int code, const char *message) {
		JsonDocument doc;
		doc["jsonrpc"] = "2.0";
		doc["id"] = nullptr;
		doc["error"]["code"] = code;
		doc["error"]["message"] = message;
		sendJson(request, statusCode, doc);
	}

	// Read a request header (nullptr if it's missing)
	const char *getHeaderValue(AsyncWebServerRequest *request, const char *name) {
		const AsyncWebHeader *header = request->getHeader(name);
		return header != nullptr ? header->value().c_str() : nullptr;
	}

	// Who the server is and what the camera can do
	McpServerInfo buildServerInfo() {
		const CameraInfo cameraInfo = getCameraInfo();
		return { PROJECT_ID, PROJECT_NAME, PIO_FIRMWARE_VERSION, cameraInfo.isAvailable, cameraInfo.sensorName, cameraInfo.maxResolutionIndex, true };
	}

	// Message for the agent when the camera couldn't take the snapshot
	const char *getSnapshotErrorMessage(SnapshotResult result) {
		switch (result) {
			// Another snapshot is being taken or sent
			case SnapshotResult::Busy:
				return "The camera is taking another snapshot. Wait a moment and try again.";

			// The sensor didn't start
			case SnapshotResult::Unavailable:
				return "The camera sensor did not start, so no snapshot can be taken.";

			// The sensor gave no frame
			default:
				return "The camera could not take the snapshot. Try again.";
		}
	}

	// Take the snapshot of a take_snapshot call, and stream the result with the image
	void sendSnapshot(AsyncWebServerRequest *request, const CaptureOptions &options, JsonDocument &response) {
		// Take the snapshot, or tell the agent why it couldn't be taken
		camera_fb_t *frame = nullptr;
		uint32_t captureTimeMs = 0;
		const SnapshotResult result = takeSnapshot(options, frame, captureTimeMs);
		if (result != SnapshotResult::Ok) {
			buildSnapshotError(response, getSnapshotErrorMessage(result));
			sendJson(request, 200, response);
			return;
		}

		// Describe the snapshot, and find where its image goes in the result
		buildSnapshotResult(response, { static_cast<uint16_t>(frame->width), static_cast<uint16_t>(frame->height), frame->len, captureTimeMs, options.useFlash });
		String json;
		serializeJson(response, json);
		size_t dataPosition = 0;
		if (!findImageDataPosition(json.c_str(), json.length(), dataPosition)) {
			releaseSnapshot(frame);
			sendJsonRpcError(request, 500, INTERNAL_ERROR, "Internal error");
			return;
		}

		// Stream the result with the image
		AsyncWebServerResponse *snapshotResponse = new McpSnapshotResponse(frame, json, dataPosition);
		snapshotResponse->addHeader("Cache-Control", "no-store");
		request->send(snapshotResponse);
	}

	// Answer one MCP message, once its body is complete
	void handleMcpRequest(AsyncWebServerRequest *request) {
		// Refuse other websites and other host names (DNS rebinding), and anything but the Wi-Fi setup through the hotspot
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Refuse bodies bigger than any MCP message to the camera
		if (request->contentLength() > gizmo::MAX_REQUEST_BODY_SIZE) {
			sendJsonRpcError(request, 413, INVALID_REQUEST, "Request too large");
			return;
		}

		// Every message is a JSON body
		const char *body = static_cast<const char *>(request->_tempObject);
		if (body == nullptr) {
			sendJsonRpcError(request, 400, PARSE_ERROR, "Missing JSON-RPC message");
			return;
		}

		// Read the message (the protocol copies what it keeps, so the body can be freed right after)
		const McpRequestHeaders headers = { getHeaderValue(request, "MCP-Protocol-Version"), getHeaderValue(request, "Mcp-Method"), getHeaderValue(request, "Mcp-Name") };
		JsonDocument response;
		const McpReply reply = handleMcpMessage(body, request->contentLength(), headers, buildServerInfo(), response);
		gizmo::releaseRequestBody(request);

		// Send the answer
		switch (reply.kind) {
			// A notification or a response of the client: accepted, with no body
			case McpReplyKind::Accepted:
				request->send(202);
				break;

			// A tool call that takes a snapshot
			case McpReplyKind::Snapshot:
				sendSnapshot(request, reply.snapshotOptions, response);
				break;

			// Any other answer
			default:
				sendJson(request, reply.httpStatus, response);
				break;
		}
	}
}

// Serve the MCP server of the camera (Streamable HTTP, one JSON-RPC message per POST, answered with JSON)
void setupMcpRoute() {
	// POST: gather the body, then answer the message
	server.on("/mcp", HTTP_POST, handleMcpRequest, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
		gizmo::accumulateRequestBody(request, data, len, index, total); // A body that is too large is left out, and refused once the request is complete
	});

	// Any other method: the server offers no stream to GET, and has no session to DELETE
	server.on("/mcp", HTTP_ANY, [](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(405);
		response->addHeader("Allow", "POST");
		request->send(response);
	});
}