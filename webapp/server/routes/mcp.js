import { getHeader, getMethod, readRawBody, setResponseHeader, setResponseStatus } from 'h3';
import { MAX_REQUEST_BODY_SIZE, byteLength } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { RESOLUTIONS, getEspMockState } from '~~/server/utils/esp-mock-state.js';
import { buildSnapshotResult, errorCodes, handleMcpMessage } from '~~/server/utils/mcp-mock.js';
import { buildTestCardPng } from '~~/server/utils/test-card-png.js';

// Time the mock takes for a snapshot, like the sensor (longer with the flash, which warms up first)
const CAPTURE_TIME_MS = 300;
const FLASH_WARMUP_MS = 150;

// Send a JSON answer with its status
const sendJson = (event, status, response) => {
    setResponseStatus(event, status);
    setResponseHeader(event, 'Content-Type', 'application/json');
    setResponseHeader(event, 'Cache-Control', 'no-store');
    return response;
};

// The MCP server of the mock camera (Streamable HTTP, one JSON-RPC message per POST, answered with JSON), like /mcp on the firmware
export default defineEventHandler(async event => {
    // Any other method: the server offers no stream to GET, and has no session to DELETE
    if (getMethod(event) !== 'POST') {
        setResponseStatus(event, 405);
        setResponseHeader(event, 'Allow', 'POST');
        return '';
    }

    // Read the body, which must fit the firmware buffer
    const body = (await readRawBody(event, 'utf8')) || '';
    if (byteLength(body) > MAX_REQUEST_BODY_SIZE) {
        return sendJson(event, 413, { jsonrpc: '2.0', id: null, error: { code: errorCodes.INVALID_REQUEST, message: 'Request too large' } });
    }
    if (body === '') {
        return sendJson(event, 400, { jsonrpc: '2.0', id: null, error: { code: errorCodes.PARSE_ERROR, message: 'Missing JSON-RPC message' } });
    }

    // Read the message with the standard MCP headers (undefined when missing)
    const state = getEspMockState();
    const headers = {
        protocolVersion: getHeader(event, 'mcp-protocol-version'),
        method: getHeader(event, 'mcp-method'),
        name: getHeader(event, 'mcp-name')
    };
    const reply = handleMcpMessage({ body, headers, state });

    // A notification or a response of the client: accepted, with no body
    if (reply.kind === 'accepted') {
        setResponseStatus(event, 202);
        return '';
    }

    // Any answer but a snapshot
    if (reply.kind !== 'snapshot') {
        return sendJson(event, reply.status, reply.response);
    }

    // Take the time of a real snapshot, then draw the test card at its resolution
    const captureTimeMs = CAPTURE_TIME_MS + (reply.options.useFlash ? FLASH_WARMUP_MS : 0);
    await new Promise(resolve => setTimeout(resolve, captureTimeMs));
    const resolution = RESOLUTIONS[reply.options.resolutionIndex];
    const image = buildTestCardPng({ width: resolution.width, height: resolution.height, useFlash: reply.options.useFlash });

    // Send the result with the image
    return sendJson(event, 200, buildSnapshotResult({ id: reply.id, state, image, mimeType: 'image/png', resolution, captureTimeMs, usedFlash: reply.options.useFlash }));
});