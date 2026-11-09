import Camera01Icon from '@hugeicons/core-free-icons/Camera01Icon';

// Name, logo and pages of the app, read by the layout and the pages of the gizmo-common layer
export default defineAppConfig({
    gizmo: {
        name: 'Gizmo Camera',

        // mDNS name of the firmware (http://gizmo-camera.local)
        hostname: 'gizmo-camera',

        logo: '/logo.svg',
        homePath: '/camera',

        // Pages of the dashboard, top to bottom in the sidebar and left to right in the bottom navigation ('wifi' is the page of the layer)
        nav: [
            { id: 'camera', labelKey: 'nav.camera.label', descriptionKey: 'nav.camera.description', icon: Camera01Icon, to: '/camera' },
            'wifi'
        ]
    }
});