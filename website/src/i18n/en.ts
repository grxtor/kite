// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

/** Keys of the five app screenshots. Files live in src/assets/screens/<locale>/. */
export type ScreenId = 'home' | 'mods' | 'newInstance' | 'accounts' | 'java';
export type FeatureKey = 'mods' | 'loaders' | 'accounts' | 'java';
export type HighlightKey =
  | 'worlds'
  | 'backups'
  | 'imports'
  | 'language'
  | 'openSource'
  | 'playTime';
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

export interface HighlightCopy {
  title: string;
  text: string;
}

export interface PlatformCopy {
  name: string;
  detail: string;
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
    download: string;
    privacy: string;
    terms: string;
    github: string;
  };
  hero: {
    title: string;
    lead: string;
    primaryCta: string;
    secondaryCta: string;
    note: string;
    screenshotAlt: string;
  };
  features: {
    title: string;
    intro: { title: string; text: string };
    rows: Record<FeatureKey, FeatureCopy>;
  };
  highlights: Record<HighlightKey, HighlightCopy>;
  closing: {
    title: string;
    text: string;
    cta: string;
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
    disclaimer: string;
    /** The same disclaimer in the page language. Null when the page is already English. */
    disclaimerLocal: string | null;
    trademark: string;
  };
}

export const en: Dictionary = {
  siteName: 'Kite',
  meta: {
    home: {
      title: 'Kite: a Minecraft launcher built around instances',
      description:
        'Kite keeps each install of Minecraft in its own instance, with its own mods, worlds and settings. Free and open source for macOS, Windows and Linux.',
    },
    download: {
      title: 'Download Kite',
      description:
        'Download Kite, the free and open source Minecraft launcher for macOS, Windows and Linux.',
    },
    notFound: {
      title: 'Page not found',
      description: 'This page does not exist.',
    },
  },
  nav: {
    skipToContent: 'Skip to content',
    mainLabel: 'Main',
    footerLabel: 'Footer',
    languageLabel: 'Language',
    homeLabel: 'Kite home',
    features: 'Features',
    download: 'Download',
    privacy: 'Privacy',
    terms: 'Terms',
    github: 'GitHub',
  },
  hero: {
    title: 'Every Minecraft, in its own place.',
    lead: 'Kite keeps each install of the game in its own instance, with its own mods, worlds and settings. Pick a version, add a modpack, sign in with Microsoft, and play.',
    primaryCta: 'Download Kite',
    secondaryCta: 'See what it does',
    note: 'Free and open source · macOS, Windows and Linux',
    screenshotAlt:
      'The Kite home screen: the last instance played with a Play button, a list of instances to continue, recent worlds on the right and the library below.',
  },
  features: {
    title: 'Built around instances',
    intro: {
      title: 'Nothing breaks the others',
      text: 'Vanilla 1.21, a 200-mod pack and a snapshot can sit side by side. Each instance has its own mods, worlds, resource packs and Java settings.',
    },
    rows: {
      mods: {
        title: 'Mods without folder digging',
        text: 'See every mod in an instance, switch one off, update it, or add more from Modrinth without leaving Kite.',
        alt: 'The Content tab of a Fabric instance listing six mods with their versions and an on and off switch for each, above buttons to check for updates and add content.',
      },
      loaders: {
        title: 'Any loader, any version',
        text: 'Pick a Minecraft version and Fabric, Quilt, Forge or NeoForge when you create an instance. Kite downloads the rest.',
        alt: 'The New instance dialog with a name field, a list of Minecraft versions and a row of mod loaders: Vanilla, Fabric, Quilt, Forge and NeoForge.',
      },
      accounts: {
        title: 'Your accounts and skins',
        text: 'Sign in with your Microsoft account, keep more than one, and change your skin and cape from the launcher.',
        alt: 'The skin and cape dialog on the Accounts screen, with a skin preview, a grid of default skins and a choice of capes.',
      },
      java: {
        title: 'Java, handled',
        text: 'Kite finds a Java runtime that fits the version you play and lets you set memory with a slider.',
        alt: 'Settings on the Java and memory page, with a slider for maximum memory set to 6 GB, a starting memory field and the Java runtime path.',
      },
    },
  },
  highlights: {
    worlds: {
      title: 'Worlds and screenshots',
      text: 'Each instance lists its own.',
    },
    backups: {
      title: 'Backups before launch',
      text: 'Optional, and you choose it in settings.',
    },
    imports: {
      title: 'Imports your packs',
      text: 'Modrinth .mrpack, CurseForge, MultiMC and Prism zips.',
    },
    language: {
      title: 'Your language',
      text: 'English, Turkish and more.',
    },
    openSource: {
      title: 'Open source',
      text: 'Apache 2.0, built on MeshMC and MultiMC.',
    },
    playTime: {
      title: 'Play time and recent worlds',
      text: 'Home shows where you left off.',
    },
  },
  closing: {
    title: 'Get Kite',
    text: 'Free, no account needed to install.',
    cta: 'Download Kite',
  },
  download: {
    title: 'Download Kite',
    lead: 'Kite is free. Pick your system.',
    notReleased: "Kite's first release isn't out yet. Download links will appear here when it is.",
    button: 'Download',
    buttonLabel: 'Download Kite for {name}',
    comingSoon: 'Coming soon',
    platforms: {
      macos: { name: 'macOS', detail: 'Apple silicon and Intel' },
      windows: { name: 'Windows', detail: 'Windows 10 and 11, 64-bit' },
      linux: { name: 'Linux', detail: 'AppImage, 64-bit' },
    },
    requirementsTitle: 'What you need',
    requirements: [
      'A Minecraft: Java Edition account (Microsoft) to play',
      'A 64-bit system',
      'An internet connection the first time an instance downloads the game',
    ],
    sourceLink: 'Source code and older versions',
  },
  legal: {
    lastUpdated: 'Last updated',
  },
  notFound: {
    title: 'Page not found',
    text: "This page doesn't exist. It may have moved.",
    home: 'Go to the home page',
    download: 'Download Kite',
  },
  footer: {
    license: '© 2026 grxtor. Kite is licensed under the Apache License 2.0.',
    fork: 'Kite is a fork of MeshMC by Project Tick, which continues MultiMC.',
    disclaimer:
      'NOT AN OFFICIAL MINECRAFT PRODUCT. NOT APPROVED BY OR ASSOCIATED WITH MOJANG OR MICROSOFT.',
    disclaimerLocal: null,
    trademark: 'Minecraft is a trademark of Mojang Synergies AB.',
  },
};
