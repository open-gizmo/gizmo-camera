#include <gizmo/embedded_assets.h>
#include <gizmo/wifi_setup.h>
#include "server.h"
#include "../webui/generated_assets.h"
#include "../mcp/agent_guide.h"

namespace {
	// Read the Accept header of a request (nullptr if it has none)
	const char *getAcceptHeader(AsyncWebServerRequest *request) {
		const AsyncWebHeader *header = request->getHeader("Accept");
		return header != nullptr ? header->value().c_str() : nullptr;
	}
}

// Setup the captive portal route to handle all unmatched requests
void setupCaptivePortalRoute() {
	server.onNotFound([](AsyncWebServerRequest *request) {
		// Redirect hotspot clients to the Wi-Fi setup page
		if (gizmo::handleCaptivePortalRequest(request)) {
			return;
		}

		// Answer the root with the guide for AI agents when the client asks for Markdown (browsers get the web app)
		if (request->url() == "/" && isMarkdownPreferred(getAcceptHeader(request))) {
			sendAgentGuide(request);
			return;
		}

		// Serve the web app, or its 404 page for a missing page
		if (gizmo::handleEmbeddedAssetRequest(request, EMBEDDED_ASSETS, EMBEDDED_ASSET_COUNT)) {
			return;
		}

		// If no route matched, return 404
		request->send(404);
	});
}