#include "../server.h"
#include "../../camera/camera.h"

namespace {
	// JPEG response that streams the snapshot straight from the frame buffer, and gives it back to the camera when the request ends (sent, or the client gone)
	class SnapshotResponse : public AsyncProgmemResponse {
	public:
		explicit SnapshotResponse(camera_fb_t *frame)
			: AsyncProgmemResponse(200, "image/jpeg", frame->buf, frame->len), frame(frame) {}

		// Give the snapshot back to the camera
		~SnapshotResponse() override {
			releaseSnapshot(frame);
		}

	private:
		camera_fb_t *frame;
	};

	// Read a query parameter, or nullptr when the request leaves it out (the pointer stays valid while the request lives)
	const char *getQueryValue(AsyncWebServerRequest *request, const char *name) {
		const AsyncWebParameter *parameter = request->getParam(name);
		return parameter != nullptr ? parameter->value().c_str() : nullptr;
	}

	// Send the error of refused snapshot options
	void sendCaptureOptionsError(AsyncWebServerRequest *request, CaptureOptionsError error) {
		switch (error) {
			// Not a resolution name
			case CaptureOptionsError::InvalidResolution:
				sendErrorResponse(request, 400, "invalid_resolution", "Unknown resolution");
				break;

			// Bigger than the sensor can take
			case CaptureOptionsError::ResolutionNotSupported:
				sendErrorResponse(request, 400, "resolution_not_supported", "Resolution not supported by the sensor");
				break;

			// Not a quality from 4 to 63
			case CaptureOptionsError::InvalidQuality:
				sendErrorResponse(request, 400, "invalid_quality", "Quality must be a whole number from 4 to 63");
				break;

			// Not a flash switch
			default:
				sendErrorResponse(request, 400, "invalid_flash", "Flash must be 0 or 1");
				break;
		}
	}

	// Send the error of a snapshot the camera couldn't take
	void sendSnapshotError(AsyncWebServerRequest *request, SnapshotResult result) {
		switch (result) {
			// Another snapshot is being taken or sent
			case SnapshotResult::Busy:
				sendErrorResponse(request, 503, "camera_busy", "The camera is taking another snapshot");
				break;

			// The sensor didn't start
			case SnapshotResult::Unavailable:
				sendErrorResponse(request, 503, "camera_unavailable", "The camera sensor did not start");
				break;

			// The sensor gave no frame
			default:
				sendErrorResponse(request, 500, "capture_failed", "The camera could not take the snapshot");
				break;
		}
	}
}

// Take a snapshot and send it as a JPEG (GET /api/capture?resolution=SVGA&quality=10&flash=1, every parameter optional)
void setupCaptureGetRoute() {
	server.on("/api/capture", HTTP_GET, [](AsyncWebServerRequest *request) {
		// Validate CORS
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Read the snapshot options
		CaptureOptions options;
		const CaptureOptionsError optionsError = parseCaptureOptions(getQueryValue(request, "resolution"), getQueryValue(request, "quality"), getQueryValue(request, "flash"), getCameraInfo().maxResolutionIndex, options);
		if (optionsError != CaptureOptionsError::None) {
			sendCaptureOptionsError(request, optionsError);
			return;
		}

		// Take the snapshot
		camera_fb_t *frame = nullptr;
		uint32_t captureTimeMs = 0;
		const SnapshotResult result = takeSnapshot(options, frame, captureTimeMs);
		if (result != SnapshotResult::Ok) {
			sendSnapshotError(request, result);
			return;
		}

		// Describe the snapshot in the headers: its size and how long it took
		char resolutionHeader[16];
		char captureTimeHeader[12];
		snprintf(resolutionHeader, sizeof(resolutionHeader), "%ux%u", frame->width, frame->height);
		snprintf(captureTimeHeader, sizeof(captureTimeHeader), "%lu", static_cast<unsigned long>(captureTimeMs));

		// Send the snapshot (never cached: every request is a new photo)
		SnapshotResponse *response = new SnapshotResponse(frame);
		response->addHeader("Content-Disposition", "inline; filename=snapshot.jpg");
		response->addHeader("Cache-Control", "no-store");
		response->addHeader("X-Resolution", resolutionHeader);
		response->addHeader("X-Capture-Time-Ms", captureTimeHeader);
		request->send(response);
	});
}