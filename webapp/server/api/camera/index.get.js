import { createSuccessResponse, handleEspMockError } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { buildCameraPayload, getEspMockState } from '~~/server/utils/esp-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Get the current mock device state
        const state = getEspMockState();

        // Return what the camera can do, like the firmware
        return createSuccessResponse(buildCameraPayload(state.camera));
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_camera_read_failed', errorMessage: 'Failed to read the camera' });
    }
});