#include "server.h"
#include <gizmo/device.h>
#include "../config/config.h"

// Setup routes
void setupRoutes() {
	// Setup the routes (the device details, restart and forget network routes come from gizmo-common)
	gizmo::setupDeviceApi(server, { PROJECT_NAME, PIO_FIRMWARE_VERSION });
	setupCameraGetRoute();
	setupCaptureGetRoute();
	setupMcpRoute();
	setupAgentGuideRoute();
	setupCaptivePortalRoute();
}

// Setup server
void setupServer() {
	setupRoutes(); // Setup routes
	server.begin(); // Start the server
}