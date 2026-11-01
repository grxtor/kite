// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

/** Rows of a cloud on its grid: x, y, width, height in cells. */
export type CloudRow = readonly [number, number, number, number];

/** Minecraft-style cloud outlines, drawn as flat slabs on a grid. */
export const CLOUD_SHAPES = {
  long: [[3, 0, 14, 2], [0, 2, 30, 2], [6, 4, 29, 2]],
  short: [[4, 0, 12, 2], [0, 2, 28, 3]],
  wide: [[10, 0, 16, 2], [0, 2, 40, 2], [4, 4, 22, 2]],
  bank: [[6, 0, 20, 2], [0, 2, 44, 2], [12, 4, 26, 2]],
} as const satisfies Record<string, readonly CloudRow[]>;

export type CloudShape = keyof typeof CLOUD_SHAPES;

/** One cloud on a section. */
export interface CloudSpot {
  shape: CloudShape;
  /** Position and width as CSS lengths, relative to the positioned parent. */
  left: string;
  top: string;
  width: string;
  opacity: number;
  /** Seconds for one drift out and back. */
  drift: number;
  /** Left out on phones, where the copy needs the room. */
  wideOnly?: boolean;
}
