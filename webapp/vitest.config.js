// The test structure is shared across projects: keep this file as it is, and apply any change to every project
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { defineVitestProject } from '@nuxt/test-utils/config';
import { defineConfig } from 'vitest/config';

const projectRoot = dirname(fileURLToPath(import.meta.url));

export default defineConfig({
	test: {
		projects: [
			// Server and shared code: plain Node, the database is mocked with SQLite
			{
				resolve: {
					alias: {
						'~~': projectRoot,
						'~': resolve(projectRoot, 'app')
					}
				},
				test: {
					name: 'server',
					environment: 'node',
					include: ['tests/server/**/*.test.js', 'tests/shared/**/*.test.js']
				}
			},

			// Frontend: runs inside a Nuxt app (auto-imports, i18n, useState...) in a simulated browser
			await defineVitestProject({
				test: {
					name: 'app',
					environment: 'nuxt',
					include: ['tests/app/**/*.test.js'],
					hookTimeout: 120000, // Booting the Nuxt app for each file can take more than the default 10s when files run in parallel
					testTimeout: 30000, // The first mount of a component transforms its whole import tree, which can exceed the default 5s on slow machines
					environmentOptions: {
						nuxt: {
							domEnvironment: 'happy-dom'
						}
					}
				}
			})
		]
	}
});