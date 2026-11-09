import { getQuery, setResponseHeader } from 'h3';
import { EspMockError, handleEspMockError } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { RESOLUTIONS, buildTestCard, getEspMockState, parseCaptureOptions } from '~~/server/utils/esp-mock-state.js';

// Time the mock takes for a snapshot, like the sensor (longer with the flash, which warms up first)
const CAPTURE_TIME_MS = 300;
const FLASH_WARMUP_MS = 150;

export default defineEventHandler(async event => {
    try {
        // Get the current mock device state
        const state = getEspMockState();

        // A camera whose sensor didn't start takes no snapshots
        if (!state.camera.isAvailable) {
            throw new EspMockError(503, 'camera_unavailable', 'The camera sensor did not start');
        }

        // Read the snapshot options with the firmware rules
        const options = parseCaptureOptions({ query: getQuery(event), camera: state.camera });

        // Take the time of a real snapshot
        const captureTimeMs = CAPTURE_TIME_MS + (options.useFlash ? FLASH_WARMUP_MS : 0);
        await new Promise(resolve => setTimeout(resolve, captureTimeMs));

        // Describe the snapshot in the headers, like the firmware (an SVG test card stands in for the JPEG)
        const resolution = RESOLUTIONS[options.resolutionIndex];
        setResponseHeader(event, 'Content-Type', 'image/svg+xml');
        setResponseHeader(event, 'Cache-Control', 'no-store');
        setResponseHeader(event, 'X-Resolution', `${resolution.width}x${resolution.height}`);
        setResponseHeader(event, 'X-Capture-Time-Ms', String(captureTimeMs));

        // Return the test card
        return buildTestCard({ resolution, quality: options.quality, useFlash: options.useFlash });
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'capture_failed', errorMessage: 'The camera could not take the snapshot' });
    }
});