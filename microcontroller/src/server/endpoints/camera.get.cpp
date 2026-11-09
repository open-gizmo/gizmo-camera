#include "../server.h"
#include <ArduinoJson.h>
#include "../../camera/camera.h"

namespace {
	// Build the JSON response with what the camera can do
	JsonDocument buildCameraData(const CameraInfo &cameraInfo) {
		JsonDocument doc;
		JsonObject data = doc.to<JsonObject>();

		// Add the sensor and whether it works
		data["available"] = cameraInfo.isAvailable;
		data["sensor"] = cameraInfo.sensorName;

		// Add the resolutions the sensor can take, smallest to largest (the largest is the default)
		JsonArray resolutions = data["resolutions"].to<JsonArray>();
		for (size_t index = 0; index <= cameraInfo.maxResolutionIndex; index++) {
			JsonObject resolution = resolutions.add<JsonObject>();
			resolution["name"] = RESOLUTIONS[index].name;
			resolution["width"] = RESOLUTIONS[index].width;
			resolution["height"] = RESOLUTIONS[index].height;
		}
		data["defaultResolution"] = RESOLUTIONS[cameraInfo.maxResolutionIndex].name;

		// Add the JPEG quality range (lower numbers mean better quality)
		JsonObject quality = data["quality"].to<JsonObject>();
		quality["min"] = JPEG_QUALITY_MIN;
		quality["max"] = JPEG_QUALITY_MAX;
		quality["default"] = JPEG_QUALITY_DEFAULT;

		// Add the flash LED (always on the ESP32-CAM)
		data["hasFlash"] = true;

		// Return the built JSON document
		return doc;
	}
}

// Get what the camera can do: its sensor, resolutions, quality range and flash
void setupCameraGetRoute() {
	server.on("/api/camera", HTTP_GET, [](AsyncWebServerRequest *request) {
		// Validate CORS
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Send what the camera can do
		JsonDocument doc = buildCameraData(getCameraInfo());
		sendSuccessResponse(request, 200, &doc);
	});
}