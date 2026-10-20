// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
//
// The comparison table on the home page. Every cell was checked on
// 2026-10-20 against the sources below (Prism Launcher 11.1.1, Modrinth App
// 0.21.10, CurseForge app 1.324). Re-check them before changing a cell.
import type { Locale } from '../i18n';

export type LauncherId = 'kite' | 'prism' | 'modrinth' | 'curseforge';

export const LAUNCHERS: ReadonlyArray<{ id: LauncherId; name: string }> = [
  { id: 'kite', name: 'Kite' },
  { id: 'prism', name: 'Prism Launcher' },
  { id: 'modrinth', name: 'Modrinth App' },
  { id: 'curseforge', name: 'CurseForge' },
];

export interface CompareRow {
  label: string;
  cells: Record<LauncherId, string>;
}

export const COMPARE: Record<Locale, { rows: CompareRow[] }> = {
  en: {
    rows: [
      {
        label: 'Interface',
        cells: {
          kite: 'Qt Quick (QML), C++',
          prism: 'Qt Widgets, C++',
          modrinth: 'Tauri: a Vue web page in the system web view',
          curseforge: 'Electron, with its own copy of Chromium',
        },
      },
      {
        label: 'Source code',
        cells: {
          kite: 'Apache 2.0, public from the first release',
          prism: 'Open, GPL-3.0',
          modrinth: 'Open, GPL-3.0',
          curseforge: 'Closed',
        },
      },
      {
        label: 'Ads',
        cells: {
          kite: 'None',
          prism: 'None',
          modrinth: 'Yes, removed with Modrinth+',
          curseforge: 'Yes, removed with Premium',
        },
      },
      {
        label: 'Usage analytics',
        cells: {
          kite: 'None',
          prism: 'None in its code',
          modrinth: 'On by default, can be turned off',
          curseforge: 'Yes, under Overwolf’s privacy policy',
        },
      },
      {
        label: 'Browse and install from',
        cells: {
          kite: 'Modrinth, CurseForge, FTB, ATLauncher, Technic',
          prism: 'Modrinth, CurseForge, FTB, ATLauncher, Technic',
          modrinth: 'Modrinth',
          curseforge: 'CurseForge',
        },
      },
      {
        label: 'Pack files it installs',
        cells: {
          kite: '.mrpack, CurseForge zip, MultiMC and Prism zip, Technic',
          prism: '.mrpack, CurseForge zip, MultiMC and Prism zip, Technic',
          modrinth: '.mrpack, plus instances from other launchers',
          curseforge: 'CurseForge packs and profile codes',
        },
      },
    ],
  },
  tr: {
    rows: [
      {
        label: 'Arayüz',
        cells: {
          kite: 'Qt Quick (QML), C++',
          prism: 'Qt Widgets, C++',
          modrinth: 'Tauri: sistem web görünümünde bir Vue sayfası',
          curseforge: 'Electron, kendi Chromium kopyasıyla',
        },
      },
      {
        label: 'Kaynak kod',
        cells: {
          kite: 'Apache 2.0, ilk sürümle herkese açık',
          prism: 'Açık, GPL-3.0',
          modrinth: 'Açık, GPL-3.0',
          curseforge: 'Kapalı',
        },
      },
      {
        label: 'Reklam',
        cells: {
          kite: 'Yok',
          prism: 'Yok',
          modrinth: 'Var, Modrinth+ ile kalkar',
          curseforge: 'Var, Premium ile kalkar',
        },
      },
      {
        label: 'Kullanım analitiği',
        cells: {
          kite: 'Yok',
          prism: 'Kodunda yok',
          modrinth: 'Varsayılan olarak açık, kapatılabilir',
          curseforge: 'Var, Overwolf’un gizlilik politikasına göre',
        },
      },
      {
        label: 'İçinden kurulan yerler',
        cells: {
          kite: 'Modrinth, CurseForge, FTB, ATLauncher, Technic',
          prism: 'Modrinth, CurseForge, FTB, ATLauncher, Technic',
          modrinth: 'Modrinth',
          curseforge: 'CurseForge',
        },
      },
      {
        label: 'Kurduğu paket dosyaları',
        cells: {
          kite: '.mrpack, CurseForge zip, MultiMC ve Prism zip, Technic',
          prism: '.mrpack, CurseForge zip, MultiMC ve Prism zip, Technic',
          modrinth: '.mrpack ve başka başlatıcılardaki kurulumlar',
          curseforge: 'CurseForge paketleri ve profil kodları',
        },
      },
    ],
  },
};

/** Where each cell above was checked. Labels stay in English. */
export const COMPARE_SOURCES: ReadonlyArray<{ label: string; url: string }> = [
  {
    label: 'Prism Launcher: CMakeLists.txt (Qt Widgets)',
    url: 'https://github.com/PrismLauncher/PrismLauncher/blob/develop/CMakeLists.txt',
  },
  {
    label: 'Prism Launcher: COPYING.md (GPL-3.0)',
    url: 'https://github.com/PrismLauncher/PrismLauncher/blob/develop/COPYING.md',
  },
  {
    label: 'Prism Launcher: InstanceImportTask.cpp (pack formats)',
    url: 'https://github.com/PrismLauncher/PrismLauncher/blob/develop/launcher/InstanceImportTask.cpp',
  },
  {
    label: 'Modrinth App: Cargo.toml (Tauri)',
    url: 'https://github.com/modrinth/code/blob/main/apps/app/Cargo.toml',
  },
  {
    label: 'Modrinth App: app-frontend package.json (Vue)',
    url: 'https://github.com/modrinth/code/blob/main/apps/app-frontend/package.json',
  },
  {
    label: 'Modrinth App: COPYING.md (GPL-3.0)',
    url: 'https://github.com/modrinth/code/blob/main/apps/app/COPYING.md',
  },
  { label: 'Modrinth+ (ad-free)', url: 'https://modrinth.com/plus' },
  {
    label: 'Modrinth App: telemetry setting default',
    url: 'https://github.com/modrinth/code/blob/main/packages/app-lib/migrations/20240711194701_init.sql',
  },
  { label: 'Modrinth privacy policy', url: 'https://modrinth.com/legal/privacy' },
  { label: 'CurseForge on Overwolf (Electron build)', url: 'https://overwolf.com/app/overwolf-curseforge' },
  { label: 'CurseForge app download (ads, Premium)', url: 'https://www.curseforge.com/download/app' },
  {
    label: 'Overwolf platform privacy policy',
    url: 'https://legal.overwolf.com/docs/overwolf/platform/platform-privacy-policy/',
  },
];
