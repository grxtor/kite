// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

/** Widths generated for every screenshot, in CSS pixels at 1x and 2x. */
export const SCREENSHOT_WIDTHS = [640, 960, 1280, 1740, 2320];

/** Gutter on each side of the page on small screens, matching --gutter. */
const GUTTER_PX = 16;
/** Breakpoint above which the hero screenshot stops filling the viewport. */
const HERO_MAX_PX = 1160;
/** Width of the screenshot column in a feature row on wide screens. */
const ROW_COLUMN_PX = 640;
/** Viewport width from which feature rows sit side by side, matching --bp-row. */
const ROW_BREAKPOINT_PX = 900;

export const SCREENSHOT_SIZES = {
  hero: `(min-width: ${HERO_MAX_PX + GUTTER_PX * 2}px) ${HERO_MAX_PX}px, calc(100vw - ${GUTTER_PX * 2}px)`,
  row: `(min-width: ${ROW_BREAKPOINT_PX}px) ${ROW_COLUMN_PX}px, calc(100vw - ${GUTTER_PX * 2}px)`,
} as const;

/** Logo is drawn at this size; the 2x file is generated for sharp screens. */
export const LOGO_SIZE_PX = 32;

/** Open Graph image size recommended by most platforms. */
export const OG_IMAGE = { width: 1200, height: 630 } as const;
