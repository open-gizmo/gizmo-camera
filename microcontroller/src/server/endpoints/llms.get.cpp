#include "../server.h"
#include "../../mcp/agent_guide.h"

namespace {
	// Write a piece of the guide into the response
	void writeGuidePart(const char *text, size_t length, void *context) {
		static_cast<AsyncResponseStream *>(context)->write(reinterpret_cast<const uint8_t *>(text), length);
	}
}

// Send the guide for AI agents (Markdown), with the address the client used to reach the device
void sendAgentGuide(AsyncWebServerRequest *request) {
	AsyncResponseStream *response = request->beginResponseStream("text/markdown; charset=utf-8");
	response->addHeader("Vary", "Accept"); // The root answers HTML or Markdown depending on the Accept header
	writeAgentGuide(request->host().c_str(), writeGuidePart, response);
	request->send(response);
}

// Serve the guide for AI agents at /llms.txt, where agents look for one
void setupAgentGuideRoute() {
	server.on("/llms.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
		sendAgentGuide(request);
	});
}