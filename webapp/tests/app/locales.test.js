import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

// Flatten a locale into { "a.b.c": "text" }
const flatten = (object, prefix = '') => Object.entries(object).flatMap(([key, value]) => {
    return value && typeof value === 'object' ? flatten(value, `${prefix}${key}.`) : [[`${prefix}${key}`, value]];
});

// Placeholders used in a translation, e.g. "{ssid}"
const readPlaceholders = text => [...String(text).matchAll(/\{(\w+)\}/g)].map(match => match[1]).sort();

// All the source files under a folder (the app's, and the firmware's for the error IDs)
const readSourceFiles = directory => readdirSync(directory, { withFileTypes: true }).flatMap(entry => {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) return readSourceFiles(path);
    return /\.(vue|js|cpp|h)$/.test(entry.name) ? [path] : [];
});

// Read a locale file as plain JSON (importing it would go through the i18n module, which compiles the messages)
const readLocale = code => JSON.parse(readFileSync(join(process.cwd(), 'i18n', 'locales', `${code}.json`), 'utf8'));

// Read a locale of the gizmo-common layer (its errors are translated for the app too)
const readLayerLocale = code => JSON.parse(readFileSync(join(process.cwd(), 'node_modules', 'gizmo-common', 'web', 'i18n', 'locales', `${code}.json`), 'utf8'));
const layerTexts = Object.fromEntries(flatten(readLayerLocale('en')));

// The locales of the app, from the files in the locales folder
const LANGUAGE_CODES = readdirSync(join(process.cwd(), 'i18n', 'locales')).map(file => file.replace(/\.json$/, ''));

const englishTexts = Object.fromEntries(flatten(readLocale('en')));
const otherLocales = Object.fromEntries(LANGUAGE_CODES.filter(code => code !== 'en').map(code => [code, readLocale(code)]));

// Translations: a missing key shows the raw key (e.g. "pages.wifi.scanAgain") to the user
describe('locales', () => {
    // Every locale has exactly the English keys
    for (const [code, locale] of Object.entries(otherLocales)) {
        it(`${code} has the same keys as en`, () => {
            const texts = Object.fromEntries(flatten(locale));
            expect(Object.keys(englishTexts).filter(key => !(key in texts)), 'missing keys').toEqual([]);
            expect(Object.keys(texts).filter(key => !(key in englishTexts)), 'extra keys').toEqual([]);
        });

        // A placeholder with a different name is shown as "{ssid}" instead of the value
        it(`${code} uses the same placeholders as en`, () => {
            const texts = Object.fromEntries(flatten(locale));
            const mismatches = Object.keys(englishTexts)
                .filter(key => typeof texts[key] === 'string')
                .filter(key => readPlaceholders(texts[key]).join() !== readPlaceholders(englishTexts[key]).join());
            expect(mismatches).toEqual([]);
        });

        // Empty strings render as blank buttons and labels
        it(`${code} has no empty translations`, () => {
            expect(flatten(locale).filter(([, text]) => text === '').map(([key]) => key)).toEqual([]);
        });
    }

    // Keys written literally in the code (t('a.b') or translate('a.b')) must exist, dynamic keys cannot be checked
    it('has every key used in the code', () => {
        const root = process.cwd();
        const files = readSourceFiles(join(root, 'app'));
        const missing = new Set();
        for (const file of files) {
            const source = readFileSync(file, 'utf8');
            for (const match of source.matchAll(/\b(?:t|translate)\(\s*['"]([a-zA-Z0-9_.]+)['"]/g)) {
                if (!(match[1] in englishTexts)) missing.add(`${match[1]} (${file.slice(root.length + 1)})`);
            }
        }
        expect([...missing]).toEqual([]);
    });

    // The pages of the app list their navigation in app.config.ts, so the keys are not written as t('...') in the code
    it('has the texts of the navigation', () => {
        const source = readFileSync(join(process.cwd(), 'app', 'app.config.ts'), 'utf8');
        const keys = [...source.matchAll(/(?:labelKey|descriptionKey):\s*'([a-zA-Z0-9_.]+)'/g)].map(match => match[1]);
        expect(keys.length).toBeGreaterThan(0);
        expect(keys.filter(key => !(key in englishTexts))).toEqual([]);
    });

    // The app keeps the layer's texts as they are (it only adds its own), so a gizmo key here would be a typo or a stale override
    it('adds no gizmo texts the layer does not have', () => {
        expect(Object.keys(englishTexts).filter(key => key.startsWith('gizmo.') && !(key in layerTexts))).toEqual([]);
    });

    // Every error the device can send has a translation, in the app or in the layer (otherwise the dialog shows the raw message, like "ssid")
    it('translates every error ID of the device', () => {
        const root = join(process.cwd(), '..', 'microcontroller');
        const libraryRoot = join(root, '.pio', 'libdeps', 'release', 'gizmo-common', 'src');
        expect(existsSync(libraryRoot), 'gizmo-common is missing: run "pio pkg install" in microcontroller/ first').toBe(true);
        const files = [...readSourceFiles(join(root, 'src')).filter(file => !file.includes('webui')), ...readSourceFiles(libraryRoot)];
        const errorIds = new Set();
        for (const file of files.filter(file => /\.(cpp|h)$/.test(file))) {
            const source = readFileSync(file, 'utf8');
            for (const match of source.matchAll(/sendErrorResponse\([^,]+,\s*\d+,\s*"([a-z_]+)"/g)) {
                errorIds.add(match[1]);
            }
        }
        expect([...errorIds].filter(errorId => !(`api.backendErrors.${errorId}` in englishTexts) && !(`gizmo.api.backendErrors.${errorId}` in layerTexts))).toEqual([]);
    });
});