// Shared device and UI state for the webapp
export const useGlobalStore = () => useState('global', () => ({
    // What the camera can do, as read from the device
    camera: {
        available: false,
        sensor: '',
        resolutions: [],
        defaultResolution: '',
        quality: {
            min: 4,
            max: 63,
            default: 10
        },
        hasFlash: false
    },

    // Flag to track whether the camera has been read at least once
    cameraLoaded: false,

    // Flag set when the camera couldn't be read (the camera page then offers to try again)
    cameraLoadFailed: false,

    // Options of the next snapshot (the resolution is set to the camera's default once it's read)
    snapshotOptions: {
        resolution: '',
        quality: 10,
        flash: false
    },

    // Last snapshot taken: { url, resolution, sizeBytes, captureTimeMs }, or null
    snapshot: null
}));