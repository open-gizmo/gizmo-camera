import { getCamera, takeSnapshot } from '~/composables/useDeviceApi.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

// Load what the camera can do into the store, and remember a failure (so the camera page offers to try again)
export const loadCamera = async () => {
    const globalStore = useGlobalStore();

    try {
        // Get what the camera can do
        const camera = await getCamera();

        // Merge it with the store and mark it as loaded
        globalStore.value.camera = {
            ...globalStore.value.camera,
            ...camera
        };
        globalStore.value.cameraLoaded = true;
        globalStore.value.cameraLoadFailed = false;

        // Start from the camera's defaults, unless a resolution it can take is already chosen
        const options = globalStore.value.snapshotOptions;
        const isResolutionAvailable = globalStore.value.camera.resolutions.some(resolution => resolution.name === options.resolution);
        if (!isResolutionAvailable) {
            options.resolution = globalStore.value.camera.defaultResolution;
            options.quality = globalStore.value.camera.quality.default;
        }
    } catch (error) {
        // Remember the failure, so the camera page offers to try again
        globalStore.value.cameraLoadFailed = true;
        throw error;
    }
};

// Take a snapshot with the chosen options, and keep it as the last one
export const captureSnapshot = async () => {
    const globalStore = useGlobalStore();

    // Take the snapshot
    const { image, resolution, captureTimeMs } = await takeSnapshot(globalStore.value.snapshotOptions);

    // Free the image of the previous snapshot (an object URL keeps its blob in memory until revoked)
    if (globalStore.value.snapshot?.url) {
        URL.revokeObjectURL(globalStore.value.snapshot.url);
    }

    // Keep the new snapshot
    globalStore.value.snapshot = {
        url: URL.createObjectURL(image),
        resolution,
        sizeBytes: image.size,
        captureTimeMs
    };
};