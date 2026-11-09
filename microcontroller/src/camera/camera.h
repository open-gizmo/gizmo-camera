#ifndef CAMERA_H
#define CAMERA_H

#include <esp_camera.h>
#include "capture_options.h"

// What the camera reports about itself
struct CameraInfo {
	bool isAvailable; // False when the sensor didn't start (the web API then answers camera_unavailable)
	const char *sensorName; // For example "OV2640"
	size_t maxResolutionIndex; // Index in RESOLUTIONS of the largest resolution of the sensor
};

// Why a snapshot couldn't be taken
enum class SnapshotResult : uint8_t {
	Ok,
	Busy, // Another snapshot is being taken or sent
	Unavailable, // The sensor didn't start
	Failed // The sensor gave no frame
};

// Start the camera and the flash LED, and take a test snapshot (the firmware keeps running without a camera, so the web app can still say what's wrong)
void setupCamera();

// Get what the camera reports about itself
CameraInfo getCameraInfo();

// Take a snapshot: on Ok, frame holds it until releaseSnapshot, and only one snapshot can be held at a time (safe to call from the web server task)
SnapshotResult takeSnapshot(const CaptureOptions &options, camera_fb_t *&frame, uint32_t &captureTimeMs);

// Give a snapshot back to the camera once it's sent
void releaseSnapshot(camera_fb_t *frame);

#endif