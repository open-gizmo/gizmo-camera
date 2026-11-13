# Changelog

What changed in each release. New notes go under **Unreleased** while the work is done; a release moves them under its version, and the release workflow publishes that section as the GitHub release notes (it refuses to release a version without them).

## Unreleased

## v1.0.0 - 2026-11-13

The first release of Gizmo Camera: an AI Thinker ESP32-CAM that answers every request with a fresh photo.

- JPEG snapshots from 160 × 120 up to 1600 × 1200 (OV2640), with adjustable quality and the on-board flash LED.
- A small HTTP API (`/api/capture`, `/api/camera`) for scripts, browsers and home automation.
- An MCP server at `/mcp` for AI agents, with `take_snapshot` and `get_camera_info`, and a Markdown guide for agents at `/llms.txt`.
- A web app built into the firmware, in English and Italian: the **Camera** tab takes and saves snapshots, the **Device** tab shows the device and restarts it or forgets the network, and the **WiFi** tab changes the network.
- Wi-Fi setup from the phone, through the `Gizmo-Camera` hotspot and its captive portal, then `gizmo-camera.local` on your network.
- Flash it from the browser on [opengizmo.dev](https://opengizmo.dev/projects/gizmo-camera).
