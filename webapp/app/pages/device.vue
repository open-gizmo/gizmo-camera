<script setup>
const { t } = useI18n();

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <GizmoPage :title="t('pages.device.title')">
        <!-- Device info, network, restart and forget network (from the gizmo-common layer, it reads the device by itself). The language is picked in the top bar, like in every OpenGizmo app -->
        <GizmoDeviceCard />
    </GizmoPage>
</template>
