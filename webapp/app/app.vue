<script setup>
import { onMounted } from 'vue';
import { useTheme } from '@brumaombra/ui-vintage/theme-selector';

const { startTheme } = useTheme();

// Apply the saved theme (shared with the theme selector) once the app mounts (the camera page reads the camera by itself)
onMounted(() => {
    startTheme();
});
</script>

<template>
    <div>
        <!-- Route content -->
        <NuxtLayout>
            <NuxtPage />
        </NuxtLayout>
    </div>
</template>