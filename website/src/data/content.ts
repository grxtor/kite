// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
import type { ImageMetadata } from 'astro';
import type { Locale } from '../i18n';
import type { FeatureKey, PlatformKey, ScreenId } from '../i18n/types';
import type { Focus } from '../lib/imaging';

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

/**
 * Bento cards in page order: the screenshot each one shows, the region of it
 * (see Focus; measured on the 2560x1600 renders) and whether it takes the
 * wide column of its row.
 */
export const FEATURE_CARDS: ReadonlyArray<{
  key: FeatureKey;
  screen: ScreenId;
  focus: Focus;
  wide: boolean;
}> = [
  { key: 'library', screen: 'library', focus: { zoom: 1.2, x: 0.06, y: 0.01, ratio: '16 / 9' }, wide: true },
  // Ends in the one gap the toolbar has at the same place in both languages:
  // after Speed up / Hızlandır, before Add-ons / Eklentiler (x 990-992).
  { key: 'mods', screen: 'mods', focus: { zoom: 1.381, x: 0.05, y: 0.075, ratio: '4 / 3' }, wide: false },
  { key: 'loaders', screen: 'newInstance', focus: { zoom: 1.3, x: 0.115, y: 0.05, ratio: '4 / 3', fade: false }, wide: false },
  { key: 'java', screen: 'java', focus: { zoom: 1.6, x: 0.26, y: 0.08, ratio: '16 / 9' }, wide: true },
];

export const PLATFORMS: ReadonlyArray<{ key: PlatformKey }> = [
  { key: 'macos' },
  { key: 'windows' },
  { key: 'linux' },
];
