// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
import { en, type Dictionary } from './en';
import { tr } from './tr';

export const LOCALES = ['en', 'tr'] as const;
export type Locale = (typeof LOCALES)[number];
export const DEFAULT_LOCALE: Locale = 'en';

/** Routes that exist in every locale, with the same slug. */
export const ROUTES = ['/', '/download', '/privacy', '/terms'] as const;
export type Route = (typeof ROUTES)[number];

interface LocaleInfo {
  /** Shown in the language switch. */
  short: string;
  /** The language's own name, used as the accessible name of its link. */
  name: string;
  /** Value for the og:locale meta tag. */
  og: string;
}

export const LOCALE_INFO: Record<Locale, LocaleInfo> = {
  en: { short: 'EN', name: 'English', og: 'en_US' },
  tr: { short: 'TR', name: 'Türkçe', og: 'tr_TR' },
};

const DICTIONARIES: Record<Locale, Dictionary> = { en, tr };

export function getDictionary(locale: Locale): Dictionary {
  return DICTIONARIES[locale];
}

/** The URL path of a route in a locale, e.g. `localePath('tr', '/download')` is `/tr/download`. */
export function localePath(locale: Locale, route: Route = '/'): string {
  const prefix = locale === DEFAULT_LOCALE ? '' : `/${locale}`;
  if (route === '/') return prefix === '' ? '/' : `${prefix}/`;
  return `${prefix}${route}`;
}

/** Reverses `localePath`. Returns null for a path that is not one of the shared routes. */
export function parsePath(pathname: string): { locale: Locale; route: Route } | null {
  const trimmed = pathname.length > 1 ? pathname.replace(/\/+$/, '') : pathname;
  for (const locale of LOCALES) {
    for (const route of ROUTES) {
      if (localePath(locale, route).replace(/(.)\/$/, '$1') === trimmed) return { locale, route };
    }
  }
  return null;
}

/** Replaces `{key}` placeholders in a dictionary string. */
export function format(template: string, values: Record<string, string>): string {
  return template.replace(/\{(\w+)\}/g, (match, key: string) => values[key] ?? match);
}
