#ifndef CONFIG_H
#define CONFIG_H

#include <ESPAsyncWebServer.h>

// Project name, shown by the device API and the web app
constexpr const char *PROJECT_NAME = "Gizmo Camera";

// Web server shared by the Wi-Fi setup, the device API and the camera API
extern AsyncWebServer server;

#endif