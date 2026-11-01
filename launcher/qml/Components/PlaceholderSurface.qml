// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What a picture slot shows when there is no real picture: a neutral tile one
 * step off the surface it sits on, with a hairline and, when the host gives one,
 * the instance's icon or a glyph centred on it (children of this item). No
 * accent tint: the accent is kept for actions, selection and links.
 */
Rectangle {
    // A raised step in the dark theme; in the light one the cards are
    // already white, so the tile steps down instead.
    color: Theme.dark ? Theme.palette.surfaceRaised : Theme.palette.surfaceSunken
    border.width: 1
    border.color: Theme.palette.border
}
