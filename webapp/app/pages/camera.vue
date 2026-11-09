<script setup>
import { computed, onMounted, ref } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import Camera01Icon from '@hugeicons/core-free-icons/Camera01Icon';
import CameraOff01Icon from '@hugeicons/core-free-icons/CameraOff01Icon';
import Download04Icon from '@hugeicons/core-free-icons/Download04Icon';
import FlashIcon from '@hugeicons/core-free-icons/FlashIcon';
import Image01Icon from '@hugeicons/core-free-icons/Image01Icon';
import ImageCompositionIcon from '@hugeicons/core-free-icons/ImageCompositionIcon';
import RefreshIcon from '@hugeicons/core-free-icons/RefreshIcon';
import AspectRatioIcon from '@hugeicons/core-free-icons/AspectRatioIcon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { setBusy } from '@brumaombra/ui-vintage/busy-indicator';
import { Button } from '@brumaombra/ui-vintage/button';
import { Card } from '@brumaombra/ui-vintage/card';
import { EmptyStateCard } from '@brumaombra/ui-vintage/empty-state-card';
import { ListGroup, ListRow } from '@brumaombra/ui-vintage/list';
import { LoadingStateCard } from '@brumaombra/ui-vintage/loading-state-card';
import { OptionPickerDialog } from '@brumaombra/ui-vintage/option-picker-dialog';
import { Switch } from '@brumaombra/ui-vintage/switch';
import { captureSnapshot, loadCamera } from '~/composables/useCamera.js';
import { buildSnapshotFileName, formatFileSize } from '~/composables/useUtils.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

const globalStore = useGlobalStore();
const { t, locale } = useI18n();
const openPicker = ref(''); // Picker currently open ('resolution', 'quality', or none)

// What the camera can do, and the options of the next snapshot
const camera = computed(() => globalStore.value.camera);
const options = computed(() => globalStore.value.snapshotOptions);
const snapshot = computed(() => globalStore.value.snapshot);

// Resolutions the sensor can take, largest first (the sharpest is the one people look for)
const resolutionOptions = computed(() => [...camera.value.resolutions].reverse().map(resolution => ({
    value: resolution.name,
    label: resolution.name,
    description: `${resolution.width} × ${resolution.height}`
})));

// Quality presets (the sensor's JPEG quality: lower numbers mean sharper photos and bigger files)
const qualityOptions = computed(() => [
    { value: 6, label: t('pages.camera.quality.best'), description: t('pages.camera.quality.bestMeta') },
    { value: 10, label: t('pages.camera.quality.high'), description: t('pages.camera.quality.highMeta') },
    { value: 20, label: t('pages.camera.quality.medium'), description: t('pages.camera.quality.mediumMeta') },
    { value: 40, label: t('pages.camera.quality.low'), description: t('pages.camera.quality.lowMeta') }
]);

// Label of the chosen quality, shown in its row (the number itself for a quality set outside the presets)
const selectedQualityLabel = computed(() => {
    return qualityOptions.value.find(option => option.value === options.value.quality)?.label || String(options.value.quality);
});

// Details of the last snapshot: resolution, size and capture time
const snapshotDetails = computed(() => {
    if (!snapshot.value) return '';
    const size = formatFileSize({ sizeBytes: snapshot.value.sizeBytes, locale: locale.value });
    return t('pages.camera.snapshotDetails', { resolution: snapshot.value.resolution, size, time: snapshot.value.captureTimeMs });
});

// Read the camera, showing the busy indicator meanwhile
const readCamera = async () => {
    try {
        setBusy(true, { label: t('common.syncingDevice') });
        await loadCamera();
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('app.loadError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Pick a resolution
const handleResolutionSelect = resolution => {
    options.value.resolution = resolution;
};

// Pick a quality
const handleQualitySelect = quality => {
    options.value.quality = quality;
};

// Take a snapshot with the chosen options
const handleSnapshotPress = async () => {
    try {
        setBusy(true, { label: t('pages.camera.takingSnapshot') });
        await captureSnapshot();
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('pages.camera.snapshotError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Save the last snapshot as a JPEG file
const handleDownloadPress = () => {
    // Nothing to save before the first snapshot
    if (!snapshot.value) return;

    // Click a temporary link to the image (the browser saves it under the given name)
    const link = document.createElement('a');
    link.href = snapshot.value.url;
    link.download = buildSnapshotFileName();
    link.click();
};

// Read the camera when the page opens (once: the store keeps it across pages)
onMounted(async () => {
    if (!globalStore.value.cameraLoaded) {
        await readCamera();
    }
});

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <GizmoPage :title="t('pages.camera.title')">
        <!-- The camera couldn't be read: offer to try again -->
        <EmptyStateCard v-if="globalStore.cameraLoadFailed && !globalStore.cameraLoaded" :icon="Alert02Icon" :title="t('pages.camera.loadErrorTitle')" :description="t('pages.camera.loadErrorDescription')">
            <template #action>
                <Button variant="primary" @click="readCamera">
                    <HugeiconsIcon :icon="RefreshIcon" class="size-4" />
                    {{ t('pages.camera.loadRetryAction') }}
                </Button>
            </template>
        </EmptyStateCard>

        <!-- Still reading the camera from the device -->
        <LoadingStateCard v-else-if="!globalStore.cameraLoaded" :title="t('common.syncingDevice')" />

        <!-- The sensor didn't start on the device -->
        <EmptyStateCard v-else-if="!camera.available" :icon="CameraOff01Icon" :title="t('pages.camera.unavailableTitle')" :description="t('pages.camera.unavailableDescription')" />

        <!-- Snapshot and its options -->
        <template v-else>
            <!-- Last snapshot, or a placeholder before the first one -->
            <Card v-if="snapshot" padding="none" class="overflow-hidden">
                <img :src="snapshot.url" :alt="t('pages.camera.snapshotAlt')" class="block h-auto w-full bg-muted">
                <p class="px-4 py-3 text-xs text-muted-foreground">{{ snapshotDetails }}</p>
            </Card>
            <EmptyStateCard v-else :icon="Image01Icon" :title="t('pages.camera.emptyTitle')" :description="t('pages.camera.emptyDescription')" />

            <!-- Actions: take a snapshot, save the last one -->
            <div class="flex flex-col gap-3 sm:flex-row">
                <Button variant="primary" class="w-full sm:flex-1" @click="handleSnapshotPress">
                    <HugeiconsIcon :icon="Camera01Icon" class="size-4" />
                    {{ t('pages.camera.snapshotAction') }}
                </Button>
                <Button variant="secondary" class="w-full sm:flex-1" :disabled="!snapshot" @click="handleDownloadPress">
                    <HugeiconsIcon :icon="Download04Icon" class="size-4" />
                    {{ t('pages.camera.downloadAction') }}
                </Button>
            </div>

            <!-- Options of the next snapshot -->
            <ListGroup :title="t('pages.camera.optionsTitle')" :hint="t('pages.camera.optionsHint', { sensor: camera.sensor })">
                <ListRow :label="t('pages.camera.resolution.label')" :icon="AspectRatioIcon" :value="options.resolution" action @click="openPicker = 'resolution'" />
                <ListRow :label="t('pages.camera.quality.label')" :icon="ImageCompositionIcon" :value="selectedQualityLabel" action @click="openPicker = 'quality'" />
                <ListRow v-if="camera.hasFlash" :label="t('pages.camera.flash.label')" :icon="FlashIcon" label-for="flash">
                    <template #trailing>
                        <Switch id="flash" v-model="options.flash" />
                    </template>
                </ListRow>
            </ListGroup>
        </template>

        <!-- Device info, network, restart and forget network (from the gizmo-common layer, it reads the device by itself) -->
        <GizmoDeviceCard />

        <!-- Resolution picker -->
        <OptionPickerDialog :open="openPicker === 'resolution'" :title="t('pages.camera.resolution.label')" :description="t('pages.camera.resolution.note')" :options="resolutionOptions" :model-value="options.resolution" @update:model-value="handleResolutionSelect" @update:open="open => { if (!open) openPicker = ''; }" />

        <!-- Quality picker -->
        <OptionPickerDialog :open="openPicker === 'quality'" :title="t('pages.camera.quality.label')" :description="t('pages.camera.quality.note')" :options="qualityOptions" :model-value="options.quality" @update:model-value="handleQualitySelect" @update:open="open => { if (!open) openPicker = ''; }" />
    </GizmoPage>
</template>