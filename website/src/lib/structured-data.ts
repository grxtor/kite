// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
import { getDictionary, localePath, type Locale } from '../i18n';

/**
 * schema.org data for the home page: the site, the app and the questions.
 * Search engines read it for the site name, the app's details and the FAQ.
 */
export function homeStructuredData(locale: Locale, site: URL): Record<string, unknown> {
  const t = getDictionary(locale);
  const home = new URL(localePath(locale, '/'), site).href;

  return {
    '@context': 'https://schema.org',
    '@graph': [
      {
        '@type': 'WebSite',
        '@id': `${site.origin}/#website`,
        name: t.siteName,
        url: new URL('/', site).href,
        inLanguage: locale,
      },
      {
        '@type': 'SoftwareApplication',
        name: t.siteName,
        url: home,
        description: t.meta.home.description,
        applicationCategory: 'GameApplication',
        operatingSystem: 'macOS, Windows, Linux',
        license: 'https://www.apache.org/licenses/LICENSE-2.0',
        downloadUrl: new URL(localePath(locale, '/download'), site).href,
        offers: { '@type': 'Offer', price: '0', priceCurrency: 'USD' },
        inLanguage: locale,
      },
      {
        '@type': 'FAQPage',
        url: home,
        inLanguage: locale,
        mainEntity: t.faq.items.map((item) => ({
          '@type': 'Question',
          name: item.question,
          acceptedAnswer: { '@type': 'Answer', text: item.answer },
        })),
      },
    ],
  };
}
