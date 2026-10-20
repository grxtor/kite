// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
import type { ImageMetadata } from 'astro';
import type { Locale } from '../i18n';
import type { FeatureKey, PlatformKey, ScreenId } from '../i18n/types';
import type { IconName } from '../lib/icons';

// Rendered from a copy of a real Kite data folder, with the accounts removed.
import enHome from '../assets/screens/en/home.webp';
import enLibrary from '../assets/screens/en/library.webp';
import enMods from '../assets/screens/en/mods.webp';
import enNewInstance from '../assets/screens/en/new-instance.webp';
import enJava from '../assets/screens/en/java.webp';
import trHome from '../assets/screens/tr/home.webp';
import trLibrary from '../assets/screens/tr/library.webp';
import trMods from '../assets/screens/tr/mods.webp';
import trNewInstance from '../assets/screens/tr/new-instance.webp';
import trJava from '../assets/screens/tr/java.webp';

export const SCREENS: Record<Locale, Record<ScreenId, ImageMetadata>> = {
  en: { home: enHome, library: enLibrary, mods: enMods, newInstance: enNewInstance, java: enJava },
  tr: { home: trHome, library: trLibrary, mods: trMods, newInstance: trNewInstance, java: trJava },
};

/**
 * An in-game screenshot behind the hero, taken in the player's own world.
 * Null until there is one: the hero then sits on the plain canvas.
 */
export const HERO_BACKDROP: ImageMetadata | null = null;

/** Feature rows in page order, with the screenshot each one shows. */
export const FEATURE_ROWS: ReadonlyArray<{ key: FeatureKey; screen: ScreenId }> = [
  { key: 'library', screen: 'library' },
  { key: 'mods', screen: 'mods' },
  { key: 'loaders', screen: 'newInstance' },
  { key: 'java', screen: 'java' },
];

export const PLATFORMS: ReadonlyArray<{ key: PlatformKey; icon: IconName }> = [
  { key: 'macos', icon: 'laptop' },
  { key: 'windows', icon: 'monitor' },
  { key: 'linux', icon: 'terminal' },
];
