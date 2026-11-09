#include "camera.h"
#include <Arduino.h>
#include <atomic>
#include "camera_pins.h"
#include "../serial/serial.h"

namespace {
	// Frame sizes of the sensor, in the order of RESOLUTIONS
	constexpr framesize_t FRAME_SIZES[] = {
		FRAMESIZE_QQVGA,
		FRAMESIZE_QVGA,
		FRAMESIZE_CIF,
		FRAMESIZE_VGA,
		FRAMESIZE_SVGA,
		FRAMESIZE_XGA,
		FRAMESIZE_HD,
		FRAMESIZE_SXGA,
		FRAMESIZE_UXGA,
		FRAMESIZE_FHD,
		FRAMESIZE_QXGA,
		FRAMESIZE_QHD,
		FRAMESIZE_WQXGA,
		FRAMESIZE_QSXGA
	};
	static_assert(sizeof(FRAME_SIZES) / sizeof(FRAME_SIZES[0]) == RESOLUTION_COUNT, "FRAME_SIZES must list every resolution of RESOLUTIONS");

	// Index in RESOLUTIONS of UXGA, the largest resolution of the OV2640
	constexpr size_t UXGA_RESOLUTION_INDEX = 8;

	// Sensor clock (20 MHz, the usual one for the OV2640)
	constexpr int XCLK_FREQUENCY_HZ = 20000000;

	// Time the flash LED is on before the frame, so the sensor's exposure adapts to it
	constexpr uint32_t FLASH_WARMUP_MS = 150;

	// Frames to throw away before a snapshot: the buffer holds one taken right after the last request, and the first frames after a settings change can be corrupted or use the old settings
	constexpr uint8_t STALE_FRAMES = 1;
	constexpr uint8_t SETTINGS_CHANGE_FRAMES = 3;
	constexpr uint8_t FLASH_FRAMES = 2;

	// State of the camera (written in setup, then only by the holder of the snapshot)
	CameraInfo cameraInfo = { false, "unknown", UXGA_RESOLUTION_INDEX };
	size_t currentResolutionIndex = UXGA_RESOLUTION_INDEX;
	uint8_t currentQuality = JPEG_QUALITY_DEFAULT;

	// Whether a snapshot is being taken or sent (the web server takes it, and the response gives it back once sent)
	std::atomic<bool> isSnapshotHeld(false);

	// Start the sensor with frame buffers sized for a resolution
	esp_err_t initSensor(framesize_t frameSize) {
		camera_config_t config = {};

		// Pins of the ESP32-CAM
		config.pin_d0 = Y2_GPIO_NUM;
		config.pin_d1 = Y3_GPIO_NUM;
		config.pin_d2 = Y4_GPIO_NUM;
		config.pin_d3 = Y5_GPIO_NUM;
		config.pin_d4 = Y6_GPIO_NUM;
		config.pin_d5 = Y7_GPIO_NUM;
		config.pin_d6 = Y8_GPIO_NUM;
		config.pin_d7 = Y9_GPIO_NUM;
		config.pin_xclk = XCLK_GPIO_NUM;
		config.pin_pclk = PCLK_GPIO_NUM;
		config.pin_vsync = VSYNC_GPIO_NUM;
		config.pin_href = HREF_GPIO_NUM;
		config.pin_sccb_sda = SIOD_GPIO_NUM;
		config.pin_sccb_scl = SIOC_GPIO_NUM;
		config.pin_pwdn = PWDN_GPIO_NUM;
		config.pin_reset = RESET_GPIO_NUM;

		// Clock of the sensor
		config.xclk_freq_hz = XCLK_FREQUENCY_HZ;
		config.ledc_channel = LEDC_CHANNEL_0;
		config.ledc_timer = LEDC_TIMER_0;

		// One JPEG frame buffer in PSRAM, filled only when empty (so a snapshot never waits for a whole new frame queue)
		config.pixel_format = PIXFORMAT_JPEG;
		config.frame_size = frameSize;
		config.jpeg_quality = JPEG_QUALITY_DEFAULT;
		config.fb_count = 1;
		config.fb_location = CAMERA_FB_IN_PSRAM;
		config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

		// Start the camera driver
		return esp_camera_init(&config);
	}

	// Find the resolution of a frame size of the sensor (UXGA if it isn't in the list)
	size_t findFrameSizeResolutionIndex(framesize_t frameSize) {
		for (size_t index = 0; index < RESOLUTION_COUNT; index++) {
			if (FRAME_SIZES[index] == frameSize) {
				return index;
			}
		}
		return UXGA_RESOLUTION_INDEX;
	}

	// Start the sensor at its largest resolution, and read its name
	bool startSensor() {
		// Start with buffers for UXGA, the largest resolution of the OV2640
		esp_err_t error = initSensor(FRAMESIZE_UXGA);
		if (error != ESP_OK) {
			printLogfln("Camera init failed: 0x%x", error);
			return false;
		}

		// Read the sensor's name and largest resolution
		sensor_t *sensor = esp_camera_sensor_get();
		const camera_sensor_info_t *sensorInfo = esp_camera_sensor_get_info(&sensor->id);
		if (sensorInfo != nullptr) {
			cameraInfo.sensorName = sensorInfo->name;
			cameraInfo.maxResolutionIndex = findFrameSizeResolutionIndex(sensorInfo->max_size);
		}

		// A bigger sensor fitted on the board (OV3660, OV5640) needs buffers sized for its own largest resolution
		if (cameraInfo.maxResolutionIndex > UXGA_RESOLUTION_INDEX) {
			esp_camera_deinit();
			error = initSensor(FRAME_SIZES[cameraInfo.maxResolutionIndex]);
			if (error != ESP_OK) {
				printLogfln("Camera init at the largest resolution failed: 0x%x, using UXGA", error);
				cameraInfo.maxResolutionIndex = UXGA_RESOLUTION_INDEX;
				if (initSensor(FRAMESIZE_UXGA) != ESP_OK) {
					return false;
				}
			}
			sensor = esp_camera_sensor_get();
		}

		// The OV3660 is mounted upside down compared to the OV2640
		if (sensor->id.PID == OV3660_PID) {
			sensor->set_vflip(sensor, 1);
		}

		// Remember the settings the sensor started with
		currentResolutionIndex = cameraInfo.maxResolutionIndex;
		currentQuality = JPEG_QUALITY_DEFAULT;
		return true;
	}

	// Throw away frames that are stale or taken with the old settings
	void discardFrames(uint8_t count) {
		for (uint8_t index = 0; index < count; index++) {
			camera_fb_t *frame = esp_camera_fb_get();
			if (frame != nullptr) {
				esp_camera_fb_return(frame);
			}
		}
	}

	// Apply the resolution and quality of a snapshot, returning how many frames to throw away before it
	uint8_t applySnapshotSettings(const CaptureOptions &options) {
		sensor_t *sensor = esp_camera_sensor_get();
		uint8_t framesToDiscard = STALE_FRAMES;

		// Change the resolution
		if (options.resolutionIndex != currentResolutionIndex) {
			sensor->set_framesize(sensor, FRAME_SIZES[options.resolutionIndex]);
			currentResolutionIndex = options.resolutionIndex;
			framesToDiscard = SETTINGS_CHANGE_FRAMES;
		}

		// Change the quality
		if (options.quality != currentQuality) {
			sensor->set_quality(sensor, options.quality);
			currentQuality = options.quality;
			framesToDiscard = SETTINGS_CHANGE_FRAMES;
		}

		// Return the number of frames to throw away
		return framesToDiscard;
	}
}

// Start the camera and the flash LED, and take a test snapshot
void setupCamera() {
	// Keep the flash LED off
	pinMode(FLASH_LED_GPIO_NUM, OUTPUT);
	digitalWrite(FLASH_LED_GPIO_NUM, LOW);

	// The frame buffers live in PSRAM, so without it only the smallest frames fit
	if (!psramFound()) {
		printLogln("PSRAM not found: the camera may fail to start");
	}

	// Start the sensor (without it, the web app still runs and reports the camera as unavailable)
	cameraInfo.isAvailable = startSensor();
	if (!cameraInfo.isAvailable) {
		printLogln("Camera not available: check the ribbon cable of the sensor");
		return;
	}
	printLogfln("Camera ready: sensor %s, up to %s", cameraInfo.sensorName, RESOLUTIONS[cameraInfo.maxResolutionIndex].name);

	// Take a test snapshot, to show in the log that the sensor works
	const uint32_t startedAt = millis();
	camera_fb_t *frame = esp_camera_fb_get();
	if (frame == nullptr) {
		printLogln("Test snapshot failed");
		return;
	}
	printLogfln("Test snapshot: %ux%u, %u bytes, %lu ms", frame->width, frame->height, frame->len, millis() - startedAt);
	esp_camera_fb_return(frame);
}

// Get what the camera reports about itself
CameraInfo getCameraInfo() {
	return cameraInfo;
}

// Take a snapshot
SnapshotResult takeSnapshot(const CaptureOptions &options, camera_fb_t *&frame, uint32_t &captureTimeMs) {
	// Nothing to take without a sensor
	if (!cameraInfo.isAvailable) {
		return SnapshotResult::Unavailable;
	}

	// Take the camera, unless another snapshot holds it
	bool wasHeld = false;
	if (!isSnapshotHeld.compare_exchange_strong(wasHeld, true)) {
		return SnapshotResult::Busy;
	}

	// Apply the settings of the snapshot
	const uint32_t startedAt = millis();
	uint8_t framesToDiscard = applySnapshotSettings(options);

	// Turn the flash LED on, and give the exposure a moment to adapt (the snapshot takes a few hundred ms anyway)
	if (options.useFlash) {
		digitalWrite(FLASH_LED_GPIO_NUM, HIGH);
		delay(FLASH_WARMUP_MS);
		framesToDiscard = max(framesToDiscard, FLASH_FRAMES);
	}

	// Throw away the stale frames, then take the snapshot
	discardFrames(framesToDiscard);
	frame = esp_camera_fb_get();

	// Turn the flash LED off
	if (options.useFlash) {
		digitalWrite(FLASH_LED_GPIO_NUM, LOW);
	}

	// Give the camera back if the sensor gave no frame
	if (frame == nullptr) {
		isSnapshotHeld = false;
		return SnapshotResult::Failed;
	}

	// Return the snapshot, held until it's released
	captureTimeMs = millis() - startedAt;
	printLogfln("Snapshot: %ux%u, %u bytes, %lu ms", frame->width, frame->height, frame->len, static_cast<unsigned long>(captureTimeMs));
	return SnapshotResult::Ok;
}

// Give a snapshot back to the camera once it's sent
void releaseSnapshot(camera_fb_t *frame) {
	if (frame != nullptr) {
		esp_camera_fb_return(frame);
	}
	isSnapshotHeld = false;
}