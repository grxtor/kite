// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

/** Widths generated for every screenshot, in CSS pixels at 1x and 2x. */
export const SCREENSHOT_WIDTHS = [640, 960, 1280, 1740, 2320];

/** Gutter on each side of the page on small screens, matching --gutter. */
const GUTTER_PX = 16;
/** Width of the hero screenshot once it sits beside the headline. */
const HERO_SHOT_PX = 900;
/** Viewport width from which the hero splits into text and screenshot, matching the hero's media query. */
const HERO_BREAKPOINT_PX = 1080;
/** Width of a screenshot inside a feature card when the cards sit two to a row. */
const CARD_SHOT_PX = 540;
/** Viewport width from which feature cards sit two to a row, matching the features grid. */
const CARD_BREAKPOINT_PX = 900;
/** Horizontal padding of a feature card, both sides together, on small screens. */
const CARD_PADDING_PX = 64;

export const SCREENSHOT_SIZES = {
  hero: `(min-width: ${HERO_BREAKPOINT_PX}px) ${HERO_SHOT_PX}px, calc(100vw - ${GUTTER_PX * 2}px)`,
  card: `(min-width: ${CARD_BREAKPOINT_PX}px) ${CARD_SHOT_PX}px, calc(100vw - ${GUTTER_PX * 2 + CARD_PADDING_PX}px)`,
} as const;

/** Widths generated for full-bleed in-game backdrops. */
export const BACKDROP_WIDTHS = [960, 1600, 2560];
export const BACKDROP_SIZES = '100vw';

/** Logo is drawn at this size; the 2x file is generated for sharp screens. */
export const LOGO_SIZE_PX = 32;

/** Open Graph image size recommended by most platforms. */
export const OG_IMAGE = { width: 1200, height: 630 } as const;
