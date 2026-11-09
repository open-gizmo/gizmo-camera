# Copilot Instructions

## What This Project Is

Gizmo Camera is a two-part project that turns an AI Thinker ESP32-CAM into a Wi-Fi snapshot camera.

- `microcontroller/` contains the PlatformIO firmware for the ESP32-CAM (OV2640 sensor, flash LED, 4 MB PSRAM). It manages the camera, the Wi-Fi setup (from gizmo-common), and the embedded HTTP server with the camera API.
- `webapp/` contains the Nuxt 4 web app, embedded in the firmware, used to set up the Wi-Fi and take snapshots.

The firmware is the runtime of the physical camera. The webapp mirrors its API, and the mock device API in `webapp/server/` answers like the firmware during development.

## Core Concepts

- Firmware code is organized by responsibility under `microcontroller/src/` (`camera`, `config`, `serial`, `server`, `wifi`).
- The Wi-Fi setup, the captive portal, the device API, logging and the embedded assets come from the shared [gizmo-common](https://github.com/open-gizmo/gizmo-common) library, pinned to an exact release.
- The web interface is a Nuxt app under `webapp/app/` that extends the gizmo-common web layer (dashboard layout, Wi-Fi pages, device card).
- Changes to the camera API (`/api/camera`, `/api/capture`) need coordinated updates in the firmware, the webapp, and the mock device API.

## Working Rules

1. Keep the split between firmware and webapp clear. Do not mix embedded-device logic into the frontend or frontend-specific behavior into firmware modules.
2. Preserve existing module boundaries in `microcontroller/src/`; prefer extending the relevant subsystem instead of adding cross-cutting logic to `main.cpp`.
3. Keep hardware-free logic (like `camera/capture_options.*`) free of hardware headers, and test it with `pio test -e native`.
4. When changing routes, payloads, or validation rules, update the firmware handlers, the mock device API and the webapp together, and translate every new error ID.
5. Never read GPIO0 as a button: on the ESP32-CAM it is the clock of the camera.
6. Prefer minimal, hardware-aware changes in firmware: avoid unnecessary dynamic allocation, blocking behavior, and broad refactors unless they are required.