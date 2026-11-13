import { describe, expect, it } from 'vitest';
import { defineComponent } from 'vue';
import { mountSuspended, registerEndpoint } from '@nuxt/test-utils/runtime';
import ErrorPage from 'gizmo-common/web/app/error.vue';
import ApSetupPage from 'gizmo-common/web/app/pages/ap-setup.vue';

// Items of the navigation, read inside a component (the composable needs the Nuxt context)
const readNavItems = async () => {
    let items;
    await mountSuspended(defineComponent({
        setup() {
            items = useGizmoNavItems();
            return () => null;
        }
    }), { route: '/' });
    return items.value;
};

// Pages the app gets from the gizmo-common layer, set up by the app.config.ts
describe('pages of the layer', () => {
    // The pages of the camera, in the order the sidebar and the bottom navigation show them
    it('lists the pages of the camera in the navigation', async () => {
        const items = await readNavItems();
        expect(items.map(item => [item.id, item.label, item.to])).toEqual([
            ['camera', 'Camera', '/camera'],
            ['settings', 'Settings', '/settings'],
            ['wifi', 'WiFi', '/wifi']
        ]);
    });

    // The error page has the name of the camera, and its button goes back to the camera page
    it('shows the error page with the camera name', async () => {
        const wrapper = await mountSuspended(ErrorPage, { props: { error: { statusCode: 404 } } });
        expect(wrapper.text()).toContain('Page not found');
        expect(wrapper.text()).toContain('Back to home');
        expect(wrapper.text()).toContain('Gizmo Camera');
    });

    // The setup page of the hotspot shows the name and the logo of the camera
    it('shows the name and the logo on the setup page', async () => {
        registerEndpoint('/api/networks', () => ({ errorId: null, errorMessage: null, data: { networks: [] } }));
        const wrapper = await mountSuspended(ApSetupPage);
        expect(wrapper.text()).toContain('Gizmo Camera');
        expect(wrapper.find('img').attributes('src')).toBe('/images/logo/gizmo-camera-mark.svg');
    });
});