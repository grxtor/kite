// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What a picture slot shows when there is no real picture to put in it: a
 * calm, low-contrast surface tinted with the palette's accent, and -- when
 * the host gives one -- a single glyph or icon centred on it. Built only from
 * Theme.palette tokens, so it follows whichever scheme and theme are active;
 * nothing here is an image.
 *
 * Whatever goes in the middle is a child of this item (the default property),
 * and is centred by the host: CoverArt puts the instance's icon there, a
 * Discover pack header a Lucide glyph.
 */
Rectangle {
    id: root

    // How much accent each end of the surface carries, over the raised
    // (top) and sunken (bottom) surface colours.
    readonly property real topTint: Theme.dark ? 0.16 : 0.14
    readonly property real bottomTint: Theme.dark ? 0.05 : 0.06

    gradient: Gradient {
        GradientStop {
            position: 0.0
            color: Qt.tint(Theme.palette.surfaceRaised, Qt.alpha(Theme.palette.accent, root.topTint))
        }
        GradientStop {
            position: 1.0
            color: Qt.tint(Theme.palette.surfaceSunken, Qt.alpha(Theme.palette.accent, root.bottomTint))
        }
    }
}
