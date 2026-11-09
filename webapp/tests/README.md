# Tests

> This test structure is shared across projects: the folders, the Vitest config and the conventions below are the same everywhere. Keep it as it is, and when something has to change, change it in every project so they stay aligned.

| Layer | Tool | Folder | What it covers |
|---|---|---|---|
| Unit and component | Vitest + `@nuxt/test-utils` (happy-dom) | `tests/app`, `tests/server` | Components, pages, composables, the mock device API, locales. Fast, precise, many tests. |
| End-to-end | Playwright + axe | `tests/e2e` | Not set up in this project yet: the web app runs on the device, so its journeys are checked on the board for now. |

## Running

```bash
npm test  # Vitest
```

The locales test also checks that every error ID of the firmware has a translation, so it reads the sources of `microcontroller/` and of gizmo-common. Run `pio pkg install -e release` in `microcontroller/` once before the first run, so gizmo-common is downloaded.

## How the tests are built

- **`tests/app`** runs inside a Nuxt app (auto-imports, i18n, `useState`) in a simulated browser. Mount components and pages with `mountSuspended`, answer their API calls with `registerEndpoint`, set state through the real stores, and assert on what the user sees.
- **`tests/server`** runs the mock device API (`server/`) in plain Node. Stub the Nitro globals (`defineEventHandler`, `readRawBody`, `setResponseStatus`) in `vi.hoisted`, then load the handlers with `await import(...)`. The mock must answer like the firmware, so these tests are its specification: change them together with the firmware.
- The library dialogs render in a portal, outside the mounted component: mock `showConfirmDialog` and `showMessageDialog` when a test is about what gets sent, and look in `document.body` when it is about the dialog itself.
- Answers from `registerEndpoint` arrive after a real fetch, so wait for their effect with `vi.waitFor` instead of a fixed number of ticks.

## Conventions

- One file per area, with a `describe` per unit and an `it` per behavior, named as lowercase sentences.
- A comment above every `describe`, `it` and step. A test for a bug fix says in its comment what the bug was.
- Create fresh state in `beforeEach`, and clean up the dialogs and mocks between tests.
