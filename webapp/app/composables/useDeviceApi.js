// The request wrapper (fetchEsp) and the Wi-Fi and device calls come from the gizmo-common layer: only the calls of the camera are here

// Fetch what the camera can do: its sensor, resolutions, quality range and flash
export const getCamera = async () => {
    const response = await fetchEsp('/api/camera');
    return response.data || {};
};

// Read the JSON error the device sends with a failed snapshot (the request asks for an image, so the error arrives as a blob that handleBackendErrors can't read)
const readSnapshotError = async error => {
    const errorBody = error?.response?._data;
    if (typeof errorBody?.text !== 'function') return; // Not a blob (checked by shape: a Blob from another realm fails instanceof)

    // Replace the blob with the device error, or with nothing when it isn't JSON
    try {
        const errorText = await errorBody.text();
        error.response._data = JSON.parse(errorText);
    } catch {
        error.response._data = null;
    }
};

// Take a snapshot, and return the image with its resolution and capture time (read from the response headers)
export const takeSnapshot = async ({ resolution, quality, flash }) => {
    let responseHeaders = null; // Set by the response hook, read once the image arrives

    try {
        // Call the device API to take the snapshot (never retried: each try is a new photo, and the flash would fire again)
        const image = await fetchEsp('/api/capture', {
            query: {
                resolution,
                quality,
                flash: flash ? 1 : 0
            },
            responseType: 'blob',
            retry: 0,
            onResponse: ({ response }) => {
                responseHeaders = response.headers;
            }
        });

        // Return the image and what the device says about it
        return {
            image,
            resolution: responseHeaders?.get('X-Resolution') || '',
            captureTimeMs: Number(responseHeaders?.get('X-Capture-Time-Ms')) || 0
        };
    } catch (error) {
        // Read the device error, so the dialog shows its translated message
        await readSnapshotError(error);
        throw error;
    }
};