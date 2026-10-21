// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

/** Keys of the app screenshots. Files live in src/assets/screens/<locale>/. */
export type ScreenId = 'home' | 'library' | 'mods' | 'newInstance' | 'java';
export type FeatureKey = 'library' | 'mods' | 'loaders' | 'java';
export type PlatformKey = 'macos' | 'windows' | 'linux';

export interface PageMeta {
  title: string;
  description: string;
}

export interface FeatureCopy {
  title: string;
  text: string;
  /** Describes what the screenshot shows. */
  alt: string;
}

export interface PlatformCopy {
  name: string;
  detail: string;
}

/** One line of the "native" fact list, e.g. Interface: Qt Quick (QML). */
export interface FactCopy {
  label: string;
  value: string;
}

/** The shape every locale dictionary must follow. */
export interface Dictionary {
  siteName: string;
  meta: {
    home: PageMeta;
    download: PageMeta;
    notFound: PageMeta;
  };
  nav: {
    skipToContent: string;
    mainLabel: string;
    footerLabel: string;
    languageLabel: string;
    homeLabel: string;
    features: string;
    compare: string;
    faq: string;
    download: string;
    privacy: string;
    terms: string;
    github: string;
  };
  hero: {
    title: string;
    lead: string;
    /** Download button before the visitor's system is known. */
    cta: string;
    /** Download button once the visitor's system is detected. */
    ctaFor: Record<PlatformKey, string>;
    /** Shown under the button while there is no release. */
    status: string;
    /** The minimum system for each platform, under the button. */
    minimum: Record<PlatformKey, string>;
    otherSystems: string;
    screenshotAlt: string;
  };
  native: {
    title: string;
    lead: string;
    body: string;
    facts: FactCopy[];
    /** Caption above the QML excerpt. `{file}` is the file's path in the repo. */
    codeCaption: string;
  };
  features: {
    title: string;
    lead: string;
    rows: Record<FeatureKey, FeatureCopy>;
  };
  compare: {
    title: string;
    lead: string;
    /** Header of the row-label column. */
    corner: string;
    sourcesTitle: string;
  };
  faq: {
    title: string;
    items: Array<{ question: string; answer: string }>;
  };
  platformsBand: {
    title: string;
    lead: string;
  };
  download: {
    title: string;
    lead: string;
    notReleased: string;
    button: string;
    /** Accessible name of a download button. `{name}` is the platform name. */
    buttonLabel: string;
    comingSoon: string;
    platforms: Record<PlatformKey, PlatformCopy>;
    requirementsTitle: string;
    requirements: string[];
    sourceLink: string;
  };
  legal: {
    lastUpdated: string;
  };
  notFound: {
    title: string;
    text: string;
    home: string;
    download: string;
  };
  footer: {
    license: string;
    fork: string;
    trademark: string;
  };
}
