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

// The release the download buttons serve. Flip it to the next version only once that release
// is published (not while it is a draft): the site deploys from main, and an unpublished
// release's files are 404s.
const SITE_DOWNLOAD_VERSION = '1.3.0';
// macOS Apple silicon and Intel Legacy (macOS 12) separately: v1.3.0 is going out without those
// two builds. Flip this to SITE_DOWNLOAD_VERSION once the 1.3.0 arm64 and Legacy .dmg files are
// on the release.
const MAC_ARM_LEGACY_VERSION = '1.2.0';

const FILES = `https://github.com/grxtor/kite/releases/download/v${SITE_DOWNLOAD_VERSION}`;
const MAC_ARM_LEGACY_FILES = `https://github.com/grxtor/kite/releases/download/v${MAC_ARM_LEGACY_VERSION}`;

export const release: Release = {
  version: SITE_DOWNLOAD_VERSION,
  downloads: {
    macos: [
      { build: 'macosArm', url: `${MAC_ARM_LEGACY_FILES}/Kite-macOS-ARM-Qt6-v${MAC_ARM_LEGACY_VERSION}.dmg` },
      { build: 'macosIntel', url: `${FILES}/Kite-macOS-Intel-Qt6-v${SITE_DOWNLOAD_VERSION}.dmg` },
      { build: 'macosLegacy', url: `${MAC_ARM_LEGACY_FILES}/Kite-macOS-Intel-Legacy-Qt6-v${MAC_ARM_LEGACY_VERSION}.dmg` },
    ],
    windows: [
      { build: 'windowsX64', url: `${FILES}/Kite-Windows-MSVC-Qt6-Setup-v${SITE_DOWNLOAD_VERSION}.exe` },
      { build: 'windowsArm', url: `${FILES}/Kite-Windows-MSVC-Qt6-arm64-Setup-v${SITE_DOWNLOAD_VERSION}.exe` },
    ],
    linux: [
      { build: 'linuxX64', url: `${FILES}/Kite-Linux-Qt6-x86_64.AppImage` },
      { build: 'linuxArm', url: `${FILES}/Kite-Linux-Qt6-aarch64.AppImage` },
    ],
  },
  repoUrl: 'https://github.com/grxtor/kite/releases',
};
