#include "serial/serial.h"
#include "camera/camera.h"
#include "wifi/wifi.h"
#include "server/server.h"
#include <gizmo/device.h>

// Setup
void setup() {
	initSerial(); // Init serial
	setupCamera(); // Start the camera first, while the PSRAM is free for its frame buffers
	setupWiFi(); // Start the Wi-Fi setup and its web API
	setupServer(); // Setup server

	// No factory reset with the BOOT button: on the ESP32-CAM, GPIO0 is the clock of the camera (the Wi-Fi can be forgotten from the web app, and a missing network opens the setup hotspot again)
	manageWiFiConnection(); // Manage the WiFi connection
}

// Loop
void loop() {
	// Restart the device if the web server asked for it
	gizmo::manageDevice();

	// Manage the WiFi connection
	manageWiFiConnection();

	// Give the CPU back between checks (the ESP32-CAM runs warm)
	delay(10);
}