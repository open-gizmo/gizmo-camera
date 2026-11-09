// The Wi-Fi and device mock (its state, the responses and errors of the firmware) comes from the gizmo-common layer: this file adds the camera
import { EspMockError, getGizmoMockState } from 'gizmo-common/web/server/utils/gizmo-mock.js';

// Resolutions of the firmware, smallest to largest (capture_options.h)
export const RESOLUTIONS = [
    { name: 'QQVGA', width: 160, height: 120 },
    { name: 'QVGA', width: 320, height: 240 },
    { name: 'CIF', width: 400, height: 296 },
    { name: 'VGA', width: 640, height: 480 },
    { name: 'SVGA', width: 800, height: 600 },
    { name: 'XGA', width: 1024, height: 768 },
    { name: 'HD', width: 1280, height: 720 },
    { name: 'SXGA', width: 1280, height: 1024 },
    { name: 'UXGA', width: 1600, height: 1200 },
    { name: 'FHD', width: 1920, height: 1080 },
    { name: 'QXGA', width: 2048, height: 1536 },
    { name: 'QHD', width: 2560, height: 1440 },
    { name: 'WQXGA', width: 2560, height: 1600 },
    { name: 'QSXGA', width: 2560, height: 1920 }
];

// JPEG quality range of the firmware (lower numbers mean better quality)
export const JPEG_QUALITY = Object.freeze({
    min: 4,
    max: 63,
    default: 10
});

// Camera of the demo device: the OV2640 of the ESP32-CAM, up to UXGA
const demoCamera = () => ({
    isAvailable: true,
    sensorName: 'OV2640',
    maxResolutionIndex: RESOLUTIONS.findIndex(resolution => resolution.name === 'UXGA')
});

// Get the mock device state: the one of the layer (Wi-Fi, uptime, kept across hot reloads) plus the camera
export const getEspMockState = () => {
    const state = getGizmoMockState();
    state.camera ??= demoCamera();
    return state;
};

// Build what the camera can do, like GET /api/camera of the firmware
export const buildCameraPayload = camera => ({
    available: camera.isAvailable,
    sensor: camera.sensorName,
    resolutions: RESOLUTIONS.slice(0, camera.maxResolutionIndex + 1),
    defaultResolution: RESOLUTIONS[camera.maxResolutionIndex].name,
    quality: { ...JPEG_QUALITY },
    hasFlash: true
});

// Read the options of a snapshot request with the same rules and errors as the firmware (each value undefined when the request leaves it out)
export const parseCaptureOptions = ({ query, camera }) => {
    const options = { resolutionIndex: camera.maxResolutionIndex, quality: JPEG_QUALITY.default, useFlash: false };

    // Read the resolution, which the sensor must be able to take
    if (query.resolution !== undefined) {
        const resolutionIndex = RESOLUTIONS.findIndex(resolution => resolution.name.toLowerCase() === String(query.resolution).toLowerCase());
        if (resolutionIndex === -1) {
            throw new EspMockError(400, 'invalid_resolution', 'Unknown resolution');
        }
        if (resolutionIndex > camera.maxResolutionIndex) {
            throw new EspMockError(400, 'resolution_not_supported', 'Resolution not supported by the sensor');
        }
        options.resolutionIndex = resolutionIndex;
    }

    // Read the JPEG quality: a whole number of one or two digits, in range
    if (query.quality !== undefined) {
        const quality = String(query.quality);
        if (!/^\d{1,2}$/.test(quality) || Number(quality) < JPEG_QUALITY.min || Number(quality) > JPEG_QUALITY.max) {
            throw new EspMockError(400, 'invalid_quality', 'Quality must be a whole number from 4 to 63');
        }
        options.quality = Number(quality);
    }

    // Read the flash switch: 1 or true turns it on, 0 or false leaves it off
    if (query.flash !== undefined) {
        const flash = String(query.flash).toLowerCase();
        if (!['0', '1', 'false', 'true'].includes(flash)) {
            throw new EspMockError(400, 'invalid_flash', 'Flash must be 0 or 1');
        }
        options.useFlash = flash === '1' || flash === 'true';
    }

    // Return the options once they are all valid
    return options;
};

// Draw the test card the mock sends instead of a photo (an SVG at the size of the snapshot, brighter with the flash)
export const buildTestCard = ({ resolution, quality, useFlash, takenAt = new Date() }) => {
    const { width, height } = resolution;
    const fontSize = Math.max(10, Math.round(height / 12));
    const background = useFlash ? '#fff4d6' : '#1b222d';
    const foreground = useFlash ? '#1b222d' : '#f7931a';
    const time = takenAt.toLocaleTimeString('en-GB');

    // Return the SVG of the test card
    return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">`
        + `<rect width="${width}" height="${height}" fill="${background}"/>`
        + `<circle cx="${width / 2}" cy="${height / 2}" r="${Math.min(width, height) / 3}" fill="none" stroke="${foreground}" stroke-width="${Math.max(2, fontSize / 6)}"/>`
        + `<text x="50%" y="46%" text-anchor="middle" font-family="sans-serif" font-size="${fontSize}" fill="${foreground}">Gizmo Camera</text>`
        + `<text x="50%" y="58%" text-anchor="middle" font-family="sans-serif" font-size="${fontSize / 2.6}" fill="${foreground}">${resolution.name} ${width}x${height} · Q${quality} · ${time}</text>`
        + '</svg>';
};