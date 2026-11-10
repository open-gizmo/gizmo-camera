#ifndef CONFIG_H
#define CONFIG_H

#include <ESPAsyncWebServer.h>

// Project name, shown by the device API and the web app, and its machine name (the mDNS hostname and the MCP server name)
constexpr const char *PROJECT_NAME = "Gizmo Camera";
constexpr const char *PROJECT_ID = "gizmo-camera";

// Firmware version shown in the web app and to MCP clients (the release build sets it)
#ifndef PIO_FIRMWARE_VERSION
#define PIO_FIRMWARE_VERSION "dev"
#endif

// Web server shared by the Wi-Fi setup, the device API and the camera API
extern AsyncWebServer server;

#endif