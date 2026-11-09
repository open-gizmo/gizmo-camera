#ifndef SERVER_H
#define SERVER_H

#include <ArduinoJson.h>
#include <gizmo/http.h>
#include "../config/config.h"

// Standard API responses and CORS checks come from gizmo-common
using gizmo::ensureCorsAllowed;
using gizmo::sendCorsPreflightResponse;
using gizmo::sendErrorResponse;
using gizmo::sendSuccessResponse;

// Web server routes (the Wi-Fi setup routes are registered by gizmo-common)
void setupCameraGetRoute();
void setupCaptureGetRoute();
void setupCaptivePortalRoute();

// Server setup
void setupRoutes();
void setupServer();

#endif