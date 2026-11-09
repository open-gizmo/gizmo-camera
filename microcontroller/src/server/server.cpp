#include "server.h"
#include <gizmo/device.h>
#include "../config/config.h"

// Firmware version shown in the web app (the release build sets it)
#ifndef PIO_FIRMWARE_VERSION
#define PIO_FIRMWARE_VERSION "dev"
#endif

// Setup routes
void setupRoutes() {
	// Setup the routes (the device details, restart and forget network routes come from gizmo-common)
	gizmo::setupDeviceApi(server, { PROJECT_NAME, PIO_FIRMWARE_VERSION });
	setupCameraGetRoute();
	setupCaptureGetRoute();
	setupCaptivePortalRoute();
}

// Setup server
void setupServer() {
	setupRoutes(); // Setup routes
	server.begin(); // Start the server
}