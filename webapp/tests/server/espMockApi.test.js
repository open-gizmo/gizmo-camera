import { IncomingMessage, ServerResponse } from 'node:http';
import { Socket } from 'node:net';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { createEvent } from 'h3';

// Stub the Nitro globals the routes of this app use before the handler modules load (the mock of the gizmo-common layer imports its h3 functions, so it runs on real h3 events)
vi.hoisted(() => {
    globalThis.defineEventHandler = handler => handler;
    globalThis.defineNitroPlugin = plugin => plugin;
    globalThis.useAppConfig = () => ({ gizmo: { name: 'Gizmo Camera', hostname: 'gizmo-camera' } });
});

// Load the handlers of the mock device API
const { default: getCamera } = await import('../../server/api/camera/index.get.js');
const { default: getCapture } = await import('../../server/api/capture/index.get.js');
const { getEspMockState } = await import('../../server/utils/esp-mock-state.js');
const { default: nameMockDevice } = await import('../../server/plugins/gizmo-mock.js');
const { buildDevicePayload, resetGizmoMockState } = await import('gizmo-common/web/server/utils/gizmo-mock.js');

// Build a real h3 GET request event for a path with its query
const buildEvent = (url = '/') => {
    const request = new IncomingMessage(new Socket());
    request.method = 'GET';
    request.url = url;
    return createEvent(request, new ServerResponse(request));
};

// Call a handler and return its answer with the status code and headers it set
const callHandler = async (handler, event = buildEvent()) => {
    const response = await handler(event);
    return { statusCode: event.node.res.statusCode, headers: event.node.res.getHeaders(), response };
};

// Start every test from the demo device the dev server boots with
beforeEach(() => {
    resetGizmoMockState();
});

// GET /api/camera: what the camera can do, like the firmware
describe('mock camera', () => {
    // The OV2640 of the ESP32-CAM, up to UXGA, with the flash
    it('describes the camera of the ESP32-CAM', async () => {
        const { statusCode, response } = await callHandler(getCamera);
        expect(statusCode).toBe(200);
        expect(response.errorId).toBeNull();
        expect(response.data.available).toBe(true);
        expect(response.data.sensor).toBe('OV2640');
        expect(response.data.defaultResolution).toBe('UXGA');
        expect(response.data.resolutions.at(-1)).toEqual({ name: 'UXGA', width: 1600, height: 1200 });
        expect(response.data.quality).toEqual({ min: 4, max: 63, default: 10 });
        expect(response.data.hasFlash).toBe(true);
    });

    // The device card of the layer shows the name and the address of the camera
    it('names the mock device like the camera', async () => {
        nameMockDevice();
        const payload = buildDevicePayload({ state: getEspMockState() });
        expect(payload.name).toBe('Gizmo Camera');
        expect(payload.hostname).toBe('gizmo-camera.local');
    });
});

// GET /api/capture: the same options, errors and headers as the firmware
describe('mock capture', () => {
    // Fake timers, so the time of a snapshot doesn't slow the tests down
    beforeEach(() => {
        vi.useFakeTimers();
    });

    // Call the capture handler, letting its snapshot time pass
    const capture = async url => {
        const pending = callHandler(getCapture, buildEvent(url));
        await vi.runAllTimersAsync();
        vi.useRealTimers();
        return pending;
    };

    // Without options: the largest resolution, with the headers of the firmware
    it('takes a snapshot at the largest resolution by default', async () => {
        const { statusCode, headers, response } = await capture('/api/capture');
        expect(statusCode).toBe(200);
        expect(headers['x-resolution']).toBe('1600x1200');
        expect(headers['x-capture-time-ms']).toBe('300');
        expect(headers['cache-control']).toBe('no-store');
        expect(response).toContain('width="1600"');
    });

    // The options are read whatever the case of the resolution, and the flash takes longer
    it('reads the options of the request', async () => {
        const { headers, response } = await capture('/api/capture?resolution=vga&quality=20&flash=true');
        expect(headers['x-resolution']).toBe('640x480');
        expect(headers['x-capture-time-ms']).toBe('450');
        expect(response).toContain('Q20');
    });

    // Every refused option answers the error of the firmware
    it.each([
        ['resolution=4K', 'invalid_resolution'],
        ['resolution=FHD', 'resolution_not_supported'],
        ['quality=3', 'invalid_quality'],
        ['quality=10x', 'invalid_quality'],
        ['flash=yes', 'invalid_flash']
    ])('refuses %s with %s', async (query, errorId) => {
        const { statusCode, response } = await capture(`/api/capture?${query}`);
        expect(statusCode).toBe(400);
        expect(response).toMatchObject({ errorId, data: null });
    });

    // A sensor that didn't start answers 503, like the firmware
    it('refuses snapshots without a sensor', async () => {
        getEspMockState().camera.isAvailable = false;
        const { statusCode, response } = await capture('/api/capture');
        expect(statusCode).toBe(503);
        expect(response.errorId).toBe('camera_unavailable');
    });
});