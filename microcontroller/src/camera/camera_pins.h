#ifndef CAMERA_PINS_H
#define CAMERA_PINS_H

// AI Thinker ESP32-CAM: the OV2640 ribbon connector and the flash LED are wired on the board
#if !defined(ARDUINO_ESP32_DEV) || !defined(BOARD_HAS_PSRAM)
#error "The camera runs on the AI Thinker ESP32-CAM (pio run -e release)"
#endif

// Camera control
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1 // Not wired: the sensor resets through its power-down pin
#define XCLK_GPIO_NUM 0 // Also the BOOT strapping pin, so the firmware never reads it as a button
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

// Camera data and sync
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Flash LED (bright white, next to the camera)
#define FLASH_LED_GPIO_NUM 4

#endif