// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A small caps caption over a group of people ("PLAYING — 2"), the one use
 * of Theme's overline. The caller gives the words already in capitals, so a
 * language's own rules for them (Turkish dotted İ) are the translator's.
 */
Text {
    // The caption sits over its rows' heads, not their hover fill.
    leftPadding: Theme.space.lg
    topPadding: Theme.space.sm
    bottomPadding: Theme.space.xs
    elide: Text.ElideRight
    color: Theme.palette.textTertiary
    font.family: Theme.font.family
    font.pixelSize: Theme.type.overline.pixelSize
    font.weight: Theme.type.overline.weight
    font.letterSpacing: Theme.type.overline.letterSpacing
    Accessible.role: Accessible.Heading
}
