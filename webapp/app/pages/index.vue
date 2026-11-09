<script setup>
import { onMounted } from 'vue';
import { LoadingStateCard } from '@brumaombra/ui-vintage/loading-state-card';

const { t } = useI18n();
const localePath = useLocalePath();

// Redirect the root page to the camera screen
onMounted(() => {
    navigateTo(localePath('/camera'), { replace: true });
});

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <div class="mx-auto w-full max-w-2xl">
        <LoadingStateCard :title="t('common.redirectingCamera')" />
    </div>
</template>