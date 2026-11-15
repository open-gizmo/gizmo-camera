# Changelog

What changed in each release. New notes go under **Unreleased** while the work is done; a release moves them under its version, and the release workflow publishes that section as the GitHub release notes (it refuses to release a version without them).

## Unreleased

- The printed enclosure has a bigger micro-USB opening (12 × 8 mm instead of 10 × 5 mm) and a matching notch in the back plate, so cables with a thick plug reach the port. Reprint the shell and the back plate to get it; the firmware does not change.
- The ball socket on the back plate grips the stand's ball harder (0.15 mm instead of 0.10 mm per side), so the camera stays where you turn it even after the joint has worn in. Reprint the back plate to get it.

## v1.1.0 - 2026-11-14

- The **Flash** switch on the Camera page now says that the flash fires during the snapshot: switching it on doesn't light the LED by itself, the next snapshot does. Flash the new firmware to see the hint.
- The **Device** tab shows the address that opens the device (`gizmo-camera.local`). It showed the Wi-Fi's default name (`esp32-XXXXXX.local`), which doesn't open it.
- The router lists the device by its name (`gizmo-camera`) instead of `esp32-XXXXXX`.
- gizmo-common v2.5.2.

## v1.0.0 - 2026-11-13

The first release of Gizmo Camera: an AI Thinker ESP32-CAM that answers every request with a fresh photo.

- JPEG snapshots from 160 × 120 up to 1600 × 1200 (OV2640), with adjustable quality and the on-board flash LED.
- A small HTTP API (`/api/capture`, `/api/camera`) for scripts, browsers and home automation.
- An MCP server at `/mcp` for AI agents, with `take_snapshot` and `get_camera_info`, and a Markdown guide for agents at `/llms.txt`.
- A web app built into the firmware, in English and Italian: the **Camera** tab takes and saves snapshots, the **Device** tab shows the device and restarts it or forgets the network, and the **WiFi** tab changes the network.
- Wi-Fi setup from the phone, through the `Gizmo-Camera` hotspot and its captive portal, then `gizmo-camera.local` on your network.
- Flash it from the browser on [opengizmo.dev](https://opengizmo.dev/projects/gizmo-camera).
