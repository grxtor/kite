// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
import type { APIRoute } from 'astro';
import { DEFAULT_LOCALE, LOCALES, ROUTES, localePath, type Locale, type Route } from '../i18n';

/** Every page in every locale, each listing its translations, as BaseLayout's hreflang links do. */
export const GET: APIRoute = ({ site }) => {
  const base = site ?? new URL('https://kiteclient.com');
  const href = (locale: Locale, route: Route) => new URL(localePath(locale, route), base).href;
  const alternate = (hreflang: string, url: string) =>
    `    <xhtml:link rel="alternate" hreflang="${hreflang}" href="${url}"/>`;

  const entries = ROUTES.flatMap((route) =>
    LOCALES.map((locale) =>
      [
        '  <url>',
        `    <loc>${href(locale, route)}</loc>`,
        ...LOCALES.map((code) => alternate(code, href(code, route))),
        alternate('x-default', href(DEFAULT_LOCALE, route)),
        '  </url>',
      ].join('\n'),
    ),
  );

  const xml = [
    '<?xml version="1.0" encoding="UTF-8"?>',
    '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" xmlns:xhtml="http://www.w3.org/1999/xhtml">',
    ...entries,
    '</urlset>',
    '',
  ].join('\n');

  return new Response(xml, { headers: { 'Content-Type': 'application/xml; charset=utf-8' } });
};
