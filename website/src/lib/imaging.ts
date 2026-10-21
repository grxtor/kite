// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

/** Widths generated for every screenshot, in CSS pixels at 1x and 2x. */
export const SCREENSHOT_WIDTHS = [640, 960, 1280, 1740, 2320];

/** Gutter on each side of the page on small screens, matching --gutter. */
const GUTTER_PX = 16;
/** Widest the hero screenshot gets, matching --hero-shot-max. */
const HERO_SHOT_PX = 1160;
/** Widest a bento card's picture area gets (the wide card on a full page). */
const BENTO_SHOT_PX = 640;
/** Viewport width from which bento cards sit side by side, matching the bento grid. */
const BENTO_BREAKPOINT_PX = 900;
/** Horizontal padding of a bento card, both sides together. */
const CARD_PADDING_PX = 64;

export const SCREENSHOT_SIZES = {
  hero: `(min-width: ${HERO_SHOT_PX + GUTTER_PX * 2}px) ${HERO_SHOT_PX}px, calc(100vw - ${GUTTER_PX * 2}px)`,
  card: `(min-width: ${BENTO_BREAKPOINT_PX}px) ${BENTO_SHOT_PX}px, calc(100vw - ${GUTTER_PX * 2 + CARD_PADDING_PX}px)`,
} as const;

/**
 * One region of a screenshot, shown enlarged. `x` and `y` are where the
 * region starts, as fractions of the picture's width and height; `zoom`
 * is how many times wider than its frame the picture is drawn; `ratio` is
 * the frame's CSS aspect ratio, e.g. '16 / 11'.
 */
export interface Focus {
  zoom: number;
  x: number;
  y: number;
  ratio: string;
  /** Fade the bottom into the card; off when the region already ends there. */
  fade?: boolean;
}

/** `sizes` for an enlarged picture: the frame's width times the zoom. */
export function focusedSizes(zoom: number): string {
  const wide = Math.round(BENTO_SHOT_PX * zoom);
  return `(min-width: ${BENTO_BREAKPOINT_PX}px) ${wide}px, calc((100vw - ${GUTTER_PX * 2 + CARD_PADDING_PX}px) * ${zoom})`;
}

/** Widths generated for full-bleed in-game backdrops. */
export const BACKDROP_WIDTHS = [960, 1600, 2560];
export const BACKDROP_SIZES = '100vw';

/** Logo is drawn at this size; the 2x file is generated for sharp screens. */
export const LOGO_SIZE_PX = 32;

/** Open Graph image size recommended by most platforms. */
export const OG_IMAGE = { width: 1200, height: 630 } as const;
