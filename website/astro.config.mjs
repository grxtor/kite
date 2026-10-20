// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
import { defineConfig } from 'astro/config';

export default defineConfig({
  site: 'https://kiteclient.com',
  output: 'static',
  trailingSlash: 'ignore',
  build: { format: 'directory' },
  i18n: {
    defaultLocale: 'en',
    locales: ['en', 'tr'],
    routing: { prefixDefaultLocale: false },
  },
});
