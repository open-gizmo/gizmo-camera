import { configureGizmoMock } from 'gizmo-common/web/server/utils/gizmo-mock.js';

// Name the mock device like the app and its firmware, so the device card of the layer shows them (both come from the app.config.ts)
export default defineNitroPlugin(() => {
    const { name, hostname } = useAppConfig().gizmo;
    configureGizmoMock({ name, hostname });
});