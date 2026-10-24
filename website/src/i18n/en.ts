// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
import type { Dictionary } from './types';

export type { Dictionary, FeatureKey, PlatformKey, ScreenId } from './types';

export const en: Dictionary = {
  siteName: 'Kite',
  meta: {
    home: {
      title: 'Kite: a native Minecraft launcher',
      description:
        'Kite is a Minecraft launcher written in C++ with a Qt Quick interface. Separate instances, modpacks from Modrinth, CurseForge, FTB, ATLauncher and Technic, and Java handled for you. Free and open source.',
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
    compare: 'Compare',
    faq: 'Questions',
    download: 'Download',
    privacy: 'Privacy',
    terms: 'Terms',
    github: 'GitHub',
  },
  hero: {
    eyebrow: 'A Minecraft launcher',
    title: ['Weighs nothing.', 'Holds everything.'],
    titleAccent: 'nothing.',
    lead: 'Written in C++ and Qt Quick, not a web page in a window. Modpacks from Modrinth, CurseForge, FTB, ATLauncher and Technic, each in an instance of its own.',
    explore: 'Look inside',
    cta: 'Download Kite',
    ctaFor: {
      macos: 'Download for macOS',
      windows: 'Download for Windows',
      linux: 'Download for Linux',
    },
    status: 'First release coming soon',
    minimum: {
      macos: 'macOS 12 or later',
      windows: 'Windows 10 and 11',
      linux: 'AppImage, x86-64 and ARM64',
    },
    otherSystems: 'Other systems',
    screenshotAlt:
      'Kite’s Home screen with real instances: MC-26.3-Animations ready to play, a Continue playing list with Zombiemania and three Minecraft versions, and recent worlds on the right.',
  },
  native: {
    title: 'Native, not a web page',
    lead: 'Many launchers today are web apps in a window. Kite’s interface is written in QML, Qt’s language for user interfaces, and Qt Quick draws it with your graphics card: Metal on macOS, Direct3D on Windows, OpenGL on Linux.',
    body: 'Underneath is C++ from MultiMC, the same family Prism Launcher comes from. Kite forked MeshMC in 2026 and gave it a new interface, written from scratch.',
    facts: [
      { label: 'Interface', value: 'Qt Quick (QML)' },
      { label: 'Core', value: 'C++17 and Qt 6' },
      { label: 'Drawn with', value: 'Metal, Direct3D or OpenGL' },
      { label: 'Written for Kite', value: '216 QML files, 35,000 lines' },
      { label: 'License', value: 'Apache 2.0' },
    ],
    codeCaption: 'The Play button, from {file}',
  },
  features: {
    title: 'Built around instances',
    lead: 'An instance is one copy of Minecraft with its own mods, worlds, resource packs and Java settings. A plain 26.3, a 437-mod pack and a snapshot sit side by side without touching each other.',
    rows: {
      library: {
        title: 'Your whole library',
        text: 'Instances wear their modpack’s logo, or an icon cut from your own game’s textures. Sort them, group them, and see how long you’ve played each one.',
        alt: 'Kite’s Library: ten real instances in a grid, including ATM10, Create: Skies and Seas, DeceasedCraft, Fabulously Optimized, Fresh & Smooth and Zombiemania, with 58 hours of total play time in the header.',
      },
      mods: {
        title: '437 mods, one list',
        text: 'This is All the Mods 10. Switch a mod off, check every mod for updates at once, or add more from Modrinth and CurseForge without opening a folder.',
        alt: 'The Content tab of the ATM10 instance, listing 437 mods with their versions and an on and off switch for each, under buttons to check for updates and add content.',
      },
      loaders: {
        title: 'Any loader, any version',
        text: 'Pick a Minecraft version, then Fabric, Quilt, Forge or NeoForge. Kite downloads the game, the loader and every library it needs.',
        alt: 'The New instance dialog with a name field, the Minecraft versions from 26.3 down to 1.21.11, and a row of mod loaders: Vanilla, Fabric, Quilt, Forge and NeoForge.',
      },
      java: {
        title: 'Java, handled',
        text: 'Each Minecraft version wants a different Java. Kite picks the right one and downloads it if you don’t have it. Memory is a slider.',
        alt: 'The Java and memory settings page, with a slider for maximum memory set to 6 GB, a starting memory field and the Java runtime in use.',
      },
    },
  },
  compare: {
    title: 'How Kite compares',
    lead: 'Checked against each launcher’s own website, source code and privacy policy in October 2026.',
    corner: 'Launcher',
    sourcesTitle: 'Sources',
  },
  faq: {
    title: 'Questions',
    items: [
      {
        question: 'Do I need to own Minecraft?',
        answer:
          'Yes. Kite signs you in with the Microsoft account that owns Minecraft: Java Edition. Without one, Kite can only start the game’s demo.',
      },
      {
        question: 'Is Kite really free?',
        answer:
          'Yes. No ads, no paid tier and no account with us. Kite is licensed under the Apache License 2.0, and its source code goes public with the first release.',
      },
      {
        question: 'Can I bring my instances from Prism or MultiMC?',
        answer:
          'Export an instance as a zip from your old launcher and import that zip in Kite. Modrinth .mrpack files, CurseForge modpack zips and Technic packs import the same way.',
      },
      {
        question: 'Will Minecraft run faster?',
        answer:
          'Kite is a launcher, so your frame rate comes from the game and the mods you install, such as Sodium. What Kite does is pick the right Java for each version and give the game the memory you set.',
      },
      {
        question: 'Is my Microsoft account safe?',
        answer:
          'You sign in on Microsoft’s own page, in your browser, so Kite never sees your password. The sign-in tokens stay on your computer, and the privacy policy lists everything Kite sends and where.',
      },
      {
        question: 'Which systems does it run on?',
        answer:
          'macOS 12 or later on Apple silicon and Intel, Windows 10 and 11 on x64 and Arm, and Linux as an AppImage for x86-64 and ARM64.',
      },
    ],
  },
  platformsBand: {
    title: 'Get Kite',
    lead: 'Free for macOS, Windows and Linux.',
  },
  download: {
    title: 'Download Kite',
    lead: 'Kite is free. Pick your system.',
    notReleased: "Kite's first release isn't out yet. Download links will appear here when it is.",
    button: 'Download',
    buttonLabel: 'Download Kite for {name}',
    comingSoon: 'Coming soon',
    platforms: {
      macos: { name: 'macOS', detail: 'Apple silicon and Intel, macOS 12 or later' },
      windows: { name: 'Windows', detail: 'Windows 10 and 11, x64 and Arm' },
      linux: { name: 'Linux', detail: 'AppImage, x86-64 and ARM64' },
    },
    requirementsTitle: 'What you need',
    requirements: [
      'A Microsoft account that owns Minecraft: Java Edition',
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
    trademark: 'Minecraft is a trademark of Mojang Synergies AB.',
  },
};
