# Project Overview

This repository contains ESP32 firmware for a Wi-Fi snapshot camera on the AI Thinker ESP32-CAM, with an embedded web UI.

## Structure

- `src/main.cpp`: entry point; initializes serial logging, the camera, Wi-Fi and the web server.
- `src/camera`: starts the OV2640 and takes snapshots (`camera.*`), with the resolutions and the request option parsing kept hardware-free in `capture_options.*`. The pins are in `camera_pins.h`.
- `src/config`: the project name and the shared web server.
- `src/wifi`: passes the camera's names to the gizmo-common Wi-Fi setup (connection, access point fallback, captive portal).
- `src/server`: exposes the AsyncWebServer routes for the camera (`/api/camera`, `/api/capture`) and the embedded assets (the Wi-Fi setup and device routes come from gizmo-common).
- `src/serial`: serial debug logging helpers.
- `src/webui`: generated assets for the embedded front-end.

## Runtime Flow

The firmware boots, starts the camera while the PSRAM is free, connects to Wi-Fi or enables access point mode, and serves the web app and the camera API. A snapshot is taken in the web server task and streamed straight from the frame buffer, which goes back to the camera when the response ends.