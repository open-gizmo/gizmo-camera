# Agent instructions

For AI agents (Claude Code, Codex, Copilot and others) working on this repository. The project guide, with its structure, conventions and working rules, is in [.github/copilot-instructions.md](.github/copilot-instructions.md): read it too.

## With every change

1. **Write the changelog.** When a maker would notice the change (a feature, the web app, the hardware or wiring, the setup steps, a fix), add a line under `## Unreleased` in [CHANGELOG.md](CHANGELOG.md), in the same commit. Write it for the people who flash and use the gadget, not as a commit message: what changed for them, and what they need to do (flash again, rewire, set something up again). Internal changes (refactoring, tests, CI) need no line unless they change what the gadget does.
2. **Keep the docs in sync.** Update the README (and its screenshots in `docs/` when the web app looks different), and tell the user when the gadget's page on [opengizmo.dev](https://opengizmo.dev) needs the same change.
3. **Check it before calling it done.** Run the tests (`pio test -e native` in `microcontroller/`, `npm test` in `webapp/`) and the release build (`npm run build:embedded` in `webapp/`, then `pio run -e release`).

## Releasing

Follow "Releasing a new version" in the README. The version's notes must be moved from `## Unreleased` to a `## vX.Y.Z - YYYY-MM-DD` heading in CHANGELOG.md: they become the GitHub release notes, and the release workflow stops before building anything when they are missing or when notes are left under Unreleased.

Work on the `feature` branch; changes reach `develop` and then `main` one step at a time, only for a release.
