// The MCP server of the mock camera: the same protocol, tools, errors and HTTP statuses as the firmware (mcp_protocol.cpp), with a PNG test card instead of the JPEG
import { MOCK_VERSION } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { JPEG_QUALITY, RESOLUTIONS } from '~~/server/utils/esp-mock-state.js';

/******************************** Protocol constants ********************************/

// Revision with the version in every request (_meta), and the earlier ones that open with initialize (newest first)
export const MODERN_PROTOCOL_VERSION = '2026-07-28';
export const LEGACY_PROTOCOL_VERSIONS = ['2025-11-25', '2025-06-18', '2025-03-26'];

// Keys of the per-request _meta of the 2026-07-28 revision
const META_PROTOCOL_VERSION = 'io.modelcontextprotocol/protocolVersion';
const META_CLIENT_CAPABILITIES = 'io.modelcontextprotocol/clientCapabilities';
const META_SERVER_INFO = 'io.modelcontextprotocol/serverInfo';

// JSON-RPC and MCP error codes
export const errorCodes = Object.freeze({
    PARSE_ERROR: -32700,
    INVALID_REQUEST: -32600,
    METHOD_NOT_FOUND: -32601,
    INVALID_PARAMS: -32602,
    HEADER_MISMATCH: -32020,
    UNSUPPORTED_PROTOCOL_VERSION: -32022
});

// How long clients may cache the tools and the server details
const CACHE_TTL_MS = 3600000; // 1 hour

// Default resolution for agents, and the names of the tools
const DEFAULT_RESOLUTION = 'SVGA';
const TAKE_SNAPSHOT_TOOL = 'take_snapshot';
const GET_CAMERA_INFO_TOOL = 'get_camera_info';

// What the server tells agents about itself (the same text as the firmware)
const SERVER_INSTRUCTIONS = 'A camera (ESP32-CAM with an OV2640 sensor) on the local network. Call take_snapshot to see what it sees right now: every call takes a new photo. The default SVGA resolution is enough to understand a scene; ask for UXGA only to read small details, it is slower and costs more tokens. Turn the flash on in the dark. The camera takes one snapshot at a time: if it is busy, wait a moment and try again.';

/******************************** Helpers ********************************/

// Check whether a value is a plain object
const isObject = value => value !== null && typeof value === 'object' && !Array.isArray(value);

// Who the mock server is
const buildServerInfo = state => ({
    name: state.hostname || 'gizmo-camera',
    title: state.name,
    version: MOCK_VERSION
});

// Answer with a JSON-RPC error and its HTTP status
const createError = ({ id = null, status, code, message, data }) => ({
    kind: 'json',
    status,
    response: {
        jsonrpc: '2.0',
        id,
        error: {
            code,
            message,
            ...(data ? { data } : {})
        }
    }
});

// Answer with a successful result, with the server's identity in its _meta
const createResult = ({ id, state, result }) => ({
    kind: 'json',
    status: 200,
    response: {
        jsonrpc: '2.0',
        id,
        result: {
            resultType: 'complete',
            _meta: { [META_SERVER_INFO]: buildServerInfo(state) },
            ...result
        }
    }
});

// Answer a tool call with an error the agent can read and correct
const createToolError = ({ id, state, message }) => createResult({
    id,
    state,
    result: { content: [{ type: 'text', text: message }], isError: true }
});

/******************************** Tools ********************************/

// Index of the largest resolution of the mock sensor, and the default for agents
const getDefaultResolutionIndex = camera => Math.min(camera.maxResolutionIndex, RESOLUTIONS.findIndex(resolution => resolution.name === DEFAULT_RESOLUTION));

// Describe the tools, in the same order as the firmware
const buildTools = camera => [{
    name: TAKE_SNAPSHOT_TOOL,
    title: 'Take snapshot',
    description: 'Take a new JPEG photo with the camera and return it. Every call takes a new photo, a few hundred ms after it\'s asked for.',
    annotations: { readOnlyHint: true },
    inputSchema: {
        type: 'object',
        additionalProperties: false,
        properties: {
            resolution: {
                type: 'string',
                description: 'Size of the photo. SVGA (800x600, the default) shows a scene well; larger ones are slower and cost more tokens.',
                enum: RESOLUTIONS.slice(0, camera.maxResolutionIndex + 1).map(resolution => resolution.name)
            },
            quality: {
                type: 'integer',
                minimum: JPEG_QUALITY.min,
                maximum: JPEG_QUALITY.max,
                description: 'JPEG quality: lower numbers are sharper and bigger. Default 10.'
            },
            flash: {
                type: 'boolean',
                description: 'Turn on the white flash LED for the photo (for dark scenes). Default false.'
            }
        }
    }
}, {
    name: GET_CAMERA_INFO_TOOL,
    title: 'Get camera info',
    description: 'Describe the camera: its sensor, the resolutions it can take, the JPEG quality range, and whether it has a flash.',
    annotations: { readOnlyHint: true },
    inputSchema: { type: 'object', additionalProperties: false }
}];

// Read the arguments of take_snapshot like the firmware: the options, or the message for the agent
const readSnapshotArguments = ({ args, camera }) => {
    const options = { resolutionIndex: getDefaultResolutionIndex(camera), quality: JPEG_QUALITY.default, useFlash: false };

    // Refuse arguments the tool doesn't take
    const unknownArgument = Object.keys(args).find(name => !['resolution', 'quality', 'flash'].includes(name));
    if (unknownArgument) {
        return { message: `Unknown argument "${unknownArgument}": take_snapshot takes resolution, quality and flash` };
    }

    // Read the resolution, which the sensor must be able to take
    if (args.resolution != null) {
        const resolutionIndex = typeof args.resolution === 'string' ? RESOLUTIONS.findIndex(resolution => resolution.name.toLowerCase() === args.resolution.toLowerCase()) : -1;
        if (resolutionIndex === -1 || resolutionIndex > camera.maxResolutionIndex) {
            return { message: `resolution must be one of QQVGA to ${RESOLUTIONS[camera.maxResolutionIndex].name} (the largest this sensor can take)` };
        }
        options.resolutionIndex = resolutionIndex;
    }

    // Read the JPEG quality (a whole number in range)
    if (args.quality != null) {
        if (!Number.isInteger(args.quality) || args.quality < JPEG_QUALITY.min || args.quality > JPEG_QUALITY.max) {
            return { message: `quality must be a whole number from ${JPEG_QUALITY.min} to ${JPEG_QUALITY.max}` };
        }
        options.quality = args.quality;
    }

    // Read the flash switch
    if (args.flash != null) {
        if (typeof args.flash !== 'boolean') {
            return { message: 'flash must be true or false' };
        }
        options.useFlash = args.flash;
    }

    // Return the options once they are all valid
    return { options };
};

// Describe the camera, like get_camera_info of the firmware
const buildCameraInfo = camera => ({
    available: camera.isAvailable,
    sensor: camera.sensorName,
    resolutions: RESOLUTIONS.slice(0, camera.maxResolutionIndex + 1),
    defaultResolution: RESOLUTIONS[getDefaultResolutionIndex(camera)].name,
    quality: { ...JPEG_QUALITY },
    hasFlash: true,
    firmwareVersion: MOCK_VERSION
});

// Answer tools/call: unknown tools and malformed params are protocol errors, wrong argument values are tool errors
const answerToolsCall = ({ id, params, state, isModern }) => {
    const errorStatus = isModern ? 400 : 200;
    const args = params.arguments ?? {};

    // Read the tool name and its arguments
    if (typeof params.name !== 'string' || !isObject(args)) {
        return createError({ id, status: errorStatus, code: errorCodes.INVALID_PARAMS, message: 'tools/call needs a tool name, and its arguments as an object' });
    }

    // Take a snapshot: the route takes it once the arguments are valid
    if (params.name === TAKE_SNAPSHOT_TOOL) {
        if (!state.camera.isAvailable) {
            return createToolError({ id, state, message: 'The camera sensor did not start, so no snapshot can be taken. Someone has to check its ribbon cable and restart the device.' });
        }
        const { options, message } = readSnapshotArguments({ args, camera: state.camera });
        if (message) {
            return createToolError({ id, state, message });
        }
        return { kind: 'snapshot', status: 200, id, options };
    }

    // Describe the camera
    if (params.name === GET_CAMERA_INFO_TOOL) {
        if (Object.keys(args).length > 0) {
            return createToolError({ id, state, message: 'get_camera_info takes no arguments' });
        }
        const camera = buildCameraInfo(state.camera);
        return createResult({ id, state, result: { structuredContent: camera, content: [{ type: 'text', text: JSON.stringify(camera) }], isError: false } });
    }

    // Unknown tool
    return createError({ id, status: errorStatus, code: errorCodes.INVALID_PARAMS, message: `Unknown tool: ${params.name}` });
};

/******************************** Eras ********************************/

// Answer a request of the 2026-07-28 revision: the version, the headers and the capabilities are checked first
const answerModernRequest = ({ id, method, params, version, headers, state }) => {
    // Refuse other versions, naming the supported one
    if (version !== MODERN_PROTOCOL_VERSION) {
        return createError({ id, status: 400, code: errorCodes.UNSUPPORTED_PROTOCOL_VERSION, message: 'Unsupported protocol version', data: { supported: [MODERN_PROTOCOL_VERSION], requested: version } });
    }

    // The headers must repeat the version, the method and (for a tool call) the tool name
    const isToolCall = method === 'tools/call';
    const areHeadersValid = headers.protocolVersion === version && headers.method === method && (!isToolCall || (headers.name !== undefined && headers.name === params.name));
    if (!areHeadersValid) {
        return createError({ id, status: 400, code: errorCodes.HEADER_MISMATCH, message: 'The MCP-Protocol-Version, Mcp-Method and Mcp-Name headers must match the request' });
    }

    // Every request carries the client's capabilities
    if (!isObject(params._meta?.[META_CLIENT_CAPABILITIES])) {
        return createError({ id, status: 400, code: errorCodes.INVALID_PARAMS, message: 'Missing _meta io.modelcontextprotocol/clientCapabilities' });
    }

    // Answer the method
    switch (method) {
        // Versions, capabilities and instructions
        case 'server/discover':
            return createResult({ id, state, result: { supportedVersions: [MODERN_PROTOCOL_VERSION], capabilities: { tools: {} }, instructions: SERVER_INSTRUCTIONS, ttlMs: CACHE_TTL_MS, cacheScope: 'public' } });

        // The tools
        case 'tools/list':
            return createResult({ id, state, result: { tools: buildTools(state.camera), ttlMs: CACHE_TTL_MS, cacheScope: 'public' } });

        // A tool call
        case 'tools/call':
            return answerToolsCall({ id, params, state, isModern: true });

        // Anything else
        default:
            return createError({ id, status: 404, code: errorCodes.METHOD_NOT_FOUND, message: 'Method not found' });
    }
};

// Answer a request of an earlier revision (errors keep a 200 status: there, a 404 means "session expired" and makes the client start over)
const answerLegacyRequest = ({ id, method, params, headers, state }) => {
    // A 2026-07-28 request without its _meta, or an unknown version in the header
    if (headers.protocolVersion !== undefined && !LEGACY_PROTOCOL_VERSIONS.includes(headers.protocolVersion)) {
        if (headers.protocolVersion === MODERN_PROTOCOL_VERSION) {
            return createError({ id, status: 400, code: errorCodes.INVALID_PARAMS, message: 'Missing _meta io.modelcontextprotocol/protocolVersion' });
        }
        return createError({ id, status: 400, code: errorCodes.INVALID_REQUEST, message: 'Unsupported MCP-Protocol-Version' });
    }

    // Answer the method
    switch (method) {
        // The handshake: the client's version when the server speaks it, the newest earlier one otherwise
        case 'initialize': {
            const protocolVersion = LEGACY_PROTOCOL_VERSIONS.includes(params.protocolVersion) ? params.protocolVersion : LEGACY_PROTOCOL_VERSIONS[0];
            return createResult({ id, state, result: { protocolVersion, capabilities: { tools: {} }, serverInfo: buildServerInfo(state), instructions: SERVER_INSTRUCTIONS } });
        }

        // An empty answer
        case 'ping':
            return { kind: 'json', status: 200, response: { jsonrpc: '2.0', id, result: {} } };

        // The tools
        case 'tools/list':
            return createResult({ id, state, result: { tools: buildTools(state.camera), ttlMs: CACHE_TTL_MS, cacheScope: 'public' } });

        // A tool call
        case 'tools/call':
            return answerToolsCall({ id, params, state, isModern: false });

        // A 2026-07-28 method without its _meta
        case 'server/discover':
            return createError({ id, status: 400, code: errorCodes.INVALID_PARAMS, message: 'Missing _meta io.modelcontextprotocol/protocolVersion' });

        // Anything else
        default:
            return createError({ id, status: 200, code: errorCodes.METHOD_NOT_FOUND, message: 'Method not found' });
    }
};

/******************************** Messages ********************************/

// Read one JSON-RPC message, and say what to answer: { kind: 'json', status, response }, { kind: 'accepted' }, or { kind: 'snapshot', id, options }
export const handleMcpMessage = ({ body, headers, state }) => {
    // Parse the body
    let request;
    try {
        request = JSON.parse(body);
    } catch {
        return createError({ status: 400, code: errorCodes.PARSE_ERROR, message: 'Parse error' });
    }

    // One JSON-RPC 2.0 message per request (no batches)
    if (!isObject(request) || request.jsonrpc !== '2.0') {
        return createError({ status: 400, code: errorCodes.INVALID_REQUEST, message: 'Expected one JSON-RPC 2.0 message' });
    }

    // A response from the client (it has no method): accepted with no body
    if (typeof request.method !== 'string') {
        if (request.result !== undefined || request.error !== undefined) {
            return { kind: 'accepted' };
        }
        return createError({ status: 400, code: errorCodes.INVALID_REQUEST, message: 'Expected a method' });
    }

    // A notification (it has no id): accepted with no body
    if (request.id === undefined || request.id === null) {
        return { kind: 'accepted' };
    }

    // The id is a string or a whole number
    if (typeof request.id !== 'string' && !Number.isInteger(request.id)) {
        return createError({ status: 400, code: errorCodes.INVALID_REQUEST, message: 'The id must be a string or a whole number' });
    }

    // Answer by era: a version in _meta means the 2026-07-28 revision
    const params = isObject(request.params) ? request.params : {};
    const version = params._meta?.[META_PROTOCOL_VERSION];
    if (typeof version === 'string') {
        return answerModernRequest({ id: request.id, method: request.method, params, version, headers, state });
    }
    return answerLegacyRequest({ id: request.id, method: request.method, params, headers, state });
};

// Build the result of a snapshot, with its image and a line of details like the firmware
export const buildSnapshotResult = ({ id, state, image, mimeType, resolution, captureTimeMs, usedFlash }) => {
    const sizeKb = Math.round(image.length / 1024);
    const text = `Snapshot ${resolution.width}x${resolution.height}, ${mimeType === 'image/png' ? 'PNG test card' : 'JPEG'}, ${sizeKb} KB, taken in ${captureTimeMs} ms${usedFlash ? ' with the flash' : ''}.`;

    // Return the text, then the image
    return createResult({
        id,
        state,
        result: {
            content: [
                { type: 'text', text },
                { type: 'image', mimeType, data: image.toString('base64') }
            ],
            isError: false
        }
    }).response;
};