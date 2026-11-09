#include "wifi.h"
#include <WiFi.h>
#include <gizmo/wifi_setup.h>
#include "../config/config.h"

/*
The Wi-Fi connection, the setup hotspot and its web API come from gizmo-common.
This module only gives them the camera's names.
*/

namespace {
	// Wi-Fi setup options of the camera
	const gizmo::WifiSetupOptions WIFI_SETUP_OPTIONS = {
		"Gizmo-Camera", // Setup hotspot name
		"gizmo-camera", // mDNS hostname, always the project name (http://gizmo-camera.local)
		"/ap-setup", // Setup page of the web app
		"/_nuxt/" // Files of the web app, never redirected
	};
}

// Start the Wi-Fi setup and register its web API (call before the web server starts)
void setupWiFi() {
	gizmo::setupWifi(server, WIFI_SETUP_OPTIONS);
	WiFi.setSleep(false); // Keep the radio awake: with power saving on, a snapshot of a few hundred KB takes several times longer to send
}

// Manage the Wi-Fi connection and the setup hotspot
void manageWiFiConnection() {
	gizmo::manageWifi();
}