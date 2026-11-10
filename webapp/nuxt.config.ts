import tailwindcss from '@tailwindcss/vite';

export default defineNuxtConfig({
    // Shared pages, layout, Wi-Fi setup, device card and mock device routes of the OpenGizmo projects
    extends: ['./node_modules/gizmo-common/web'],

    srcDir: 'app/',

    ssr: false,

    app: {
        // Global head configuration
        head: {
            title: 'Gizmo Camera',
            link: [
                { rel: 'icon', type: 'image/svg+xml', href: '/logo.svg' },

                // Guide for AI agents, in the raw HTML (an agent that doesn't run the app still finds it)
                { rel: 'alternate', type: 'text/markdown', href: '/llms.txt', title: 'Guide for AI agents' }
            ]
        },

        // Page transition: the new page fades in while the old one goes at once (main.css). Not "out-in", which left the page blank when a navigation was interrupted
        pageTransition: {
            name: 'page'
        }
    },

    runtimeConfig: {
        public: {
            espBaseUrl: ''
        }
    },

    css: ['~/assets/css/main.css'],

    modules: [
        '@brumaombra/ui-vintage',
        '@nuxtjs/i18n'
    ],

    i18n: {
        defaultLocale: 'en',
        langDir: 'locales',
        detectBrowserLanguage: false,
        locales: [
            { code: 'en', name: 'English', language: 'en-US', iso: 'en-US', file: 'en.json' },
            { code: 'it', name: 'Italiano', language: 'it-IT', iso: 'it-IT', file: 'it.json' }
        ]
    },

    vite: {
        plugins: [
            tailwindcss()
        ],
        optimizeDeps: {
            include: [
                '@brumaombra/ui-vintage',
                '@hugeicons/vue'
            ]
        }
    },

    devtools: {
        enabled: false
    },

    nitro: {
        prerender: {
            routes: [
                '/',
                '/camera'
            ]
        }
    },

    compatibilityDate: '2026-06-18'
});