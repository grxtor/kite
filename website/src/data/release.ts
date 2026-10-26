// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
import type { BuildKey, PlatformKey } from '../i18n/en';

export interface ReleaseFile {
  build: BuildKey;
  url: string;
}

export interface Release {
  version: string | null;
  /**
   * Each system's builds, the one most people need first: that one gets the
   * card's button, the others a link under it. Null while a system has no
   * published build.
   */
  downloads: Record<PlatformKey, readonly ReleaseFile[] | null>;
  repoUrl: string | null;
}

const VERSION = '1.1.0';
const FILES = `https://github.com/grxtor/kite/releases/download/v${VERSION}`;

export const release: Release = {
  version: VERSION,
  downloads: {
    macos: [
      { build: 'macosArm', url: `${FILES}/Kite-macOS-ARM-Qt6-v${VERSION}.dmg` },
      { build: 'macosIntel', url: `${FILES}/Kite-macOS-Intel-Qt6-v${VERSION}.dmg` },
      { build: 'macosLegacy', url: `${FILES}/Kite-macOS-Intel-Legacy-Qt6-v${VERSION}.dmg` },
    ],
    windows: [
      { build: 'windowsX64', url: `${FILES}/Kite-Windows-MSVC-Qt6-Setup-v${VERSION}.exe` },
      { build: 'windowsArm', url: `${FILES}/Kite-Windows-MSVC-Qt6-arm64-Setup-v${VERSION}.exe` },
    ],
    linux: [
      { build: 'linuxX64', url: `${FILES}/Kite-Linux-Qt6-x86_64.AppImage` },
      { build: 'linuxArm', url: `${FILES}/Kite-Linux-Qt6-aarch64.AppImage` },
    ],
  },
  repoUrl: 'https://github.com/grxtor/kite/releases',
};
