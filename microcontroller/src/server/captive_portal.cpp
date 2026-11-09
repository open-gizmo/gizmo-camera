#include <gizmo/embedded_assets.h>
#include <gizmo/wifi_setup.h>
#include "server.h"
#include "../webui/generated_assets.h"

// Setup the captive portal route to handle all unmatched requests
void setupCaptivePortalRoute() {
	server.onNotFound([](AsyncWebServerRequest *request) {
		// Redirect hotspot clients to the Wi-Fi setup page
		if (gizmo::handleCaptivePortalRequest(request)) {
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