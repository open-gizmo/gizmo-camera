## Embedded Web UI Pipeline

The Nuxt app is built into the firmware as gzipped C++ arrays, with the shared tools of [gizmo-common](https://github.com/open-gizmo/gizmo-common).

### Commands

From [webapp/package.json](../webapp/package.json):

```bash
npm run build:embedded
```

This does two things:

1. Runs `nuxt generate`.
2. Runs `gizmo-embed`, which turns the static output into C++ files under `microcontroller/src/webui/`.

The `gizmo-embed` tool comes from the `gizmo-common` dev dependency, pinned to the same version as the library in `microcontroller/platformio.ini`. Keep the two pins in step.

### Generated Files

The tool writes:

- `microcontroller/src/webui/generated_assets.h`
- `microcontroller/src/webui/generated_assets.cpp`

They are ignored by git: CI and the release workflow generate them before building the firmware.

### Firmware Integration

[captive_portal.cpp](../microcontroller/src/server/captive_portal.cpp) serves the files with `gizmo::handleEmbeddedAssetRequest` in the server's not-found handler, after the captive portal redirect:

- pages are served at their folder, with or without a trailing slash
- every file is sent gzipped
- the hashed files under `/_nuxt/` are cached for a year, while pages are checked again on every load
- a missing page gets the prerendered `/404.html`, and a missing API call or file gets a plain 404
- JSON endpoints live under `/api`, so page routes and API routes don't overlap

### Deployment Flow

When the web UI changes:

1. run `npm run build:embedded` from `webapp/`
2. rebuild the firmware from `microcontroller/`
3. upload the firmware to the ESP32