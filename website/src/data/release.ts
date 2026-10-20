// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
import type { PlatformKey } from '../i18n/en';

export interface Release {
  version: string | null;
  downloads: Record<PlatformKey, string | null>;
  repoUrl: string | null;
}

/** Everything is null until the first release is published. */
export const release: Release = {
  version: null,
  downloads: { macos: null, windows: null, linux: null },
  repoUrl: null,
};
