// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
import type { ImageMetadata } from 'astro';
import type { Locale } from '../i18n';
import type { FeatureKey, HighlightKey, PlatformKey, ScreenId } from '../i18n/en';
import type { IconName } from '../lib/icons';

import enHome from '../assets/screens/en/home.webp';
import enMods from '../assets/screens/en/mods.webp';
import enNewInstance from '../assets/screens/en/new-instance.webp';
import enAccounts from '../assets/screens/en/accounts.webp';
import enJava from '../assets/screens/en/java.webp';
import trHome from '../assets/screens/tr/home.webp';
import trMods from '../assets/screens/tr/mods.webp';
import trNewInstance from '../assets/screens/tr/new-instance.webp';
import trAccounts from '../assets/screens/tr/accounts.webp';
import trJava from '../assets/screens/tr/java.webp';

export const SCREENS: Record<Locale, Record<ScreenId, ImageMetadata>> = {
  en: { home: enHome, mods: enMods, newInstance: enNewInstance, accounts: enAccounts, java: enJava },
  tr: { home: trHome, mods: trMods, newInstance: trNewInstance, accounts: trAccounts, java: trJava },
};

/** Feature rows in page order, with the screenshot each one shows. */
export const FEATURE_ROWS: ReadonlyArray<{ key: FeatureKey; screen: ScreenId }> = [
  { key: 'mods', screen: 'mods' },
  { key: 'loaders', screen: 'newInstance' },
  { key: 'accounts', screen: 'accounts' },
  { key: 'java', screen: 'java' },
];

export const HIGHLIGHTS: ReadonlyArray<{ key: HighlightKey; icon: IconName }> = [
  { key: 'worlds', icon: 'image' },
  { key: 'backups', icon: 'archive' },
  { key: 'imports', icon: 'package' },
  { key: 'language', icon: 'languages' },
  { key: 'openSource', icon: 'code' },
  { key: 'playTime', icon: 'clock' },
];

export const PLATFORMS: ReadonlyArray<{ key: PlatformKey; icon: IconName }> = [
  { key: 'macos', icon: 'laptop' },
  { key: 'windows', icon: 'monitor' },
  { key: 'linux', icon: 'terminal' },
];
